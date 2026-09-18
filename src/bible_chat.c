/* ============================================================
   bible_chat: symbolic conversational engine (no tensors, no
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
#include "bible_chat.h"
#include "tool_contract.h"

#define CHAT_MAX_TOKS 16

/* struct CHAT_ is defined in bible_chat.h (shared with tests) */

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
    case 0xE1: case 0xC1: *out = 'a'; break; /* a-acute */
    case 0xE9: case 0xC9: *out = 'e'; break; /* e-acute */
    case 0xED: case 0xCD: *out = 'i'; break; /* i-acute */
    case 0xF3: case 0xD3: *out = 'o'; break; /* o-acute */
    case 0xFA: case 0xDA: *out = 'u'; break; /* u-acute */
    case 0xFC: case 0xDC: *out = 'u'; break; /* u-diaeresis */
    case 0xF1: case 0xD1: *out = 'n'; break; /* n-tilde */
    default:
        *out = (char)tolower(c);
        if (c >= 0x80)
            *out = '?'; /* any other high byte: not corpus vocab */
        break;
    }
    return 1;
}

/* ---- FASE 4 CanonicalizeQuery: surface flags (SURFACE_FLAGS lives
   in bible_chat.h) + canonical tokens. Punctuation is signal, not
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
        /* leading inverted question marks (UTF-8 C2 BF, possibly
           repeated): interrogative force, never token content */
        while (len >= 2 && (unsigned char)start[0] == 0xC2 &&
               (unsigned char)start[1] == 0xBF)
        {
            if (sf)
                sf->question = 1;
            start += 2;
            len -= 2;
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

/* FASE 4: closed-class structural particles (functional vocabulary
   already present as literals across the parser; never content).
   Shared by the G1 topic test (inverse) and the G2 orphan-slot veto. */
static int IsStopTok(const char *tok)
{
    static const char *STOP[] = {
        "de", "of", "del", "'s", "el", "la", "los", "las", "the",
        "un", "una", "unos", "unas", "a", "an", "en", "y", "e",
        "o", "u", "que", "quien", "quienes", "cual", "cuales",
        "su", "sus", "his", "her", "mi", "my", "tu", "your",
        "es", "era", "fue", "is", "was", "son", "por", "why",
        "no", "si",
    };
    for (size_t i = 0; i < sizeof(STOP) / sizeof(STOP[0]); i++)
        if (strcmp(tok, STOP[i]) == 0)
            return 1;
    return 0;
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

static const char *BibleRelToConn(const char *rel)
{
    if (strcmp(rel, "HIJO_DE") == 0)
        return "isa";
    if (strcmp(rel, "HERMANO_DE") == 0)
        return "sibling_of";
    if (strcmp(rel, "PADRE_DE") == 0)
        return "father_of";
    if (strcmp(rel, "REY_DE") == 0)
        return "reigns";
    if (strcmp(rel, "ESPOSA_DE") == 0)
        return "wife_of";
    return NULL;
}

/* Ingest the TSV into the working KB. Returns rows learned.
   The relation index (kw index) is DEDUCED here: the Spanish stem
   of REL (REL minus the "_DE" suffix), the English stem = the
   surface connective it was learned with, and the family where
   the pairs landed. No relation word is hardcoded. */
static void KwdRecord(CHAT *ch, const char *rel, const char *conn)
{
    char es[CHAT_TOKEN_MAX];
    size_t len = strlen(rel);
    if (len > 3 && strcmp(rel + len - 3, "_DE") == 0)
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
    if (elen > 3 && strcmp(en + elen - 3, "_of") == 0)
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

static uint32_t ChatIngestCorpus(CHAT *ch, const char *path)
{
    FILE *f = fopen(path, "r");
    if (f == NULL)
        return 0;
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
        char *eol = strpbrk(obj, "\t\r\n");
        if (eol != NULL)
            *eol = '\0';
        if (strcmp(buf, obj) == 0)
            continue; /* self-loop: never admitted */
        const char *conn = BibleRelToConn(rel);
        if (conn == NULL)
            continue;
        char sent[LEARN_MAX_LINE];
        snprintf(sent, sizeof(sent), "%s %s %s", buf, conn, obj);
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
    for (uint32_t i = 0; i < ch->kb.num_pairs && n < max_out; i++)
    {
        const PAIR_EVID *p = &ch->kb.pairs[i];
        const char *par = NULL;
        if (strcmp(p->family, "taxonomy") == 0)
        {
            if (strcmp(p->subject, child) != 0)
                continue;
            par = p->object;
        }
        else if (strcmp(p->family, "father") == 0)
        {
            if (strcmp(p->object, child) != 0)
                continue;
            par = p->subject;
        }
        else
            continue;
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
    if (!MetaHasProperty(&ch->mk, "taxonomy", META_PROP_TRANSITIVE))
        return 0;
    for (uint32_t i = 0; i < ch->kb.num_pairs; i++)
    {
        const PAIR_EVID *p = &ch->kb.pairs[i];
        if (strcmp(p->family, "taxonomy") != 0)
            continue;
        if (strcmp(p->subject, grandchild) != 0)
            continue;
        if (strcmp(p->subject, p->object) == 0)
            continue;
        /* p = (X -> P): find (P -> GP) */
        for (uint32_t j = 0; j < ch->kb.num_pairs; j++)
        {
            const PAIR_EVID *q = &ch->kb.pairs[j];
            if (strcmp(q->family, "taxonomy") != 0)
                continue;
            if (strcmp(q->subject, p->object) != 0)
                continue;
            if (strcmp(q->subject, q->object) == 0)
                continue;
            if (gp_size < strlen(q->object) + 1 ||
                mid_size < strlen(p->object) + 1)
                return 0;
            strcpy(gp, q->object);
            strcpy(mid, p->object);
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
    INT_COMPOSE_WHY   /* why is A <kw1> of B, given B <kw2> of C */
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
                           &whole) &&
            (whole.intent == INT_WHY || whole.intent == INT_COMPOSE_WHY))
            return EmitGoal(ch, toks, 0, n, -1, plan);
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

    int kw_parent = -1, kw_child = -1, kw_grand = -1, kw_desc = -1;
    int kw_why = -1, kw_who = -1, kw_is = -1;
    static const char *PARENT_W[] = {"padre", "father", "engendro",
                                     "begat", "progenitor"};
    static const char *CHILD_W[] = {"hijo",  "hijos",    "children",
                                    "child", "son",      "sons",
                                    "daughter"};
    static const char *GRAND_W[] = {"abuelo", "grandfather"};
    static const char *DESC_W[] = {"descendiente", "descendants",
                                   "descendant", "descendientes"};
    for (uint32_t i = 0; i < n; i++)
    {
        for (int k = 0; k < 5; k++)
            if (kw_parent < 0 && strcmp(toks[i], PARENT_W[k]) == 0)
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
        return 0;
    }

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
    strncpy(ch->focus, tok, sizeof(ch->focus) - 1);
    ch->focus[sizeof(ch->focus) - 1] = '\0';
    ch->focus_valid = 1;
}

/* goal outcomes live in bible_chat.h (wrapper-visible) */

#define CHAT_ANSWER_MAX 4096

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
            st = GOAL_ANSWER;
            char capP[CHAT_TOKEN_MAX];
            Cap(parents[0], capP, sizeof(capP));
            EMIT("El padre de %s es %s, segun consta en los registros "
                   "directos.\n",
                   capA, capP);
        }
        else if (found == 0)
        {
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
        RememberFocus(ch, p->a);
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
        {
            EMIT("No entendi la pregunta.\n");
            break;
        }
        char gp[CHAT_TOKEN_MAX], mid[CHAT_TOKEN_MAX];
        if (ChatGrandparent(ch, p->a, gp, sizeof(gp), mid, sizeof(mid)))
        {
            char capG[CHAT_TOKEN_MAX];
            Cap(gp, capG, sizeof(capG));
            Cap(mid, capM, sizeof(capM));
            st = GOAL_ANSWER;
            EMIT("El abuelo de %s es %s: %s es %s de %s, y %s es %s "
                   "de %s.\n",
                   capA, capG, capA, stem, capM, capM, stem, capG);
        }
        else
        {
            EMIT("No tengo constancia del abuelo de %s.\n", capA);
        }
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
        if (found == 0)
        {
            EMIT("No tengo constancia de %s de %s.\n", kw->es_stem, capA);
        }
        else
        {
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
static void ChatHandleMulti(CHAT *ch, const QueryPlan *plan,
                            const char toks[][CHAT_TOKEN_MAX],
                            int force_q, int force_gen)
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
            printf("No entendi la pregunta.\n");
            return;
        }
        printf("%s\n", out);
    }
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

/* ---- public API ---- */

void ChatInit(CHAT *ch, const char *corpus_path)
{
    memset(ch, 0, sizeof(*ch));
    SchemaKBInit(&ch->kb);
    MetaKBInit(&ch->mk);
    LearnerInit(&ch->lr, &ch->kb, &ch->mk);
    uint32_t n = ChatIngestCorpus(ch, corpus_path);
    MetaDiscover(&ch->mk);
    MetaRuleDiscover(&ch->mk);
    printf("[chat] corpus: %u facts, %u vocab, metas=%u, rules=%u\n", n,
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
    default:
        break;
    }
    if (p->kw >= 0 && (uint32_t)p->kw < ch->num_kws)
        return ch->kws[p->kw].family;
    return "";
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

void ChatHandle(CHAT *ch, const char *line)
{
    /* Fase A: multi-goal lines take the composite dispatcher; single
       goals keep the legacy single-intent path byte-identical. */
    char toks[CHAT_MAX_TOKS][CHAT_TOKEN_MAX];
    QueryPlan plan;
    uint32_t ntok = 0;
    SURFACE_FLAGS sf;
    memset(&sf, 0, sizeof(sf));
    if (ChatBuildPlan(ch, line, &plan, toks, &ntok, &sf) >= 2)
    {
        ChatHandleMulti(ch, &plan, toks,
                        sf.question || HasWh(toks, ntok), sf.genitive);
        return;
    }
    char ans[CHAT_ANSWER_MAX];
    char slot[CHAT_TOKEN_MAX];
    int st = ChatResolveLine(ch, line, ans, sizeof(ans), slot,
                             sizeof(slot), NULL, 0, NULL);
    (void)slot;
    (void)st;
    if (st < 0)
    {
        printf("No entendi la pregunta.\n");
        return;
    }
    printf("%s", ans);
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

static const struct
{
    const char *family;
    ToolId      tool;
    int         needs_known;
} TOOL_CONTRACT[] = {
    {"reigns", TOOL_LOOKUP_RELATION, 0},
    {"taxonomy", TOOL_LOOKUP_RELATION, 1},
    {"father", TOOL_LOOKUP_RELATION, 1},
    {"sibling", TOOL_LOOKUP_RELATION, 1},
    {"wife", TOOL_LOOKUP_RELATION, 1},
    {"parent", TOOL_LOOKUP_RELATION, 1},
    {"children", TOOL_LOOKUP_RELATION, 1},
    {"grandparent", TOOL_LOOKUP_RELATION, 1},
    {"descendant", TOOL_LOOKUP_RELATION, 1},
};

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
        for (size_t i = 0;
             i < sizeof(TOOL_CONTRACT) / sizeof(TOOL_CONTRACT[0]); i++)
            if (family != NULL &&
                strcmp(family, TOOL_CONTRACT[i].family) == 0 &&
                (!TOOL_CONTRACT[i].needs_known || known))
            {
                if (req != NULL)
                {
                    req->tool = TOOL_CONTRACT[i].tool;
                    strncpy(req->subject, slot == NULL ? "" : slot,
                            sizeof(req->subject) - 1);
                    req->subject[sizeof(req->subject) - 1] = '\0';
                    strncpy(req->relation, FamDisplay(ch, family),
                            sizeof(req->relation) - 1);
                    req->relation[sizeof(req->relation) - 1] = '\0';
                }
                return DEC_NEEDS_TOOL;
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