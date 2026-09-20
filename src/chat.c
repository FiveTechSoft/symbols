/* ============================================================
   chat: symbolic conversational engine (no tensors, no
   backprop, no external LLM). Three stages over the C motor:

     1. INTENT PARSER  (slot-frames EN + ES)
          "quien fue el padre de X"       -> PARENT_OF(X)
          "quienes son los hijos de X"    -> CHILDREN(X)
          "es A hijo de B"                -> IS_PARENT(A,B) boolean
          "quien es el abuelo de X"       -> GRANDPARENT(X) 2-hop
          "quien es el descendiente de X" -> DESCENDANT(X) 2-hop
          "por que A es hijo de B"        -> WHY(A,B): proof chain
          "quienes son sus hijos"         -> anaphora to focus
     2. NUCLEO SIMBOLICO (schema+meta+transfer, frozen layers)
     3. NLG (surface templates ES) with honest UNKNOWN.

   Evidence policy (fail-closed, mirrors the frozen layers):
     - direct edge: pair evidence (1-hop, from ingest)
     - derived:     TRANSITIVE meta + both links in the KB (2-hop)
     - else:        "sin constancia" (UNKNOWN, never invented)
   ============================================================ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "schema.h"
#include "metaschema.h"
#include "learn.h"
#include "transfer.h"
#include "chat.h"
#include "tool_contract.h"
#include "c_rules.h"
#include "qa_layer.h"
#include "model.h"
#include "commonsense.h"
#include "persona.h"


#define CHAT_MAX_TOKS 16

/* struct CHAT_ is defined in chat.h (shared with tests) */

/* ASCII-fold one character in place: Latin-1 accents and UTF-8
   two-byte encodings collapse to the bare letter (the corpus
   lexicon is accent-free, so surface noise must fold before
   matching). Returns bytes consumed (1 or 2) and writes 1 byte. */
static int FoldChar(const char *s, char *out)
{
    unsigned char c = (unsigned char)s[0];
    /* UTF-8 two-byte lead 0xC3: the second byte selects the letter
       (A1 a, A9 e, AD i, B1 n, B3 o, BA u, BC u; same letters +0x20
       shifted for the uppercase forms 81..9F) */
    if (c == 0xC3 && (unsigned char)s[1] >= 0x80)
    {
        switch ((unsigned char)s[1])
        {
        case 0x81: case 0xA1: *out = 'a'; break;
        case 0x89: case 0xA9: *out = 'e'; break;
        case 0x8D: case 0xAD: *out = 'i'; break;
        case 0x91: case 0xB1: *out = 'n'; break;
        case 0x93: case 0xB3: *out = 'o'; break;
        case 0x9A: case 0xBA: *out = 'u'; break;
        case 0x9C: case 0xBC: *out = 'u'; break;
        default:
            *out = '?';
            break;
        }
        return 2;
    }
    switch (c)
    {
    case 0xE1: case 0xC1: *out = 'a'; break; /* Latin-1 a-acute */
    case 0xE9: case 0xC9: *out = 'e'; break; /* Latin-1 e-acute */
    case 0xED: case 0xCD: *out = 'i'; break; /* Latin-1 i-acute */
    case 0xF3: case 0xD3: *out = 'o'; break; /* Latin-1 o-acute */
    case 0xFA: case 0xDA: *out = 'u'; break; /* Latin-1 u-acute */
    case 0xFC: case 0xDC: *out = 'u'; break; /* Latin-1 u-diaeresis */
    case 0xF1: case 0xD1: *out = 'n'; break; /* Latin-1 n-tilde */
    /* CP850 (DOS OEM Spanish/Western Europe) */
    case 0x82: case 0x90: *out = 'e'; break; /* CP850 e-acute */
    case 0xA0:            *out = 'a'; break; /* CP850 a-acute */
    case 0xA1:            *out = 'i'; break; /* CP850 i-acute */
    case 0xA2:            *out = 'o'; break; /* CP850 o-acute */
    case 0xA3:            *out = 'u'; break; /* CP850 u-acute */
    case 0x81: case 0x9A: *out = 'u'; break; /* CP850 u-diaeresis */
    case 0xA4: case 0xA5: *out = 'n'; break; /* CP850 n-tilde */
    default:
        *out = (char)tolower(c);
        if (c >= 0x80)
            *out = '?'; /* any other high byte: not corpus vocab */
        break;
    }
    return 1;
}


/* ---- FASE 4 CanonicalizeQuery: surface flags (SURFACE_FLAGS lives
   in chat.h) + canonical tokens. Punctuation is signal, not
   content: trailing ASCII punctuation is peeled off each raw token
   and recorded as flags (question/comma/period); a leading inverted
   question mark (UTF-8 C2 BF) marks interrogative force and is
   dropped; "del" expands to "de"+"el"; a trailing "'s" detaches
   into its own token and marks the genitive flag. FoldChar itself
   is untouched. */

/* split on whitespace, lowercase + accent-folded (mirrors learn.c
   contract; surface noise like "reinó"/"reyó" folds to corpus form) */
static uint32_t Split(const char *line, char toks[][CHAT_TOKEN_MAX],
                      uint32_t max_toks, SURFACE_FLAGS *sf)
{
    uint32_t n = 0;
    const char *p = line;
    if (sf)
        memset(sf, 0, sizeof(*sf));
    while (*p && n < max_toks)
    {
        while (*p && isspace((unsigned char)*p))
            p++;
        if (!*p)
            break;
        const char *start = p;
        while (*p && !isspace((unsigned char)*p))
            p++;
        size_t len = (size_t)(p - start);
        if (len == 0)
            continue;
        /* leading inverted question marks (UTF-8 C2 BF or Latin-1 BF) and inverted
           exclamation marks (UTF-8 C2 A1 or Latin-1 A1, possibly repeated):
           interrogative/exclamative force, never token content */
        while (len >= 1)
        {
            if (len >= 2 && (unsigned char)start[0] == 0xC2 &&
                ((unsigned char)start[1] == 0xBF || (unsigned char)start[1] == 0xA1))
            {
                if ((unsigned char)start[1] == 0xBF && sf)
                    sf->question = 1;
                start += 2;
                len -= 2;
            }
            else if ((unsigned char)start[0] == 0xBF || (unsigned char)start[0] == 0xA1)
            {
                if ((unsigned char)start[0] == 0xBF && sf)
                    sf->question = 1;
                start += 1;
                len -= 1;
            }
            else
                break;
        }

        /* trailing ASCII punctuation run: peel it, keep the signal */
        while (len > 0)
        {
            char c = start[len - 1];
            if (c == '?')
            {
                if (sf)
                    sf->question = 1;
                len--;
            }
            else if (c == ',')
            {
                if (sf)
                    sf->comma = 1;
                len--;
            }
            else if (c == '.' || c == ';' || c == ':' || c == '!')
            {
                if (sf)
                    sf->period = 1;
                len--;
            }
            else
                break;
        }
        if (len == 0)
            continue; /* punctuation-only token: flags kept, no token */
        /* trailing possessive 's detaches into its own token; the
           genitive flag survives even when the table is full */
        int detach_s = 0;
        if (len > 2 && start[len - 2] == '\'' &&
            (start[len - 1] == 's' || start[len - 1] == 'S'))
        {
            if (sf)
                sf->genitive = 1;
            detach_s = 1;
            len -= 2;
        }
        size_t flen = len;
        if (flen >= CHAT_TOKEN_MAX)
            flen = CHAT_TOKEN_MAX - 1;
        size_t o = 0;
        for (size_t i = 0; i < len;)
        {
            if (o >= flen)
                break;
            i += FoldChar(start + i, &toks[n][o]);
            o++;
        }
        toks[n][o] = '\0';
        if (strcmp(toks[n], "'s") == 0)
        {
            /* spaced genitive marker ("David 's son"): force, kept */
            if (sf)
                sf->genitive = 1;
            n++;
        }
        else if (strcmp(toks[n], "del") == 0)
        {
            /* the only ES possession contraction: canonical split */
            strncpy(toks[n], "de", CHAT_TOKEN_MAX - 1);
            toks[n][CHAT_TOKEN_MAX - 1] = '\0';
            n++;
            if (n < max_toks)
            {
                strncpy(toks[n], "el", CHAT_TOKEN_MAX - 1);
                toks[n][CHAT_TOKEN_MAX - 1] = '\0';
                n++;
            }
        }
        else
            n++;
        if (detach_s && n < max_toks)
        {
            strncpy(toks[n], "'s", CHAT_TOKEN_MAX - 1);
            toks[n][CHAT_TOKEN_MAX - 1] = '\0';
            n++;
        }
    }
    return n;
}

/* Parent surface forms: shared by intent parse and text retrieval. */
static const char *const PARENT_SURFACE[] = {
    "padre", "father", "engendro", "begat", "progenitor"
};
#define PARENT_SURFACE_N 5

/* FASE 4: closed-class structural particles (functional vocabulary
   already present as literals across the parser; never content).
   Shared by the G1 topic test (inverse) and the G2 orphan-slot veto. */
static int IsStopTok(const char *tok)
{
    static const char *STOP[] = {
        /* Connectives and prepositions */
        "de", "of", "del", "'s", "en", "in", "on", "at", "for", "with", "y", "e", "and",
        "o", "u", "or", "por",
        /* Articles and determiners */
        "el", "la", "los", "las", "the", "un", "una", "unos", "unas", "a", "an",
        /* Question particles (wh-words) */
        "que", "quien", "quienes", "cual", "cuales", "donde", "como", "cuantos", "cuantas",
        "who", "what", "where", "when", "why", "how", "which", "whom", "whose",
        /* Possessives and pronouns */
        "su", "sus", "his", "her", "its", "their", "mi", "my", "tu", "your",
        /* Copulas and auxiliaries */
        "es", "era", "fue", "son", "is", "was", "are", "were", "be", "been",
        "do", "did", "does",
        /* Polarity */
        "no", "not", "si", "yes",
    };
    for (size_t i = 0; i < sizeof(STOP) / sizeof(STOP[0]); i++)
        if (strcmp(tok, STOP[i]) == 0)
            return 1;
    return 0;
}

/* Spanish verb+clitic shape (explicamelo, dime, hazlo): a stem
   of 2+ chars plus a clitic ending. Closed functional material
   (like STOP), productive morphology, never topic words. Bare
   unknown singles without it stay UNKNOWN. */
static int HasCliticEnding(const char *tok)
{
    static const char *CL[] = {
        "me", "te", "se", "nos", "lo", "la", "los", "las",
        "le", "les",
    };
    size_t L;
    size_t i;
    if (tok == NULL)
        return 0;
    L = strlen(tok);
    if (L < 4)
        return 0;
    for (i = 0; i < sizeof(CL) / sizeof(CL[0]); i++)
    {
        size_t c = strlen(CL[i]);
        if (L > c + 1 && strcmp(tok + L - c, CL[i]) == 0)
            return 1;
    }
    return 0;
}

/* bounded edit distance over bytes (early exit past bound).
   Geometric fallback proposer for unknown query tokens:
   pure algorithm, never a vocabulary list. */
static uint32_t LevBounded(const char *a, const char *b,
                           uint32_t bound)
{
    size_t la;
    size_t lb;
    size_t i;
    size_t j;
    uint32_t prev[128];
    uint32_t cur[128];
    if (a == NULL || b == NULL)
        return bound + 1;
    la = strlen(a);
    lb = strlen(b);
    if (la >= 128 || lb >= 128)
        return bound + 1;
    if (la > lb + bound || lb > la + bound)
        return bound + 1;
    for (j = 0; j <= lb; j++)
        prev[j] = (uint32_t)j;
    for (i = 1; i <= la; i++)
    {
        uint32_t rowmin;
        cur[0] = (uint32_t)i;
        rowmin = cur[0];
        for (j = 1; j <= lb; j++)
        {
            uint32_t cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            uint32_t v = prev[j] + 1;
            if (cur[j - 1] + 1 < v)
                v = cur[j - 1] + 1;
            if (prev[j - 1] + cost < v)
                v = prev[j - 1] + cost;
            cur[j] = v;
            if (v < rowmin)
                rowmin = v;
        }
        if (rowmin > bound)
            return bound + 1;
        for (j = 0; j <= lb; j++)
            prev[j] = cur[j];
    }
    return prev[lb] <= bound ? prev[lb] : bound + 1;
}

/* nearest graph symbol within bound (deterministic: lowest id
   wins ties). 0 = none. */
static SYMBOL_ID LevNearest(const GRAPH *graph, const char *tok,
                           uint32_t bound, uint32_t *out_dist)
{
    uint32_t n;
    uint32_t i;
    SYMBOL_ID best = SYMBOL_INVALID;
    uint32_t bestd = bound + 1;
    if (graph == NULL || tok == NULL)
        return SYMBOL_INVALID;
    n = SymbolCount(graph->symbols);
    for (i = 1; i <= n; i++)
    {
        const SYMBOL *s = SymbolGet(graph->symbols, i);
        uint32_t d;
        if (s == NULL || s->name == NULL)
            continue;
        d = LevBounded(tok, s->name, bound);
        if (d < bestd)
        {
            bestd = d;
            best = i;
            if (d == 0)
                break;
        }
    }
    if (out_dist != NULL)
        *out_dist = bestd;
    return bestd <= bound ? best : SYMBOL_INVALID;
}

/* G2: a captured slot is structurally orphaned when empty or a
   particle (the delimiter itself, an article, a possessive...). */
static int SlotOk(const char *slot)
{
    return slot != NULL && slot[0] != '\0' && !IsStopTok(slot);
}

/* possessive pronoun right before the relation keyword: "su padre"
/   "his father" read as "father of [focus]" (anaphora resolved by
   the context layer; G3 abstains when no valid focus exists). */
static int PrevIsPoss(const char toks[][CHAT_TOKEN_MAX], int kwpos)
{
    if (kwpos <= 0)
        return 0;
    const char *prev = toks[kwpos - 1];
    return strcmp(prev, "su") == 0 || strcmp(prev, "sus") == 0 ||
           strcmp(prev, "his") == 0 || strcmp(prev, "her") == 0;
}

/* EN genitive: [X 's REL] claims X as the argument
   ("David's father" asks for the father of David). */
static int GenitiveArg(const char toks[][CHAT_TOKEN_MAX], int kwpos,
                       char *out)
{
    if (kwpos < 2 || strcmp(toks[kwpos - 1], "'s") != 0)
        return 0;
    strncpy(out, toks[kwpos - 2], CHAT_TOKEN_MAX - 1);
    out[CHAT_TOKEN_MAX - 1] = '\0';
    return 1;
}

/* G1 topic: a content-word-shaped token before the keyword (an
   elliptical "David, padre?" carries its own force; a bare
   "[det] REL of ARG" fragment does not). Closed-class-proof: the
   stop set is functional particles, and ES/EN name variants
   ("Jonas"/"Jonah") never need to match the KB vocabulary. */
static int HasTopicBefore(const char toks[][CHAT_TOKEN_MAX], int kwpos)
{
    for (int i = 0; i < kwpos; i++)
        if (!IsStopTok(toks[i]))
            return 1;
    return 0;
}

/* vocabulary index of a token, -1 when absent (the KB keeps one
   canonical copy per token, so the index is stable) */
static int VocabIdx(const CHAT *ch, const char *tok)
{
    if (tok == NULL)
        return -1;
    for (uint32_t i = 0; i < ch->kb.num_vocab; i++)
        if (strcmp(ch->kb.vocab[i], tok) == 0)
            return (int)i;
    return -1;
}

/* deduced surface stem for a family, from the corpus-derived
   relation index (never a literal). NULL when the family has no
   deduced word: templates that need the word fail closed. */
static const char *ChatFamStem(const CHAT *ch, const char *family)
{
    for (uint32_t i = 0; i < ch->num_kws; i++)
        if (strcmp(ch->kws[i].family, family) == 0)
            return ch->kws[i].es_stem;
    return NULL;
}

/* display form: capitalize first letter (KB stores lowercase) */
static void Cap(const char *tok, char *out, size_t out_size)
{
    if (out_size < 2 || tok == NULL || tok[0] == '\0')
    {
        if (out_size > 0)
            out[0] = '\0';
        return;
    }
    snprintf(out, out_size, "%s", tok);
    if (out[0] >= 'a' && out[0] <= 'z')
        out[0] = (char)(out[0] - 32);
}

void ChatCapStr(const char *tok, char *out, size_t size)
{
    Cap(tok, out, size);
}

void ChatNormTok(const char *in, char *out, size_t size)
{
    size_t o = 0;
    if (size == 0)
        return;
    for (size_t i = 0; in[i] != '\0' && o + 1 < size;)
    {
        char c = '\0';
        i += (size_t)FoldChar(in + i, &c);
        out[o++] = c;
    }
    out[o] = '\0';
}

void ChatAnswerParentSingle(const CHAT *ch, const char *child,
                            const char *parent, char *out, size_t size)
{
    char capC[CHAT_TOKEN_MAX], capP[CHAT_TOKEN_MAX];
    (void)ch;
    Cap(child, capC, sizeof(capC));
    Cap(parent, capP, sizeof(capP));
    if (size == 0)
        return;
    snprintf(out, size,
             "El padre de %s es %s, segun consta en los registros "
             "directos.\n",
             capC, capP);
}

/* ---- corpus ingest (same contract as test_bible_kinship) ---- */

typedef struct
{
    char tag[32];
    char conn[32];
} RelMapRow;

#define RELMAP_MAX 64

static const RelMapRow COMPILED_RELMAP[] = {
    {"HIJO_DE", "isa"},
    {"HERMANO_DE", "sibling_of"},
    {"PADRE_DE", "father_of"},
    {"REY_DE", "reigns"},
    {"ESPOSA_DE", "wife_of"},
    {"CAPITAL", "capital_of"},
    {"MONEDA", "currency_of"},
    {"GOBIERNO", "government_of"},
    {"AUTOR", "author_of"},
    {"DIRECTOR", "director_of"},
    {"PREMIO", "award_of"},
    {"PAIS", "country_of"},
    {"IDIOMA", "language_of"},
    {"IDIOMA_OFICIAL", "language_of"},
    {"MIEMBRO_DE", "member_of"},
};

static RelMapRow g_relmap[RELMAP_MAX];
static uint32_t g_nrelmap = 0;
static int g_relmap_loaded = 0;

static void RelMapInit(void)
{
    if (g_relmap_loaded)
        return;
    g_relmap_loaded = 1;
    g_nrelmap = 0;
    FILE *f = fopen("data/agentic/relations.tsv", "r");
    if (f != NULL)
    {
        char line[256];
        while (fgets(line, sizeof(line), f) != NULL && g_nrelmap < RELMAP_MAX)
        {
            char *p = line;
            while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
                p++;
            if (*p == '\0' || *p == '#')
                continue;
            char tag[32], conn[32];
            if (sscanf(p, "%31s %31s", tag, conn) == 2)
            {
                strncpy(g_relmap[g_nrelmap].tag, tag, 31);
                g_relmap[g_nrelmap].tag[31] = '\0';
                strncpy(g_relmap[g_nrelmap].conn, conn, 31);
                g_relmap[g_nrelmap].conn[31] = '\0';
                g_nrelmap++;
            }
        }
        fclose(f);
    }
    if (g_nrelmap == 0)
    {
        size_t n = sizeof(COMPILED_RELMAP) / sizeof(COMPILED_RELMAP[0]);
        for (size_t i = 0; i < n && g_nrelmap < RELMAP_MAX; i++)
            g_relmap[g_nrelmap++] = COMPILED_RELMAP[i];
    }
}

static const char *GenericRelToConn(const char *rel, char *buf, size_t bsize)
{
    if (rel == NULL || rel[0] == '\0')
        return NULL;
    RelMapInit();

    /* 1. Consult table */
    for (uint32_t i = 0; i < g_nrelmap; i++)
    {
        if (strcmp(rel, g_relmap[i].tag) == 0)
            return g_relmap[i].conn;
    }

    /* 2. Already a valid connective */
    if (LearnerIsConnective(rel))
        return rel;

    /* 3. Suffix rule for _DE / _de or _OF / _of */
    size_t len = strlen(rel);
    if (len > 3 && (strcmp(rel + len - 3, "_DE") == 0 || strcmp(rel + len - 3, "_de") == 0))
    {
        size_t stem_len = len - 3;
        if (stem_len + 4 < bsize)
        {
            for (size_t i = 0; i < stem_len; i++)
                buf[i] = (char)tolower((unsigned char)rel[i]);
            strcpy(buf + stem_len, "_of");
            return buf;
        }
    }
    if (len > 3 && (strcmp(rel + len - 3, "_OF") == 0 || strcmp(rel + len - 3, "_of") == 0))
    {
        size_t stem_len = len - 3;
        if (stem_len + 4 < bsize)
        {
            for (size_t i = 0; i < stem_len; i++)
                buf[i] = (char)tolower((unsigned char)rel[i]);
            strcpy(buf + stem_len, "_of");
            return buf;
        }
    }
    return NULL;
}

/* Ingest the TSV into the working KB. Returns rows learned.
   The relation index (kw index) is DEDUCED here: the Spanish stem
   of REL (REL minus the "_DE" / "_OF" suffix), the English stem = the
   surface connective it was learned with, and the family where
   the pairs landed. No relation word is hardcoded. */
static void KwdRecord(CHAT *ch, const char *rel, const char *conn)
{
    char es[CHAT_TOKEN_MAX];
    size_t len = strlen(rel);
    if (len > 3 && (strcmp(rel + len - 3, "_DE") == 0 || strcmp(rel + len - 3, "_de") == 0))
        len -= 3;
    else if (len > 3 && (strcmp(rel + len - 3, "_OF") == 0 || strcmp(rel + len - 3, "_of") == 0))
        len -= 3;
    if (len == 0 || len >= sizeof(es))
        return;
    for (size_t i = 0; i < len; i++)
        es[i] = (char)tolower((unsigned char)rel[i]);
    es[len] = '\0';

    /* EN stem: the connective minus a trailing "_of" suffix
       (generic string rule, no word list) */
    char en[CHAT_TOKEN_MAX];
    snprintf(en, sizeof(en), "%s", conn);
    size_t elen = strlen(en);
    if (elen > 3 && (strcmp(en + elen - 3, "_of") == 0 || strcmp(en + elen - 3, "_OF") == 0))
        en[elen - 3] = '\0';

    uint32_t i;
    for (i = 0; i < ch->num_kws; i++)
        if (strcmp(ch->kws[i].es_stem, es) == 0)
            break;
    if (i == ch->num_kws)
    {
        if (ch->num_kws >= CHAT_KW_MAX)
            return;
        ch->num_kws++;
        memset(&ch->kws[i], 0, sizeof(REL_KW));
        strcpy(ch->kws[i].es_stem, es);
    }
    strncpy(ch->kws[i].en_stem, en, sizeof(ch->kws[i].en_stem) - 1);
    ch->kws[i].en_stem[sizeof(ch->kws[i].en_stem) - 1] = '\0';
    strncpy(ch->kws[i].conn, conn, sizeof(ch->kws[i].conn) - 1);
    ch->kws[i].conn[sizeof(ch->kws[i].conn) - 1] = '\0';
    strncpy(ch->kws[i].family, ch->lr.last_family,
            sizeof(ch->kws[i].family) - 1);
    ch->kws[i].family[sizeof(ch->kws[i].family) - 1] = '\0';
}

static uint32_t ChatIngestCorpus(CHAT *ch, const char *path,
                                  uint32_t *scanned)
{
    FILE *f = fopen(path, "r");
    if (f == NULL)
        return 0;
    if (scanned != NULL)
        *scanned = 0;
    char line[LEARN_MAX_LINE];
    uint32_t learned = 0;
    while (fgets(line, sizeof(line), f) != NULL)
    {
        size_t len = strlen(line);
        while (len && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        if (len == 0)
            continue;
        char buf[LEARN_MAX_LINE];
        strncpy(buf, line, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        char *t1 = strchr(buf, '\t');
        if (t1 == NULL)
            continue;
        *t1 = '\0';
        char *rel = t1 + 1;
        char *t2 = strchr(rel, '\t');
        if (t2 == NULL)
            continue;
        *t2 = '\0';
        char *obj = t2 + 1;
        if (scanned != NULL)
            (*scanned)++;
        char *eol = strpbrk(obj, "\t\r\n");
        if (eol != NULL)
            *eol = '\0';
        char norm_s[CHAT_TOKEN_MAX], norm_o[CHAT_TOKEN_MAX];
        ChatNormTok(buf, norm_s, sizeof(norm_s));
        ChatNormTok(obj, norm_o, sizeof(norm_o));
        if (strcmp(norm_s, norm_o) == 0)
            continue; /* self-loop: never admitted */
        char conn_buf[LEARN_MAX_LINE];
        const char *conn = GenericRelToConn(rel, conn_buf, sizeof(conn_buf));
        if (conn == NULL)
            continue;
        char sent[LEARN_MAX_LINE];
        snprintf(sent, sizeof(sent), "%s %s %s", norm_s, conn, norm_o);
        if (LearnerLearnLine(&ch->lr, sent))
        {
            /* the kw index is deduced AFTER the row is learned:
               last_family is now THIS row's family (the TSV is
               interleaved, so reading it before would lag one
               row behind) */
            KwdRecord(ch, rel, conn);
            learned++;
        }
    }
    fclose(f);
    return learned;
}

/* session text graph (lazy): shared symbol space for dynamic
   corpus loads. SchemaKB/metas never involved, so unload is
   exact (rebuild these from the file list). */
static int TextSessionEnsure(CHAT *ch)
{
    GRAPH *g;
    EMBEDDING_TABLE *e;
    if (ch->tgraph != NULL && ch->temb != NULL)
        return 1;
    g = GraphCreate(262144, 512);
    if (g == NULL)
        return 0;
    e = EmbeddingTableCreate(262144);
    if (e == NULL)
    {
        GraphDestroy(g);
        return 0;
    }
    GraphSetEmbeddingTable(g, e);
    ch->tgraph = g;
    ch->temb = e;
    return 1;
}

/* file list for future unload-by-rebuild (dedup, bounded) */
static void TextSessionRemember(CHAT *ch, const char *name)
{
    uint32_t i;
    for (i = 0; i < ch->ntfiles; i++)
        if (strcmp(ch->tfiles[i], name) == 0)
            return;
    if (ch->ntfiles >= CHAT_TEXT_FILES_MAX)
        return;
    strncpy(ch->tfiles[ch->ntfiles], name, sizeof(ch->tfiles[0]) - 1);
    ch->tfiles[ch->ntfiles][sizeof(ch->tfiles[0]) - 1] = '\0';
    ch->ntfiles++;
}

/* lazy commonsense & world knowledge graph (Pillar 3) */
static GRAPH *ChatGetCommonsenseGraph(CHAT *ch)
{
    if (ch == NULL)
        return NULL;
    if (ch->cs_graph != NULL)
        return ch->cs_graph;
    GRAPH *g = GraphCreate(32768, 65536);
    if (g == NULL)
        return NULL;
    CS_STATS stats;
    CommonsenseIngestSeed(g, &stats);
    ch->cs_graph = g;
    return ch->cs_graph;
}


/* ---- queries against the frozen layers ----

   Per-family derivation policy (census-verified, measured on the
   corpus before writing code; the layers stay generic, this is a
   consultable table at the chat layer, like BibleRelToConn):
     taxonomy: direct + TRANSITIVE chain (368 novel, frozen)
     father:   direct + TRANSITIVE chain (3 novel, all true) +
               inverse read through the taxonomy dual scan
     sibling:  direct + swap over OBSERVED pairs only; chain
               REFUSED (its 3 chains yield false conclusions)
     reigns:   direct only; chain REFUSED (1 false conclusion)
     wife:     direct only (no chains, no swaps)              */

/* Per-family chain policy: consultable table (census-verified).
   sibling chain yields 3 false conclusions, reigns chain 1, wife
   has no chains; taxonomy (368 novel) and father (3 novel, all
   true) are licensed. Public so tests pin the policy. */
int ChatFamilyChainAllowed(const char *family)
{
    return strcmp(family, "taxonomy") == 0 ||
           strcmp(family, "father") == 0;
}

static int FamilyChainAllowed(const char *family)
{
    return ChatFamilyChainAllowed(family);
}

/* direct edge (1-hop): pair evidence + vocabulary */
static int ChatDirect(const CHAT *ch, const char *s, const char *o)
{
    char out[128];
    return TransferDerive(&ch->kb, &ch->mk, "taxonomy", s, o, out,
                          sizeof(out));
}

/* 2-hop derived conclusion (TRANSFER meta path), policy-gated */
static int ChatChainFam(const CHAT *ch, const char *family,
                        const char *s, const char *o, char *middle,
                        size_t middle_size)
{
    if (!FamilyChainAllowed(family))
        return 0;
    char out[128];
    return TransferExplainChain(&ch->kb, &ch->mk, family, s, o, out,
                                sizeof(out), middle, middle_size);
}

static int ChatChain(const CHAT *ch, const char *s, const char *o,
                     char *middle, size_t middle_size)
{
    return ChatChainFam(ch, "taxonomy", s, o, middle, middle_size);
}

/* children scan: every direct (X -> parent) edge in taxonomy plus
   the INVERSE read of father pairs (s,X): the PADRE_DE dual. */
static uint32_t ChatChildren(const CHAT *ch, const char *parent,
                             char out[][CHAT_TOKEN_MAX], uint32_t max_out)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < ch->kb.num_pairs && n < max_out; i++)
    {
        const PAIR_EVID *p = &ch->kb.pairs[i];
        const char *kid = NULL;
        if (strcmp(p->family, "taxonomy") == 0)
        {
            if (strcmp(p->object, parent) != 0)
                continue;
            kid = p->subject; /* (kid, parent): kid isa parent */
        }
        else if (strcmp(p->family, "father") == 0)
        {
            if (strcmp(p->subject, parent) != 0)
                continue;
            kid = p->object; /* (parent, kid): father_of */
        }
        else
            continue;
        if (strcmp(kid, parent) == 0)
            continue;
        int dup = 0;
        for (uint32_t k = 0; k < n; k++)
            if (strcmp(out[k], kid) == 0)
                dup = 1;
        if (!dup)
        {
            strncpy(out[n], kid, CHAT_TOKEN_MAX - 1);
            out[n][CHAT_TOKEN_MAX - 1] = '\0';
            n++;
        }
    }
    return n;
}

