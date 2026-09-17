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

/* split on whitespace, lowercase + accent-folded (mirrors learn.c
   contract; surface noise like "reinó"/"reyó" folds to corpus form) */
static uint32_t Split(const char *line, char toks[][CHAT_TOKEN_MAX],
                      uint32_t max_toks)
{
    uint32_t n = 0;
    const char *p = line;
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
        if (len >= CHAT_TOKEN_MAX)
            len = CHAT_TOKEN_MAX - 1;
        size_t o = 0;
        for (size_t i = 0; i < len;)
        {
            if (o >= CHAT_TOKEN_MAX - 1)
                break;
            i += FoldChar(start + i, &toks[n][o]);
            o++;
        }
        toks[n][o] = '\0';
        n++;
    }
    return n;
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
    INT_REL_BOOL      /* generic deduced frame: es A <kw> de B */
} INTENT;

typedef struct
{
    INTENT intent;
    char   a[CHAT_TOKEN_MAX]; /* slot A */
    char   b[CHAT_TOKEN_MAX]; /* slot B (boolean/why) */
    int    has_b;
    int    kw;                /* deduced relation index (generic) */
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

static int ParseIntent(const CHAT *ch, const char *line, PARSED *p)
{
    char toks[CHAT_MAX_TOKS][CHAT_TOKEN_MAX];
    uint32_t n = Split(line, toks, CHAT_MAX_TOKS);
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

    /* EN surface forms: the connective-derived en_stem (sibling,
       reigns, wife_of...) is not the English noun people use
       (brother, king...). Like BibleRelToConn, this is a
       consultable ingestion-vocabulary table at the chat layer,
       not logic; unknown words match nothing. */
    static const struct
    {
        const char *en_word;
        const char *es_stem;
    } EN_SURFACE[] = {
        {"brother", "hermano"}, {"king", "rey"},
        {"husband", "esposa"},  {"wife", "esposa"},
    };

    /* generic deduced-relation match: scan the tokens against the
       ingest-time index (kws) — no relation word lives in code */
    int kwx = -1; /* index of the matched deduced keyword */
    int kwpos = -1; /* token position of that keyword */
    for (uint32_t i = 0; i < n && kwx < 0; i++)
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
                for (size_t e = 0;
                     e < sizeof(EN_SURFACE) / sizeof(EN_SURFACE[0]); e++)
                    if (strcmp(toks[i], EN_SURFACE[e].en_word) == 0 &&
                        strcmp(EN_SURFACE[e].es_stem, kw->es_stem) == 0)
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
            (uint32_t)kw_grand + 1 < n)
        {
            if (p->a[0] == '\0')
                strncpy(p->a, toks[kw_grand + 1], CHAT_TOKEN_MAX - 1),
                    p->a[CHAT_TOKEN_MAX - 1] = '\0';
            /* strip article if captured ("el padre" style) */
            if (strcmp(p->a, "el") == 0 || strcmp(p->a, "la") == 0 ||
                strcmp(p->a, "los") == 0 || strcmp(p->a, "las") == 0)
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
            (uint32_t)kw_desc + 1 < n)
        {
            if (p->a[0] == '\0')
                strncpy(p->a, toks[kw_desc + 1], CHAT_TOKEN_MAX - 1),
                    p->a[CHAT_TOKEN_MAX - 1] = '\0';
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
            /* reject the relation word itself in any folded form */
            char rcand[4][CHAT_TOKEN_MAX];
            uint32_t nrc = 0;
            MorphFold(cand, rcand, &nrc);
            int is_kw_variant = 0;
            for (uint32_t c = 0; c < nrc; c++)
                if (strcmp(rcand[c], ch->kws[kwx].es_stem) == 0 ||
                    strcmp(rcand[c], ch->kws[kwx].en_stem) == 0)
                {
                    is_kw_variant = 1;
                    break;
                }
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

    /* CHILDREN: "quienes son los hijos de X"; anaphora "sus" */
    if (kw_child >= 0 && kw_parent < 0)
    {
        int sus = HasTok(toks, n, "sus");
        sus = sus < 0 ? HasTok(toks, n, "su") : sus;
        if (TokAfterDe(toks, n, (uint32_t)kw_child + 1, p->a,
                       CHAT_TOKEN_MAX))
        {
            p->intent = INT_CHILDREN_OF;
            return 1;
        }
        if (sus >= 0)
        {
            p->a[0] = '\0'; /* focus fills it */
            p->intent = INT_CHILDREN_OF;
            return 1;
        }
        if ((uint32_t)kw_child + 1 < n)
        {
            strncpy(p->a, toks[kw_child + 1], CHAT_TOKEN_MAX - 1);
            p->a[CHAT_TOKEN_MAX - 1] = '\0';
            p->intent = INT_CHILDREN_OF;
            return 1;
        }
        return 0;
    }

    /* PARENT: "quien fue el padre de X" */
    if (kw_parent >= 0)
    {
        if (TokAfterDe(toks, n, (uint32_t)kw_parent + 1, p->a,
                       CHAT_TOKEN_MAX))
        {
            p->intent = INT_PARENT_OF;
            return 1;
        }
        if ((uint32_t)kw_parent + 1 < n)
        {
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
            p->kw = kwx;
            p->intent = INT_REL_QUERY;
            return 1;
        }
        if (from < n)
        {
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

/* ---- NLG (ES) ---- */

static void RememberFocus(CHAT *ch, const char *tok)
{
    strncpy(ch->focus, tok, sizeof(ch->focus) - 1);
    ch->focus[sizeof(ch->focus) - 1] = '\0';
    ch->focus_valid = 1;
}

static void ChatAnswer(CHAT *ch, const PARSED *p)
{
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
            char capP[CHAT_TOKEN_MAX];
            Cap(parents[0], capP, sizeof(capP));
            printf("El padre de %s es %s, segun consta en los registros "
                   "directos.\n",
                   capA, capP);
        }
        else if (found == 0)
        {
            printf("No tengo constancia del padre de %s en los textos "
                   "cargados.\n",
                   capA);
        }
        else
        {
            printf("Hay %u constancias del padre de %s: ambiguo, necesito "
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
            printf("No tengo constancia de hijos de %s.\n", capA);
        }
        else
        {
            printf("Los hijos de %s son:", capA);
            for (uint32_t i = 0; i < n; i++)
            {
                char capK[CHAT_TOKEN_MAX];
                Cap(kids[i], capK, sizeof(capK));
                printf("%s %s", i ? "," : "", capK);
            }
            printf(".\n");
        }
        break;
    }
    case INT_IS_PARENT:
    {
        RememberFocus(ch, p->a);
        int yes = ChatDirect(ch, p->a, p->b);
        char mid[CHAT_TOKEN_MAX];
        if (!yes)
            yes = ChatChain(ch, p->a, p->b, mid, sizeof(mid));
        if (yes)
            printf("Si, %s es hijo de %s.\n", capA, capB);
        else
            printf("No tengo constancia de que %s sea hijo de %s.\n", capA,
                   capB);
        break;
    }
    case INT_GRANDPARENT:
    {
        RememberFocus(ch, p->a);
        char gp[CHAT_TOKEN_MAX], mid[CHAT_TOKEN_MAX];
        if (ChatGrandparent(ch, p->a, gp, sizeof(gp), mid, sizeof(mid)))
        {
            char capG[CHAT_TOKEN_MAX];
            Cap(gp, capG, sizeof(capG));
            Cap(mid, capM, sizeof(capM));
            printf("El abuelo de %s es %s: %s es hijo de %s, y %s es hijo "
                   "de %s.\n",
                   capA, capG, capA, capM, capM, capG);
        }
        else
        {
            printf("No tengo constancia del abuelo de %s.\n", capA);
        }
        break;
    }
    case INT_DESCENDANT:
    {
        RememberFocus(ch, p->a);
        char gd[CHAT_TOKEN_MAX], mid[CHAT_TOKEN_MAX];
        if (ChatDescendant(ch, p->a, gd, sizeof(gd), mid, sizeof(mid)))
        {
            char capD[CHAT_TOKEN_MAX];
            Cap(gd, capD, sizeof(capD));
            Cap(mid, capM, sizeof(capM));
            printf("Un descendiente de %s es %s: %s es hijo de %s, y %s "
                   "es hijo de %s.\n",
                   capA, capD, capD, capM, capM, capA);
        }
        else
        {
            printf("No tengo constancia de descendientes de %s.\n", capA);
        }
        break;
    }
    case INT_WHY:
    {
        RememberFocus(ch, p->a);
        char mid[CHAT_TOKEN_MAX];
        if (ChatDirect(ch, p->a, p->b))
        {
            printf("%s es hijo de %s segun constancia directa.\n", capA,
                   capB);
        }
        else if (ChatChain(ch, p->a, p->b, mid, sizeof(mid)))
        {
            char capM2[CHAT_TOKEN_MAX];
            Cap(mid, capM2, sizeof(capM2));
            printf("Lo se porque %s es hijo de %s, y %s es hijo de %s.\n",
                   capA, capM2, capM2, capB);
        }
        else
        {
            printf("No tengo constancia de una relacion entre %s y %s.\n",
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
            yes = ChatDirect(ch, p->a, p->b) ||
                  ChatChainFam(ch, fam, p->a, p->b, mid, sizeof(mid));
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
            printf("Si, %s %s de %s.\n", capA, kw->es_stem, capB);
        else
            printf("No tengo constancia de que %s %s de %s.\n", capA,
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
            printf("No tengo constancia de %s de %s.\n", kw->es_stem, capA);
        }
        else
        {
            printf("%s de %s:", kw->es_stem, capA);
            for (uint32_t i = 0; i < found; i++)
            {
                char capH[CHAT_TOKEN_MAX];
                Cap(hits[i], capH, sizeof(capH));
                printf("%s %s", i ? "," : "", capH);
            }
            printf(".\n");
        }
        break;
    }
    default:
        printf("No entendi la pregunta.\n");
        break;
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
    printf("[chat] corpus: %u hechos, %u vocab, metas=%u\n", n,
           ch->kb.num_vocab, MetaCount(&ch->mk));
}

void ChatHandle(CHAT *ch, const char *line)
{
    PARSED p;
    if (!ParseIntent(ch, line, &p))
    {
        printf("No entendi la pregunta.\n");
        return;
    }
    ApplyFocus(ch, &p);
    ChatAnswer(ch, &p);
}