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

/* split on whitespace, lowercase (mirrors learn.c contract) */
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
        for (size_t i = 0; i < len; i++)
            toks[n][i] = (char)tolower((unsigned char)start[i]);
        toks[n][len] = '\0';
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
    return NULL;
}

/* Ingest the TSV into the working KB. Returns rows learned. */
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
            learned++;
    }
    fclose(f);
    return learned;
}

/* ---- queries against the frozen layers ---- */

/* direct edge (1-hop): pair evidence + vocabulary */
static int ChatDirect(const CHAT *ch, const char *s, const char *o)
{
    char out[128];
    return TransferDerive(&ch->kb, &ch->mk, "taxonomy", s, o, out,
                          sizeof(out));
}

/* 2-hop derived conclusion (TRANSFER meta path) */
static int ChatChain(const CHAT *ch, const char *s, const char *o,
                     char *middle, size_t middle_size)
{
    char out[128];
    return TransferExplainChain(&ch->kb, &ch->mk, "taxonomy", s, o, out,
                                sizeof(out), middle, middle_size);
}

/* children scan: every direct (X -> parent) edge */
static uint32_t ChatChildren(const CHAT *ch, const char *parent,
                             char out[][CHAT_TOKEN_MAX], uint32_t max_out)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < ch->kb.num_pairs && n < max_out; i++)
    {
        const PAIR_EVID *p = &ch->kb.pairs[i];
        if (strcmp(p->family, "taxonomy") != 0)
            continue;
        if (strcmp(p->object, parent) != 0)
            continue;
        if (strcmp(p->subject, p->object) == 0)
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

/* father scan: direct parents of child (child -> parent edges).
   Pair (S,O) = "S isa O" = S is child of O, so the parents of
   `child` are the OBJECTS of pairs whose SUBJECT is `child`. */
static uint32_t ChatParents(const CHAT *ch, const char *child,
                            char out[][CHAT_TOKEN_MAX], uint32_t max_out)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < ch->kb.num_pairs && n < max_out; i++)
    {
        const PAIR_EVID *p = &ch->kb.pairs[i];
        if (strcmp(p->family, "taxonomy") != 0)
            continue;
        if (strcmp(p->subject, child) != 0)
            continue;
        if (strcmp(p->subject, p->object) == 0)
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
    INT_WHY           /* por que A hijo de B */
} INTENT;

typedef struct
{
    INTENT intent;
    char   a[CHAT_TOKEN_MAX]; /* slot A */
    char   b[CHAT_TOKEN_MAX]; /* slot B (boolean/why) */
    int    has_b;
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

static int HasTok(const char toks[][CHAT_TOKEN_MAX], uint32_t n,
                  const char *w)
{
    for (uint32_t i = 0; i < n; i++)
        if (strcmp(toks[i], w) == 0)
            return (int)i;
    return -1;
}

static int ParseIntent(const char *line, PARSED *p)
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
                kw_child = (int)i;
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
    if (!ParseIntent(line, &p))
    {
        printf("No entendi la pregunta.\n");
        return;
    }
    ApplyFocus(ch, &p);
    ChatAnswer(ch, &p);
}