/* father scan: direct parents of child: taxonomy objects of pairs
   whose SUBJECT is `child`, plus father-family direct pairs
   (parent, child) read in inverse. */
static uint32_t ChatParents(const CHAT *ch, const char *child,
                            char out[][CHAT_TOKEN_MAX], uint32_t max_out)
{
    uint32_t n = 0;
    /* Pass 1: father family only (specific) */
    for (uint32_t i = 0; i < ch->kb.num_pairs && n < max_out; i++)
    {
        const PAIR_EVID *p = &ch->kb.pairs[i];
        if (strcmp(p->family, "father") == 0)
        {
            if (strcmp(p->object, child) != 0)
                continue;
            const char *par = p->subject;
            if (strcmp(par, child) == 0)
                continue;
            int dup = 0;
            for (uint32_t k = 0; k < n; k++)
                if (strcmp(out[k], par) == 0)
                    dup = 1;
            if (!dup)
            {
                strncpy(out[n], par, CHAT_TOKEN_MAX - 1);
                out[n][CHAT_TOKEN_MAX - 1] = '\0';
                n++;
            }
        }
    }
    if (n > 0)
        return n;
    /* Pass 2: taxonomy fallback (general) */
    for (uint32_t i = 0; i < ch->kb.num_pairs && n < max_out; i++)
    {
        const PAIR_EVID *p = &ch->kb.pairs[i];
        if (strcmp(p->family, "taxonomy") == 0)
        {
            if (strcmp(p->subject, child) != 0)
                continue;
            const char *par = p->object;
            if (strcmp(par, child) == 0)
                continue;
            int dup = 0;
            for (uint32_t k = 0; k < n; k++)
                if (strcmp(out[k], par) == 0)
                    dup = 1;
            if (!dup)
            {
                strncpy(out[n], par, CHAT_TOKEN_MAX - 1);
                out[n][CHAT_TOKEN_MAX - 1] = '\0';
                n++;
            }
        }
    }
    return n;
}

/* Parent candidates for the wrapper (Fase B): same scan, ingest
   order, deduped. */
uint32_t ChatParentsList(const CHAT *ch, const char *child,
                         char out[][CHAT_TOKEN_MAX], uint32_t max_out)
{
    return ChatParents(ch, child, out, max_out);
}

/* sibling scan: SYMMETRIC family, but the swap path in transfer
   needs declared roles; at the chat layer the honest equivalent
   is scanning BOTH directions of OBSERVED pairs only. */
uint32_t ChatSiblings(const CHAT *ch, const char *who,
                      char out[][CHAT_TOKEN_MAX], uint32_t max_out)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < ch->kb.num_pairs && n < max_out; i++)
    {
        const PAIR_EVID *p = &ch->kb.pairs[i];
        if (strcmp(p->family, "sibling") != 0)
            continue;
        const char *sib = NULL;
        if (strcmp(p->subject, who) == 0)
            sib = p->object;
        else if (strcmp(p->object, who) == 0)
            sib = p->subject;
        else
            continue;
        if (strcmp(sib, who) == 0)
            continue;
        int dup = 0;
        for (uint32_t k = 0; k < n; k++)
            if (strcmp(out[k], sib) == 0)
                dup = 1;
        if (!dup)
        {
            strncpy(out[n], sib, CHAT_TOKEN_MAX - 1);
            out[n][CHAT_TOKEN_MAX - 1] = '\0';
            n++;
        }
    }
    return n;
}

/* kings scan: reigns pairs (king, territory); direction X:
   object == territory -> subject = king. */
static uint32_t ChatKings(const CHAT *ch, const char *territory,
                          char out[][CHAT_TOKEN_MAX], uint32_t max_out)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < ch->kb.num_pairs && n < max_out; i++)
    {
        const PAIR_EVID *p = &ch->kb.pairs[i];
        if (strcmp(p->family, "reigns") != 0)
            continue;
        if (strcmp(p->object, territory) != 0)
            continue;
        int dup = 0;
        for (uint32_t k = 0; k < n; k++)
            if (strcmp(out[k], p->subject) == 0)
                dup = 1;
        if (!dup)
        {
            strncpy(out[n], p->subject, CHAT_TOKEN_MAX - 1);
            out[n][CHAT_TOKEN_MAX - 1] = '\0';
            n++;
        }
    }
    return n;
}

/* kingdoms scan: reigns pairs with subject == king -> territories */
static uint32_t ChatKingdoms(const CHAT *ch, const char *king,
                             char out[][CHAT_TOKEN_MAX], uint32_t max_out)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < ch->kb.num_pairs && n < max_out; i++)
    {
        const PAIR_EVID *p = &ch->kb.pairs[i];
        if (strcmp(p->family, "reigns") != 0)
            continue;
        if (strcmp(p->subject, king) != 0)
            continue;
        int dup = 0;
        for (uint32_t k = 0; k < n; k++)
            if (strcmp(out[k], p->object) == 0)
                dup = 1;
        if (!dup)
        {
            strncpy(out[n], p->object, CHAT_TOKEN_MAX - 1);
            out[n][CHAT_TOKEN_MAX - 1] = '\0';
            n++;
        }
    }
    return n;
}

/* spouse scan: wife pairs (wife, husband); side 0 = wives of a
   husband (object == husband), side 1 = husbands of a wife. */
static uint32_t ChatSpouses(const CHAT *ch, const char *who, int side,
                            char out[][CHAT_TOKEN_MAX], uint32_t max_out)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < ch->kb.num_pairs && n < max_out; i++)
    {
        const PAIR_EVID *p = &ch->kb.pairs[i];
        if (strcmp(p->family, "wife") != 0)
            continue;
        const char *hit = NULL;
        if (side == 0 && strcmp(p->object, who) == 0)
            hit = p->subject;
        else if (side == 1 && strcmp(p->subject, who) == 0)
            hit = p->object;
        else
            continue;
        int dup = 0;
        for (uint32_t k = 0; k < n; k++)
            if (strcmp(out[k], hit) == 0)
                dup = 1;
        if (!dup)
        {
            strncpy(out[n], hit, CHAT_TOKEN_MAX - 1);
            out[n][CHAT_TOKEN_MAX - 1] = '\0';
            n++;
        }
    }
    return n;
}

/* grandparent 2-hop for grandchild X: pair (S,O) = S child of O, so
   outer pair p = (X -> P) [X child of P], inner pair q = (P -> GP)
   [P child of GP]. GP out, mid=P out (proof). */
static int ChatGrandparent(const CHAT *ch, const char *grandchild,
                           char *gp, size_t gp_size, char *mid,
                           size_t mid_size)
{
    if (grandchild == NULL || gp == NULL || gp_size < 2 || mid == NULL ||
        mid_size < 2)
        return 0;
    int has_tax = MetaHasProperty(&ch->mk, "taxonomy", META_PROP_TRANSITIVE);
    int has_father = MetaHasProperty(&ch->mk, "father", META_PROP_TRANSITIVE);
    int has_father_pairs = 0;
    for (uint32_t i = 0; i < ch->kb.num_pairs; i++)
        if (strcmp(ch->kb.pairs[i].family, "father") == 0)
        {
            has_father_pairs = 1;
            break;
        }
    /* Grandparent is an explicit 2-hop walk, not father transitivity.
       Promoted text pairs must be usable without licensing
       father_of(A,C) as a false 1-hop. */
    if (!has_tax && !has_father && !has_father_pairs)
        return 0;
    for (uint32_t i = 0; i < ch->kb.num_pairs; i++)
    {
        const PAIR_EVID *p = &ch->kb.pairs[i];
        const char *p_mid = NULL; /* intermediate: grandchild→mid */
        if (strcmp(p->family, "taxonomy") == 0)
        {
            /* taxonomy(S,O): S isa O → S is child type, O is parent type */
            if (strcmp(p->subject, grandchild) != 0)
                continue;
            p_mid = p->object;
        }
        else if (strcmp(p->family, "father") == 0)
        {
            /* father(S,O): S is parent of O → O is child, S is parent.
               For grandchild lookup, we need O == grandchild */
            if (strcmp(p->object, grandchild) != 0)
                continue;
            p_mid = p->subject;
        }
        else
            continue;
        if (strcmp(p_mid, grandchild) == 0)
            continue;
        /* p connects grandchild to p_mid; now find p_mid to GP */
        for (uint32_t j = 0; j < ch->kb.num_pairs; j++)
        {
            const PAIR_EVID *q = &ch->kb.pairs[j];
            const char *q_gp = NULL;
            if (strcmp(q->family, "taxonomy") == 0)
            {
                if (strcmp(q->subject, p_mid) != 0)
                    continue;
                q_gp = q->object;
            }
            else if (strcmp(q->family, "father") == 0)
            {
                if (strcmp(q->object, p_mid) != 0)
                    continue;
                q_gp = q->subject;
            }
            else
                continue;
            if (strcmp(q_gp, p_mid) == 0)
                continue;
            if (gp_size < strlen(q_gp) + 1 ||
                mid_size < strlen(p_mid) + 1)
                return 0;
            strcpy(gp, q_gp);
            strcpy(mid, p_mid);
            return 1;
        }
    }
    return 0;
}

/* descendant 2-hop for ancestor X: pair (S,O) = S child of O, so
   outer pair p = (X -> M) [X child of M] is WRONG direction: we
   want D child of ... child of X. Outer pair p = (M -> X)
   [M child of X], inner pair q = (D -> M) [D child of M].
   D out, mid=M out (proof). */
static int ChatDescendant(const CHAT *ch, const char *ancestor,
                          char *gd, size_t gd_size, char *mid,
                          size_t mid_size)
{
    if (ancestor == NULL || gd == NULL || gd_size < 2 || mid == NULL ||
        mid_size < 2)
        return 0;
    if (!MetaHasProperty(&ch->mk, "taxonomy", META_PROP_TRANSITIVE))
        return 0;
    for (uint32_t i = 0; i < ch->kb.num_pairs; i++)
    {
        const PAIR_EVID *p = &ch->kb.pairs[i];
        if (strcmp(p->family, "taxonomy") != 0)
            continue;
        if (strcmp(p->object, ancestor) != 0)
            continue;
        if (strcmp(p->subject, p->object) == 0)
            continue;
        /* p = (M -> X): find (D -> M) */
        for (uint32_t j = 0; j < ch->kb.num_pairs; j++)
        {
            const PAIR_EVID *q = &ch->kb.pairs[j];
            if (strcmp(q->family, "taxonomy") != 0)
                continue;
            if (strcmp(q->object, p->subject) != 0)
                continue;
            if (strcmp(q->subject, q->object) == 0)
                continue;
            if (gd_size < strlen(q->subject) + 1 ||
                mid_size < strlen(p->subject) + 1)
                return 0;
            strcpy(gd, q->subject);
            strcpy(mid, p->subject);
            return 1;
        }
    }
    return 0;
}

/* ---- Phase 2: BFS >= 3-hop over taxonomy (fail-closed) ----

    The 2-hop conclusion stays owned by the TRANSFER meta path
    (TransferExplainChain / ChatChainFam). This layer answers
    conclusions whose shortest proof needs MORE edges: BFS over
    taxonomy pair evidence, min distance >= 2, policy-gated to
    the transitive family. Static memory only (frontier/parents/
    visited sized over the KB vocabulary), cycles can never enter
    (a node is enqueued at most once), and the goal must sit in
    the presented vocabulary or the answer stays UNKNOWN. */

#define CHAT_BFS_VISIT_MAX SCHEMA_VOCAB_MAX

typedef struct
{
    int      idx;  /* index into ch->kb.vocab */
    uint32_t hops; /* edges from start */
} BFS_SLOT;

/* adjacency scan: every taxonomy parent of `node` (pairs whose
   SUBJECT is `node`, i.e. "node isa P": the ancestry direction,
   child -> parent; self-loops excluded) */
static uint32_t BfsParents(const CHAT *ch, const char *node,
                           const int *exclude, int nexclude,
                           uint32_t out_idx[], uint32_t max_out)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < ch->kb.num_pairs && n < max_out; i++)
    {
        const PAIR_EVID *p = &ch->kb.pairs[i];
        if (strcmp(p->family, "taxonomy") != 0)
            continue;
        if (strcmp(p->subject, node) != 0)
            continue;
        if (strcmp(p->subject, p->object) == 0)
            continue;
        int vi = VocabIdx(ch, p->object);
        if (vi < 0)
            continue;
        int dup = 0;
        for (int e = 0; e < nexclude; e++)
            if (exclude[e] == vi)
            {
                dup = 1;
                break;
            }
        if (dup)
            continue;
        int seen = 0;
        for (uint32_t k = 0; k < n; k++)
            if (out_idx[k] == (uint32_t)vi)
            {
                seen = 1;
                break;
            }
        if (!seen)
            out_idx[n++] = (uint32_t)vi;
    }
    return n;
}

/* one BFS run; fills visit order + parents. Returns 1 if `goal`
    was reached (dist >= 2 guaranteed by the direct-pair veto).
    goal_idx < 0 means "explore everything" (reach census): no
    early return, the queue drains completely. */
static int BfsRun(const CHAT *ch, int start_idx, int goal_idx,
                  uint32_t hops[], int parent[], int *visited,
                  BFS_SLOT *frontier)
{
    uint32_t nvisit = 0, head = 0, tail = 0;
    hops[start_idx] = 0;
    parent[start_idx] = -1;
    visited[nvisit++] = start_idx;
    frontier[tail].idx = start_idx;
    frontier[tail].hops = 0;
    tail++;
    int found = 0;
    while (head < tail)
    {
        BFS_SLOT cur = frontier[head++];
        if (goal_idx >= 0 && cur.idx == goal_idx)
        {
            found = 1;
            break;
        }
        uint32_t kids[CHAT_BFS_VISIT_MAX];
        uint32_t nk = BfsParents(ch, ch->kb.vocab[cur.idx],
                                 visited, (int)nvisit, kids,
                                 CHAT_BFS_VISIT_MAX);
        for (uint32_t k = 0; k < nk; k++)
        {
            uint32_t vi = kids[k];
            hops[vi] = cur.hops + 1;
            parent[vi] = cur.idx;
            visited[nvisit++] = vi;
            frontier[tail].idx = (int)vi;
            frontier[tail].hops = cur.hops + 1;
            tail++;
        }
    }
    return found;
}

int ChatBfsPath(const CHAT *ch, const char *start, const char *goal,
                char path[][CHAT_TOKEN_MAX])
{
    if (ch == NULL || start == NULL || goal == NULL || path == NULL)
        return 0;
    if (!ChatFamilyChainAllowed("taxonomy"))
        return 0;
    if (!MetaHasProperty(&ch->mk, "taxonomy", META_PROP_TRANSITIVE))
        return 0;
    if (start[0] == '\0' || goal[0] == '\0')
        return 0;
    if (strcmp(start, goal) == 0)
        return 0;
    int si = VocabIdx(ch, start), gi = VocabIdx(ch, goal);
    if (si < 0 || gi < 0)
        return 0;
    /* the plain path owns 1-hop: a direct pair is not ours */
    for (uint32_t i = 0; i < ch->kb.num_pairs; i++)
    {
        const PAIR_EVID *p = &ch->kb.pairs[i];
        if (strcmp(p->family, "taxonomy") == 0 &&
            strcmp(p->subject, start) == 0 &&
            strcmp(p->object, goal) == 0)
            return 0;
    }
    /* static work arrays over the vocabulary bound */
    static uint32_t hops[CHAT_BFS_VISIT_MAX];
    static int parent[CHAT_BFS_VISIT_MAX];
    static int visited[CHAT_BFS_VISIT_MAX];
    static BFS_SLOT frontier[CHAT_BFS_VISIT_MAX];
    memset(hops, 0, sizeof(hops));
    for (uint32_t i = 0; i < ch->kb.num_vocab; i++)
        parent[i] = -2; /* unvisited marker */
    if (!BfsRun(ch, si, gi, hops, parent, visited, frontier))
        return 0;
    if (hops[gi] < 2)
        return 0;
    uint32_t want = hops[gi] + 1; /* nodes incl. both ends */
    if (want > CHAT_BFS_PATH_MAX)
        return 0; /* trace longer than the report buffer */
    /* walk parents backwards, then reverse in place */
    int walk[CHAT_BFS_PATH_MAX];
    int at = gi;
    for (uint32_t k = 0; k < want; k++)
    {
        walk[k] = at;
        at = parent[at];
    }
    for (uint32_t k = 0; k < want; k++)
    {
        strncpy(path[k], ch->kb.vocab[walk[want - 1 - k]],
                CHAT_TOKEN_MAX - 1);
        path[k][CHAT_TOKEN_MAX - 1] = '\0';
    }
    return (int)hops[gi];
}

uint32_t ChatBfsReach(const CHAT *ch, const char *start,
                      char names[][CHAT_TOKEN_MAX], uint32_t *depths,
                      uint32_t max_out)
{
    if (ch == NULL || start == NULL || start[0] == '\0')
        return 0;
    int si = VocabIdx(ch, start);
    if (si < 0)
        return 0;
    static uint32_t hops[CHAT_BFS_VISIT_MAX];
    static int parent[CHAT_BFS_VISIT_MAX];
    static int visited[CHAT_BFS_VISIT_MAX];
    static BFS_SLOT frontier[CHAT_BFS_VISIT_MAX];
    memset(hops, 0, sizeof(hops));
    for (uint32_t i = 0; i < ch->kb.num_vocab; i++)
        parent[i] = -2;
    /* goal < 0: explore the whole reachable set (reach census) */
    BfsRun(ch, si, -1, hops, parent, visited, frontier);
    uint32_t n = 0;
    for (uint32_t i = 0; i < ch->kb.num_vocab && n < max_out; i++)
    {
        if (i == (uint32_t)si || parent[i] == -2)
            continue;
        if (hops[i] < 2)
            continue;
        strncpy(names[n], ch->kb.vocab[i], CHAT_TOKEN_MAX - 1);
        names[n][CHAT_TOKEN_MAX - 1] = '\0';
        depths[n] = hops[i];
        n++;
    }
    return n;
}

/* ---- intents ---- */

typedef enum
{
    INT_NONE = 0,
    INT_PARENT_OF,    /* quien ... padre ... (de X) */
    INT_CHILDREN_OF,  /* hijos ... (de X) */
    INT_IS_PARENT,    /* es A hijo de B */
    INT_GRANDPARENT,  /* abuelo ... (de X) */
    INT_DESCENDANT,   /* descendiente ... (de X) */
    INT_WHY,          /* por que A hijo de B */
    INT_REL_QUERY,    /* generic deduced frame: <kw> question */
    INT_REL_BOOL,     /* generic deduced frame: es A <kw> de B */
    INT_COMPOSE_WHY,  /* why is A <kw1> of B, given B <kw2> of C */
    INT_LOAD,          /* carga <file.tsv> (sandboxed dataset load) */
    INT_INGIERE,      /* load <file.tsv> (hot knowledge triples) */
    INT_TEXTLOAD,     /* load <file.txt> (intact corpus to graph) */
    INT_TEXTQ,        /* content question over session texts */
    INT_TEXTSTATUS,   /* estado: session inventory (counts from stores) */
    INT_TEXT_TOPICS,  /* dime las areas que conoces / de que temas podemos hablar */
    INT_TEXT_START,   /* inicia una conversacion / hablemos */
    INT_UNLOAD,       /* unload <file.txt>: drop + exact rebuild */
    /* Structural QA intents (HARDCODING=0: detected by pattern, not vocabulary) */
    INT_QA_ENTITY,    /* structural: <wh> <copula> <entity> → entity lookup */
    INT_QA_WHERE,     /* structural: <wh> <location_marker> <entity> → location lookup */
    INT_QA_COUNT,     /* structural: <wh> <count_marker> <entity> → count triples */
    INT_QA_WHY_QA,    /* structural: <wh_cause> <entity> <relation> → cause lookup */
    INT_QA_WHAT,      /* structural: <wh> <copula> <entity> → definition/role lookup */
    INT_QA_CONSEQUENCE, /* structural: <wh_consequence> <condition...> → causal consequence */
    INT_QA_AFFORDANCE   /* structural: <wh_affordance> <entity...> → used_for / capable_of */
} INTENT;


typedef struct
{
    INTENT intent;
    char   a[CHAT_TOKEN_MAX]; /* slot A */
    char   b[CHAT_TOKEN_MAX]; /* slot B (boolean/why) */
    char   cc[CHAT_TOKEN_MAX]; /* slot C (compose why) */
    int    has_b;
    int    kw;                /* deduced relation index (generic) */
    int    kw2;               /* second relation index (compose why) */
    int    hops;              /* ancestor depth: 2 = abuelo, 3 = padre+abuelo */
    char   toks[CHAT_TEXT_WORDS_MAX][CHAT_TOKEN_MAX]; /* TEXTQ query words (as asked) */
    uint32_t ntoks;
    int    t_following;      /* TEXTQ follow-up: reuse cached topic */
    char   t_sub[CHAT_TOKEN_MAX]; /* geometric substitute (marked) */
} PARSED;

/* token right after the first "de"/"of" following position from */
static int TokAfterDe(const char toks[][CHAT_TOKEN_MAX], uint32_t n,
                      uint32_t from, char *out, size_t out_size)
{
    (void)out_size; /* out is CHAT_TOKEN_MAX-sized by contract */
    for (uint32_t i = from; i + 1 < n; i++)
    {
        if (strcmp(toks[i], "de") == 0 || strcmp(toks[i], "of") == 0)
        {
            strncpy(out, toks[i + 1], CHAT_TOKEN_MAX - 1);
            out[CHAT_TOKEN_MAX - 1] = '\0';
            return 1;
        }
    }
    return 0;
}

/* morphological fold: produce the raw word plus one candidate per
   productive rule (ES plural 's'/'es', ES gender swap o<->a,
   chained plural->gender: esposos -> esposo -> esposa). Rules,
   not word lists; callers decide what a candidate may match. */
static void MorphFold(const char *tok, char cand[][CHAT_TOKEN_MAX],
                      uint32_t *ncand)
{
    uint32_t nc = 0;
    snprintf(cand[nc++], CHAT_TOKEN_MAX, "%s", tok);
    size_t tl = strlen(tok);
    if (tl >= 2 && tok[tl - 1] == 's')
    { /* plural: strip 's'; ES o-stems pluralize as -es */
        memcpy(cand[nc], tok, tl - 1);
        cand[nc][tl - 1] = '\0';
        nc++;
        if (tl >= 3 && tok[tl - 2] == 'e')
        {
            memcpy(cand[nc], tok, tl - 2);
            cand[nc][tl - 2] = '\0';
            nc++;
        }
    }
    for (uint32_t c = 0; c < nc && nc < 4; c++)
    {
        size_t cl = strlen(cand[c]);
        if (cl < 2)
            continue;
        char flip = cand[c][cl - 1] == 'o'
                        ? 'a'
                        : (cand[c][cl - 1] == 'a' ? 'o' : '\0');
        if (flip == '\0')
            continue; /* ES gender swap: productive o<->a rule */
        memcpy(cand[nc], cand[c], cl - 1);
        cand[nc][cl - 1] = flip;
        cand[nc][cl] = '\0';
        nc++;
    }
    *ncand = nc;
}

static int HasTok(const char toks[][CHAT_TOKEN_MAX], uint32_t n,
                  const char *w)
{
    for (uint32_t i = 0; i < n; i++)
        if (strcmp(toks[i], w) == 0)
            return (int)i;
    return -1;
}

/* EN surface forms: the connective-derived en_stem (sibling,
   reigns, wife_of...) is not the English noun people use
   (brother, king...). Like BibleRelToConn, this is a
   consultable ingestion-vocabulary table at the chat layer,
   not logic; unknown words match nothing. Shared by the matcher
   and the BOOL guard so both know the same words. */
static const struct
{
    const char *en_word;
    const char *es_stem;
} CHAT_EN_SURFACE[] = {
    {"brother", "hermano"}, {"king", "rey"},
    {"husband", "esposa"},  {"wife", "esposa"},
};
#define CHAT_EN_SURFACE_N \
    (sizeof(CHAT_EN_SURFACE) / sizeof(CHAT_EN_SURFACE[0]))

/* span-restricted deduced-relation match (Fase A): the ParseIntent
   scan over absolute token indices in [start,end) — shared table,
   MorphFold rules and the son/sons wh-guard intact. */
static void MatchKwSpan(const CHAT *ch, const char toks[][CHAT_TOKEN_MAX],
                        uint32_t start, uint32_t end,
                        int *kwx_out, int *kwpos_out)
{

    /* generic deduced-relation match: scan the tokens against the
       ingest-time index (kws) — no relation word lives in code */
    int kwx = -1; /* index of the matched deduced keyword */
    int kwpos = -1; /* token position of that keyword */
    for (uint32_t i = start; i < end && kwx < 0; i++)
    {
        /* morphological fold of the raw token (rules, not word
           lists); a candidate only survives if it equals a DEDUCED
           stem, so folds of ordinary words are inert unless the KB
           deduced it */
        char cand[4][CHAT_TOKEN_MAX];
        uint32_t ncand = 0;
        MorphFold(toks[i], cand, &ncand);

        for (uint32_t k = 0; k < ch->num_kws; k++)
        {
            const REL_KW *kw = &ch->kws[k];
            int hit = strcmp(toks[i], kw->es_stem) == 0 ||
                      strcmp(toks[i], kw->en_stem) == 0;
            if (!hit)
                for (size_t e = 0; e < CHAT_EN_SURFACE_N; e++)
                    if (strcmp(toks[i], CHAT_EN_SURFACE[e].en_word) == 0 &&
                        strcmp(CHAT_EN_SURFACE[e].es_stem,
                               kw->es_stem) == 0)
                    {
                        hit = 1;
                        break;
                    }
            if (!hit)
                for (uint32_t c = 1; c < ncand && !hit; c++)
                    if (strcmp(cand[c], kw->es_stem) == 0 ||
                        strcmp(cand[c], kw->en_stem) == 0)
                        hit = 1;
            if (hit)
            {
                kwx = (int)k;
                kwpos = (int)i;
                break;
            }
        }
    }
    *kwx_out = kwx;
    *kwpos_out = kwpos;
}

/* Fase A deduced split points: no coordinator is ever named. A
   droppable token is outside the ingested vocabulary, not a deduced
   keyword, and not a frozen delimiter (de/of/'s). Stop-tokens,
   copulas, wh-words and prepositions are all tried as split points:
   left-span trial parse plus right-span viability reject every false
   split (ES-2 composed NPs, R7 adjuncts, R9 bare fragments and
   relative-like tails collapse back to a single goal). */
static int ParseIntentToks(const CHAT *ch, const char toks[][CHAT_TOKEN_MAX],
                           uint32_t n, int q_force, int gen_force,
                           int comma_veto, PARSED *p);
static int IsDigitTok(const char *tok);
static int HasWh(const char toks[][CHAT_TOKEN_MAX], uint32_t n);
static void SpanText(const char toks[][CHAT_TOKEN_MAX], uint32_t s,
                     uint32_t e, char *out, size_t size);
/* single-token deduced-keyword hit via MatchKwSpan (no tables, no
   word lists: everything comes from the ingest-time index). */
static int KwHitTok(const CHAT *ch, const char toks[][CHAT_TOKEN_MAX],
                    uint32_t i)
{
    int kwx = -1, kwpos = -1;
    MatchKwSpan(ch, toks, i, i + 1, &kwx, &kwpos);
    return kwx >= 0;
}
/* Fase A deduced split points: no coordinator is ever named. A
   droppable token is outside the ingested vocabulary, not a deduced
   keyword, and not a frozen delimiter (de/of/'s). Everything else is
   tried: left-span trial-parse (or tool-route) with a content-closed
   left end, plus a valid right opening (below), reject every false
   split (ES-2 composed NPs, R7 adjuncts, R9 bare fragments,
   appositions and relative-like tails collapse to a single goal). */
static int IsDroppable(const CHAT *ch, const char toks[][CHAT_TOKEN_MAX],
                       uint32_t n, uint32_t i)
{
    (void)n;
    if (VocabIdx(ch, toks[i]) >= 0)
        return 0; /* content word, never dropped */
    if (KwHitTok(ch, toks, i))
        return 0; /* relation word keeps its span */
    if (strcmp(toks[i], "de") == 0 || strcmp(toks[i], "of") == 0 ||
        strcmp(toks[i], "'s") == 0)
        return 0; /* frozen delimiters are structure, not glue */
    return 1;
}

/* frozen copula set (defined near the dispatcher; same literals
   as the kw_is scan, no new words). */
static int IsCopulaTok(const char *tok);

/* a right span opens validly when it starts with a frozen delimiter
   or wh-word, is a bare single content token (elliptical "and
   Solomon"), or holds an anaphoric token whose referent arrives via
   execution memory ("he" after was, "eso" after es). Appositions
   ([the king...]) and bare kw-led spans fail here without naming
   any article. */
static int HasAnaphoricTok(const CHAT *ch, const char toks[][CHAT_TOKEN_MAX],
                           uint32_t s, uint32_t e)
{
    uint32_t j;
    for (j = s + 1; j < e; j++)
    {
        if (!IsCopulaTok(toks[j - 1]))
            continue;
        if (VocabIdx(ch, toks[j]) >= 0 || KwHitTok(ch, toks, j) ||
            IsStopTok(toks[j]) || IsDigitTok(toks[j]))
            continue;
        return 1;
    }
    return 0;
}

static int RightOpens(const CHAT *ch, const char toks[][CHAT_TOKEN_MAX],
                      uint32_t r, uint32_t e)
{
    if (IsCopulaTok(toks[r]))
    {
        /* copula-led span: only when it binds its own relation
           ("es el padre de..." parses whole); bare "was he..."
           tails never open a goal. Checked first so the anaphoric
           rule below cannot rescue a fragment. */
        int kwx = -1, kwpos = -1;
        MatchKwSpan(ch, toks, r, e, &kwx, &kwpos);
        return kwx >= 0;
    }
    if (strcmp(toks[r], "de") == 0 || strcmp(toks[r], "of") == 0)
        return 1;
    if (HasWh(toks + r, 1))
        return 1;
    if (e == r + 1 && !IsStopTok(toks[r]) && !IsDigitTok(toks[r]))
        return 1;
    if (HasAnaphoricTok(ch, toks, r, e))
        return 1;
    return 0;
}

/* standalone trial parse of a left span (Fase A selection): pure
   ParseIntentToks, no focus or NLG side effects. With inherited kw
   text the elliptical span is rehydrated first ("padre"+"de
   salomon"), exactly as the evaluator will see it. */
static int TrialParseGoal(const CHAT *ch, const char toks[][CHAT_TOKEN_MAX],
                          uint32_t s, uint32_t e, int inh_kwpos,
                          int force_q, int force_gen, PARSED *out)
{
    char ltoks[CHAT_MAX_TOKS][CHAT_TOKEN_MAX];
    uint32_t ln = 0;
    if (inh_kwpos >= 0)
    {
        strncpy(ltoks[0], toks[inh_kwpos], CHAT_TOKEN_MAX - 1);
        ltoks[0][CHAT_TOKEN_MAX - 1] = '\0';
        ln = 1;
    }
    for (uint32_t i = s; i < e && ln < CHAT_MAX_TOKS; i++)
    {
        strncpy(ltoks[ln], toks[i], CHAT_TOKEN_MAX - 1);
        ltoks[ln][CHAT_TOKEN_MAX - 1] = '\0';
        ln++;
    }
    PARSED tmp;
    int ok = ParseIntentToks(ch, ltoks, ln, force_q, force_gen, 0,
                             &tmp);
    if (out != NULL)
        *out = tmp;
    return ok;
}

static int EmitGoal(const CHAT *ch, const char toks[][CHAT_TOKEN_MAX],
                    uint32_t s, uint32_t e, int inh_kwpos,
                    QueryPlan *plan)
{
    if (plan->count >= QP_MAX_GOALS)
        return -1;
    QueryGoal *g = &plan->goals[plan->count];
    g->start = s;
    g->end = e;
    g->connector = (plan->count == 0) ? 0 : 1;
    g->kwx = -1;
    g->kwpos = -1;
    g->inherit = 0;
    MatchKwSpan(ch, toks, s, e, &g->kwx, &g->kwpos);
    if (g->kwx < 0 && inh_kwpos >= 0)
    {
        g->inherit = 1;
        MatchKwSpan(ch, toks, (uint32_t)inh_kwpos,
                    (uint32_t)inh_kwpos + 1, &g->kwx, &g->kwpos);
    }
    plan->count++;
    return 0;
}

/* tool-routed span (Agent Core selection): a left span that fails
   frames but classifies NEEDS_TOOL still opens a split (calculator
   and unframed person/relation shapes have no trial-parse). The
   rehydrated text is classified exactly as the evaluator will see
   it; vetoed shapes (bare fragments) stay GENUINE and never split. */
static int SpanRoutesTool(const CHAT *ch, const char toks[][CHAT_TOKEN_MAX],
                          uint32_t s, uint32_t e, int inh_kwpos)
{
    char ltoks[CHAT_MAX_TOKS][CHAT_TOKEN_MAX];
    uint32_t ln = 0;
    char line[512];
    ToolRequest treq;
    uint32_t i;
    if (inh_kwpos >= 0)
    {
        strncpy(ltoks[0], toks[inh_kwpos], CHAT_TOKEN_MAX - 1);
        ltoks[0][CHAT_TOKEN_MAX - 1] = '\0';
        ln = 1;
    }
    for (i = s; i < e && ln < CHAT_MAX_TOKS; i++)
    {
        strncpy(ltoks[ln], toks[i], CHAT_TOKEN_MAX - 1);
        ltoks[ln][CHAT_TOKEN_MAX - 1] = '\0';
        ln++;
    }
    SpanText(ltoks, 0, ln, line, sizeof(line));
    memset(&treq, 0, sizeof(treq));
    return ToolClassify(ch, line, GOAL_UNKNOWN, CAUSE_PARSE_FAIL, "",
                        "", &treq) == DEC_NEEDS_TOOL;
}

/* recursive splitter: leftmost drop point whose left span
   trial-parses (or tool-routes) and whose right span stays viable;
   the right span recurses with the inherited kw text. No valid
   split: the whole range is one goal (legacy single-intent path
   decides). */
static int PlanRange(const CHAT *ch, const char toks[][CHAT_TOKEN_MAX],
                     uint32_t s, uint32_t e, int inh_kwpos,
                     int force_q, int force_gen, QueryPlan *plan)
{
    for (uint32_t i = s; i < e; i++)
    {
        uint32_t r = i + 1;
        if (i == s || r >= e)
            continue;
        if (!IsDroppable(ch, toks, e, i))
            continue;
        if (IsStopTok(toks[i - 1]))
            continue; /* dangling functional end (R7 adjuncts...) */
        if (!TrialParseGoal(ch, toks, s, i, inh_kwpos, force_q,
                            force_gen, NULL) &&
            !SpanRoutesTool(ch, toks, s, i, inh_kwpos))
            continue;
        if (!RightOpens(ch, toks, r, e))
            continue;
        if (EmitGoal(ch, toks, s, i, inh_kwpos, plan) < 0)
            return -1;
        {
            const QueryGoal *lg = &plan->goals[plan->count - 1];
            int nkw = (lg->kwx >= 0) ? lg->kwpos : inh_kwpos;
            return PlanRange(ch, toks, r, e, nkw, force_q, force_gen,
                             plan);
        }
    }
    return EmitGoal(ch, toks, s, e, inh_kwpos, plan);
}

/* Fase A Paso 1 (representation only): segment the canonical token
   stream at deduced split points into isolated QueryGoals.
   - leftmost drop point whose left span trial-parses wins;
   - the right span recurses with the inherited kw text;
   - no valid split: single goal, legacy path decides;
   - over-coordination or empty input refuses (count = 0).
   No vetoes here (G1/G2/G5 run per goal in step 3). */
static int HasWh(const char toks[][CHAT_TOKEN_MAX], uint32_t n);
uint32_t ChatBuildPlan(const CHAT *ch, const char *line, QueryPlan *plan,
                       char toks[][CHAT_TOKEN_MAX], uint32_t *ntok,
                       SURFACE_FLAGS *sfout)
{
    SURFACE_FLAGS sf;
    uint32_t n = Split(line, toks, CHAT_MAX_TOKS, &sf);
    memset(plan, 0, sizeof(*plan));
    if (ntok != NULL)
        *ntok = n;
    if (sfout != NULL)
        *sfout = sf;
    if (n == 0)
        return 0;
    {
        /* cross-clausal frames (WHY/COMPOSE) are single discourse
           units spanning both sides: a line that parses as one is
           never split, intent-detected not word-detected. */
        PARSED whole;
        int force_q = sf.question || HasWh(toks, n);
        if (TrialParseGoal(ch, toks, 0, n, -1, force_q, sf.genitive,
                           &whole))
        {
            if (whole.intent == INT_WHY || whole.intent == INT_COMPOSE_WHY ||
                whole.intent == INT_QA_CONSEQUENCE || whole.intent == INT_QA_AFFORDANCE)
                return EmitGoal(ch, toks, 0, n, -1, plan);

            /* If session has loaded text corpora and the whole line parses as
               a unified text or QA query without explicit coordinators, keep it intact. */
            if (ch->ntfiles > 0 &&
                (whole.intent == INT_TEXTQ || whole.intent == INT_QA_ENTITY))
            {
                int has_coord = 0;
                for (uint32_t ci = 0; ci < n; ci++)
                {
                    if (strcmp(toks[ci], "y") == 0 || strcmp(toks[ci], "and") == 0 ||
                        strcmp(toks[ci], ";") == 0)
                    {
                        has_coord = 1;
                        break;
                    }
                }
                if (!has_coord)
                    return EmitGoal(ch, toks, 0, n, -1, plan);
            }
        }
    }
    if (PlanRange(ch, toks, 0, n, -1,
                  sf.question || HasWh(toks, n), sf.genitive,
                  plan) < 0)
    {
        memset(plan, 0, sizeof(*plan));
        return 0;
    }
    return plan->count;
}

/* fallback capture guard (positional, frozen copulas only): the
   slot at cappos is taken only if a token exists there and the one
   after it is not a copula (a slot directly followed by a copula is
   the subject of a clause, never an argument). */
static int FallbackOpen(const char toks[][CHAT_TOKEN_MAX], uint32_t n,
                        uint32_t cappos)
{
    return cappos < n &&
           (cappos + 1 >= n || !IsCopulaTok(toks[cappos + 1]));
}

/* Forward declarations for structural QA classifier */
static int LooksLikeQuestionWord(const char *tok, uint32_t pos,
                                  const char toks[][CHAT_TOKEN_MAX],
                                  uint32_t n);
static INTENT DetectQuestionType(const char toks[][CHAT_TOKEN_MAX],
                                  uint32_t n, uint32_t wh_pos,
                                  char *entity_out, size_t entity_size);

/* Parse physical consequence condition: extracts subject, action, target */
static int ParseConsequenceCondition(const DICT *dict,
                                     const char toks[][CHAT_TOKEN_MAX],
                                     uint32_t start, uint32_t n,
                                     char *sub_out, size_t sub_size,
                                     char *act_out, size_t act_size,
                                     char *tgt_out, size_t tgt_size)
{
    if (start >= n || sub_out == NULL || act_out == NULL || tgt_out == NULL)
        return 0;
    sub_out[0] = '\0';
    act_out[0] = '\0';
    tgt_out[0] = '\0';

    /* Multi-word entity check: e.g. "vaso de cristal" */
    for (uint32_t i = start; i + 2 < n; i++)
    {
        if (strcmp(toks[i], "vaso") == 0 &&
            strcmp(toks[i + 1], "de") == 0 &&
            strcmp(toks[i + 2], "cristal") == 0)
        {
            strncpy(sub_out, "glass", sub_size - 1);
            sub_out[sub_size - 1] = '\0';
            break;
        }
    }

    for (uint32_t i = start; i < n; i++)
    {
        /* Action detection */
        if (act_out[0] == '\0')
        {
            if (strcmp(toks[i], "cae") == 0 || strcmp(toks[i], "caer") == 0 ||
                strcmp(toks[i], "caen") == 0 || strcmp(toks[i], "caiga") == 0 ||
                strcmp(toks[i], "falls") == 0 || strcmp(toks[i], "fall") == 0 ||
                strcmp(toks[i], "dropped") == 0 || strcmp(toks[i], "drop") == 0 ||
                strcmp(toks[i], "drops") == 0)
            {
                strncpy(act_out, "dropped on", act_size - 1);
                act_out[act_size - 1] = '\0';
                continue;
            }
        }

        /* Target surface detection */
        if (tgt_out[0] == '\0')
        {
            if (strcmp(toks[i], "suelo") == 0 || strcmp(toks[i], "piso") == 0 ||
                strcmp(toks[i], "floor") == 0 || strcmp(toks[i], "ground") == 0 ||
                strcmp(toks[i], "concrete") == 0 || strcmp(toks[i], "hormigon") == 0)
            {
                const char *tr = DictTranslate(dict, toks[i]);
                strncpy(tgt_out, tr ? tr : toks[i], tgt_size - 1);
                tgt_out[tgt_size - 1] = '\0';
                continue;
            }
        }

        /* Subject detection if not yet found by multi-word */
        if (sub_out[0] == '\0')
        {
            /* Skip grammatical particles / articles */
            if (strcmp(toks[i], "se") == 0 || strcmp(toks[i], "un") == 0 ||
                strcmp(toks[i], "una") == 0 || strcmp(toks[i], "el") == 0 ||
                strcmp(toks[i], "la") == 0 || strcmp(toks[i], "al") == 0 ||
                strcmp(toks[i], "a") == 0 || strcmp(toks[i], "en") == 0 ||
                strcmp(toks[i], "de") == 0 || strcmp(toks[i], "the") == 0 ||
                strcmp(toks[i], "is") == 0 || strcmp(toks[i], "to") == 0)
            {
                continue;
            }
            const char *tr = DictTranslate(dict, toks[i]);
            if (tr != NULL)
            {
                strncpy(sub_out, tr, sub_size - 1);
                sub_out[sub_size - 1] = '\0';
            }
            else
            {
                strncpy(sub_out, toks[i], sub_size - 1);
                sub_out[sub_size - 1] = '\0';
            }
        }
    }

    /* Fallback defaults for missing components */
    if (act_out[0] == '\0')
    {
        strncpy(act_out, "dropped on", act_size - 1);
        act_out[act_size - 1] = '\0';
    }
    if (tgt_out[0] == '\0')
    {
        strncpy(tgt_out, "floor", tgt_size - 1);
        tgt_out[tgt_size - 1] = '\0';
    }

    return (sub_out[0] != '\0');
}

/* token-based intent core (Fase A): the former ParseIntent body
   over caller-provided canonical tokens. q_force/gen_force/comma_veto
   are the plan-level surface signals; single-intent callers pass the
   line-level ones (behavior identical). */
static int ParseIntentToks(const CHAT *ch, const char toks[][CHAT_TOKEN_MAX],
                           uint32_t n, int q_force, int gen_force,
                           int comma_veto, PARSED *p)
{
    memset(p, 0, sizeof(*p));
    if (n == 0)
        return 0;

    /* ---- Structural QA intercept (top priority) ----
       Detect question patterns BEFORE stop-word filtering.
       Stop words (que, es, la, el...) would kill the hit scan,
       so structural detection runs first. Pattern: first token
       is 2-8 chars at position 0 → looks like a question word;
       then classify by what follows (copula, location prep, etc.).

       EXCEPTION: when a frozen-frame keyword (padre, hijo, abuelo,
       descendiente) is present, skip the QA intercept so the frozen
       frame can handle it with proper relational semantics. */
    if (LooksLikeQuestionWord(toks[0], 0, toks, n))
    {
        /* Pre-scan for frozen keywords: if present, let frames win */
        int has_frozen_kw = 0;
        for (uint32_t fi = 0; fi < n && !has_frozen_kw; fi++)
        {
            has_frozen_kw =
                /* parent keywords */
                strcmp(toks[fi], "padre") == 0 ||
                strcmp(toks[fi], "father") == 0 ||
                strcmp(toks[fi], "engendro") == 0 ||
                strcmp(toks[fi], "begat") == 0 ||
                strcmp(toks[fi], "progenitor") == 0 ||
                /* child keywords */
                strcmp(toks[fi], "hijo") == 0 ||
                strcmp(toks[fi], "hijos") == 0 ||
                strcmp(toks[fi], "children") == 0 ||
                strcmp(toks[fi], "child") == 0 ||
                strcmp(toks[fi], "son") == 0 ||
                strcmp(toks[fi], "sons") == 0 ||
                strcmp(toks[fi], "daughter") == 0 ||
                /* grandparent keywords */
                strcmp(toks[fi], "abuelo") == 0 ||
                strcmp(toks[fi], "grandfather") == 0 ||
                /* descendant keywords */
                strcmp(toks[fi], "descendiente") == 0 ||
                strcmp(toks[fi], "descendants") == 0 ||
                strcmp(toks[fi], "descendant") == 0 ||
                strcmp(toks[fi], "descendientes") == 0 ||
                /* spouse keywords */
                strcmp(toks[fi], "esposa") == 0 ||
                strcmp(toks[fi], "esposo") == 0 ||
                strcmp(toks[fi], "wife") == 0 ||
                strcmp(toks[fi], "husband") == 0 ||
                /* sibling keywords */
                strcmp(toks[fi], "hermano") == 0 ||
                strcmp(toks[fi], "hermana") == 0 ||
                strcmp(toks[fi], "brother") == 0 ||
                strcmp(toks[fi], "sister") == 0 ||
                /* ruler / king keywords */
                strcmp(toks[fi], "rey") == 0 ||
                strcmp(toks[fi], "reina") == 0 ||
                strcmp(toks[fi], "king") == 0 ||
                strcmp(toks[fi], "queen") == 0 ||
                strcmp(toks[fi], "reigns") == 0;
        }
        if (!has_frozen_kw)
        {
            /* Check physical consequence question */
            if (n >= 4 &&
                ((strcmp(toks[0], "que") == 0 &&
                  (strcmp(toks[1], "pasa") == 0 || strcmp(toks[1], "ocurre") == 0 || strcmp(toks[1], "sucede") == 0) &&
                  strcmp(toks[2], "si") == 0) ||
                 (strcmp(toks[0], "what") == 0 &&
                  strcmp(toks[1], "happens") == 0 &&
                  (strcmp(toks[2], "if") == 0 || strcmp(toks[2], "when") == 0))))
            {
                if (ParseConsequenceCondition(&ch->dict, toks, 3, n,
                                              p->a, sizeof(p->a),
                                              p->cc, sizeof(p->cc),
                                              p->b, sizeof(p->b)))
                {
                    p->intent = INT_QA_CONSEQUENCE;
                    p->has_b = 1;
                    p->ntoks = 0;
                    for (uint32_t ti = 0; ti < n && ti < CHAT_TEXT_WORDS_MAX; ti++)
                    {
                        strncpy(p->toks[p->ntoks], toks[ti], CHAT_TOKEN_MAX - 1);
                        p->toks[p->ntoks][CHAT_TOKEN_MAX - 1] = '\0';
                        p->ntoks++;
                    }
                    return 1;
                }
            }

            char entity_out[CHAT_TOKEN_MAX];
            INTENT qa_int = DetectQuestionType(toks, n, 0,
                                               entity_out,
                                               sizeof(entity_out));
            if (qa_int != INT_NONE)
            {
                p->intent = qa_int;
                strncpy(p->a, entity_out, CHAT_TOKEN_MAX - 1);
                p->a[CHAT_TOKEN_MAX - 1] = '\0';

                p->ntoks = 0;
                for (uint32_t ti = 0; ti < n && ti < CHAT_TEXT_WORDS_MAX; ti++)
                {
                    strncpy(p->toks[p->ntoks], toks[ti],
                            CHAT_TOKEN_MAX - 1);
                    p->toks[p->ntoks][CHAT_TOKEN_MAX - 1] = '\0';
                    p->ntoks++;
                }
                return 1;
            }
        }
    }

    /* ASK-load: verb + bare .tsv or .txt name, exactly two tokens. The verb
       is a frame keyword (frozen precedent: PARENT_W et al); the
       suffix rule is structural. Sandbox + existence probed
       here (read-only); the load itself happens at answer time. */
    if (n == 2 && strcmp(toks[0], "carga") == 0)
    {
        char path[512];
        size_t L = strlen(toks[1]);
        if (L > 4 && strcmp(toks[1] + L - 4, ".tsv") == 0 &&
            CRulesResolvePath(toks[1], path, sizeof(path)))
        {
            strncpy(p->a, toks[1], CHAT_TOKEN_MAX - 1);
            p->a[CHAT_TOKEN_MAX - 1] = '\0';
            p->intent = INT_LOAD;
            return 1;
        }
        if (L > 4 && strcmp(toks[1] + L - 4, ".txt") == 0 &&
            CRulesResolvePath(toks[1], path, sizeof(path)))
        {
            strncpy(p->a, toks[1], CHAT_TOKEN_MAX - 1);
            p->a[CHAT_TOKEN_MAX - 1] = '\0';
            p->intent = INT_TEXTLOAD;
            return 1;
        }
        return 0;
    }

    /* ASK-ingest: load <file.tsv> hot knowledge triples (same
       shape/probe rules as carga; the answer counts admitted rows,
       never silent). load <file.txt> transfers intact running
       text to the session graph (converter counts sentences and
       symbols; zero training, SchemaKB untouched). */
    if (n == 2 && strcmp(toks[0], "load") == 0)
    {
        char path[512];
        size_t L = strlen(toks[1]);
        if (L > 4 && strcmp(toks[1] + L - 4, ".tsv") == 0 &&
            CRulesResolvePath(toks[1], path, sizeof(path)))
        {
            strncpy(p->a, toks[1], CHAT_TOKEN_MAX - 1);
            p->a[CHAT_TOKEN_MAX - 1] = '\0';
            p->intent = INT_INGIERE;
            return 1;
        }
        if (L > 4 && strcmp(toks[1] + L - 4, ".txt") == 0 &&
            CRulesResolvePath(toks[1], path, sizeof(path)))
        {
            strncpy(p->a, toks[1], CHAT_TOKEN_MAX - 1);
            p->a[CHAT_TOKEN_MAX - 1] = '\0';
            p->intent = INT_TEXTLOAD;
            return 1;
        }
        return 0;
    }

    /* ASK-unload: verb + bare .txt name. Resolve probes shape;
       the answer requires the file loaded (tfiles) and rebuilds
       the session stores exactly. */
    if (n == 2 && strcmp(toks[0], "unload") == 0)
    {
        char path[512];
        size_t L = strlen(toks[1]);
        if (L > 4 && strcmp(toks[1] + L - 4, ".txt") == 0 &&
            CRulesResolvePath(toks[1], path, sizeof(path)))
        {
            strncpy(p->a, toks[1], CHAT_TOKEN_MAX - 1);
            p->a[CHAT_TOKEN_MAX - 1] = '\0';
            p->intent = INT_UNLOAD;
            return 1;
        }
        return 0;
    }

    /* session inventory: what texts are loaded (counts live in
       the stores, so replays report honest totals). Slot carries
       the verb so focus/anaphora guards pass it through. */
    if (n == 1 && (strcmp(toks[0], "estado") == 0 ||
                   strcmp(toks[0], "status") == 0))
    {
        strncpy(p->a, toks[0], CHAT_TOKEN_MAX - 1);
        p->a[CHAT_TOKEN_MAX - 1] = '\0';
        p->intent = INT_TEXTSTATUS;
        return 1;
    }

    /* TEXT_TOPICS: thematic introspection over loaded corpus */
    {
        int has_topic = 0;
        int has_question = 0;
        for (uint32_t i = 0; i < n; i++)
        {
            if (strcmp(toks[i], "temas") == 0 || strcmp(toks[i], "areas") == 0 ||
                strcmp(toks[i], "topics") == 0 || strcmp(toks[i], "materias") == 0 ||
                strcmp(toks[i], "ambitos") == 0)
                has_topic = 1;
            if (strcmp(toks[i], "conoces") == 0 || strcmp(toks[i], "hablar") == 0 ||
                strcmp(toks[i], "dime") == 0 || strcmp(toks[i], "sabes") == 0 ||
                strcmp(toks[i], "hay") == 0 || strcmp(toks[i], "trata") == 0 ||
                strcmp(toks[i], "tell") == 0 || strcmp(toks[i], "know") == 0 ||
                strcmp(toks[i], "talk") == 0 || strcmp(toks[i], "podemos") == 0)
                has_question = 1;
        }
        if ((has_topic && (has_question || n <= 2 || q_force)) ||
            (has_question && n >= 2 &&
             ((strcmp(toks[0], "hablar") == 0 || strcmp(toks[n - 1], "hablar") == 0) ||
              (strcmp(toks[0], "talk") == 0 || strcmp(toks[n - 1], "talk") == 0))))
        {
            strncpy(p->a, "temas", CHAT_TOKEN_MAX - 1);
            p->a[CHAT_TOKEN_MAX - 1] = '\0';
            p->intent = INT_TEXT_TOPICS;
            return 1;
        }
    }

    /* TEXT_START: initiate dialogue over loaded corpus */
    {
        int has_start = 0;
        int has_conv = 0;
        for (uint32_t i = 0; i < n; i++)
        {
            if (strcmp(toks[i], "inicia") == 0 || strcmp(toks[i], "iniciar") == 0 ||
                strcmp(toks[i], "start") == 0 || strcmp(toks[i], "empezar") == 0 ||
                strcmp(toks[i], "comienza") == 0 || strcmp(toks[i], "comencemos") == 0)
                has_start = 1;
            if (strcmp(toks[i], "conversacion") == 0 || strcmp(toks[i], "conversation") == 0 ||
                strcmp(toks[i], "charla") == 0 || strcmp(toks[i], "dialogo") == 0)
                has_conv = 1;
        }
        if ((has_start && has_conv) ||
            (n <= 2 && (strcmp(toks[0], "hablemos") == 0 ||
                        strcmp(toks[0], "comencemos") == 0 ||
                        strcmp(toks[0], "comienza") == 0)))
        {
            strncpy(p->a, "start", CHAT_TOKEN_MAX - 1);
            p->a[CHAT_TOKEN_MAX - 1] = '\0';
            p->intent = INT_TEXT_START;
            return 1;
        }
    }

    int kw_parent = -1, kw_child = -1, kw_grand = -1, kw_desc = -1;
    int kw_why = -1, kw_who = -1, kw_is = -1;
    static const char *CHILD_W[] = {"hijo",  "hijos",    "children",
                                    "child", "son",      "sons",
                                    "daughter"};
    static const char *GRAND_W[] = {"abuelo", "grandfather"};
    static const char *DESC_W[] = {"descendiente", "descendants",
                                   "descendant", "descendientes"};
    for (uint32_t i = 0; i < n; i++)
    {
        for (int k = 0; k < PARENT_SURFACE_N; k++)
            if (kw_parent < 0 && strcmp(toks[i], PARENT_SURFACE[k]) == 0)
                kw_parent = (int)i;
        for (int k = 0; k < 7; k++)
            if (kw_child < 0 && strcmp(toks[i], CHILD_W[k]) == 0)
            {
                /* "son"/"sons" right after a wh-word is the ES
                   plural copula ("quienes son ..."), not the child
                   noun; the loop keeps scanning for a real one */
                if ((strcmp(toks[i], "son") == 0 ||
                     strcmp(toks[i], "sons") == 0) &&
                    i > 0 &&
                    (strcmp(toks[i - 1], "quien") == 0 ||
                     strcmp(toks[i - 1], "quienes") == 0 ||
                     strcmp(toks[i - 1], "who") == 0 ||
                     strcmp(toks[i - 1], "whom") == 0 ||
                     strcmp(toks[i - 1], "cual") == 0 ||
                     strcmp(toks[i - 1], "cuales") == 0))
                    continue;
                kw_child = (int)i;
            }
        for (int k = 0; k < 2; k++)
            if (kw_grand < 0 && strcmp(toks[i], GRAND_W[k]) == 0)
                kw_grand = (int)i;
        for (int k = 0; k < 4; k++)
            if (kw_desc < 0 && strcmp(toks[i], DESC_W[k]) == 0)
                kw_desc = (int)i;
        if (kw_why < 0 &&
            (strcmp(toks[i], "por") == 0 || strcmp(toks[i], "why") == 0))
            kw_why = (int)i;
        if (kw_who < 0 &&
            (strcmp(toks[i], "quien") == 0 || strcmp(toks[i], "quienes") == 0 ||
             strcmp(toks[i], "who") == 0 || strcmp(toks[i], "whom") == 0))
            kw_who = (int)i;
        if (kw_is < 0 &&
            (strcmp(toks[i], "es") == 0 || strcmp(toks[i], "era") == 0 ||
             strcmp(toks[i], "fue") == 0 || strcmp(toks[i], "is") == 0 ||
             strcmp(toks[i], "was") == 0))
            kw_is = (int)i;
    }
    (void)kw_who;
    /* FASE 4 interrogative force: surface question flag or a wh-word
       anywhere in the line (wh-optional elliptical queries keep
       working through the topic/genitive exemptions below). */
    int flag_q = q_force || kw_who >= 0;

    /* generic deduced-relation match over the whole line */
    int kwx = -1;
    int kwpos = -1;
    MatchKwSpan(ch, toks, 0, n, &kwx, &kwpos);

    /* second deduced keyword (compose frame): the LAST relation
       word that is NOT the primary one (primary = first match at
       kwpos). Returns -1 when absent. */
    int kw2 = -1;
    int kw2pos = -1;
    for (int i = n - 1; i >= 0; i--)
    {
        if (i == kwpos)
            continue;
        char cand[4][CHAT_TOKEN_MAX];
        uint32_t ncand = 0;
        MorphFold(toks[i], cand, &ncand);
        for (uint32_t k = 0; k < ch->num_kws; k++)
        {
            if ((int)k == kwx)
                continue;
            int hit = strcmp(toks[i], ch->kws[k].es_stem) == 0 ||
                      strcmp(toks[i], ch->kws[k].en_stem) == 0;
            if (!hit)
                for (uint32_t c = 1; c < ncand && !hit; c++)
                    if (strcmp(cand[c], ch->kws[k].es_stem) == 0 ||
                        strcmp(cand[c], ch->kws[k].en_stem) == 0)
                        hit = 1;
            if (hit)
            {
                kw2 = (int)k;
                kw2pos = i;
                break;
            }
        }
        if (kw2 >= 0)
            break;
    }
    (void)kw2pos;

    /* COMPOSE WHY (Spanish frame "por que A es <kw1> de B siendo B
       <kw2> de C"): justification of a heterogeneous composition
       conclusion via the bridging entity B. Slots: A = after the
       why marker (skipping "que"/copula), B = after kwpos+"de",
       C = after kw2pos+"de". Frozen frames above keep priority. */
    if (kw_why >= 0 && kw2 >= 0 && kwpos >= 0 && kw2pos > kwpos)
    {
        int ia = kw_why + 1;
        if (ia < (int)n && (strcmp(toks[ia], "que") == 0 ||
                            strcmp(toks[ia], "es") == 0 ||
                            strcmp(toks[ia], "is") == 0 ||
                            strcmp(toks[ia], "was") == 0))
            ia++;
        char slotB[CHAT_TOKEN_MAX], slotC[CHAT_TOKEN_MAX];
        if (ia < (int)n && ia < kwpos &&
            TokAfterDe(toks, n, (uint32_t)kwpos + 1, slotB,
                       CHAT_TOKEN_MAX) &&
            TokAfterDe(toks, n, (uint32_t)kw2pos + 1, slotC,
                       CHAT_TOKEN_MAX))
        {
            strncpy(p->a, toks[ia], CHAT_TOKEN_MAX - 1);
            p->a[CHAT_TOKEN_MAX - 1] = '\0';
            strncpy(p->b, slotB, CHAT_TOKEN_MAX - 1);
            p->b[CHAT_TOKEN_MAX - 1] = '\0';
            strncpy(p->cc, slotC, CHAT_TOKEN_MAX - 1);
            p->cc[CHAT_TOKEN_MAX - 1] = '\0';
            p->has_b = 1;
            p->kw = kwx;
            p->kw2 = kw2;
            p->intent = INT_COMPOSE_WHY;
            return 1;
        }
        return 0;
    }

    /* WHY: "por que A es hijo de B" / "why is A the child of B" */
    if (kw_why >= 0 && kw_parent < 0 && kw_child >= 0)
    {
        /* B = token after "hijo de" */
        if (kw_child >= 0 && kw_child + 2 < (int)n)
        {
            strncpy(p->b, toks[kw_child + 2], CHAT_TOKEN_MAX - 1);
            p->b[CHAT_TOKEN_MAX - 1] = '\0';
            p->has_b = 1;
        }
        /* A = token after the why marker, skipping "que" (ES) or
           copula "is/was" (EN) */
        int ia = kw_why + 1;
        if (ia < (int)n && (strcmp(toks[ia], "que") == 0 ||
                            strcmp(toks[ia], "es") == 0 ||
                            strcmp(toks[ia], "is") == 0 ||
                            strcmp(toks[ia], "was") == 0))
            ia++;
        if (ia < (int)n)
        {
            strncpy(p->a, toks[ia], CHAT_TOKEN_MAX - 1);
            p->a[CHAT_TOKEN_MAX - 1] = '\0';
            p->intent = INT_WHY;
            return 1;
        }
        return 0;
    }

    /* 3-hop: padre + abuelo in the same line ("padre del abuelo de X").
       Uses existing relation words; hop count is composition, not a
       new lexicon entry. Entity is the argument of the later keyword. */
    if (kw_parent >= 0 && kw_grand >= 0)
    {
        uint32_t from =
            (uint32_t)(kw_grand > kw_parent ? kw_grand : kw_parent) + 1;
        if (TokAfterDe(toks, n, from, p->a, CHAT_TOKEN_MAX) ||
            FallbackOpen(toks, n, from))
        {
            if (p->a[0] == '\0' && from < n)
                strncpy(p->a, toks[from], CHAT_TOKEN_MAX - 1),
                    p->a[CHAT_TOKEN_MAX - 1] = '\0';
            if (!SlotOk(p->a))
                return 0;
            p->intent = INT_GRANDPARENT;
            p->hops = 3;
            return 1;
        }
        return 0;
    }

    /* GRANDPARENT: "quien es el abuelo de X" */
    if (kw_grand >= 0)
    {
        if (TokAfterDe(toks, n, (uint32_t)kw_grand + 1, p->a,
                       CHAT_TOKEN_MAX) ||
            FallbackOpen(toks, n, (uint32_t)kw_grand + 1))
        {
            if (p->a[0] == '\0')
                strncpy(p->a, toks[kw_grand + 1], CHAT_TOKEN_MAX - 1),
                    p->a[CHAT_TOKEN_MAX - 1] = '\0';
            /* FASE 4 G2: orphan slot (article, delimiter...) vetoes */
            if (!SlotOk(p->a))
                return 0;
            p->intent = INT_GRANDPARENT;
            p->hops = 2;
            return 1;
        }
        return 0;
    }

    /* DESCENDANT */
    if (kw_desc >= 0)
    {
        if (TokAfterDe(toks, n, (uint32_t)kw_desc + 1, p->a,
                       CHAT_TOKEN_MAX) ||
            FallbackOpen(toks, n, (uint32_t)kw_desc + 1))
        {
            if (p->a[0] == '\0')
                strncpy(p->a, toks[kw_desc + 1], CHAT_TOKEN_MAX - 1),
                    p->a[CHAT_TOKEN_MAX - 1] = '\0';
            /* FASE 4 G2: orphan slot (article, delimiter...) vetoes */
            if (!SlotOk(p->a))
                return 0;
            p->intent = INT_DESCENDANT;
            return 1;
        }
        return 0;
    }

    /* IS PARENT boolean: "es A hijo de B" (kw_is BEFORE kw_child;
       articles in slot A mean it is a wh-question, not boolean) */
    if (kw_is >= 0 && kw_child >= 0 && kw_is < kw_child &&
        kw_child + 2 < (int)n && (uint32_t)kw_is + 1 < n)
    {
        const char *cand = toks[kw_is + 1];
        if (strcmp(cand, "el") != 0 && strcmp(cand, "la") != 0 &&
            strcmp(cand, "los") != 0 && strcmp(cand, "las") != 0 &&
            strcmp(cand, "the") != 0)
        {
            strncpy(p->a, cand, CHAT_TOKEN_MAX - 1);
            p->a[CHAT_TOKEN_MAX - 1] = '\0';
            strncpy(p->b, toks[kw_child + 2], CHAT_TOKEN_MAX - 1);
            p->b[CHAT_TOKEN_MAX - 1] = '\0';
            p->has_b = 1;
            p->intent = INT_IS_PARENT;
            return 1;
        }
    }

    /* GENERIC BOOL: "es A <kw> de B" / "A es <kw> de B" / "is A the
       <kw> of B" for any deduced relation; frozen frames above keep
       priority (their keywords disable this path). The candidate
       right after the copula must not be an article nor the
       relation word itself in any folded form — UNLESS the subject
       sits BEFORE the copula ("nabal es esposo de abigail"), in
       which case the candidate is the relation word and slot A is
       the pre-copula token (never a wh-word). */
    if (kwx >= 0 && kw_is >= 0 && kw_child < 0 && kw_grand < 0 &&
        kw_desc < 0 && (uint32_t)kw_is + 1 < n)
    {
        const char *cand = toks[kw_is + 1];
        int prev_is_name =
            kw_is > 0 && strcmp(toks[kw_is - 1], "quien") != 0 &&
            strcmp(toks[kw_is - 1], "quienes") != 0 &&
            strcmp(toks[kw_is - 1], "who") != 0 &&
            strcmp(toks[kw_is - 1], "el") != 0 &&
            strcmp(toks[kw_is - 1], "la") != 0 &&
            strcmp(toks[kw_is - 1], "the") != 0;
        if (strcmp(cand, "el") != 0 && strcmp(cand, "la") != 0 &&
            strcmp(cand, "los") != 0 && strcmp(cand, "las") != 0 &&
            strcmp(cand, "the") != 0 && (int)kw_is != kwpos)
        {
            /* reject the relation word itself in any folded form,
               including the shared EN surface table (the matcher
               knows "king" is rey, so the guard must too: otherwise
               "who was king of X" misreads King as a person) */
            char rcand[4][CHAT_TOKEN_MAX];
            uint32_t nrc = 0;
            MorphFold(cand, rcand, &nrc);
            int is_kw_variant = 0;
            for (uint32_t c = 0; c < nrc && !is_kw_variant; c++)
                if (strcmp(rcand[c], ch->kws[kwx].es_stem) == 0 ||
                    strcmp(rcand[c], ch->kws[kwx].en_stem) == 0)
                    is_kw_variant = 1;
            for (size_t e = 0; e < CHAT_EN_SURFACE_N && !is_kw_variant;
                 e++)
                if (strcmp(cand, CHAT_EN_SURFACE[e].en_word) == 0 &&
                    strcmp(CHAT_EN_SURFACE[e].es_stem,
                           ch->kws[kwx].es_stem) == 0)
                    is_kw_variant = 1;
            /* A-copula-kw-de-B: candidate being a kw variant is
               expected (it IS the relation word) */
            if (!is_kw_variant || prev_is_name)
            {
                char bslot[CHAT_TOKEN_MAX];
                if (TokAfterDe(toks, n, (uint32_t)kwpos + 1, bslot,
                               CHAT_TOKEN_MAX))
                {
                    if (prev_is_name)
                        strncpy(p->a, toks[kw_is - 1],
                                CHAT_TOKEN_MAX - 1);
                    else
                        strncpy(p->a, cand, CHAT_TOKEN_MAX - 1);
                    p->a[CHAT_TOKEN_MAX - 1] = '\0';
                    strncpy(p->b, bslot, CHAT_TOKEN_MAX - 1);
                    p->b[CHAT_TOKEN_MAX - 1] = '\0';
                    p->has_b = 1;
                    p->kw = kwx;
                    p->intent = INT_REL_BOOL;
                    return 1;
                }
            }
        }
    }

    /* CHILDREN: "quienes son los hijos de X"; anaphora "sus"
       (or "su/his/her + hijo" = child of [focus]). FASE 4: G1 vetoes
       bare "[det] hijo of ARG" fragments (topic/force required), G2
       vetoes orphan slots, G5 defers comma appositions, and the
       's-genitive claims X. */
    if (kw_child >= 0 && kw_parent < 0)
    {
        char slot[CHAT_TOKEN_MAX];
        if (comma_veto)
            return 0; /* G5: apposition out of scope for F4 */
        if (TokAfterDe(toks, n, (uint32_t)kw_child + 1, slot,
                       CHAT_TOKEN_MAX))
        {
            if (!SlotOk(slot))
                return 0; /* G2: delimiter/article/possessive as arg */
            if (!flag_q && !gen_force && !HasTopicBefore(toks, kw_child))
                return 0; /* G1: declarative fragment, no force */
            strncpy(p->a, slot, CHAT_TOKEN_MAX - 1);
            p->a[CHAT_TOKEN_MAX - 1] = '\0';
            p->intent = INT_CHILDREN_OF;
            return 1;
        }
        if (GenitiveArg(toks, kw_child, slot))
        {
            if (!SlotOk(slot))
                return 0;
            strncpy(p->a, slot, CHAT_TOKEN_MAX - 1);
            p->a[CHAT_TOKEN_MAX - 1] = '\0';
            p->intent = INT_CHILDREN_OF;
            return 1;
        }
        int sus = HasTok(toks, n, "sus");
        sus = sus < 0 ? HasTok(toks, n, "su") : sus;
        if (sus >= 0 || PrevIsPoss(toks, kw_child))
        {
            p->a[0] = '\0'; /* focus fills it (G3 abstains when absent) */
            p->intent = INT_CHILDREN_OF;
            return 1;
        }
        if (FallbackOpen(toks, n, (uint32_t)kw_child + 1))
        {
            if (!SlotOk(toks[kw_child + 1]))
                return 0; /* G2 */
            if (!flag_q && !gen_force && !HasTopicBefore(toks, kw_child))
                return 0; /* G1 */
            strncpy(p->a, toks[kw_child + 1], CHAT_TOKEN_MAX - 1);
            p->a[CHAT_TOKEN_MAX - 1] = '\0';
            p->intent = INT_CHILDREN_OF;
            return 1;
        }
        if (kw_child > 1 && TokAfterDe(toks, (uint32_t)kw_child, 0, slot, CHAT_TOKEN_MAX))
        {
            if (SlotOk(slot))
            {
                strncpy(p->a, slot, CHAT_TOKEN_MAX - 1);
                p->a[CHAT_TOKEN_MAX - 1] = '\0';
                p->intent = INT_CHILDREN_OF;
                return 1;
            }
        }
        return 0;
    }

    /* PARENT: "quien fue el padre de X" (or "su padre" / "his
       father" = father of [focus]). FASE 4 guards as in CHILDREN. */
    if (kw_parent >= 0)
    {
        char slot[CHAT_TOKEN_MAX];
        if (comma_veto)
            return 0; /* G5: apposition out of scope for F4 */
        if (TokAfterDe(toks, n, (uint32_t)kw_parent + 1, slot,
                       CHAT_TOKEN_MAX))
        {
            if (!SlotOk(slot))
                return 0; /* G2 */
            if (!flag_q && !gen_force && !HasTopicBefore(toks, kw_parent))
                return 0; /* G1 */
            strncpy(p->a, slot, CHAT_TOKEN_MAX - 1);
            p->a[CHAT_TOKEN_MAX - 1] = '\0';
            p->intent = INT_PARENT_OF;
            return 1;
        }
        if (GenitiveArg(toks, kw_parent, slot))
        {
            if (!SlotOk(slot))
                return 0;
            strncpy(p->a, slot, CHAT_TOKEN_MAX - 1);
            p->a[CHAT_TOKEN_MAX - 1] = '\0';
            p->intent = INT_PARENT_OF;
            return 1;
        }
        if (PrevIsPoss(toks, kw_parent))
        {
            p->a[0] = '\0'; /* focus fills it (G3 abstains when absent) */
            p->intent = INT_PARENT_OF;
            return 1;
        }
        if (FallbackOpen(toks, n, (uint32_t)kw_parent + 1))
        {
            if (!SlotOk(toks[kw_parent + 1]))
                return 0; /* G2 */
            if (!flag_q && !gen_force && !HasTopicBefore(toks, kw_parent))
                return 0; /* G1 */
            strncpy(p->a, toks[kw_parent + 1], CHAT_TOKEN_MAX - 1);
            p->a[CHAT_TOKEN_MAX - 1] = '\0';
            p->intent = INT_PARENT_OF;
            return 1;
        }
        if (kw_parent > 1 && TokAfterDe(toks, (uint32_t)kw_parent, 0, slot, CHAT_TOKEN_MAX))
        {
            if (SlotOk(slot))
            {
                strncpy(p->a, slot, CHAT_TOKEN_MAX - 1);
                p->a[CHAT_TOKEN_MAX - 1] = '\0';
                p->intent = INT_PARENT_OF;
                return 1;
            }
        }
        return 0;
    }

    /* IS PARENT boolean: "es A hijo de B" */
    if (kw_is >= 0 && kw_child >= 0 && kw_child + 2 < (int)n &&
        (uint32_t)kw_is + 1 < n)
    {
        strncpy(p->a, toks[kw_is + 1], CHAT_TOKEN_MAX - 1);
        p->a[CHAT_TOKEN_MAX - 1] = '\0';
        strncpy(p->b, toks[kw_child + 2], CHAT_TOKEN_MAX - 1);
        p->b[CHAT_TOKEN_MAX - 1] = '\0';
        p->has_b = 1;
        p->intent = INT_IS_PARENT;
        return 1;
    }

    /* GENERIC QUERY: "quien es el <kw> de X" / "who is the <kw> of
       X" / "de que <kw> fue rey X" — for any deduced relation not
       claimed by the frozen frames above. Also claims the
       father-family wh-question ("quien es el padre de X") when the
       candidate after the copula IS the relation word itself, which
       the BOOL guard above rejects. Slot A = the token after the
       first "de"/"of" following the keyword (article-stripped), or
       the token right after the keyword. Wh-word optional
       ("james reino de israel" still parses). */
    if (kwx >= 0 && kw_child < 0 && kw_grand < 0 && kw_desc < 0 &&
        kw_why < 0)
    {
        const REL_KW *kw = &ch->kws[kwx];
        /* copula candidate matches the relation word? exact or any
           morphological variant counts (esposo/esposa for esposa) */
        int cop_is_kw = 0;
        if (kw_is >= 0 && (uint32_t)kw_is + 1 < n)
        {
            char cc[4][CHAT_TOKEN_MAX];
            uint32_t ncc = 0;
            MorphFold(toks[kw_is + 1], cc, &ncc);
            for (uint32_t c = 0; c < ncc; c++)
                if (strcmp(cc[c], kw->es_stem) == 0 ||
                    strcmp(cc[c], kw->en_stem) == 0)
                {
                    cop_is_kw = 1;
                    break;
                }
        }
        /* the father wh-question with kw_parent would fall through
           the frozen PARENT frame only if TokAfterDe fails; here it
           is claimed when the copula candidate is the stem */
        if (kw_parent >= 0 && !cop_is_kw)
            goto generic_query_done;
        /* EN query "who is the brother of X": a copula directly
           after the wh-word means the slot follows the keyword */
        uint32_t from = (uint32_t)kwpos + 1;
        if (cop_is_kw)
            from = (uint32_t)kw_is + 2;
        if (TokAfterDe(toks, n, from, p->a, CHAT_TOKEN_MAX))
        {
            if (!SlotOk(p->a))
                return 0; /* FASE 4 G2 (no G1 here: wh-optional
                             bare fragments stay valid, Fase 1 pin) */
            p->kw = kwx;
            p->intent = INT_REL_QUERY;
            return 1;
        }
        if (FallbackOpen(toks, n, from))
        {
            if (!SlotOk(toks[from]))
                return 0; /* FASE 4 G2 */
            strncpy(p->a, toks[from], CHAT_TOKEN_MAX - 1);
            p->a[CHAT_TOKEN_MAX - 1] = '\0';
            p->kw = kwx;
            p->intent = INT_REL_QUERY;
            return 1;
        }
        if (kwpos > 1 && TokAfterDe(toks, (uint32_t)kwpos, 0, p->a, CHAT_TOKEN_MAX))
        {
            if (SlotOk(p->a))
            {
                p->kw = kwx;
                p->intent = INT_REL_QUERY;
                return 1;
            }
        }
        return 0;
    }

    /* TEXTQ fallback: content line over session text graphs.
       No family claimed the line (deduced kw and frozen keywords
       all absent); at least one query token hits a text symbol
       (case variants probed). No question-word lists: shape comes
       from the failure of every family frame, identity from the
       data. The answer ranks by QKV attention. */
    if (kwx < 0 && kw_parent < 0 && kw_child < 0 && kw_grand < 0 &&
        kw_desc < 0 && kw_why < 0 && ch->ntfiles > 0 &&
        ch->tgraph != NULL)
    {
        int hit = -1;
        uint32_t i;
        for (i = 0; i < n; i++)
        {
            if (IsStopTok(toks[i]))
                continue;
            if (p->ntoks < 8)
            {
                strncpy(p->toks[p->ntoks], toks[i], CHAT_TOKEN_MAX - 1);
                p->toks[p->ntoks][CHAT_TOKEN_MAX - 1] = '\0';
                p->ntoks++;
            }
            if (hit < 0 &&
                TextLexFindSymbol(ch->tgraph, toks[i]) !=
                    SYMBOL_INVALID)
                hit = (int)i;
        }
        if (hit >= 0)
        {
            strncpy(p->a, toks[hit], CHAT_TOKEN_MAX - 1);
            p->a[CHAT_TOKEN_MAX - 1] = '\0';
            p->intent = INT_TEXTQ;
            p->t_following = 0;
            return 1;
        }
        /* follow-up: single verb+clitic token, no hits, live topic
           cache (KV). Advances through ranked sentences instead of
           repeating or abstaining. Bare-? with zero content tokens
           (por que?) joins the same path: pure continuation shape. */
        if (hit < 0 && ch->tnw > 0 &&
            ((n == 1 && HasCliticEnding(toks[0])) ||
             (q_force && p->ntoks == 0)))
        {
            strncpy(p->a, toks[0], CHAT_TOKEN_MAX - 1);
            p->a[CHAT_TOKEN_MAX - 1] = '\0';
            p->intent = INT_TEXTQ;
            p->t_following = 1;
            return 1;
        }
        /* geometric fallback: unknown token nearest symbol within
           bound 3 (psique~psyche). Runs after exact hits (exact
           wins) and clitics (follow-ups win). Orientation uses
           the substitute; NLG marks it. Names longer than a slot
           cannot route (truncation would unground them). */
        if (hit < 0 && ch->ntfiles > 0 && ch->tgraph != NULL)
        {
            uint32_t i;
            for (i = 0; i < n; i++)
            {
                SYMBOL_ID sub;
                const SYMBOL *ss;
                uint32_t dd = 99;
                uint32_t k;
                if (IsStopTok(toks[i]))
                    continue;
                sub = LevNearest(ch->tgraph, toks[i], 3, &dd);
                if (sub == SYMBOL_INVALID)
                    continue;
                ss = SymbolGet(ch->tgraph->symbols, sub);
                if (ss == NULL || ss->name == NULL ||
                    strlen(ss->name) >= CHAT_TOKEN_MAX)
                    continue;
                for (k = 0; k < p->ntoks; k++)
                {
                    if (strcmp(p->toks[k], toks[i]) == 0)
                    {
                        strncpy(p->toks[k], ss->name,
                                CHAT_TOKEN_MAX - 1);
                        p->toks[k][CHAT_TOKEN_MAX - 1] = '\0';
                    }
                }
                strncpy(p->a, ss->name, CHAT_TOKEN_MAX - 1);
                p->a[CHAT_TOKEN_MAX - 1] = '\0';
                strncpy(p->t_sub, ss->name, CHAT_TOKEN_MAX - 1);
                p->t_sub[CHAT_TOKEN_MAX - 1] = '\0';
                p->intent = INT_TEXTQ;
                return 1;
            }
        }
    }

    /* ---- Structural QA fallback (HARDCODING=0) ----
       Deferred: fires ONLY when the tool planner has already been
       consulted and returned UNKNOWN. The tool planner lives in
       ChatHandleToBuf, so this block is intentionally left empty
       here. The structural QA logic lives in the answer layer
       (ChatAnswerToBuf) where it can consult the KB and text stores
       after the tool planner has given up. */

generic_query_done:
    return 0;
}

static int ParseIntent(const CHAT *ch, const char *line, PARSED *p)
{
    char toks[CHAT_MAX_TOKS][CHAT_TOKEN_MAX];
    SURFACE_FLAGS sf;
    uint32_t n = Split(line, toks, CHAT_MAX_TOKS, &sf);
    return ParseIntentToks(ch, toks, n, sf.question, sf.genitive,
                           sf.comma, p);
}

/* ---- NLG (ES) ---- */

static void RememberFocus(CHAT *ch, const char *tok)
{
    if (tok == NULL || tok[0] == '\0')
        return; /* FASE 4 G3: never store the empty string as focus */
    if (ch->focus_valid && ch->focus[0] != '\0' && strcmp(ch->focus, tok) != 0)
    {
        strncpy(ch->focus_secondary, ch->focus, sizeof(ch->focus_secondary) - 1);
        ch->focus_secondary[sizeof(ch->focus_secondary) - 1] = '\0';
        ch->focus_secondary_valid = 1;
    }
    strncpy(ch->focus, tok, sizeof(ch->focus) - 1);
    ch->focus[sizeof(ch->focus) - 1] = '\0';
    ch->focus_valid = 1;
}

static void RememberFocus2(CHAT *ch, const char *subj, const char *obj)
{
    RememberFocus(ch, subj);
    if (obj != NULL && obj[0] != '\0')
    {
        strncpy(ch->focus_secondary, obj, sizeof(ch->focus_secondary) - 1);
        ch->focus_secondary[sizeof(ch->focus_secondary) - 1] = '\0';
        ch->focus_secondary_valid = 1;
    }
}

/* goal outcomes live in chat.h (wrapper-visible) */

#define CHAT_ANSWER_MAX 4096

static void CopyLexTok(const TEXTLEX *tl, const TL_SENT *st, uint32_t idx,
                       char *buf, size_t buf_size)
{
    size_t n;
    if (buf == NULL || buf_size < 2 || st == NULL || idx >= st->ntok)
        return;
    n = (size_t)st->lens[idx];
    if (n >= buf_size)
        n = buf_size - 1;
    memcpy(buf, tl->image + st->offs[idx], n);
    buf[n] = '\0';
}

/* helper: find sentence with father-of relationship for entity.
   Pattern A: "Name father of Entity" (entity = keyword+2 tokens)
   Pattern B: "Entity ... Name his father" (entity before keyword,
              parent before possessive before keyword)
   Pattern C: "Name KEYWORD Entity" (Boaz begat Obed)
   Returns 1 on success, fills sent_buf. parent_buf (optional) gets
   the capitalized name at the winning match, not the first keyword
   in the sentence (genealogies list many begats in one period). */
static int FindFatherOfEntity(CHAT *ch, const char *entity,
                              const char *keyword, char *sent_buf,
                              size_t sent_buf_size,
                              char *parent_buf, size_t parent_buf_size)
{
    char elower[CHAT_TOKEN_MAX];
    char best_parent[CHAT_TOKEN_MAX];
    uint32_t ei;
    for (ei = 0; entity[ei] && ei < CHAT_TOKEN_MAX - 1; ei++)
        elower[ei] = (char)tolower((unsigned char)entity[ei]);
    elower[ei] = '\0';
    size_t elen = strlen(elower);
    size_t kwlen = strlen(keyword);
    uint32_t best_f = 0, best_s = 0;
    int best_dist = INT_MAX, best_ok = 0;
    best_parent[0] = '\0';
    for (uint32_t f = 0; f < ch->ntfiles; f++)
    {
        TEXTLEX *tl = &ch->tlex[f];
        if (tl->image == NULL || tl->imagelen == 0) continue;
        for (uint32_t s = 0; s < tl->nsent; s++)
        {
            TL_SENT *st2 = &tl->sents[s];
            if (st2->ntok < 3) continue;
            /* scan for keyword token */
            for (uint32_t t = 0; t < st2->ntok; t++)
            {
                const char *tok = tl->image + st2->offs[t];
                size_t tlen = (size_t)st2->lens[t];
                if ((int)tlen != (int)kwlen) continue;
                int m = 1;
                for (size_t k = 0; k < kwlen; k++)
                    if (tolower((unsigned char)tok[k]) !=
                        (unsigned char)keyword[k])
                        { m = 0; break; }
                if (!m) continue;
                /* Pattern A: keyword [stopword] Entity
                   parent = token before keyword.
                   The token after keyword must be a stopword (function
                   word like "of", "de", etc.) connecting to entity. */
                if (t >= 2 && t + 2 < st2->ntok)
                {
                    uint32_t next_idx = t + 1;
                    size_t tl_next = (size_t)st2->lens[next_idx];
                    int is_sw = 0;
                    if (tl_next >= 1 && tl_next <= 4)
                    {
                        char next_buf[8];
                        memcpy(next_buf,
                               tl->image + st2->offs[next_idx], tl_next);
                        next_buf[tl_next] = '\0';
                        is_sw = IsStopTok(next_buf);
                    }
                    if (is_sw)
                    {
                        const char *t_ent = tl->image + st2->offs[t+2];
                        size_t tl_ent = (size_t)st2->lens[t+2];
                        if ((int)tl_ent == (int)elen)
                        {
                            int me = 1;
                            for (size_t k = 0; k < elen; k++)
                                if (tolower((unsigned char)t_ent[k]) !=
                                    (unsigned char)elower[k])
                                    { me = 0; break; }
                            if (me)
                            {
                                int dist = 2; /* keyword-entity gap: father of Entity */
                                if (dist < best_dist)
                                {
                                    uint32_t pi = t;
                                    best_dist = dist;
                                    best_f = f;
                                    best_s = s;
                                    best_ok = 1;
                                    best_parent[0] = '\0';
                                    while (pi > 0)
                                    {
                                        char tmp[CHAT_TOKEN_MAX];
                                        pi--;
                                        CopyLexTok(tl, st2, pi, tmp,
                                                    sizeof(tmp));
                                        if ((st2->lens[pi] <= 4 &&
                                             (IsStopTok(tmp) ||
                                              st2->lens[pi] == 1)))
                                            continue;
                                        if (tmp[0] >= 'A' && tmp[0] <= 'Z')
                                        {
                                            strncpy(best_parent, tmp,
                                                    sizeof(best_parent) - 1);
                                            best_parent[sizeof(best_parent) - 1] =
                                                '\0';
                                            break;
                                        }
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
                /* Pattern B: Entity [..] Name [stopword] keyword */
                if (t >= 3)
                {
                    uint32_t pi = t - 1;
                    size_t tpl = (size_t)st2->lens[pi];
                    int is_sw2 = 0;
                    if (tpl >= 1 && tpl <= 4)
                    {
                        char pb[8];
                        memcpy(pb, tl->image + st2->offs[pi], tpl);
                        pb[tpl] = '\0';
                        is_sw2 = IsStopTok(pb);
                    }
                    if (is_sw2)
                    {
                        const char *tp = tl->image + st2->offs[t - 2];
                        size_t tpl2 = (size_t)st2->lens[t - 2];
                        if (tpl2 > 0 && tp[0] >= 'A' && tp[0] <= 'Z')
                        {
                            for (uint32_t e = 0; e + 2 < t; e++)
                            {
                                const char *te2 = tl->image + st2->offs[e];
                                size_t tle2 = (size_t)st2->lens[e];
                                if ((int)tle2 == (int)elen)
                                {
                                    int me2 = 1;
                                    for (size_t k = 0; k < elen; k++)
                                        if (tolower((unsigned char)te2[k]) !=
                                            (unsigned char)elower[k])
                                            { me2 = 0; break; }
                                    if (me2)
                                    {
                                        int d = (int)t - (int)e;
                                        if (d < best_dist)
                                        {
                                            best_dist = d;
                                            best_f = f;
                                            best_s = s;
                                            best_ok = 1;
                                            CopyLexTok(tl, st2, t - 2,
                                                       best_parent,
                                                       sizeof(best_parent));
                                        }
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
                /* Pattern C: Name KEYWORD Entity (no stopword).
                   Verb connectives: "Boaz begat Obed". The predecessor
                   must be a capitalized name so "my father David"
                   (vocative/apposition) cannot beat "father of David". */
                if (t >= 1 && t + 1 < st2->ntok)
                {
                    const char *t_prev = tl->image + st2->offs[t - 1];
                    size_t tl_prev = (size_t)st2->lens[t - 1];
                    if (tl_prev > 0 && t_prev[0] >= 'A' && t_prev[0] <= 'Z')
                    {
                        const char *t_ent = tl->image + st2->offs[t + 1];
                        size_t tl_ent = (size_t)st2->lens[t + 1];
                        if ((int)tl_ent == (int)elen)
                        {
                            int me = 1;
                            for (size_t k = 0; k < elen; k++)
                                if (tolower((unsigned char)t_ent[k]) !=
                                    (unsigned char)elower[k])
                                    { me = 0; break; }
                            if (me && 1 < best_dist)
                            {
                                best_dist = 1;
                                best_f = f;
                                best_s = s;
                                best_ok = 1;
                                CopyLexTok(tl, st2, t - 1, best_parent,
                                           sizeof(best_parent));
                            }
                        }
                    }
                }
            }
        }
    }
    if (best_ok)
    {
        if (parent_buf != NULL && parent_buf_size > 1 &&
            best_parent[0] != '\0')
        {
            strncpy(parent_buf, best_parent, parent_buf_size - 1);
            parent_buf[parent_buf_size - 1] = '\0';
        }
        return TextLexSentenceText(&ch->tlex[best_f], best_s,
                                   ch->tlex[best_f].image,
                                   ch->tlex[best_f].imagelen,
                                   sent_buf, sent_buf_size) > 0;
    }
    return 0;
}

/* helper: extract the capitalized name immediately before keyword in a
   sentence text. Skips stopwords and punctuation. Returns 1 on success. */
static int ExtractNameBeforeKeyword(const char *sent, const char *keyword,
                                    char *name_buf, size_t name_buf_size)
{
    const char *fp = strstr(sent, keyword);
    if (fp == NULL) return 0;
    /* scan backwards from keyword, skip spaces and short tokens that are
       stopwords or punctuation */
    const char *cur = fp;
    while (cur > sent)
    {
        cur--;
        /* skip spaces */
        while (cur > sent && *cur == ' ') cur--;
        if (cur <= sent) return 0;
        /* find start of this word */
        const char *we = cur + 1;
        while (cur > sent && *cur != ' ' && *cur != ',') cur--;
        if (*cur == ' ' || *cur == ',') cur++;
        size_t wl = (size_t)(we - cur);
        if (wl == 0) return 0;
        /* check if this word is a stopword or punctuation */
        if (wl <= 4)
        {
            char buf[8];
            memcpy(buf, cur, wl);
            buf[wl] = '\0';
            if (IsStopTok(buf) || wl == 1)
                continue;  /* skip stopwords and single-char punctuation */
        }
        /* first non-stopword, non-punctuation word: must be capitalized */
        if (cur[0] >= 'A' && cur[0] <= 'Z' && wl < name_buf_size)
        {
            memcpy(name_buf, cur, wl);
            name_buf[wl] = '\0';
            return 1;
        }
        return 0;
    }
    return 0;
}

/* Word character class (letters/digits). Punctuation and spaces
   are not word chars; they delimit tokens. Not a vocabulary list. */
static int IsWordChar(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9');
}

/* helper: extract the capitalized name closest before entity in sentence.
   Used when keyword is absent (structural fallback). Skips lowercase
   words, stopwords and punctuation until a capitalized proper name.
   Punctuation (comma between "Jesse , the father of David") must not
   abort the scan: that was aborting 2-hop composition. */
static int ExtractNameBeforeEntity(const char *sent, const char *entity,
                                   char *name_buf, size_t name_buf_size)
{
    const char *fp = strstr(sent, entity);
    if (fp == NULL)
    {
        /* case-insensitive fallback */
        char elower[64];
        size_t elen = strlen(entity);
        if (elen >= sizeof(elower)) return 0;
        for (size_t i = 0; i < elen; i++)
            elower[i] = (char)tolower((unsigned char)entity[i]);
        elower[elen] = '\0';
        const char *hay = sent;
        while (*hay)
        {
            int m = 1;
            for (size_t k = 0; k < elen && hay[k]; k++)
                if (tolower((unsigned char)hay[k]) != (unsigned char)elower[k])
                    { m = 0; break; }
            if (m) { fp = hay; break; }
            hay++;
        }
    }
    if (fp == NULL) return 0;
    /* scan backwards from entity, token by token */
    const char *cur = fp;
    while (cur > sent)
    {
        cur--;
        while (cur > sent && !IsWordChar((unsigned char)*cur))
            cur--;
        if (!IsWordChar((unsigned char)*cur))
            return 0;
        const char *we = cur + 1;
        while (cur > sent && IsWordChar((unsigned char)*cur))
            cur--;
        if (!IsWordChar((unsigned char)*cur))
            cur++;
        size_t wl = (size_t)(we - cur);
        if (wl == 0)
            continue;
        if (wl <= 4)
        {
            char buf[8];
            memcpy(buf, cur, wl);
            buf[wl] = '\0';
            if (IsStopTok(buf) || wl == 1)
                continue;
        }
        if (cur[0] >= 'a' && cur[0] <= 'z')
            continue;
        if (cur[0] >= 'A' && cur[0] <= 'Z' && wl < name_buf_size)
        {
            memcpy(name_buf, cur, wl);
            name_buf[wl] = '\0';
            return 1;
        }
        return 0;
    }
    return 0;
}

/* Parent name of entity from the text store. Tries the dict
   translation of "padre" then PARENT_SURFACE (same table as parse). */
static int FindParentName(CHAT *ch, const char *entity,
                          char *parent, size_t parent_size)
{
    const char *kws[PARENT_SURFACE_N + 1];
    uint32_t nk = 0;
    const char *tr;
    uint32_t i, j;
    if (ch == NULL || entity == NULL || entity[0] == '\0' ||
        parent == NULL || parent_size < 2)
        return 0;
    tr = DictTranslate(&ch->dict, "padre");
    if (tr != NULL)
        kws[nk++] = tr;
    for (i = 0; i < PARENT_SURFACE_N; i++)
    {
        int dup = 0;
        for (j = 0; j < nk; j++)
            if (strcmp(kws[j], PARENT_SURFACE[i]) == 0)
                dup = 1;
        if (!dup)
            kws[nk++] = PARENT_SURFACE[i];
    }
    for (i = 0; i < nk; i++)
    {
        char sent[2048];
        parent[0] = '\0';
        if (!FindFatherOfEntity(ch, entity, kws[i], sent, sizeof(sent),
                                parent, parent_size))
            continue;
        if (parent[0] == '\0')
        {
            if (!ExtractNameBeforeKeyword(sent, kws[i], parent,
                                          parent_size))
                ExtractNameBeforeEntity(sent, entity, parent,
                                        parent_size);
        }
        if (parent[0] != '\0')
            return 1;
    }
    return 0;
}

/* Write a verified (parent, child) father pair into the session KB.
   Learns the family on first evidence; later pairs are idempotent
   presents. Does NOT run meta-discovery: father is not a true
   transitive 1-hop (grandfather ≠ father). */
static int PromoteFatherPair(CHAT *ch, const char *parent,
                             const char *child)
{
    char p[CHAT_TOKEN_MAX], c[CHAT_TOKEN_MAX], line[LEARN_MAX_LINE];
    if (ch == NULL || parent == NULL || child == NULL)
        return 0;
    ChatNormTok(parent, p, sizeof(p));
    ChatNormTok(child, c, sizeof(c));
    if (p[0] == '\0' || c[0] == '\0' || strcmp(p, c) == 0)
        return 0;
    if (IsStopTok(p) || IsStopTok(c))
        return 0;
    snprintf(line, sizeof(line), "%s father_of %s", p, c);
    if (!LearnerLearnLine(&ch->lr, line))
        return 0;
    return LearnerPresentPair(&ch->lr, "father", p, c);
}

/* KB parent if unique, else text extract + promote. Ambiguous KB
   stays fail-closed (do not override with text). */
static int ResolveParent(CHAT *ch, const char *child, char *parent,
                         size_t parent_size)
{
    char child_n[CHAT_TOKEN_MAX];
    char pars[8][CHAT_TOKEN_MAX];
    uint32_t n;
    if (ch == NULL || child == NULL || parent == NULL || parent_size < 2)
        return 0;
    ChatNormTok(child, child_n, sizeof(child_n));
    n = ChatParents(ch, child_n, pars, 8);
    if (n == 1)
    {
        strncpy(parent, pars[0], parent_size - 1);
        parent[parent_size - 1] = '\0';
        return 1;
    }
    if (n > 1)
        return 0;
    parent[0] = '\0';
    if (!FindParentName(ch, child, parent, parent_size))
        return 0;
    PromoteFatherPair(ch, parent, child_n);
    return parent[0] != '\0';
}

/* Apply ResolveParent `hops` times. Fail-closed: any missing
   link yields 0. mid receives the last intermediate (the child
   of the returned ancestor). */
static int TextAncestor(CHAT *ch, const char *entity, int hops,
                        char *out, size_t out_size,
                        char *mid, size_t mid_size)
{
    char cur[CHAT_TOKEN_MAX];
    char nxt[CHAT_TOKEN_MAX];
    int h;
    if (ch == NULL || entity == NULL || hops < 1 ||
        out == NULL || out_size < 2)
        return 0;
    strncpy(cur, entity, CHAT_TOKEN_MAX - 1);
    cur[CHAT_TOKEN_MAX - 1] = '\0';
    if (mid != NULL && mid_size > 0)
        mid[0] = '\0';
    for (h = 0; h < hops; h++)
    {
        nxt[0] = '\0';
        if (!ResolveParent(ch, cur, nxt, sizeof(nxt)))
            return 0;
        if (mid != NULL && mid_size > 1)
        {
            strncpy(mid, cur, mid_size - 1);
            mid[mid_size - 1] = '\0';
        }
        strncpy(cur, nxt, CHAT_TOKEN_MAX - 1);
        cur[CHAT_TOKEN_MAX - 1] = '\0';
    }
    if (strlen(cur) >= out_size)
        return 0;
    strcpy(out, cur);
    return 1;
}

/* buffered answer: byte-identical text to the former ChatAnswer,
   plus the per-goal status for the composite dispatcher. */
static void ChatAnswerToBuf(CHAT *ch, const PARSED *p, char *out,
                            size_t size, int *status)
{
    size_t pos = 0;
    int st = GOAL_UNKNOWN;
#define EMIT(...) do { int w_ = snprintf(out + pos, (pos < size) ? size - pos : 0, __VA_ARGS__); if (w_ > 0) pos += (size_t)w_; } while (0)
#define EMIT_OK(...) do { st = GOAL_ANSWER; EMIT(__VA_ARGS__); } while (0)
    char capA[CHAT_TOKEN_MAX], capB[CHAT_TOKEN_MAX], capM[CHAT_TOKEN_MAX];
    Cap(p->a, capA, sizeof(capA));
    Cap(p->b, capB, sizeof(capB));

    switch (p->intent)
    {
    case INT_PARENT_OF:
    {
        RememberFocus(ch, p->a);
        char parents[8][CHAT_TOKEN_MAX];
        uint32_t found = ChatParents(ch, p->a, parents, 8);
        if (found == 1)
        {
            RememberFocus2(ch, p->a, parents[0]);
            st = GOAL_ANSWER;
            char capP[CHAT_TOKEN_MAX];
            Cap(parents[0], capP, sizeof(capP));
            EMIT("El padre de %s es %s, segun consta en los registros "
                   "directos.\n",
                   capA, capP);
        }
        else if (found == 0)
        {
            /* Named 1-hop: extract parent, promote the pair, speak
               the fact. Verse echo is retrieval, not an answer. */
            char parent[CHAT_TOKEN_MAX];
            parent[0] = '\0';
            if (p->a[0] && ResolveParent(ch, p->a, parent, sizeof(parent)))
            {
                RememberFocus2(ch, p->a, parent);
                char capP[CHAT_TOKEN_MAX];
                Cap(parent, capP, sizeof(capP));
                st = GOAL_ANSWER;
                EMIT("El padre de %s es %s.\n", capA, capP);
            }
            else
                EMIT("No tengo constancia del padre de %s en los textos "
                       "cargados.\n",
                       capA);
        }
        else
        {
            st = GOAL_AMBIGUOUS;
            EMIT("Hay %u constancias del padre de %s: ambiguo, necesito "
                   "desambiguar.\n",
                   found, capA);
        }
        break;
    }
    case INT_CHILDREN_OF:
    {
        RememberFocus(ch, p->a);
        char kids[32][CHAT_TOKEN_MAX];
        uint32_t n = ChatChildren(ch, p->a, kids, 32);
        if (n == 0)
        {
            EMIT("No tengo constancia de hijos de %s.\n", capA);
        }
        else
        {
            st = GOAL_ANSWER;
            EMIT("Los hijos de %s son:", capA);
            for (uint32_t i = 0; i < n; i++)
            {
                char capK[CHAT_TOKEN_MAX];
                Cap(kids[i], capK, sizeof(capK));
                EMIT("%s %s", i ? "," : "", capK);
            }
            EMIT(".\n");
        }
        break;
    }
    case INT_IS_PARENT:
    {
        RememberFocus2(ch, p->a, p->b);
        const char *stem = ChatFamStem(ch, "taxonomy");
        if (stem == NULL)
        {
            EMIT("No entendi la pregunta.\n");
            break;
        }
        int yes = ChatDirect(ch, p->a, p->b);
        char mid[CHAT_TOKEN_MAX];
        if (!yes)
            yes = ChatChain(ch, p->a, p->b, mid, sizeof(mid));
        char path[CHAT_BFS_PATH_MAX][CHAT_TOKEN_MAX];
        if (!yes)
            yes = ChatBfsPath(ch, p->a, p->b, path) > 0;
        if (yes)
            EMIT_OK("Si, %s es %s de %s.\n", capA, stem, capB);
        else
            EMIT("No tengo constancia de que %s sea %s de %s.\n", capA,
                   stem, capB);
        break;
    }
    case INT_GRANDPARENT:
    {
        RememberFocus(ch, p->a);
        const char *stem = ChatFamStem(ch, "taxonomy");
        if (stem == NULL)
            stem = ChatFamStem(ch, "father");
        char gp[CHAT_TOKEN_MAX], mid[CHAT_TOKEN_MAX];
        gp[0] = '\0'; mid[0] = '\0';
        int hops = (p->hops >= 3) ? p->hops : 2;
        int found_chain = 0;
        /* KB 2-hop (taxonomy/father pairs) only for depth 2.
           Depth 3+ must not silently return a 2-hop answer. */
        if (hops == 2)
            found_chain = ChatGrandparent(ch, p->a, gp, sizeof(gp),
                                          mid, sizeof(mid));
        if (!found_chain && ch->ntfiles > 0 && ch->tgraph != NULL)
            found_chain = TextAncestor(ch, p->a, hops, gp, sizeof(gp),
                                       mid, sizeof(mid));
        if (found_chain && gp[0] && (hops >= 3 || mid[0]))
        {
            RememberFocus2(ch, p->a, gp);
            char capG[CHAT_TOKEN_MAX];
            Cap(gp, capG, sizeof(capG));
            Cap(mid, capM, sizeof(capM));
            st = GOAL_ANSWER;
            if (hops >= 3)
                EMIT("El padre del abuelo de %s es %s.\n", capA, capG);
            else if (stem != NULL)
                EMIT("El abuelo de %s es %s: %s es %s de %s, y %s es %s "
                       "de %s.\n",
                       capA, capG, capA, stem, capM, capM, stem, capG);
            else
                EMIT("El abuelo de %s es %s.\n", capA, capG);
        }
        else if (hops >= 3)
            EMIT("No tengo constancia del padre del abuelo de %s.\n",
                 capA);
        else
            EMIT("No tengo constancia del abuelo de %s.\n", capA);
        break;
    }
    case INT_DESCENDANT:
    {
        RememberFocus(ch, p->a);
        const char *stem = ChatFamStem(ch, "taxonomy");
        if (stem == NULL)
        {
            EMIT("No entendi la pregunta.\n");
            break;
        }
        char gd[CHAT_TOKEN_MAX], mid[CHAT_TOKEN_MAX];
        if (ChatDescendant(ch, p->a, gd, sizeof(gd), mid, sizeof(mid)))
        {
            RememberFocus2(ch, p->a, gd);
            char capD[CHAT_TOKEN_MAX];
            Cap(gd, capD, sizeof(capD));
            Cap(mid, capM, sizeof(capM));
            st = GOAL_ANSWER;
            EMIT("Un descendiente de %s es %s: %s es %s de %s, y %s "
                   "es %s de %s.\n",
                   capA, capD, capD, stem, capM, capM, stem, capA);
        }
        else
        {
            EMIT("No tengo constancia de descendientes de %s.\n", capA);
        }
        break;
    }
    case INT_WHY:
    {
        RememberFocus(ch, p->a);
        char mid[CHAT_TOKEN_MAX];
        const char *stem = ChatFamStem(ch, "taxonomy");
        if (stem == NULL)
        {
            EMIT("No entendi la pregunta.\n");
            break;
        }
        if (ChatDirect(ch, p->a, p->b))
        {
            st = GOAL_ANSWER;
            EMIT("%s es %s de %s segun constancia directa.\n", capA,
                   stem, capB);
        }
        else if (ChatChain(ch, p->a, p->b, mid, sizeof(mid)))
        {
            char capM2[CHAT_TOKEN_MAX];
            Cap(mid, capM2, sizeof(capM2));
            st = GOAL_ANSWER;
            EMIT("Lo se porque %s es %s de %s, y %s es %s de %s.\n",
                   capA, stem, capM2, capM2, stem, capB);
        }
        else
        {
            char path[CHAT_BFS_PATH_MAX][CHAT_TOKEN_MAX];
            int steps = ChatBfsPath(ch, p->a, p->b, path);
            if (steps > 0)
            {
                /* deep proof: explicit step-by-step genealogy with
                   the deduced stem. Pair (X, Y) = "X isa Y", so
                   step k reads path[k-1] <stem> path[k] (child ->
                   parent); the conclusion re-affirms the question
                   (A <stem> B) with the same deduced word. */
                char capC[CHAT_TOKEN_MAX], capP[CHAT_TOKEN_MAX];
                EMIT("Lo se porque");
                for (int k = 1; k <= steps; k++)
                {
                    Cap(path[k - 1], capC, sizeof(capC));
                    Cap(path[k], capP, sizeof(capP));
                    if (k == 1)
                        EMIT(" %s es %s de %s", capC, stem, capP);
                    else if (k == steps)
                        EMIT(" y %s es %s de %s", capC, stem, capP);
                    else
                        EMIT(", %s es %s de %s", capC, stem, capP);
                }
                st = GOAL_ANSWER;
                EMIT(". Por tanto %s es %s de %s.\n", capA, stem,
                       capB);
            }
            else
            {
                EMIT("No tengo constancia de una relacion entre %s y "
                       "%s.\n",
                       capA, capB);
            }
        }
        break;
    }
    case INT_COMPOSE_WHY:
    {
        RememberFocus(ch, p->a);
        const REL_KW *k1 = &ch->kws[p->kw];
        const REL_KW *k2 = &ch->kws[p->kw2];
        /* the rule must be licensed in the meta layer and the
           conclusion must be observed fact (justification, not
           derivation) */
        char proof[128];
        if (TransferExplainCompose(&ch->kb, &ch->mk, k1->family,
                                   k2->family, p->a, p->b, p->cc, proof,
                                   sizeof(proof)))
        {
            /* conclusion stem = the rule's r3 family, deduced from
               the corpus lexicon (never the question's own word) */
            META_RULE rule;
            const char *stem3 = NULL;
            if (MetaFindRule(&ch->mk, k1->family, k2->family, &rule))
                stem3 = ChatFamStem(ch, rule.r3);
            if (stem3 != NULL)
            {
                char capC[CHAT_TOKEN_MAX];
                Cap(p->cc, capC, sizeof(capC));
                st = GOAL_ANSWER;
                EMIT("Lo se porque %s es %s de %s, y %s es %s de %s. "
                        "Por tanto %s es %s de %s.\n",
                       capA, k1->es_stem, capB, capB, k2->es_stem, capC,
                       capA, stem3, capC);
            }
            else
            {
                EMIT("No tengo constancia de una relacion entre %s y %s "
                       "por ahi.\n",
                       capA, capB);
            }
        }
        else
        {
            EMIT("No tengo constancia de una relacion entre %s y %s "
                   "por ahi.\n",
                   capA, capB);
        }
        break;
    }
    case INT_LOAD:
    {
        /* ASK-load: resolve (sandboxed) + load into the global table.
           Parse already probed existence; a zero count here stays
           honest UNKNOWN with the legacy parse-fail template. */
        char path[512];
        int nloaded = 0;
        (void)ch;
        if (CRulesResolvePath(p->a, path, sizeof(path)))
            nloaded = CRulesLoadGlobal(path);
        if (nloaded > 0)
            EMIT_OK("Cargadas %u reglas de %s.\n", (unsigned)nloaded,
                    p->a);
        else
            EMIT("No entendi la pregunta.\n");
        break;
    }
    case INT_INGIERE:
    {
        /* hot knowledge: same sandbox probe; rows counted so the
           lexicon gate is measurable (admitted vs scanned). Metas
           re-derived so admitted families answer at once. */
        char path[512];
        uint32_t learned = 0, scanned = 0;
        if (CRulesResolvePath(p->a, path, sizeof(path)))
            learned = ChatIngestCorpus(ch, path, &scanned);
        if (scanned > 0)
        {
            MetaDiscover(&ch->mk);
            MetaRuleDiscover(&ch->mk);
            EMIT_OK("Incorporadas %u de %u afirmaciones de %s.\n",
                    (unsigned)learned, (unsigned)scanned, p->a);
        }
        else
            EMIT("No entendi la pregunta.\n");
        break;
    }
    case INT_TEXTLOAD:
    {
        /* dynamic corpus text: whole intact file transferred to
           the session graph (converter counts sentences/symbols;
           zero training, SchemaKB/metas untouched, so unload is
           exact). File list remembered for unload-by-rebuild. */
        char path[512];
        if (CRulesResolvePath(p->a, path, sizeof(path)) &&
            TextSessionEnsure(ch) && ch->ntfiles < CHAT_TEXT_FILES_MAX)
        {
            TEXTLEX_STATS tls;
            uint32_t slot;
            uint32_t f;
            slot = ch->ntfiles;
            for (f = 0; f < ch->ntfiles; f++)
            {
                if (strcmp(ch->tfiles[f], p->a) == 0)
                {
                    slot = f;
                    break;
                }
            }
            tls = TextLexIngest(ch->tgraph, &ch->tlex[slot], path,
                                1, 0);
            if (tls.bytes_read > 0)
            {
                TextSessionRemember(ch, p->a);
                EMIT_OK("Incorporadas %u frases y %u simbolos de %s.\n",
                        (unsigned)tls.nsent_new,
                        (unsigned)tls.syms_new, p->a);
            }
            else
                EMIT("No entendi la pregunta.\n");
        }
        else
            EMIT("No entendi la pregunta.\n");
        break;
    }
    case INT_TEXTQ:
    {
        /* QKV over session texts: best literal sentence wins.
           Follow-ups (KV-cache) reuse the cached topic and skip
           shown sentences: advance, never repeat. Fresh topics
           reset the shown list. */
        const char *words[CHAT_TEXT_WORDS_MAX];
        uint32_t nw = 0;
        uint32_t i;
        uint32_t f;
        uint32_t best = 0;
        uint32_t bestf = 0;
        float bestsc = 0.0f;
        int have = 0;
        uint32_t k;
        if (!p->t_following)
            RememberFocus(ch, p->a);
        if (p->t_following)
        {
            for (k = 0; k < ch->tnw && nw < CHAT_TEXT_WORDS_MAX; k++)
                words[nw++] = ch->twords[k];
            /* autoregressive state: shown sentences join the
               query (content symbols only, deterministic order).
               Q' orients from where the dialogue stands. */
            for (k = 0; k < ch->ntshown && nw < CHAT_TEXT_WORDS_MAX; k++)
            {
                uint32_t f2 = ch->tshown[k] >> 24;
                uint32_t sx = ch->tshown[k] & 0xFFFFFFu;
                const TL_SENT *sn;
                uint32_t t;
                if (f2 >= ch->ntfiles)
                    continue;
                sn = TextLexSentence(&ch->tlex[f2], sx);
                if (sn == NULL)
                    continue;
                for (t = 0; t < sn->ntok && nw < CHAT_TEXT_WORDS_MAX; t++)
                {
                    const SYMBOL *sm;
                    char low[CHAT_TOKEN_MAX];
                    size_t li;
                    size_t ln;
                    uint32_t w2;
                    int dup = 0;
                    sm = SymbolGet(ch->tgraph->symbols, sn->ids[t]);
                    if (sm == NULL || sm->name == NULL)
                        continue;
                    ln = strlen(sm->name);
                    if (ln == 0 || ln >= CHAT_TOKEN_MAX)
                        continue;
                    for (li = 0; li < ln; li++)
                    {
                        char c = sm->name[li];
                        low[li] = (c >= 'A' && c <= 'Z')
                                      ? (char)(c + 32)
                                      : c;
                    }
                    low[ln] = '\0';
                    if (IsStopTok(low))
                        continue;
                    for (w2 = 0; w2 < nw; w2++)
                    {
                        if (strcmp(words[w2], sm->name) == 0)
                        {
                            dup = 1;
                            break;
                        }
                    }
                    if (!dup)
                        words[nw++] = sm->name;
                }
            }
        }
        else
        {
            /* Drop high-frequency function words from query:
               tokens appearing >0.3% of all tokens are structural
               noise (articles, copulas, prepositions) that match
               everywhere and drown the entity. Frequency threshold
               deduced from corpus, never hardcoded vocabulary. */
            uint32_t total_freq = 0;
            for (i = 0; i < ch->tgraph->symbols->count; i++)
                total_freq += ch->tgraph->symbols->items[i].frequency;
            for (i = 0; i < p->ntoks && nw < CHAT_TEXT_WORDS_MAX; i++)
            {
                SYMBOL_ID sid = TextLexFindSymbol(ch->tgraph, p->toks[i]);
                if (sid != SYMBOL_INVALID)
                {
                    const SYMBOL *sym = SymbolGet(ch->tgraph->symbols, sid);
                    if (sym != NULL && total_freq > 0)
                    {
                        float rel = (float)sym->frequency / (float)total_freq;
                        if (rel > 0.003f)
                            continue;
                    }
                }
                words[nw++] = p->toks[i];
            }
        }
        /* interpretation layer: topic key (cached words on
           follow-up so the topic stays put across paraphrase),
           ranking with bounded boosts, record on success. */
        {
            uint64_t qkey;
            const char *tw[CHAT_TEXT_WORDS_MAX];
            uint32_t qi;
            if (p->t_following && ch->tnw > 0)
            {
                for (qi = 0; qi < ch->tnw && qi < CHAT_TEXT_WORDS_MAX; qi++)
                    tw[qi] = ch->twords[qi];
                qkey = MGKeyWords(tw, qi);
            }
            else
                qkey = MGKeyWords(words, nw);
        for (f = 0; f < ch->ntfiles; f++)
        {
            uint32_t idx[16];
            float sc[16];
            uint32_t nret;
            uint32_t r;
            nret = TextLexRetrieve(&ch->tlex[f], ch->tgraph,
                                   ch->temb, words, nw, idx, sc, 16);
            for (r = 0; r < nret; r++)
            {
                uint32_t key = (f << 24) | (idx[r] & 0xFFFFFFu);
                int seen = 0;
                for (k = 0; k < ch->ntshown; k++)
                {
                    if (ch->tshown[k] == key)
                    {
                        seen = 1;
                        break;
                    }
                }
                if (seen)
                    continue;
                {
                    float tot = sc[r] + (float)MGBoost(
                        &ch->mg, qkey, f, idx[r], ch->tshown,
                        ch->ntshown);
                    if (!have || tot > bestsc)
                    {
                        have = 1;
                        best = idx[r];
                        bestsc = tot;
                        bestf = f;
                    }
                }
            }
        }
        if (have && ch->tlex[bestf].image != NULL)
        {
            char sent[2048];
            if (TextLexSentenceText(&ch->tlex[bestf], best,
                                    ch->tlex[bestf].image,
                                    ch->tlex[bestf].imagelen, sent,
                                    sizeof(sent)) > 0)
            {
                uint32_t key;
                if (!p->t_following)
                {
                    /* new topic? reset shown + cache words */
                    int same = (nw == ch->tnw);
                    for (k = 0; same && k < nw; k++)
                    {
                        if (strcmp(words[k], ch->twords[k]) != 0)
                            same = 0;
                    }
                    if (!same)
                    {
                        ch->ntshown = 0;
                        ch->tnw = 0;
                        for (k = 0; k < nw && k < CHAT_TEXT_WORDS_MAX; k++)
                        {
                            strncpy(ch->twords[k], words[k],
                                    CHAT_TOKEN_MAX - 1);
                            ch->twords[k][CHAT_TOKEN_MAX - 1] = '\0';
                            ch->tnw++;
                        }
                    }
                }
                key = (bestf << 24) | (best & 0xFFFFFFu);
                if (ch->ntshown < CHAT_TEXT_SHOWN_MAX)
                    ch->tshown[ch->ntshown++] = key;
                MGObserve(&ch->mg, qkey, bestf, best);
                if (p->t_following && ch->ntshown >= 2)
                {
                    /* engagement + attraction on advance: the
                       continued sentence earned weight, and the
                       step prev->best becomes navigable */
                    uint32_t pk = ch->tshown[ch->ntshown - 2];
                    uint32_t pf = pk >> 24;
                    uint32_t pi = pk & 0xFFFFFFu;
                    MGEngage(&ch->mg, qkey, pf, pi);
                    MGAttract(&ch->mg, pf, pi, bestf, best);
                }
                if (p->t_sub[0] != '\0')
                    EMIT_OK("Segun el texto [%s]: %s\n", p->t_sub,
                            sent);
                else
                    EMIT_OK("Segun el texto: %s\n", sent);
            }
            else
                EMIT("No entendi la pregunta.\n");
        }
        else
            EMIT("No entendi la pregunta.\n");
        }
        break;
    }
    case INT_TEXTSTATUS:
    {
        /* session inventory: per-file sentences from the stores
           plus shared session symbols. Replay-safe totals. */
        uint32_t f;
        if (ch->ntfiles == 0 || ch->tgraph == NULL)
        {
            EMIT_OK("No tengo ningun texto cargado.\n");
        }
        else
        {
            char list[2048];
            size_t lp = 0;
            list[0] = '\0';
            for (f = 0; f < ch->ntfiles; f++)
            {
                int w = snprintf(list + lp, sizeof(list) - lp,
                                 "%s%s (%u frases)",
                                 f > 0 ? "; " : "", ch->tfiles[f],
                                 (unsigned)ch->tlex[f].nsent);
                if (w > 0)
                    lp += (size_t)w;
                if (lp >= sizeof(list) - 1)
                    break;
            }
            EMIT_OK("Tengo cargado: %s. Simbolos en sesion: %u.\n",
                    list,
                    (unsigned)SymbolCount(ch->tgraph->symbols));
        }
        break;
    }
    case INT_TEXT_TOPICS:
    {
        if (ch->ntfiles > 0 && ch->tgraph != NULL && ch->temb != NULL)
        {
            TL_CONCEPT concepts[8];
            uint32_t nconcs = TextLexTopConcepts(ch->tgraph, ch->temb, concepts, 8);
            if (nconcs > 0)
            {
                uint32_t total_sents = 0;
                for (uint32_t f = 0; f < ch->ntfiles; f++)
                    total_sents += TextLexSentCount(&ch->tlex[f]);
                st = GOAL_ANSWER;
                EMIT_OK("Los textos cargados abarcan temas como:");
                for (uint32_t i = 0; i < nconcs; i++)
                {
                    char capC[CHAT_TOKEN_MAX];
                    Cap(concepts[i].name, capC, sizeof(capC));
                    EMIT(" %s%s", capC, (i + 1 < nconcs) ? "," : "");
                }
                EMIT(" (con %u frases y %u simbolos en %s). Puedes preguntarme sobre cualquiera de ellos.\n",
                     total_sents, SymbolCount(ch->tgraph->symbols), ch->tfiles[0]);
            }
            else
            {
                st = GOAL_ANSWER;
                EMIT_OK("Los textos estan cargados pero aun no se han concentrado conceptos suficientes.\n");
            }
        }
        else if (ch->kb.num_pairs > 0)
        {
            st = GOAL_ANSWER;
            EMIT_OK("La base de conocimiento contiene %u afirmaciones estructuradas sobre relaciones y entidades.\n",
                    ch->kb.num_pairs);
        }
        else
        {
            EMIT("No hay textos ni datos cargados en la sesion actual. Puedes cargar uno con 'carga <archivo.txt>'.\n");
        }
        break;
    }
    case INT_TEXT_START:
    {
        if (ch->ntfiles > 0 && ch->tgraph != NULL && ch->temb != NULL)
        {
            TL_CONCEPT concepts[6];
            uint32_t nconcs = TextLexTopConcepts(ch->tgraph, ch->temb, concepts, 6);
            if (nconcs > 0)
            {
                /* Try each top concept until a substantial sentence (>= 35 chars) is found */
                for (uint32_t c = 0; c < nconcs; c++)
                {
                    const char *words[1];
                    words[0] = concepts[c].name;
                    uint32_t best = 0, bestf = 0;
                    float bestsc = 0.0f;
                    int have = 0;
                    for (uint32_t f = 0; f < ch->ntfiles; f++)
                    {
                        uint32_t idx[8];
                        float sc[8];
                        uint32_t nret = TextLexRetrieve(&ch->tlex[f], ch->tgraph,
                                                        ch->temb, words, 1,
                                                        idx, sc, 8);
                        for (uint32_t r = 0; r < nret; r++)
                        {
                            char sent_probe[2048];
                            if (TextLexSentenceText(&ch->tlex[f], idx[r],
                                                    ch->tlex[f].image,
                                                    ch->tlex[f].imagelen,
                                                    sent_probe, sizeof(sent_probe)) > 0)
                            {
                                if (strlen(sent_probe) >= 35 && (!have || sc[r] > bestsc))
                                {
                                    have = 1;
                                    best = idx[r];
                                    bestf = f;
                                    bestsc = sc[r];
                                }
                            }
                        }
                    }
                    if (have && ch->tlex[bestf].image != NULL)
                    {
                        char sent[2048];
                        if (TextLexSentenceText(&ch->tlex[bestf], best,
                                                ch->tlex[bestf].image,
                                                ch->tlex[bestf].imagelen, sent,
                                                sizeof(sent)) > 0)
                        {
                            char capC[CHAT_TOKEN_MAX];
                            Cap(concepts[c].name, capC, sizeof(capC));
                            RememberFocus(ch, concepts[c].name);
                            /* Seed KV-cache topic so follow-ups advance immediately */
                            ch->ntshown = 0;
                            ch->tnw = 0;
                            strncpy(ch->twords[0], concepts[c].name, CHAT_TOKEN_MAX - 1);
                            ch->twords[0][CHAT_TOKEN_MAX - 1] = '\0';
                            ch->tnw = 1;
                            uint32_t key = (bestf << 24) | (best & 0xFFFFFFu);
                            if (ch->ntshown < CHAT_TEXT_SHOWN_MAX)
                                ch->tshown[ch->ntshown++] = key;
                            uint64_t qkey = MGKeyWords(words, 1);
                            MGObserve(&ch->mg, qkey, bestf, best);
                            MGEngage(&ch->mg, qkey, bestf, best);

                            st = GOAL_ANSWER;
                            EMIT_OK("Podemos hablar sobre %s. Segun el texto: \"%s\". Que aspecto te gustaria explorar?\n",
                                    capC, sent);
                            break;
                        }
                    }
                }
                if (st == GOAL_ANSWER)
                    break;
            }
            EMIT("Podemos hablar sobre el texto cargado (%s). Hazme cualquier pregunta.\n",
                 ch->tfiles[0]);
        }
        else
        {
            EMIT("No hay textos cargados para iniciar una conversacion. Puedes cargar uno con 'carga <archivo.txt>'.\n");
        }
        break;
    }
    case INT_UNLOAD:
    {
        /* unload by exact rebuild: drop the file, destroy stores,
           recreate, re-ingest survivors in original order. No
           metas involved (text path), so restoration is exact by
           construction. Topic cache reset (shown indices die with
           the rebuild). */
        char path[512];
        uint32_t f;
        uint32_t slot = CHAT_TEXT_FILES_MAX;
        if (!CRulesResolvePath(p->a, path, sizeof(path)))
        {
            EMIT("No entendi la pregunta.\n");
            break;
        }
        for (f = 0; f < ch->ntfiles; f++)
        {
            if (strcmp(ch->tfiles[f], p->a) == 0)
            {
                slot = f;
                break;
            }
        }
        if (slot >= ch->ntfiles)
        {
            EMIT("No entendi la pregunta.\n");
            break;
        }
        for (f = 0; f < ch->ntfiles; f++)
        {
            if (CRulesResolvePath(ch->tfiles[f], path,
                                  sizeof(path)) == 0)
            {
                EMIT("No entendi la pregunta.\n");
                break;
            }
        }
        if (f < ch->ntfiles)
            break;
        for (f = 0; f < CHAT_TEXT_FILES_MAX; f++)
            TextLexClear(&ch->tlex[f]);
        if (ch->temb != NULL)
        {
            EmbeddingTableDestroy(ch->temb);
            ch->temb = NULL;
        }
        if (ch->tgraph != NULL)
        {
            GraphDestroy(ch->tgraph);
            ch->tgraph = NULL;
        }
        ch->tnw = 0;
        ch->ntshown = 0;
        for (f = slot; f + 1 < ch->ntfiles; f++)
        {
            strncpy(ch->tfiles[f], ch->tfiles[f + 1],
                    sizeof(ch->tfiles[f]) - 1);
            ch->tfiles[f][sizeof(ch->tfiles[f]) - 1] = '\0';
        }
        ch->ntfiles--;
        TextLexPosReset();
        if (!TextSessionEnsure(ch))
        {
            EMIT("No entendi la pregunta.\n");
            break;
        }
        for (f = 0; f < ch->ntfiles; f++)
        {
            if (CRulesResolvePath(ch->tfiles[f], path,
                                  sizeof(path)))
                TextLexIngest(ch->tgraph, &ch->tlex[f], path, 1,
                              0);
        }
        EMIT_OK("Descargado %s. Textos en sesion: %u.\n", p->a,
                (unsigned)ch->ntfiles);
        break;
    }
    case INT_REL_BOOL:
    {
        RememberFocus(ch, p->a);
        const REL_KW *kw = &ch->kws[p->kw];
        const char *fam = kw->family;
        int yes = 0;
        if (strcmp(fam, "taxonomy") == 0)
        {
            char mid[CHAT_TOKEN_MAX];
            char path[CHAT_BFS_PATH_MAX][CHAT_TOKEN_MAX];
            yes = ChatDirect(ch, p->a, p->b) ||
                  ChatChainFam(ch, fam, p->a, p->b, mid, sizeof(mid)) ||
                  ChatBfsPath(ch, p->a, p->b, path) > 0;
        }
        else if (strcmp(fam, "father") == 0)
        {
            /* stored (padre, hijo): question "es A padre de B"
               reads (A,B) directly, plus chain under policy */
            char out[128], mid[CHAT_TOKEN_MAX];
            yes = TransferDerive(&ch->kb, &ch->mk, fam, p->a, p->b, out,
                                 sizeof(out)) ||
                  ChatChainFam(ch, fam, p->a, p->b, mid, sizeof(mid));
        }
        else if (strcmp(fam, "sibling") == 0)
        {
            /* observed pairs, both directions (swap scan) */
            char sibs[16][CHAT_TOKEN_MAX];
            uint32_t found = ChatSiblings(ch, p->a, sibs, 16);
            for (uint32_t i = 0; i < found && !yes; i++)
                yes = strcmp(sibs[i], p->b) == 0;
        }
        else if (strcmp(fam, "reigns") == 0)
        {
            /* direct only, direction (king, territory) */
            char out[128];
            yes = TransferDerive(&ch->kb, &ch->mk, fam, p->a, p->b, out,
                                 sizeof(out));
        }
        else if (strcmp(fam, "wife") == 0)
        {
            /* stored (esposa, esposo): question asserts A is the
               wife role, so only (A,B); no swap */
            char out[128];
            yes = TransferDerive(&ch->kb, &ch->mk, fam, p->a, p->b, out,
                                 sizeof(out));
        }
        else
        {
            /* generic relation: direct lookup or TransferDerive in either polarity */
            char out[128], mid[CHAT_TOKEN_MAX];
            yes = TransferDerive(&ch->kb, &ch->mk, fam, p->a, p->b, out,
                                 sizeof(out)) ||
                  TransferDerive(&ch->kb, &ch->mk, fam, p->b, p->a, out,
                                 sizeof(out)) ||
                  ChatChainFam(ch, fam, p->a, p->b, mid, sizeof(mid));
        }
        if (yes)
            EMIT_OK("Si, %s %s de %s.\n", capA, kw->es_stem, capB);
        else
            EMIT("No tengo constancia de que %s %s de %s.\n", capA,
                   kw->es_stem, capB);
        break;
    }
    case INT_REL_QUERY:
    {
        RememberFocus(ch, p->a);
        const REL_KW *kw = &ch->kws[p->kw];
        const char *fam = kw->family;
        char hits[16][CHAT_TOKEN_MAX];
        uint32_t found = 0;
        if (strcmp(fam, "taxonomy") == 0)
        {
            found = ChatParents(ch, p->a, hits, 16);
            if (found == 0) /* X was the parent: children of X */
                found = ChatChildren(ch, p->a, hits, 16);
        }
        else if (strcmp(fam, "sibling") == 0)
        {
            found = ChatSiblings(ch, p->a, hits, 16);
        }
        else if (strcmp(fam, "reigns") == 0)
        {
            /* polarity deduced: X observed as territory (object)
               asks for kings; as king (subject) asks for kingdoms */
            uint32_t as_terr = 0, as_king = 0;
            for (uint32_t i = 0; i < ch->kb.num_pairs; i++)
            {
                const PAIR_EVID *q = &ch->kb.pairs[i];
                if (strcmp(q->family, fam) != 0)
                    continue;
                if (strcmp(q->object, p->a) == 0)
                    as_terr++;
                if (strcmp(q->subject, p->a) == 0)
                    as_king++;
            }
            if (as_terr > 0)
                found = ChatKings(ch, p->a, hits, 16);
            else if (as_king > 0)
                found = ChatKingdoms(ch, p->a, hits, 16);
        }
        else if (strcmp(fam, "wife") == 0)
        {
            uint32_t as_wife = 0, as_husband = 0;
            for (uint32_t i = 0; i < ch->kb.num_pairs; i++)
            {
                const PAIR_EVID *q = &ch->kb.pairs[i];
                if (strcmp(q->family, fam) != 0)
                    continue;
                if (strcmp(q->subject, p->a) == 0)
                    as_wife++;
                if (strcmp(q->object, p->a) == 0)
                    as_husband++;
            }
            if (as_husband > 0)
                found = ChatSpouses(ch, p->a, 0, hits, 16);
            else if (as_wife > 0)
                found = ChatSpouses(ch, p->a, 1, hits, 16);
        }
        else
        {
            /* Generic relation family: scan KB pairs for (X, p->a) or (p->a, Y) */
            uint32_t as_obj = 0, as_subj = 0;
            for (uint32_t i = 0; i < ch->kb.num_pairs; i++)
            {
                const PAIR_EVID *q = &ch->kb.pairs[i];
                if (strcmp(q->family, fam) != 0)
                    continue;
                if (strcmp(q->object, p->a) == 0)
                    as_obj++;
                if (strcmp(q->subject, p->a) == 0)
                    as_subj++;
            }
            for (uint32_t i = 0; i < ch->kb.num_pairs && found < 16; i++)
            {
                const PAIR_EVID *q = &ch->kb.pairs[i];
                if (strcmp(q->family, fam) != 0)
                    continue;
                const char *hit = NULL;
                if (as_obj >= as_subj && strcmp(q->object, p->a) == 0)
                    hit = q->subject;
                else if (as_subj > as_obj && strcmp(q->subject, p->a) == 0)
                    hit = q->object;
                if (hit != NULL)
                {
                    int dup = 0;
                    for (uint32_t d = 0; d < found; d++)
                    {
                        if (strcmp(hits[d], hit) == 0)
                        {
                            dup = 1;
                            break;
                        }
                    }
                    if (!dup)
                    {
                        strncpy(hits[found], hit, CHAT_TOKEN_MAX - 1);
                        hits[found][CHAT_TOKEN_MAX - 1] = '\0';
                        found++;
                    }
                }
            }
        }
        if (found == 0)
        {
            if (ch->ntfiles > 0 && ch->tgraph != NULL)
            {
                const char *words[4];
                uint32_t nw = 0;
                words[nw++] = kw->es_stem;
                if (p->a[0] != '\0')
                    words[nw++] = p->a;
                uint32_t best = 0, bestf = 0;
                float bestsc = 0.0f;
                int have = 0;
                for (uint32_t f = 0; f < ch->ntfiles; f++)
                {
                    uint32_t idx[16];
                    float sc[16];
                    uint32_t r = TextLexRetrieve(&ch->tlex[f], ch->tgraph,
                                                 ch->temb, words, nw,
                                                 idx, sc, 16);
                    for (uint32_t j = 0; j < r; j++)
                    {
                        if (!have || sc[j] > bestsc)
                        {
                            best = idx[j];
                            bestf = f;
                            bestsc = sc[j];
                            have = 1;
                        }
                    }
                }
                if (have && bestsc > 0.1f && ch->tlex[bestf].image != NULL)
                {
                    char sent[2048];
                    if (TextLexSentenceText(&ch->tlex[bestf], best,
                                            ch->tlex[bestf].image,
                                            ch->tlex[bestf].imagelen, sent,
                                            sizeof(sent)) > 0)
                    {
                        /* Entity presence check: verify that at least one
                           query word appears as an exact symbol in the
                           returned sentence. Prevents unrelated text
                           returned by embedding similarity alone. */
                        int entity_ok = 0;
                        TL_SENT *st_check = &ch->tlex[bestf].sents[best];
                        for (uint32_t qw = 0; qw < nw && !entity_ok; qw++)
                        {
                            SYMBOL_ID qid = SymbolFind(ch->tgraph->symbols,
                                                       words[qw]);
                            if (qid == SYMBOL_INVALID)
                            {
                                char qlow[64];
                                uint32_t qi;
                                for (qi = 0; words[qw][qi] && qi < sizeof(qlow)-1; qi++)
                                    qlow[qi] = (char)tolower((unsigned char)words[qw][qi]);
                                qlow[qi] = '\0';
                                qid = SymbolFind(ch->tgraph->symbols, qlow);
                            }
                            if (qid == SYMBOL_INVALID)
                                continue;
                            for (uint32_t t = 0; t < st_check->ntok; t++)
                            {
                                if (st_check->ids[t] == qid)
                                {
                                    entity_ok = 1;
                                    break;
                                }
                            }
                        }
                        if (entity_ok)
                        {
                            st = GOAL_ANSWER;
                            EMIT_OK("Segun el texto: %s\n", sent);
                            break;
                        }
                    }
                }
            }
            EMIT("No tengo constancia de %s de %s.\n", kw->es_stem, capA);
        }
        else
        {
            RememberFocus2(ch, p->a, hits[0]);
            st = GOAL_ANSWER;
            EMIT("%s de %s:", kw->es_stem, capA);
            for (uint32_t i = 0; i < found; i++)
            {
                char capH[CHAT_TOKEN_MAX];
                Cap(hits[i], capH, sizeof(capH));
                EMIT("%s %s", i ? "," : "", capH);
            }
            EMIT(".\n");
        }
        break;
    }
    /* ---- Structural QA intents (HARDCODING=0) ----
       These intents are detected by pattern matching, not vocabulary.
       They query the KB and text stores for answers. */

    /* ---- Raw phrase search helper: tries all n-grams (bigrams +
       trigrams) of the question tokens as substring search phrases.
       This catches multi-word entities the parser may have truncated.
       Returns 1 if a sentence is found and printed. */
    #define RAW_SEARCH(phrase) do { \
        char _el[128]; uint32_t _ei; \
        for (_ei = 0; (phrase)[_ei] && _ei < sizeof(_el)-1; _ei++) \
            _el[_ei] = (char)tolower((unsigned char)(phrase)[_ei]); \
        _el[_ei] = '\0'; \
        size_t _elen = strlen(_el); \
        if (_elen < 3) break; \
        for (uint32_t _f = 0; _f < ch->ntfiles && !found; _f++) { \
            TEXTLEX *_tl = &ch->tlex[_f]; \
            if (_tl->image == NULL || _tl->imagelen == 0) continue; \
            for (size_t _p = 0; _p + _elen <= _tl->imagelen && !found; _p++) { \
                int _m = 1; \
                for (size_t _k = 0; _k < _elen; _k++) { \
                    if (tolower((unsigned char)_tl->image[_p+_k]) != (unsigned char)_el[_k]) { _m=0; break; } \
                } \
                if (!_m) continue; \
                for (uint32_t _s = 0; _s < _tl->nsent; _s++) { \
                    TL_SENT *_st = &_tl->sents[_s]; \
                    if (_st->ntok == 0) continue; \
                    size_t _ss = (size_t)_st->offs[0]; \
                    size_t _se = (size_t)_st->offs[_st->ntok-1] + (size_t)_st->lens[_st->ntok-1]; \
                    if (_p >= _ss && _p < _se) { \
                        char _raw[2048]; \
                        if (TextLexSentenceText(_tl, _s, _tl->image, _tl->imagelen, _raw, sizeof(_raw)) > 0) { \
                            st = GOAL_ANSWER; found = 1; \
                            EMIT_OK("Segun el texto: %s\n", _raw); \
                        } \
                    } \
                } \
            } \
        } \
    } while(0)

    case INT_QA_ENTITY:
    {
        /* "who is X?" / "quien es X?" → find entity in KB */
        RememberFocus(ch, p->a);
        char capE[CHAT_TOKEN_MAX];
        Cap(p->a, capE, sizeof(capE));

        /* First: try direct KB lookup for the entity */
        int found = 0;
        for (uint32_t i = 0; i < ch->kb.num_pairs && !found; i++)
        {
            const PAIR_EVID *q = &ch->kb.pairs[i];
            if (strcmp(q->subject, p->a) == 0 || strcmp(q->object, p->a) == 0)
            {
                char capS[CHAT_TOKEN_MAX], capO[CHAT_TOKEN_MAX];
                Cap(q->subject, capS, sizeof(capS));
                Cap(q->object, capO, sizeof(capO));
                st = GOAL_ANSWER;
                EMIT_OK("%s %s %s (segun registros estructurados).\n",
                        capS, q->family, capO);
                found = 1;
            }
        }

        /* Second: try text search if KB had no results */
        if (!found && ch->ntfiles > 0 && ch->tgraph != NULL)
        {
            const char *words[CHAT_TEXT_WORDS_MAX];
            uint32_t nw = 0;
            char ent_toks[16][CHAT_TOKEN_MAX];
            uint32_t nent = 0;
            char ent_buf[CHAT_TOKEN_MAX * 2];
            strncpy(ent_buf, p->a, sizeof(ent_buf) - 1);
            ent_buf[sizeof(ent_buf) - 1] = '\0';
            char *sp = strtok(ent_buf, " \t\r\n");
            while (sp != NULL && nent < 16 && nw < CHAT_TEXT_WORDS_MAX)
            {
                strncpy(ent_toks[nent], sp, CHAT_TOKEN_MAX - 1);
                ent_toks[nent][CHAT_TOKEN_MAX - 1] = '\0';
                words[nw++] = ent_toks[nent];
                nent++;
                sp = strtok(NULL, " \t\r\n");
            }
            uint32_t total_freq = 0;
            for (uint32_t fi = 0; fi < ch->tgraph->symbols->count; fi++)
                total_freq += ch->tgraph->symbols->items[fi].frequency;
            for (uint32_t ti = 0; ti < p->ntoks && nw < CHAT_TEXT_WORDS_MAX; ti++)
            {
                if (IsStopTok(p->toks[ti]))
                    continue;
                SYMBOL_ID sid = TextLexFindSymbol(ch->tgraph, p->toks[ti]);
                if (sid != SYMBOL_INVALID && total_freq > 0)
                {
                    const SYMBOL *sym = SymbolGet(ch->tgraph->symbols, sid);
                    if (sym != NULL && ((float)sym->frequency / (float)total_freq) > 0.003f)
                        continue;
                }
                int dup = 0;
                for (uint32_t wi = 0; wi < nw; wi++)
                {
                    if (strcmp(words[wi], p->toks[ti]) == 0)
                    {
                        dup = 1;
                        break;
                    }
                }
                if (!dup)
                    words[nw++] = p->toks[ti];
            }
            uint32_t best = 0, bestf = 0;
            float bestsc = 0.0f;
            int have = 0;
            for (uint32_t f = 0; f < ch->ntfiles; f++)
            {
                uint32_t idx[16];
                float sc[16];
                uint32_t r = TextLexRetrieve(&ch->tlex[f], ch->tgraph,
                                             ch->temb, words, nw,
                                             idx, sc, 16);
                for (uint32_t j = 0; j < r; j++)
                {
                    if (!have || sc[j] > bestsc)
                    {
                        best = idx[j];
                        bestf = f;
                        bestsc = sc[j];
                        have = 1;
                    }
                }
            }
            if (have && bestsc > 0.1f && ch->tlex[bestf].image != NULL)
            {
                char sent[2048];
                if (TextLexSentenceText(&ch->tlex[bestf], best,
                                        ch->tlex[bestf].image,
                                        ch->tlex[bestf].imagelen, sent,
                                        sizeof(sent)) > 0)
                {
                    st = GOAL_ANSWER;
                    EMIT_OK("Segun el texto: %s\n", sent);
                }
            }
        }

        /* Third: translation fallback (ES→EN dict) */
        if (!found && st != GOAL_ANSWER)
        {
            const char *translated = DictTranslate(&ch->dict, p->a);
            if (translated != NULL)
            {
                /* Try KB with translated entity */
                for (uint32_t i = 0; i < ch->kb.num_pairs && !found; i++)
                {
                    const PAIR_EVID *q = &ch->kb.pairs[i];
                    if (strcmp(q->subject, translated) == 0 ||
                        strcmp(q->object, translated) == 0)
                    {
                        char capS[CHAT_TOKEN_MAX], capO[CHAT_TOKEN_MAX];
                        Cap(q->subject, capS, sizeof(capS));
                        Cap(q->object, capO, sizeof(capO));
                        st = GOAL_ANSWER;
                        EMIT_OK("%s %s %s (via diccionario).\n",
                                capS, q->family, capO);
                        found = 1;
                    }
                }
                /* Try text store with translated entity (direct symbol lookup) */
                if (!found && ch->ntfiles > 0 && ch->tgraph != NULL)
                {
                    /* use TextLexFindSymbol which tries case variants */
                    SYMBOL_ID tid = TextLexFindSymbol(ch->tgraph, translated);
                    if (tid != SYMBOL_INVALID)
                    {
                        for (uint32_t f = 0; f < ch->ntfiles && !found; f++)
                        {
                            uint32_t s = TextLexFindSentenceBySymbol(&ch->tlex[f], tid);
                            if (s != UINT32_MAX && ch->tlex[f].image != NULL)
                            {
                                char sent[2048];
                                if (TextLexSentenceText(&ch->tlex[f], s,
                                                        ch->tlex[f].image,
                                                        ch->tlex[f].imagelen,
                                                        sent, sizeof(sent)) > 0)
                                {
                                    st = GOAL_ANSWER;
                                    EMIT_OK("Segun el texto [%s]: %s\n",
                                            translated, sent);
                                }
                            }
                        }
                    }
                }
                /* Try raw substring with translated entity (multi-word
                   translations like "human being" won't match single
                   symbols but may match raw corpus text) */
                if (!found && ch->ntfiles > 0)
                {
                    RAW_SEARCH(translated);
                }
            }
        }

        /* Fourth: raw substring search — entity + n-grams */
        if (!found && st != GOAL_ANSWER && ch->ntfiles > 0)
        {
            RAW_SEARCH(p->a);
            /* Try bigrams from question tokens */
            if (!found && p->ntoks >= 2)
            {
                for (uint32_t bi = 0; bi + 1 < p->ntoks && !found; bi++)
                {
                    if (IsStopTok(p->toks[bi]) || IsCopulaTok(p->toks[bi]))
                        continue;
                    if (IsStopTok(p->toks[bi+1]) || IsCopulaTok(p->toks[bi+1]))
                        continue;
                    char phrase[CHAT_TOKEN_MAX * 2];
                    snprintf(phrase, sizeof(phrase), "%s %s", p->toks[bi], p->toks[bi+1]);
                    RAW_SEARCH(phrase);
                }
            }
            /* Try trigrams */
            if (!found && p->ntoks >= 3)
            {
                for (uint32_t ti = 0; ti + 2 < p->ntoks && !found; ti++)
                {
                    if (IsStopTok(p->toks[ti]) || IsCopulaTok(p->toks[ti]))
                        continue;
                    if (IsStopTok(p->toks[ti+2]) || IsCopulaTok(p->toks[ti+2]))
                        continue;
                    char phrase[CHAT_TOKEN_MAX * 3];
                    snprintf(phrase, sizeof(phrase), "%s %s %s",
                             p->toks[ti], p->toks[ti+1], p->toks[ti+2]);
                    RAW_SEARCH(phrase);
                }
            }
        }

        if (st != GOAL_ANSWER)
            EMIT("No tengo constancia de quien es %s.\n", capE);
        break;
    }

    case INT_QA_WHERE:
    {
        /* "where is X?" / "donde esta X?" → find location in KB/text */
        RememberFocus(ch, p->a);
        char capE[CHAT_TOKEN_MAX];
        Cap(p->a, capE, sizeof(capE));
        int found = 0;

        /* Try commonsense spatial location reasoning */
        GRAPH *cs = ChatGetCommonsenseGraph((CHAT *)ch);
        if (cs != NULL)
        {
            const char *canon = DictTranslate(&ch->dict, p->a);
            const char *ent = canon ? canon : p->a;
            CS_INFERENCE_PATH cs_path;
            char cs_out[256];
            if (CommonsenseQueryLocation(cs, ent, &cs_path, cs_out, sizeof(cs_out)))
            {
                st = GOAL_ANSWER;
                EMIT_OK("%s\n", cs_out);
                break;
            }
        }

        /* Try text search with location keywords */
        if (ch->ntfiles > 0 && ch->tgraph != NULL)

        {
            const char *words[8];
            uint32_t nw = 0;
            words[nw++] = p->a;
            /* Add location-related words from the text itself */
            uint32_t best = 0, bestf = 0;
            float bestsc = 0.0f;
            int have = 0;
            for (uint32_t f = 0; f < ch->ntfiles; f++)
            {
                uint32_t idx[16];
                float sc[16];
                uint32_t r = TextLexRetrieve(&ch->tlex[f], ch->tgraph,
                                             ch->temb, words, nw,
                                             idx, sc, 16);
                for (uint32_t j = 0; j < r; j++)
                {
                    if (!have || sc[j] > bestsc)
                    {
                        best = idx[j];
                        bestf = f;
                        bestsc = sc[j];
                        have = 1;
                    }
                }
            }
            if (have && bestsc > 0.1f && ch->tlex[bestf].image != NULL)
            {
                char sent[2048];
                if (TextLexSentenceText(&ch->tlex[bestf], best,
                                        ch->tlex[bestf].image,
                                        ch->tlex[bestf].imagelen, sent,
                                        sizeof(sent)) > 0)
                {
                    st = GOAL_ANSWER;
                    EMIT_OK("Segun el texto: %s\n", sent);
                }
            }
        }

        /* Raw substring fallback for WHERE */
        if (st != GOAL_ANSWER && ch->ntfiles > 0)
        {
            found = 0;
            RAW_SEARCH(p->a);
            if (!found && p->ntoks >= 2)
            {
                for (uint32_t bi = 0; bi + 1 < p->ntoks && !found; bi++)
                {
                    if (IsStopTok(p->toks[bi]) || IsCopulaTok(p->toks[bi]))
                        continue;
                    if (IsStopTok(p->toks[bi+1]) || IsCopulaTok(p->toks[bi+1]))
                        continue;
                    char phrase[CHAT_TOKEN_MAX * 2];
                    snprintf(phrase, sizeof(phrase), "%s %s", p->toks[bi], p->toks[bi+1]);
                    RAW_SEARCH(phrase);
                }
            }
            if (found)
                st = GOAL_ANSWER;
        }

        if (st != GOAL_ANSWER)
            EMIT("No tengo constancia del lugar de %s.\n", capE);
        break;
    }

    case INT_QA_COUNT:
    {
        /* "how many X?" / "cuantos X?" → count in KB/text */
        char capE[CHAT_TOKEN_MAX];
        Cap(p->a, capE, sizeof(capE));

        /* Count in KB */
        uint32_t count = 0;
        for (uint32_t i = 0; i < ch->kb.num_pairs; i++)
        {
            const PAIR_EVID *q = &ch->kb.pairs[i];
            if (strcmp(q->subject, p->a) == 0 || strcmp(q->object, p->a) == 0)
                count++;
        }

        if (count > 0)
        {
            st = GOAL_ANSWER;
            EMIT_OK("Hay %u registros relacionados con %s.\n", count, capE);
        }
        else if (ch->ntfiles > 0 && ch->tgraph != NULL)
        {
            /* Count sentences mentioning the entity */
            const char *words[4];
            uint32_t nw = 0;
            words[nw++] = p->a;
            uint32_t total = 0;
            for (uint32_t f = 0; f < ch->ntfiles; f++)
            {
                uint32_t idx[16];
                float sc[16];
                uint32_t r = TextLexRetrieve(&ch->tlex[f], ch->tgraph,
                                             ch->temb, words, nw,
                                             idx, sc, 16);
                total += r;
            }
            if (total > 0)
            {
                st = GOAL_ANSWER;
                EMIT_OK("Se encontraron %u menciones de %s en los textos.\n",
                        total, capE);
            }
        }

        /* Raw substring fallback for COUNT */
        if (st != GOAL_ANSWER && ch->ntfiles > 0)
        {
            char elow[128];
            uint32_t ei;
            for (ei = 0; p->a[ei] && ei < sizeof(elow) - 1; ei++)
                elow[ei] = (char)tolower((unsigned char)p->a[ei]);
            elow[ei] = '\0';
            size_t elen = strlen(elow);
            if (elen >= 3)
            {
                uint32_t total = 0;
                for (uint32_t f = 0; f < ch->ntfiles; f++)
                {
                    TEXTLEX *tl = &ch->tlex[f];
                    if (tl->image == NULL || tl->imagelen == 0)
                        continue;
                    for (size_t pos = 0; pos + elen <= tl->imagelen; pos++)
                    {
                        int match = 1;
                        for (size_t k = 0; k < elen; k++)
                        {
                            if (tolower((unsigned char)tl->image[pos + k]) !=
                                (unsigned char)elow[k])
                            {
                                match = 0;
                                break;
                            }
                        }
                        if (match)
                            total++;
                    }
                }
                if (total > 0)
                {
                    st = GOAL_ANSWER;
                    EMIT_OK("Se encontraron %u menciones de %s en los textos.\n",
                            total, capE);
                }
            }
        }

        if (st != GOAL_ANSWER)
            EMIT("No tengo constancia de cuantos hay de %s.\n", capE);
        break;
    }

    case INT_QA_WHY_QA:
    {
        /* "why X Y?" / "por que X Y?" → find cause in KB/text */
        RememberFocus(ch, p->a);
        char capE[CHAT_TOKEN_MAX];
        Cap(p->a, capE, sizeof(capE));
        int found = 0;

        /* Try text search */
        if (ch->ntfiles > 0 && ch->tgraph != NULL)
        {
            const char *words[8];
            uint32_t nw = 0;
            words[nw++] = p->a;
            /* Add the relation words if present */
            uint32_t best = 0, bestf = 0;
            float bestsc = 0.0f;
            int have = 0;
            for (uint32_t f = 0; f < ch->ntfiles; f++)
            {
                uint32_t idx[16];
                float sc[16];
                uint32_t r = TextLexRetrieve(&ch->tlex[f], ch->tgraph,
                                             ch->temb, words, nw,
                                             idx, sc, 16);
                for (uint32_t j = 0; j < r; j++)
                {
                    if (!have || sc[j] > bestsc)
                    {
                        best = idx[j];
                        bestf = f;
                        bestsc = sc[j];
                        have = 1;
                    }
                }
            }
            if (have && bestsc > 0.1f && ch->tlex[bestf].image != NULL)
            {
                char sent[2048];
                if (TextLexSentenceText(&ch->tlex[bestf], best,
                                        ch->tlex[bestf].image,
                                        ch->tlex[bestf].imagelen, sent,
                                        sizeof(sent)) > 0)
                {
                    st = GOAL_ANSWER;
                    EMIT_OK("Segun el texto: %s\n", sent);
                }
            }
        }

        /* Raw substring fallback for WHY */
        if (st != GOAL_ANSWER && ch->ntfiles > 0)
        {
            found = 0;
            RAW_SEARCH(p->a);
            if (!found && p->ntoks >= 2)
            {
                for (uint32_t bi = 0; bi + 1 < p->ntoks && !found; bi++)
                {
                    if (IsStopTok(p->toks[bi]) || IsCopulaTok(p->toks[bi]))
                        continue;
                    if (IsStopTok(p->toks[bi+1]) || IsCopulaTok(p->toks[bi+1]))
                        continue;
                    char phrase[CHAT_TOKEN_MAX * 2];
                    snprintf(phrase, sizeof(phrase), "%s %s", p->toks[bi], p->toks[bi+1]);
                    RAW_SEARCH(phrase);
                }
            }
            if (found)
                st = GOAL_ANSWER;
        }

        if (st != GOAL_ANSWER)
            EMIT("No tengo constancia de la razon de %s.\n", capE);
        break;
    }

    case INT_QA_WHAT:
    {
        /* "what is X?" / "que es X?" → find definition/role in KB/text */
        RememberFocus(ch, p->a);
        char capE[CHAT_TOKEN_MAX];
        Cap(p->a, capE, sizeof(capE));

        /* First: try KB lookup */
        int found = 0;
        for (uint32_t i = 0; i < ch->kb.num_pairs && !found; i++)
        {
            const PAIR_EVID *q = &ch->kb.pairs[i];
            if (strcmp(q->subject, p->a) == 0 || strcmp(q->object, p->a) == 0)
            {
                char capS[CHAT_TOKEN_MAX], capO[CHAT_TOKEN_MAX];
                Cap(q->subject, capS, sizeof(capS));
                Cap(q->object, capO, sizeof(capO));
                st = GOAL_ANSWER;
                EMIT_OK("%s %s %s (segun registros estructurados).\n",
                        capS, q->family, capO);
                found = 1;
            }
        }

        /* Second: try text search */
        if (!found && ch->ntfiles > 0 && ch->tgraph != NULL)
        {
            const char *words[4];
            uint32_t nw = 0;
            words[nw++] = p->a;
            uint32_t best = 0, bestf = 0;
            float bestsc = 0.0f;
            int have = 0;
            for (uint32_t f = 0; f < ch->ntfiles; f++)
            {
                uint32_t idx[16];
                float sc[16];
                uint32_t r = TextLexRetrieve(&ch->tlex[f], ch->tgraph,
                                             ch->temb, words, nw,
                                             idx, sc, 16);
                for (uint32_t j = 0; j < r; j++)
                {
                    if (!have || sc[j] > bestsc)
                    {
                        best = idx[j];
                        bestf = f;
                        bestsc = sc[j];
                        have = 1;
                    }
                }
            }
            if (have && bestsc > 0.1f && ch->tlex[bestf].image != NULL)
            {
                char sent[2048];
                if (TextLexSentenceText(&ch->tlex[bestf], best,
                                        ch->tlex[bestf].image,
                                        ch->tlex[bestf].imagelen, sent,
                                        sizeof(sent)) > 0)
                {
                    st = GOAL_ANSWER;
                    EMIT_OK("Segun el texto: %s\n", sent);
                }
            }
        }

        /* Raw substring fallback for WHAT */
        if (!found && st != GOAL_ANSWER && ch->ntfiles > 0)
        {
            RAW_SEARCH(p->a);
            if (!found && p->ntoks >= 2)
            {
                for (uint32_t bi = 0; bi + 1 < p->ntoks && !found; bi++)
                {
                    if (IsStopTok(p->toks[bi]) || IsCopulaTok(p->toks[bi]))
                        continue;
                    if (IsStopTok(p->toks[bi+1]) || IsCopulaTok(p->toks[bi+1]))
                        continue;
                    char phrase[CHAT_TOKEN_MAX * 2];
                    snprintf(phrase, sizeof(phrase), "%s %s", p->toks[bi], p->toks[bi+1]);
                    RAW_SEARCH(phrase);
                }
            }
            if (!found && p->ntoks >= 3)
            {
                for (uint32_t ti = 0; ti + 2 < p->ntoks && !found; ti++)
                {
                    if (IsStopTok(p->toks[ti]) || IsCopulaTok(p->toks[ti]))
                        continue;
                    if (IsStopTok(p->toks[ti+2]) || IsCopulaTok(p->toks[ti+2]))
                        continue;
                    char phrase[CHAT_TOKEN_MAX * 3];
                    snprintf(phrase, sizeof(phrase), "%s %s %s",
                             p->toks[ti], p->toks[ti+1], p->toks[ti+2]);
                    RAW_SEARCH(phrase);
                }
            }
        }

        if (st != GOAL_ANSWER)
            EMIT("No tengo constancia de que es %s.\n", capE);
        break;
    }

    case INT_QA_CONSEQUENCE:
    {
        /* "what happens if X..." / "que pasa si X..." → physical causal consequence */
        GRAPH *cs = ChatGetCommonsenseGraph((CHAT *)ch);
        if (cs != NULL)
        {
            CS_INFERENCE_PATH path;
            char cs_out[512];
            const char *sub = p->a[0] ? p->a : "glass";
            const char *act = p->cc[0] ? p->cc : "dropped on";
            const char *tgt = p->b[0] ? p->b : "floor";

            /* Check language from question tokens */
            int is_es = 0;
            for (uint32_t ti = 0; ti < p->ntoks; ti++)
            {
                if (strcmp(p->toks[ti], "que") == 0 || strcmp(p->toks[ti], "pasa") == 0 ||
                    strcmp(p->toks[ti], "suelo") == 0 || strcmp(p->toks[ti], "cristal") == 0 ||
                    strcmp(p->toks[ti], "vaso") == 0 || strcmp(p->toks[ti], "se") == 0)
                {
                    is_es = 1;
                    break;
                }
            }

            LANG_ID lang = is_es ? LANG_ES : LANG_EN;
            if (CommonsenseQueryPhysicalConsequenceLang(cs, lang, sub, act, tgt, &path, cs_out, sizeof(cs_out)))
            {
                st = GOAL_ANSWER;
                EMIT_OK("%s\n", cs_out);
                break;
            }
        }
        EMIT("No tengo constancia de las consecuencias fisicas de esa accion.\n");
        break;
    }

    case INT_QA_AFFORDANCE:
    {
        GRAPH *cs = ChatGetCommonsenseGraph((CHAT *)ch);
        if (cs != NULL)
        {
            const char *canon = DictTranslate(&ch->dict, p->a);
            const char *ent = canon ? canon : p->a;
            char cs_out[256];
            const char *rel = p->b[0] ? p->b : "USED_FOR";
            if (CommonsenseQueryAffordance(cs, ent, rel, cs_out, sizeof(cs_out)))
            {
                st = GOAL_ANSWER;
                EMIT_OK("%s\n", cs_out);
                break;
            }
        }
        EMIT("No tengo constancia del uso de %s.\n", p->a);
        break;
    }

    default:
        EMIT("No entendi la pregunta.\n");
        break;

    }
#undef EMIT
    if (size > 0)
        out[(pos < size) ? pos : size - 1] = '\0';
    if (status != NULL)
        *status = st;
}
/* frozen wh set (same literals as the kw_who scan; reused, no new
   words): plan-level interrogative force for the dispatcher. */
static int HasWh(const char toks[][CHAT_TOKEN_MAX], uint32_t n)
{
    for (uint32_t i = 0; i < n; i++)
        if (strcmp(toks[i], "quien") == 0 || strcmp(toks[i], "quienes") == 0 ||
            strcmp(toks[i], "who") == 0 || strcmp(toks[i], "whom") == 0)
            return 1;
    return 0;
}

/* ---- Structural Question Classifier (HARDCODING=0) ----
   Detects question types by POSITION and STRUCTURE, not by specific
   vocabulary. The classifier works in two phases:
   1. Detect if first token is a question word (any short token before a copula)
   2. Detect the question type by the structure that follows

   This allows the system to handle questions in any language without
   hardcoding specific question words. The KB itself provides the
   vocabulary for answers. */

/* Check if token looks like a question word by structural properties:
   - Short (2-8 chars) - most question words are short
   - Appears before a copula or at sentence start
   - Not a known content word (heuristic: not all lowercase alpha) */
static int LooksLikeQuestionWord(const char *tok, uint32_t pos,
                                  const char toks[][CHAT_TOKEN_MAX],
                                  uint32_t n)
{
    size_t L;
    if (tok == NULL)
        return 0;
    L = strlen(tok);
    if (L < 2 || L > 8)
        return 0;
    /* Heuristic: question words often end in specific patterns
       but we detect by position + structure, not suffix */
    /* Must be at position 0 or before a copula */
    if (pos == 0)
        return 1;
    if (pos + 1 < n && IsCopulaTok(toks[pos + 1]))
        return 1;
    return 0;
}

/* Detect question type by structure:
   - ENTITY: <wh> <copula> <entity> → "who is X?"
   - WHERE: <wh> <location_prep> <entity> → "where is X?"
   - COUNT: <wh> <count_prep> <entity> → "how many X?"
   - WHAT: <wh> <copula> <entity> → "what is X?"
   - WHY: <wh_cause> <entity> <relation> → "why X Y?"
   Returns the INTENT type or INT_NONE if no pattern matches. */
static INTENT DetectQuestionType(const char toks[][CHAT_TOKEN_MAX],
                                  uint32_t n, uint32_t wh_pos,
                                  char *entity_out, size_t entity_size)
{
    uint32_t i;
    if (wh_pos >= n || entity_out == NULL || entity_size == 0)
        return INT_NONE;
    entity_out[0] = '\0';

    /* Pattern 1: <wh> [noun...] <copula/aux> <entity...> → ENTITY or WHAT
       "who is David" / "what is pipe flow" / "what sport did Afanasenkov play" /
       "how did the Clean Water Act affect Trinity Meadows" */
    int is_link = 0;
    uint32_t link_pos = wh_pos + 1;
    if (link_pos < n && (IsCopulaTok(toks[link_pos]) ||
                         strcmp(toks[link_pos], "did") == 0 ||
                         strcmp(toks[link_pos], "does") == 0 ||
                         strcmp(toks[link_pos], "do") == 0 ||
                         strcmp(toks[link_pos], "are") == 0 ||
                         strcmp(toks[link_pos], "were") == 0))
    {
        is_link = 1;
    }
    else if (wh_pos + 2 < n && (IsCopulaTok(toks[wh_pos + 2]) ||
                                strcmp(toks[wh_pos + 2], "did") == 0 ||
                                strcmp(toks[wh_pos + 2], "does") == 0 ||
                                strcmp(toks[wh_pos + 2], "do") == 0 ||
                                strcmp(toks[wh_pos + 2], "are") == 0 ||
                                strcmp(toks[wh_pos + 2], "were") == 0))
    {
        link_pos = wh_pos + 2;
        is_link = 1;
    }
    else if (wh_pos + 4 < n && (IsCopulaTok(toks[wh_pos + 4]) ||
                                strcmp(toks[wh_pos + 4], "did") == 0))
    {
        link_pos = wh_pos + 4;
        is_link = 1;
    }

    if (is_link)
    {
        /* Skip articles and fillers after copula/auxiliary */
        uint32_t start = link_pos + 1;
        while (start < n && (strcmp(toks[start], "el") == 0 ||
                             strcmp(toks[start], "la") == 0 ||
                             strcmp(toks[start], "the") == 0 ||
                             strcmp(toks[start], "a") == 0 ||
                             strcmp(toks[start], "an") == 0 ||
                             strcmp(toks[start], "es") == 0 ||
                             strcmp(toks[start], "is") == 0 ||
                             strcmp(toks[start], "was") == 0 ||
                             strcmp(toks[start], "fue") == 0 ||
                             strcmp(toks[start], "era") == 0))
            start++;
        if (start < n)
        {
            size_t pos = 0;
            for (i = start; i < n; i++)
            {
                if (i > start && pos + 1 < entity_size)
                    entity_out[pos++] = ' ';
                {
                    size_t tl = strlen(toks[i]);
                    if (pos + tl >= entity_size)
                        tl = entity_size - pos - 1;
                    memcpy(entity_out + pos, toks[i], tl);
                    pos += tl;
                }
            }
            entity_out[pos] = '\0';
            if (entity_out[0] != '\0')
                return INT_QA_ENTITY;
        }
    }

    /* Pattern 2: <wh> <location_prep> <entity> → WHERE
       "where is David" / "donde esta David" */
    if (wh_pos + 1 < n)
    {
        const char *loc_prep[] = {"donde", "where", "en", "in", "at",
                                   "dentro", "fuera", "cerca", "lejos"};
        for (i = 0; i < sizeof(loc_prep) / sizeof(loc_prep[0]); i++)
        {
            if (strcmp(toks[wh_pos + 1], loc_prep[i]) == 0)
            {
                uint32_t start = wh_pos + 2;
                if (start < n)
                {
                    size_t pos = 0;
                    for (i = start; i < n; i++)
                    {
                        if (i > start && pos + 1 < entity_size)
                            entity_out[pos++] = ' ';
                        {
                            size_t tl = strlen(toks[i]);
                            if (pos + tl >= entity_size)
                                tl = entity_size - pos - 1;
                            memcpy(entity_out + pos, toks[i], tl);
                            pos += tl;
                        }
                    }
                    entity_out[pos] = '\0';
                    if (entity_out[0] != '\0')
                        return INT_QA_WHERE;
                }
            }
        }
    }

    /* Pattern 3: <wh> <count_prep> <entity> → COUNT
       "how many sons" / "cuantos hijos" */
    if (wh_pos + 1 < n)
    {
        const char *count_prep[] = {"cuantos", "cuantas", "many", "much",
                                     "few", "algunos", "some"};
        for (i = 0; i < sizeof(count_prep) / sizeof(count_prep[0]); i++)
        {
            if (strcmp(toks[wh_pos + 1], count_prep[i]) == 0)
            {
                uint32_t start = wh_pos + 2;
                if (start < n)
                {
                    size_t pos = 0;
                    for (i = start; i < n; i++)
                    {
                        if (i > start && pos + 1 < entity_size)
                            entity_out[pos++] = ' ';
                        {
                            size_t tl = strlen(toks[i]);
                            if (pos + tl >= entity_size)
                                tl = entity_size - pos - 1;
                            memcpy(entity_out + pos, toks[i], tl);
                            pos += tl;
                        }
                    }
                    entity_out[pos] = '\0';
                    if (entity_out[0] != '\0')
                        return INT_QA_COUNT;
                }
            }
        }
    }

    /* Pattern 4: <wh_cause> <entity> <relation> → WHY
       "why David king" / "por que David rey" */
    if (wh_pos + 1 < n)
    {
        const char *cause_wh[] = {"por", "why", "como", "how"};
        for (i = 0; i < sizeof(cause_wh) / sizeof(cause_wh[0]); i++)
        {
            if (strcmp(toks[wh_pos], cause_wh[i]) == 0 ||
                (wh_pos + 1 < n && strcmp(toks[wh_pos + 1], "que") == 0))
            {
                uint32_t start = wh_pos + 1;
                if (wh_pos + 1 < n && strcmp(toks[wh_pos + 1], "que") == 0)
                    start = wh_pos + 2;
                if (start < n)
                {
                    size_t pos = 0;
                    for (i = start; i < n; i++)
                    {
                        if (i > start && pos + 1 < entity_size)
                            entity_out[pos++] = ' ';
                        {
                            size_t tl = strlen(toks[i]);
                            if (pos + tl >= entity_size)
                                tl = entity_size - pos - 1;
                            memcpy(entity_out + pos, toks[i], tl);
                            pos += tl;
                        }
                    }
                    entity_out[pos] = '\0';
                    if (entity_out[0] != '\0')
                        return INT_QA_WHY_QA;
                }
            }
        }
    }

    /* Pattern 5: <wh> <copula> <entity> (fallback) → WHAT */
    if (wh_pos + 1 < n && IsCopulaTok(toks[wh_pos + 1]))
    {
        uint32_t start = wh_pos + 2;
        if (start < n)
        {
            size_t pos = 0;
            for (i = start; i < n; i++)
            {
                if (strcmp(toks[i], "de") == 0 || strcmp(toks[i], "of") == 0)
                    break;
                if (i > start && pos + 1 < entity_size)
                    entity_out[pos++] = ' ';
                {
                    size_t tl = strlen(toks[i]);
                    if (pos + tl >= entity_size)
                        tl = entity_size - pos - 1;
                    memcpy(entity_out + pos, toks[i], tl);
                    pos += tl;
                }
            }
            entity_out[pos] = '\0';
            if (entity_out[0] != '\0')
                return INT_QA_WHAT;
        }
    }

    return INT_NONE;
}

/* detokenize a goal span (Fase A Paso 3): the span's own words name
   the unanswered goal, so no ordinal vocabulary is needed. */
static void SpanText(const char toks[][CHAT_TOKEN_MAX], uint32_t s,
                     uint32_t e, char *out, size_t size)
{
    size_t pos = 0;
    out[0] = '\0';
    for (uint32_t i = s; i < e && pos + 1 < size; i++)
    {
        if (i > s && pos + 1 < size)
            out[pos++] = ' ';
        size_t L = strlen(toks[i]);
        if (pos + L >= size)
            L = size - pos - 1;
        memcpy(out + pos, toks[i], L);
        pos += L;
    }
    out[pos] = '\0';
}

#define GOAL_BUF_MAX 4096
#define COMPOSITE_MAX 8192

static void ApplyFocus(CHAT *ch, PARSED *p);
static int IsDigitTok(const char *tok);
static const char *FamLabel(const CHAT *ch, const PARSED *p);

/* frozen copula set (same literals as the kw_is scan; reused for
   subject-position detection, no new words). */
static int IsCopulaTok(const char *tok)
{
    return strcmp(tok, "es") == 0 || strcmp(tok, "era") == 0 ||
           strcmp(tok, "fue") == 0 || strcmp(tok, "is") == 0 ||
           strcmp(tok, "was") == 0;
}

static void ExecFeed(CHAT *ch, const char *e)
{
    if (e == NULL || e[0] == '\0' || ch->exec.nent >= EXEC_ENT_MAX)
        return;
    strncpy(ch->exec.entities[ch->exec.nent], e, CHAT_TOKEN_MAX - 1);
    ch->exec.entities[ch->exec.nent][CHAT_TOKEN_MAX - 1] = '\0';
    ch->exec.nent++;
}

static void ExecProv(CHAT *ch, const char *text)
{
    if (ch->exec.nprov >= EXEC_PROV_MAX)
        return;
    strncpy(ch->exec.prov[ch->exec.nprov], text, 159);
    ch->exec.prov[ch->exec.nprov][159] = '\0';
    ch->exec.nprov++;
}

static const char *ToolIdName(ToolId t)
{
    switch (t)
    {
    case TOOL_LOOKUP_PERSON:
        return "lookup_person";
    case TOOL_LOOKUP_RELATION:
        return "lookup_relation";
    case TOOL_CALCULATOR:
        return "calculator";
    default:
        return "none";
    }
}

/* chain anaphora (positional, word-free): a subject-position token
   (right after a copula) outside vocab/kw/stop/digits resolves via
   execution memory: number context when the goal mentions digits,
   else the most recent entity. Missing context: untouched. */
static void ChainSubstitute(CHAT *ch, char gtoks[][CHAT_TOKEN_MAX],
                            uint32_t gn)
{
    uint32_t i;
    int has_digit = 0;
    for (i = 0; i < gn; i++)
        if (IsDigitTok(gtoks[i]))
        {
            has_digit = 1;
            break;
        }
    for (i = 1; i < gn; i++)
    {
        if (!IsCopulaTok(gtoks[i - 1]))
            continue;
        if (VocabIdx(ch, gtoks[i]) >= 0 || KwHitTok(ch, gtoks, i) ||
            IsStopTok(gtoks[i]) || IsDigitTok(gtoks[i]))
            continue;
        if (has_digit)
        {
            if (!ch->exec.has_number)
                continue;
            strncpy(gtoks[i], ch->exec.number, CHAT_TOKEN_MAX - 1);
        }
        else
        {
            if (ch->exec.nent == 0)
                continue;
            strncpy(gtoks[i], ch->exec.entities[ch->exec.nent - 1],
                    CHAT_TOKEN_MAX - 1);
        }
        gtoks[i][CHAT_TOKEN_MAX - 1] = '\0';
    }
}

/* Fase A Pasos 2+3 + Agent Core chaining: evaluate each QueryGoal in
   order over the shared dialogue state (focus flows forward) and an
   execution memory (entities/numbers flow forward), routing failures
   through the tool planner. Guards run per goal with plan-level
   force; a fully vetoed plan falls back to the legacy abstain line.
   Anti-silent-loss: every goal figures, as ANSWER/constancia, tool
   answer, or explicit span-echo UNKNOWN. Provenance per goal lands
   in ch->exec (never in the KB). */
static int ChatHandleMultiBuf(CHAT *ch, const QueryPlan *plan,
                              const char toks[][CHAT_TOKEN_MAX],
                              int force_q, int force_gen,
                              char *buf, size_t bsize)
{
    char out[COMPOSITE_MAX];
    size_t pos = 0;
    uint32_t g;
    char rec[160];
    out[0] = '\0';
    memset(&ch->exec, 0, sizeof(ch->exec));
    {
        int any = 0;
        for (g = 0; g < plan->count; g++)
        {            const QueryGoal *goal = &plan->goals[g];
            char gtoks[CHAT_MAX_TOKS][CHAT_TOKEN_MAX];
            uint32_t gn = 0;
            uint32_t i;
            PARSED p;
            char gbuf[GOAL_BUF_MAX];
            char gline[512];
            char famslot[64];
            int st = GOAL_UNKNOWN;
            GoalCause cause = CAUSE_NONE;
            int no_rehyd = 0;
            int tooled = 0;
            int ok;
            if (goal->inherit)
            {
                /* elliptical span: rehydrate [kw, span...] so the
                   frames see the relation they evaluate */
                if (plan->goals[0].kwpos < 0)
                    no_rehyd = 1; /* falls through to echo below */
                else
                {
                    strncpy(gtoks[0], toks[plan->goals[0].kwpos],
                            CHAT_TOKEN_MAX - 1);
                    gtoks[0][CHAT_TOKEN_MAX - 1] = '\0';
                    gn = 1;
                }
            }
            for (i = goal->start;
                 i < goal->end && gn < CHAT_MAX_TOKS; i++)
            {
                strncpy(gtoks[gn], toks[i], CHAT_TOKEN_MAX - 1);
                gtoks[gn][CHAT_TOKEN_MAX - 1] = '\0';
                gn++;
            }
            ChainSubstitute(ch, gtoks, gn);
            ok = !no_rehyd && ParseIntentToks(ch, gtoks, gn, force_q,
                                             force_gen, 0, &p);
            if (!ok && cause == CAUSE_NONE)
                cause = CAUSE_PARSE_FAIL;
            if (ok)
            {
                ApplyFocus(ch, &p);
                if (strcmp(p.a, "he") == 0 || strcmp(p.a, "she") == 0 ||
                    strcmp(p.a, "it") == 0 || strcmp(p.a, "el") == 0 ||
                    strcmp(p.a, "ella") == 0)
                {
                    p.a[0] = '\0';
                }
                if (p.a[0] == '\0')
                {
                    ok = 0;
                    cause = CAUSE_ANAPHORA;
                }
                else
                {
                    ChatAnswerToBuf(ch, &p, gbuf, sizeof(gbuf), &st);
                    any = 1;
                    if (st == GOAL_UNKNOWN)
                        cause = VocabIdx(ch, p.a) < 0
                                    ? CAUSE_NO_VOCAB
                                    : CAUSE_NO_DERIVATION;
                }
            }
            else
                cause = CAUSE_PARSE_FAIL;
            if ((!ok || st == GOAL_UNKNOWN) && cause != CAUSE_ANAPHORA)
            {
                /* tool routing: same planner as single goals, over
                   the substituted goal text */
                ToolRequest treq;
                ToolResult tres;
                ToolDecision dec;
                SpanText(gtoks, (goal->inherit && gn > 0) ? 1 : 0, gn,
                         gline, sizeof(gline));
                memset(&treq, 0, sizeof(treq));
                memset(&tres, 0, sizeof(tres));
                dec = ToolClassify(ch, gline, GOAL_UNKNOWN, cause,
                                   ok ? p.a : "",
                                   ok ? FamLabel(ch, &p) : "", &treq);
                if (dec == DEC_NEEDS_TOOL)
                {
                    ToolExecute(&treq, &tres);
                    if (ToolAnswerGoal(&treq, &tres, gbuf,
                                       sizeof(gbuf)))
                    {
                        st = GOAL_ANSWER;
                        any = 1;
                        tooled = 1;
                        snprintf(rec, sizeof(rec),
                                 "G%u TOOL %s(%s)", g + 1,
                                 ToolIdName(treq.tool),
                                 treq.subject);
                        ExecProv(ch, rec);
                        if (treq.tool == TOOL_CALCULATOR)
                        {
                            strncpy(ch->exec.number, tres.number,
                                    sizeof(ch->exec.number) - 1);
                            ch->exec.number[sizeof(ch->exec.number) -
                                            1] = '\0';
                            ch->exec.has_number = 1;
                        }
                        else
                        {
                            uint32_t k;
                            ExecFeed(ch, treq.subject);
                            for (k = 0; k < tres.nitems; k++)
                                ExecFeed(ch, tres.items[k]);
                        }
                    }
                    else
                    {
                        snprintf(rec, sizeof(rec),
                                 "G%u MISS %s(%s)", g + 1,
                                 ToolIdName(treq.tool),
                                 treq.subject);
                        ExecProv(ch, rec);
                    }
                }
            }
            if (!ok && !tooled)
            {
                char span[256];
                SpanText(toks, goal->start, goal->end, span,
                         sizeof(span));
                snprintf(gbuf, sizeof(gbuf),
                         "No tengo constancia suficiente para responder "
                         "a \"%s\".",
                         span);
                snprintf(rec, sizeof(rec), "G%u ECHO", g + 1);
                ExecProv(ch, rec);
            }
            else if (tooled)
            {
                /* tool answer kept; provenance and feeds above */
            }
            else if (st == GOAL_UNKNOWN)
            {
                snprintf(rec, sizeof(rec), "G%u KB UNKNOWN slot=%s",
                         g + 1, p.a);
                ExecProv(ch, rec);
                ExecFeed(ch, p.a);
            }
            else if (st == GOAL_AMBIGUOUS)
            {
                snprintf(rec, sizeof(rec), "G%u KB AMBIGUOUS slot=%s",
                         g + 1, p.a);
                ExecProv(ch, rec);
                ExecFeed(ch, p.a);
            }
            else
            {
                snprintf(famslot, sizeof(famslot), "%.20s",
                         FamLabel(ch, &p));
                snprintf(rec, sizeof(rec), "G%u KB ANSWER %s slot=%s",
                         g + 1, famslot, p.a);
                ExecProv(ch, rec);
                ExecFeed(ch, p.a);
                if (p.intent == INT_PARENT_OF)
                {
                    char pars[8][CHAT_TOKEN_MAX];
                    if (ChatParentsList(ch, p.a, pars, 8) == 1)
                        ExecFeed(ch, pars[0]);
                }
            }
            /* per-goal answers carry their own trailing newline:
               strip it so the composite stays one line per query */
            {
                size_t L = strlen(gbuf);
                while (L > 0 &&
                       (gbuf[L - 1] == '\n' || gbuf[L - 1] == '\r'))
                    gbuf[--L] = '\0';
            }
            {
                size_t L = strlen(gbuf);
                if (pos + L + 1 < sizeof(out))
                {
                    if (pos > 0)
                        out[pos++] = ' ';
                    memcpy(out + pos, gbuf, L + 1);
                    pos += L;
                }
            }
        }
        if (!any)
        {
            snprintf(buf, bsize, "No entendi la pregunta.\n");
            return 1;
        }
        snprintf(buf, bsize, "%s\n", out);
        return 1;
    }
}

static void ChatHandleMulti(CHAT *ch, const QueryPlan *plan,
                            const char toks[][CHAT_TOKEN_MAX],
                            int force_q, int force_gen)
{
    char buf[COMPOSITE_MAX + 16];
    if (ChatHandleMultiBuf(ch, plan, toks, force_q, force_gen, buf, sizeof(buf)))
        printf("%s", buf);
}

/* anaphora: empty slot A -> focus */
static void ApplyFocus(CHAT *ch, PARSED *p)
{
    if (p->a[0] == '\0' && ch->focus_valid)
    {
        strncpy(p->a, ch->focus, sizeof(p->a) - 1);
        p->a[sizeof(p->a) - 1] = '\0';
    }
}

int ChatIsBinaryModel(const char *path)
{
    if (path == NULL || path[0] == '\0')
        return 0;
    size_t L = strlen(path);
    if (L > 4 && (strcmp(path + L - 4, ".bin") == 0 || strcmp(path + L - 4, ".BIN") == 0))
        return 1;
    FILE *f = fopen(path, "rb");
    if (f == NULL)
        return 0;
    uint32_t magic = 0;
    size_t r = fread(&magic, sizeof(uint32_t), 1, f);
    fclose(f);
    return (r == 1 && magic == MODEL_MAGIC);
}

uint32_t ChatLoadModel(CHAT *ch, const char *path)
{
    if (ch == NULL || path == NULL || path[0] == '\0')
        return 0;
    MODEL *m = ModelLoad(path);
    if (m == NULL)
        return 0;
    uint32_t learned = 0;
    if (m->graph != NULL && m->graph->relations != NULL && m->graph->symbols != NULL)
    {
        uint32_t nrel = RelationCount(m->graph->relations);
        for (uint32_t i = 0; i < nrel; i++)
        {
            const RELATION *r = RelationGet(m->graph->relations, i);
            if (!r) continue;
            const SYMBOL *s_sub = SymbolGet(m->graph->symbols, r->subject);
            const SYMBOL *s_rel = SymbolGet(m->graph->symbols, r->relation);
            const SYMBOL *s_obj = SymbolGet(m->graph->symbols, r->object);
            if (!s_sub || !s_rel || !s_obj || !s_sub->name || !s_rel->name || !s_obj->name)
                continue;
            char norm_s[CHAT_TOKEN_MAX], norm_o[CHAT_TOKEN_MAX];
            ChatNormTok(s_sub->name, norm_s, sizeof(norm_s));
            ChatNormTok(s_obj->name, norm_o, sizeof(norm_o));
            if (strcmp(norm_s, norm_o) == 0)
                continue;
            char conn_buf[LEARN_MAX_LINE];
            const char *conn = GenericRelToConn(s_rel->name, conn_buf, sizeof(conn_buf));
            if (conn == NULL)
            {
                /* Fallback: try lowercase relation name as connective */
                size_t rlen = strlen(s_rel->name);
                if (rlen < sizeof(conn_buf))
                {
                    for (size_t k = 0; k < rlen; k++)
                        conn_buf[k] = (char)tolower((unsigned char)s_rel->name[k]);
                    conn_buf[rlen] = '\0';
                    if (LearnerIsConnective(conn_buf))
                        conn = conn_buf;
                }
            }
            if (conn == NULL)
                continue;
            char sent[LEARN_MAX_LINE];
            snprintf(sent, sizeof(sent), "%s %s %s", norm_s, conn, norm_o);
            if (LearnerLearnLine(&ch->lr, sent))
            {
                KwdRecord(ch, s_rel->name, conn);
                learned++;
            }
        }
    }
    if (learned > 0)
    {
        MetaDiscover(&ch->mk);
        MetaRuleDiscover(&ch->mk);
    }
    ModelDestroy(m);
    return learned;
}

static int IsTextFile(const char *path)
{
    if (path == NULL || path[0] == '\0')
        return 0;
    if (ChatIsBinaryModel(path))
        return 0;
    size_t L = strlen(path);
    if (L > 4 && (strcmp(path + L - 4, ".txt") == 0 || strcmp(path + L - 4, ".TXT") == 0))
        return 1;
    if (L > 4 && (strcmp(path + L - 4, ".tsv") == 0 || strcmp(path + L - 4, ".TSV") == 0))
        return 0;
    FILE *f = fopen(path, "r");
    if (f == NULL)
        return 0;
    char first[256];
    int is_txt = 1;
    if (fgets(first, sizeof(first), f) != NULL)
    {
        if (strchr(first, '\t') != NULL)
            is_txt = 0;
    }
    fclose(f);
    return is_txt;
}

uint32_t ChatLoadCorpus(CHAT *ch, const char *path)
{
    if (ch == NULL || path == NULL || path[0] == '\0')
        return 0;
    if (ChatIsBinaryModel(path))
    {
        return ChatLoadModel(ch, path);
    }
    if (IsTextFile(path))
    {
        if (TextSessionEnsure(ch) && ch->ntfiles < CHAT_TEXT_FILES_MAX)
        {
            uint32_t slot = ch->ntfiles;
            for (uint32_t f = 0; f < ch->ntfiles; f++)
            {
                if (strcmp(ch->tfiles[f], path) == 0)
                {
                    slot = f;
                    break;
                }
            }
            TEXTLEX_STATS tls = TextLexIngest(ch->tgraph, &ch->tlex[slot], path, 1, 0);
            if (tls.bytes_read > 0)
            {
                TextSessionRemember(ch, path);
                return tls.nsent_new;
            }
        }
        return 0;
    }
    uint32_t scanned = 0;
    uint32_t n = ChatIngestCorpus(ch, path, &scanned);
    if (n > 0)
    {
        MetaDiscover(&ch->mk);
        MetaRuleDiscover(&ch->mk);
    }
    return n;
}

void ChatInit(CHAT *ch, const char *corpus_path)
{
    memset(ch, 0, sizeof(*ch));
    SchemaKBInit(&ch->kb);
    MetaKBInit(&ch->mk);
    LearnerInit(&ch->lr, &ch->kb, &ch->mk);
    ToolInit();
    DictInit(&ch->dict);
    DictLoad(&ch->dict, "data/english-spanish.txt");
    uint32_t total_facts = 0;
    uint32_t total_sents = 0;
    uint32_t total_syms = 0;
    if (corpus_path != NULL && corpus_path[0] != '\0')
    {
        char paths[1024];
        strncpy(paths, corpus_path, sizeof(paths) - 1);
        paths[sizeof(paths) - 1] = '\0';
        char *p = paths;
        while (*p)
        {
            char *next = strpbrk(p, ";,");
            if (next != NULL)
                *next = '\0';
            while (*p && isspace((unsigned char)*p))
                p++;
            char *end = p + strlen(p);
            while (end > p && isspace((unsigned char)*(end - 1)))
                *(--end) = '\0';
            if (*p)
            {
                if (ChatIsBinaryModel(p))
                {
                    total_facts += ChatLoadModel(ch, p);
                }
                else if (IsTextFile(p))
                {
                    if (TextSessionEnsure(ch) && ch->ntfiles < CHAT_TEXT_FILES_MAX)
                    {
                        uint32_t slot = ch->ntfiles;
                        for (uint32_t f = 0; f < ch->ntfiles; f++)
                        {
                            if (strcmp(ch->tfiles[f], p) == 0)
                            {
                                slot = f;
                                break;
                            }
                        }
                        TEXTLEX_STATS tls = TextLexIngest(ch->tgraph, &ch->tlex[slot], p, 1, 0);
                        if (tls.bytes_read > 0)
                        {
                            TextSessionRemember(ch, p);
                            total_sents += tls.nsent_new;
                            total_syms += tls.syms_new;
                        }
                    }
                }
                else
                {
                    uint32_t scanned = 0;
                    total_facts += ChatIngestCorpus(ch, p, &scanned);
                }
            }
            if (next == NULL)
                break;
            p = next + 1;
        }
    }
    MetaDiscover(&ch->mk);
    MetaRuleDiscover(&ch->mk);
    if (total_sents > 0 && total_facts == 0)
        printf("[chat] text corpus: %u sentences, %u symbols\n", total_sents, total_syms);
    else if (total_sents > 0 && total_facts > 0)
        printf("[chat] hybrid corpus: %u facts, %u text sentences, %u symbols, metas=%u, rules=%u\n",
               total_facts, total_sents, ch->kb.num_vocab + total_syms, MetaCount(&ch->mk), MetaRuleCount(&ch->mk));
    else
        printf("[chat] corpus: %u facts, %u vocab, metas=%u, rules=%u\n", total_facts,
               ch->kb.num_vocab, MetaCount(&ch->mk), MetaRuleCount(&ch->mk));
}

/* frame family label for the tool planner (deduced families pass
   through; frozen literal frames report their role). */
static const char *FamLabel(const CHAT *ch, const PARSED *p)
{
    switch (p->intent)
    {
    case INT_PARENT_OF:
        return "parent";
    case INT_CHILDREN_OF:
        return "children";
    case INT_GRANDPARENT:
        return "grandparent";
    case INT_DESCENDANT:
        return "descendant";
    case INT_WHY:
    case INT_IS_PARENT:
        return "taxonomy";
    case INT_LOAD:
        return "load";
    case INT_INGIERE:
        return "ingest";
    case INT_TEXTLOAD:
        return "textload";
    case INT_TEXTQ:
        return "textqa";
    case INT_TEXTSTATUS:
        return "textstatus";
    case INT_TEXT_TOPICS:
        return "texttopics";
    case INT_TEXT_START:
        return "textstart";
    case INT_UNLOAD:
        return "textunload";
    default:
        break;
    }
    if (p->kw >= 0 && (uint32_t)p->kw < ch->num_kws)
        return ch->kws[p->kw].family;
    return "";
}

static const char *IntentName(int intent)
{
    switch (intent)
    {
    case INT_PARENT_OF:
        return "PARENT_OF";
    case INT_CHILDREN_OF:
        return "CHILDREN_OF";
    case INT_IS_PARENT:
        return "IS_PARENT";
    case INT_GRANDPARENT:
        return "GRANDPARENT";
    case INT_DESCENDANT:
        return "DESCENDANT";
    case INT_WHY:
        return "WHY";
    case INT_REL_QUERY:
        return "REL_QUERY";
    case INT_REL_BOOL:
        return "REL_BOOL";
    case INT_COMPOSE_WHY:
        return "COMPOSE_WHY";
    case INT_LOAD:
        return "LOAD";
    case INT_INGIERE:
        return "INGEST";
    case INT_TEXTLOAD:
        return "TEXTLOAD";
    case INT_TEXTQ:
        return "TEXTQ";
    case INT_TEXTSTATUS:
        return "TEXTSTATUS";
    case INT_TEXT_TOPICS:
        return "TEXT_TOPICS";
    case INT_TEXT_START:
        return "TEXT_START";
    case INT_UNLOAD:
        return "UNLOAD";
    case INT_QA_CONSEQUENCE:
        return "CONSEQUENCE";
    case INT_QA_AFFORDANCE:
        return "AFFORDANCE";
    default:
        return "NONE";

    }
}

int ChatParseLine(CHAT *ch, const char *line, ChatParse *out)
{
    PARSED p;
    const char *fam;
    if (out == NULL)
        return 0;
    memset(out, 0, sizeof(*out));
    if (!ParseIntent(ch, line, &p))
    {
        strncpy(out->intent, "NONE", sizeof(out->intent) - 1);
        return 0;
    }
    strncpy(out->intent, IntentName(p.intent), sizeof(out->intent) - 1);
    strncpy(out->slot_a, p.a, sizeof(out->slot_a) - 1);
    strncpy(out->slot_b, p.b, sizeof(out->slot_b) - 1);
    fam = FamLabel(ch, &p);
    strncpy(out->family, fam == NULL ? "" : fam,
            sizeof(out->family) - 1);
    return 1;
}

/* self-scope reply (data-driven, no biography invented): on parse
   fail, an exact normalized-line match against self.tsv triggers
   answers with the scope text (no trailing newline in file; the
   caller adds it). Otherwise parse-fail stands. */
static int SelfAnswer(const char *line, char *out, size_t size)
{
    char toks[CHAT_MAX_TOKS][CHAT_TOKEN_MAX];
    SURFACE_FLAGS sf;
    uint32_t n = Split(line, toks, CHAT_MAX_TOKS, &sf);
    char norm[256];
    size_t pos = 0;
    uint32_t i, nt;
    (void)sf;
    if (out == NULL || size == 0 || (SelfScopeText()[0] == '\0' &&
                                        SelfGreetText()[0] == '\0'))
        return 0;
    norm[0] = '\0';
    for (i = 0; i < n && pos + 1 < sizeof(norm); i++)
    {
        size_t L = strlen(toks[i]);
        if (i > 0 && pos + 1 < sizeof(norm))
            norm[pos++] = ' ';
        if (pos + L >= sizeof(norm))
            return 0;
        memcpy(norm + pos, toks[i], L);
        pos += L;
    }
    norm[pos] = '\0';
    nt = SelfTriggerCount();
    for (i = 0; i < nt; i++)
    {
        const char *t = SelfTriggerAt(i);
        const char *rk = SelfTriggerReplyAt(i);
        const char *txt = NULL;
        if (t != NULL && rk != NULL && strcmp(norm, t) == 0)
        {
            if (strcmp(rk, "scope") == 0)
                txt = SelfScopeText();
            else if (strcmp(rk, "greeting") == 0)
                txt = SelfGreetText();
            if (txt != NULL && txt[0] != '\0')
            {
                /* trailing newline included: all NLG consumers
                   print raw buffers (ChatHandle, wrapper). */
                snprintf(out, size, "%s\n", txt);
                return 1;
            }
            return 0;
        }
    }
    return 0;
}

int ChatTryTextLine(CHAT *ch, const char *line, char *out,
                      size_t size)
{
    char toks[CHAT_MAX_TOKS][CHAT_TOKEN_MAX];
    SURFACE_FLAGS sf;
    uint32_t n;
    PARSED tp;
    int st;
    if (ch == NULL || line == NULL || out == NULL || size == 0)
        return 0;
    memset(&sf, 0, sizeof(sf));
    n = Split(line, toks, CHAT_MAX_TOKS, &sf);
    if (n == 0)
        return 0;
    memset(&tp, 0, sizeof(tp));
    if (!ParseIntentToks(ch, toks, n, sf.question || HasWh(toks, n),
                         sf.genitive, 0, &tp))
        return 0;
    if (tp.intent != INT_TEXTLOAD && tp.intent != INT_TEXTQ &&
        tp.intent != INT_TEXTSTATUS && tp.intent != INT_UNLOAD &&
        tp.intent != INT_TEXT_TOPICS && tp.intent != INT_TEXT_START)
        return 0;
    /* Structured questions (wh-word at position 0: quien, que, donde,
       cuantos, etc.) must NOT be handled by the TEXT fast path — they
       need the QA layer which verifies entity presence and definitional
       patterns. Detection is structural: any 2-8 char token at pos 0
       before a copula or at sentence start. */
    if (tp.intent == INT_TEXTQ && n > 0 && LooksLikeQuestionWord(toks[0], 0, toks, n))
        return 0;
    st = ChatResolveLine(ch, line, out, size, NULL, 0, NULL, 0,
                         NULL);
    if (st < 0)
    {
        snprintf(out, size, "No entendi la pregunta.\n");
        return 1;
    }
    return 1;
}

int ChatResolveLine(CHAT *ch, const char *line, char *out, size_t size,
                    char *slot, size_t slot_size,
                    char *family, size_t family_size,
                    GoalCause *cause)
{
    PARSED p;
    if (size > 0)
        out[0] = '\0';
    if (slot != NULL && slot_size > 0)
        slot[0] = '\0';
    if (family != NULL && family_size > 0)
        family[0] = '\0';
    if (cause != NULL)
        *cause = CAUSE_NONE;
    if (!ParseIntent(ch, line, &p))
    {
        if (cause != NULL)
            *cause = CAUSE_PARSE_FAIL;
        if (SelfAnswer(line, out, size))
            return GOAL_ANSWER;
        return -1;
    }
    ApplyFocus(ch, &p);
    if (p.a[0] == '\0')
    {
        if (size > 0)
            snprintf(out, size,
                     "No tengo constancia de a quien te refieres en los "
                     "textos cargados.\n");
        if (cause != NULL)
            *cause = CAUSE_ANAPHORA;
        return GOAL_UNKNOWN;
    }
    if (slot != NULL && slot_size > 0)
    {
        strncpy(slot, p.a, slot_size - 1);
        slot[slot_size - 1] = '\0';
    }
    if (family != NULL && family_size > 0)
    {
        strncpy(family, FamLabel(ch, &p), family_size - 1);
        family[family_size - 1] = '\0';
    }
    int st = GOAL_UNKNOWN;
    ChatAnswerToBuf(ch, &p, out, size, &st);
    if (st == GOAL_UNKNOWN && cause != NULL)
        *cause = VocabIdx(ch, p.a) < 0 ? CAUSE_NO_VOCAB
                                       : CAUSE_NO_DERIVATION;
    return st;
}

int ChatHandleToBuf(CHAT *ch, const char *line, char *out, size_t size)
{
    if (ch == NULL || line == NULL || out == NULL || size == 0)
        return 0;
    out[0] = '\0';
    char toks[CHAT_MAX_TOKS][CHAT_TOKEN_MAX];
    QueryPlan plan;
    uint32_t ntok = 0;
    SURFACE_FLAGS sf;
    memset(&sf, 0, sizeof(sf));
    {
        /* TEXT fast path first: dynamic-corpus lines bypass the
           plan splitter (fragments re-parse into vetoed goals,
           gluing answers or abstaining whole plans). */
        char tbuf[CHAT_ANSWER_MAX];
        if (ChatTryTextLine(ch, line, tbuf, sizeof(tbuf)))
        {
            strncpy(out, tbuf, size - 1);
            out[size - 1] = '\0';
            return 1;
        }
    }
    if (ChatBuildPlan(ch, line, &plan, toks, &ntok, &sf) >= 2)
    {
        return ChatHandleMultiBuf(ch, &plan, toks,
                                 sf.question || HasWh(toks, ntok), sf.genitive,
                                 out, size);
    }
    char ans[CHAT_ANSWER_MAX];
    char slot[CHAT_TOKEN_MAX];
    int st = ChatResolveLine(ch, line, ans, sizeof(ans), slot,
                             sizeof(slot), NULL, 0, NULL);
    (void)slot;
    (void)st;
    if (st < 0)
    {
        char self[1024];
        if (SelfAnswer(line, self, sizeof(self)))
        {
            strncpy(out, self, size - 1);
            out[size - 1] = '\0';
            return 1;
        }
        /* QA fallback: structural question answering over KB + text */
        {
            QA_ANSWER qa;
            if (QAAnswer(ch, line, &qa) && qa.confidence > 0.0f)
            {
                strncpy(out, qa.text, size - 1);
                out[size - 1] = '\0';
                return 1;
            }
        }
        strncpy(out, "No entendi la pregunta.\n", size - 1);
        out[size - 1] = '\0';
        return 1;
    }
    strncpy(out, ans, size - 1);
    out[size - 1] = '\0';
    return 1;
}

void ChatHandle(CHAT *ch, const char *line)
{
    char buf[CHAT_ANSWER_MAX];
    if (ChatHandleToBuf(ch, line, buf, sizeof(buf)))
        printf("%s", buf);
}

/* ---- Agent Core: ToolContract (diagnostic, no execution) ----
   Declarative table: which tool may satisfy which goal shape.
   Policy v1 (documented, not derived): kinship tools require known
   slots (unknown strings stay UNKNOWN so tools cannot launder
   arbitrary names into relatives); reigns is open-world either way
   (who-rules-where changes outside the corpus charter); parse-fail
   shapes route by structure (numbers, W-de/of-ARG, person mention).
   The planner never resolves and executes nothing. */
#include "tool_contract.h"

/* contract rows live in tool_config.c (file rows win, frozen
   compiled fallback): consulted here read-only. */

static int IsDigitTok(const char *tok)
{
    if (tok == NULL || tok[0] == '\0')
        return 0;
    for (size_t i = 0; tok[i] != '\0'; i++)
        if (!isdigit((unsigned char)tok[i]))
            return 0;
    return 1;
}

/* person-candidate token (structural, word-free): not a particle,
   keyword or number, and carrying at least one letter (operators
   like "*" or "+" are never person mentions). */
static int IsPersonTok(const CHAT *ch, const char toks[][CHAT_TOKEN_MAX],
                       uint32_t i)
{
    int alpha = 0;
    size_t k;
    if (IsStopTok(toks[i]) || IsDigitTok(toks[i]))
        return 0;
    if (KwHitTok(ch, toks, i))
        return 0;
    for (k = 0; toks[i][k] != '\0'; k++)
        if (isalpha((unsigned char)toks[i][k]))
        {
            alpha = 1;
            break;
        }
    return alpha;
}

/* display token for a family: the deduced Spanish stem when the
   corpus deduced one, else the family label itself. Data, not
   words: the executor echoes it, never decides with it. */
static const char *FamDisplay(const CHAT *ch, const char *family)
{
    for (uint32_t k = 0; k < ch->num_kws; k++)
        if (strcmp(ch->kws[k].family, family) == 0)
            return ch->kws[k].es_stem;
    return family;
}

ToolDecision ToolClassify(const CHAT *ch, const char *line,
                          GOAL_STATUS status, GoalCause cause,
                          const char *slot, const char *family,
                          ToolRequest *req)
{
    if (req != NULL)
    {
        req->tool = TOOL_NONE;
        req->subject[0] = '\0';
        req->relation[0] = '\0';
    }
    if (status == GOAL_AMBIGUOUS)
        return DEC_AMBIGUOUS;
    if (status == GOAL_ANSWER)
        return DEC_IS_ANSWER;
    if (cause == CAUSE_ANAPHORA)
        return DEC_UNKNOWN;
    if (cause == CAUSE_NO_VOCAB || cause == CAUSE_NO_DERIVATION)
    {
        int known = (slot != NULL && slot[0] != '\0' &&
                     VocabIdx(ch, slot) >= 0);
        {
            uint32_t i, nrows = ToolContractCount();
            for (i = 0; i < nrows; i++)
            {
                const ToolContractRow *row = ToolContractRowAt(i);
                if (row == NULL)
                    continue;
                if (family != NULL &&
                    strcmp(family, row->family) == 0 &&
                    (!row->needs_known || known))
                {
                    if (req != NULL)
                    {
                        req->tool = row->tool;
                        strncpy(req->subject, slot == NULL ? "" : slot,
                                sizeof(req->subject) - 1);
                        req->subject[sizeof(req->subject) - 1] = '\0';
                        strncpy(req->relation, FamDisplay(ch, family),
                                sizeof(req->relation) - 1);
                        req->relation[sizeof(req->relation) - 1] = '\0';
                    }
                    return DEC_NEEDS_TOOL;
                }
            }
        }
        return DEC_UNKNOWN;
    }
    if (cause == CAUSE_PARSE_FAIL)
    {
        char toks[CHAT_MAX_TOKS][CHAT_TOKEN_MAX];
        SURFACE_FLAGS sf;
        uint32_t n = Split(line, toks, CHAT_MAX_TOKS, &sf);
        (void)sf;
        uint32_t nums = 0, first = 0, last = 0;
        for (uint32_t i = 0; i < n; i++)
            if (IsDigitTok(toks[i]))
            {
                if (nums == 0)
                    first = i;
                last = i;
                nums++;
            }
        if (nums >= 2)
        {
            if (req != NULL)
            {
                size_t pos = 0;
                req->tool = TOOL_CALCULATOR;
                req->subject[0] = '\0';
                for (uint32_t i = first;
                     i <= last && pos + 1 < sizeof(req->subject); i++)
                {
                    size_t L = strlen(toks[i]);
                    if (i > first && pos + 1 < sizeof(req->subject))
                        req->subject[pos++] = ' ';
                    if (pos + L >= sizeof(req->subject))
                        L = sizeof(req->subject) - pos - 1;
                    memcpy(req->subject + pos, toks[i], L);
                    pos += L;
                }
                req->subject[pos < sizeof(req->subject)
                                 ? pos
                                 : sizeof(req->subject) - 1] = '\0';
                strncpy(req->relation, "expression",
                        sizeof(req->relation) - 1);
                req->relation[sizeof(req->relation) - 1] = '\0';
            }
            return DEC_NEEDS_TOOL;
        }
        {
            /* framed spans fail closed here: the frames owned the
               relation word and refused it, so no tool second-guesses
               them (orphan kingship stays echo, not person-routed). */
            int kwx = -1, kwpos = -1;
            MatchKwSpan(ch, toks, 0, n, &kwx, &kwpos);
            if (kwx >= 0)
                return DEC_UNKNOWN;
        }
        for (uint32_t i = 1; i + 1 < n; i++)
        {
            if (strcmp(toks[i], "de") != 0 &&
                strcmp(toks[i], "of") != 0)
                continue;
            if (IsStopTok(toks[i - 1]) || KwHitTok(ch, toks, i - 1) ||
                IsDigitTok(toks[i - 1]) || IsStopTok(toks[i + 1]) ||
                IsDigitTok(toks[i + 1]))
                continue;
            if (req != NULL)
            {
                req->tool = TOOL_LOOKUP_RELATION;
                strncpy(req->subject, toks[i + 1],
                        sizeof(req->subject) - 1);
                req->subject[sizeof(req->subject) - 1] = '\0';
                    strncpy(req->relation, toks[i - 1],
                            sizeof(req->relation) - 1);
                    req->relation[sizeof(req->relation) - 1] = '\0';
                }
                return DEC_NEEDS_TOOL;
            }
        {
            /* shell-shape: a known tool/command token anywhere (verbs
               before it are ignored, never listed); argv runs from
               the trigger, default when empty. Explicit mention
               outranks the person heuristic below. */
            uint32_t i;
            for (i = 0; i < n; i++)
            {
                char be[16], df[64];
                if (!ShellLookup(toks[i], be, sizeof(be), df,
                                 sizeof(df)))
                    continue;
                if (req != NULL)
                {
                    size_t pos = 0;
                    uint32_t j;
                    req->tool = TOOL_SHELL;
                    strncpy(req->relation, be,
                            sizeof(req->relation) - 1);
                    req->relation[sizeof(req->relation) - 1] = '\0';
                    for (j = i;
                         j < n && pos + 1 < sizeof(req->subject); j++)
                    {
                        size_t L = strlen(toks[j]);
                        if (j > i && pos + 1 < sizeof(req->subject))
                            req->subject[pos++] = ' ';
                        if (pos + L >= sizeof(req->subject))
                            break;
                        memcpy(req->subject + pos, toks[j], L);
                        pos += L;
                    }
                    if (i + 1 >= n && df[0] != '\0')
                    {
                        size_t L = strlen(df);
                        if (pos > 0 && pos + 1 < sizeof(req->subject))
                            req->subject[pos++] = ' ';
                        if (pos + L < sizeof(req->subject))
                        {
                            memcpy(req->subject + pos, df, L);
                            pos += L;
                        }
                    }
                    req->subject[pos < sizeof(req->subject)
                                     ? pos
                                     : sizeof(req->subject) - 1] = '\0';
                }
                return DEC_NEEDS_TOOL;
            }
        }
        {
            /* file-shape: sandbox-confined name with known source/text
               extension (structural dotted token; the executor
               re-validates before touching the filesystem). */
            uint32_t i;
            for (i = 0; i < n; i++)
                if (FsNameOk(toks[i]))
                {
                    if (req != NULL)
                    {
                        req->tool = TOOL_FS_READ;
                        strncpy(req->subject, toks[i],
                                sizeof(req->subject) - 1);
                        req->subject[sizeof(req->subject) - 1] = '\0';
                    }
                    return DEC_NEEDS_TOOL;
                }
        }
        {
            /* person mention: last ingested-vocabulary member when
               present (substituted "jesse"), else last content token
               ("jonas"); needs verb + name (>= 2) so bare "el
               primero" stays GENUINE. Data-driven, no names. */
            uint32_t content = 0;
            int pick = -1, vpick = -1;
            for (uint32_t i = 0; i < n; i++)
                if (IsPersonTok(ch, toks, i))
                {
                    content++;
                    if (VocabIdx(ch, toks[i]) >= 0)
                        vpick = (int)i;
                    else
                        pick = (int)i;
                }
            if (content >= 2 && (vpick >= 0 || pick >= 0))
            {
                int bp = (vpick >= 0) ? vpick : pick;
                if (req != NULL)
                {
                    req->tool = TOOL_LOOKUP_PERSON;
                    strncpy(req->subject, toks[bp],
                            sizeof(req->subject) - 1);
                    req->subject[sizeof(req->subject) - 1] = '\0';
                }
                return DEC_NEEDS_TOOL;
            }
        }
        return DEC_UNKNOWN;
    }
    return DEC_UNKNOWN;
}

/* Single-goal tool routing for serving paths (multi-goal lines
   already route per goal in ChatHandleMulti): on parse-fail or
   honest UNKNOWN, ask the planner; on NEEDS_TOOL execute and answer
   from the result. ANSWER and AMBIGUOUS never route. Misses and
   genuine unknowns keep the original text. Returns 1 when a tool
   answered (out[] replaced). Pure orchestration over the frozen
   planner/executor contract. */
int ChatToolAnswer(CHAT *ch, const char *line, int st, GoalCause cause,
                   const char *slot, const char *family, char *out,
                   size_t size)
{
    ToolRequest treq;
    ToolResult tres;
    ToolDecision dec;
    if (st != GOAL_UNKNOWN && st >= 0)
        return 0;
    if (out == NULL || size == 0)
        return 0;
    memset(&treq, 0, sizeof(treq));
    memset(&tres, 0, sizeof(tres));
    dec = ToolClassify(ch, line, GOAL_UNKNOWN, cause,
                       slot == NULL ? "" : slot,
                       family == NULL ? "" : family, &treq);
    if (dec != DEC_NEEDS_TOOL)
        return 0;
    ToolExecute(&treq, &tres);
    return ToolAnswerGoal(&treq, &tres, out, size);
}