/* ============================================================
   deep_nlg: Deep Symbolic Natural Language Generation (Graph-to-Text).
   Pure C99, zero tensors, zero backprop, fail-closed honest UNKNOWN.

   Three-stage symbolic generation:
     1. MACROPLANNING: Discourse structure & rhetorical relations
     2. MICROPLANNING: Sentence aggregation & referring expressions
     3. SURFACE REALIZATION: Morphology, connectors & concord

   English code comments (project rule); Spanish only in NLG literals.
   HARDCODING=0: Lexical connectors are defined in consultable tables.
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "deep_nlg.h"

/* Declarative connector tables by discourse role and language */
typedef struct
{
    const char *intro;
    const char *chain_step1;
    const char *chain_step2;
    const char *chain_step_last;
    const char *conclusion;
    const char *coord_and;
    const char *coord_also;
    const char *is_rel;
    const char *of_rel;
    const char *no_record;
    const char *no_rel_record;
    const char *hint_prefix;
} DISCOURSE_LEXICON;

static const DISCOURSE_LEXICON g_lexicon[LANG_COUNT] = {
    [LANG_EN] = {
        .intro           = "According to the verified records",
        .chain_step1     = "begat",
        .chain_step2     = "who in turn was the father of",
        .chain_step_last = "and the latter was the father of",
        .conclusion      = "Therefore, %s is the %s of %s.",
        .coord_and       = "and",
        .coord_also      = "and furthermore",
        .is_rel          = "is",
        .of_rel          = "of",
        .no_record       = "There is no record of %s in the available texts.",
        .no_rel_record   = "Although %s is referenced, there is no verified record of %s.",
        .hint_prefix     = "The closest reference mentions that"
    },
    [LANG_ES] = {
        .intro           = "Segun consta en los registros verificados",
        .chain_step1     = "engendro a",
        .chain_step2     = "quien a su vez fue padre de",
        .chain_step_last = "y este ultimo fue padre de",
        .conclusion      = "Por tanto, %s es el %s de %s.",
        .coord_and       = "y",
        .coord_also      = "y ademas",
        .is_rel          = "es",
        .of_rel          = "de",
        .no_record       = "No tengo constancia de %s en los textos disponibles.",
        .no_rel_record   = "Aunque consta referencia a %s, no hay constancia verificada de %s.",
        .hint_prefix     = "La referencia mas cercana senala que"
    },
    [LANG_FR] = {
        .intro           = "Selon les enregistrements verifies",
        .chain_step1     = "engendra",
        .chain_step2     = "qui a son tour fut le pere de",
        .chain_step_last = "et ce dernier fut le pere de",
        .conclusion      = "Par consequent, %s est le %s de %s.",
        .coord_and       = "et",
        .coord_also      = "et de plus",
        .is_rel          = "est",
        .of_rel          = "de",
        .no_record       = "Il n'y a aucune trace de %s dans les textes disponibles.",
        .no_rel_record   = "Bien que %s soit mentionne, il n'y a aucune trace verifiable de %s.",
        .hint_prefix     = "La reference la plus proche indique que"
    }
};

/* Safe token capitalization */
static void SafeCapitalize(const char *in, char *out, size_t out_size)
{
    if (!in || !out || out_size == 0) return;
    size_t i = 0;
    while (in[i] && i < out_size - 1)
    {
        out[i] = (i == 0) ? (char)toupper((unsigned char)in[i]) : in[i];
        i++;
    }
    out[i] = '\0';
}

/* Safe string append */
static void AppendSafe(char *dst, size_t max_size, const char *src)
{
    if (!dst || !src || max_size == 0) return;
    size_t cur = strlen(dst);
    if (cur >= max_size - 1) return;
    size_t add = strlen(src);
    if (cur + add >= max_size)
        add = max_size - 1 - cur;
    memcpy(dst + cur, src, add);
    dst[cur + add] = '\0';
}

/* ---- Plan Construction (Macroplanning) ---- */

void DeepNLG_PlanInit(NLG_PLAN *plan, const char *topic)
{
    if (!plan) return;
    memset(plan, 0, sizeof(*plan));
    if (topic && topic[0])
    {
        strncpy(plan->topic, topic, sizeof(plan->topic) - 1);
        strncpy(plan->focus, topic, sizeof(plan->focus) - 1);
        plan->has_focus = 1;
    }
}

int DeepNLG_PlanAdd(NLG_PLAN *plan,
                    const char *subj,
                    const char *rel,
                    const char *obj,
                    NLG_DISCOURSE_ROLE role,
                    NLG_POLARITY pol,
                    float conf)
{
    if (!plan || plan->nprops >= DEEP_NLG_PROPS_MAX)
        return 0;

    NLG_PROP *p = &plan->props[plan->nprops++];
    memset(p, 0, sizeof(*p));
    if (subj) strncpy(p->subject, subj, sizeof(p->subject) - 1);
    if (rel)  strncpy(p->relation, rel, sizeof(p->relation) - 1);
    if (obj)  strncpy(p->object, obj, sizeof(p->object) - 1);
    p->role = role;
    p->polarity = pol;
    p->confidence = conf;
    return 1;
}

int DeepNLG_PlanChain(NLG_PLAN *plan,
                      const char *start_node,
                      const char *step_rel,
                      const char hops[][DEEP_NLG_TOKEN_MAX],
                      uint32_t nhops,
                      const char *conclusion_rel,
                      const char *end_node)
{
    if (!plan || !start_node || !end_node || nhops == 0)
        return 0;

    DeepNLG_PlanInit(plan, start_node);

    /* Build intermediate steps */
    const char *prev = start_node;
    for (uint32_t i = 0; i < nhops; i++)
    {
        DeepNLG_PlanAdd(plan, prev, step_rel ? step_rel : "padre", hops[i],
                        NLG_ROLE_CHAIN_STEP, NLG_POL_AFFIRMATIVE, 1.0f);
        prev = hops[i];
    }

    /* Build conclusion */
    DeepNLG_PlanAdd(plan, start_node,
                    conclusion_rel ? conclusion_rel : "ancestro",
                    end_node,
                    NLG_ROLE_CONCLUSION, NLG_POL_AFFIRMATIVE, 1.0f);

    return 1;
}

int DeepNLG_PlanEntityOverview(NLG_PLAN *plan,
                               const char *entity,
                               const char relations[][DEEP_NLG_TOKEN_MAX],
                               const char objects[][DEEP_NLG_TOKEN_MAX],
                               uint32_t nfacts)
{
    if (!plan || !entity || nfacts == 0)
        return 0;

    DeepNLG_PlanInit(plan, entity);

    for (uint32_t i = 0; i < nfacts && plan->nprops < DEEP_NLG_PROPS_MAX; i++)
    {
        DeepNLG_PlanAdd(plan, entity, relations[i], objects[i],
                        (i == 0) ? NLG_ROLE_ASSERTION : NLG_ROLE_ELABORATION,
                        NLG_POL_AFFIRMATIVE, 1.0f);
    }
    return 1;
}

void DeepNLG_PlanAbstain(NLG_PLAN *plan,
                         const char *subject,
                         const char *relation,
                         const char *hint)
{
    if (!plan) return;
    DeepNLG_PlanInit(plan, subject);
    NLG_PROP *p = &plan->props[plan->nprops++];
    memset(p, 0, sizeof(*p));
    if (subject)  strncpy(p->subject, subject, sizeof(p->subject) - 1);
    if (relation) strncpy(p->relation, relation, sizeof(p->relation) - 1);
    if (hint)     strncpy(p->attribute, hint, sizeof(p->attribute) - 1);
    p->role = NLG_ROLE_ABSTENTION;
    p->polarity = NLG_POL_UNKNOWN;
    p->confidence = 0.0f;
}

/* ---- Microplanning & Surface Realization ---- */

static const DISCOURSE_LEXICON *GetLexicon(void)
{
    int lang = LangGet();
    if (lang < 0 || lang >= LANG_COUNT)
        lang = LANG_ES;
    return &g_lexicon[lang];
}

/* Realize an abstention plan */
static uint32_t RealizeAbstention(const NLG_PROP *p, char *out, size_t out_size)
{
    const DISCOURSE_LEXICON *lex = GetLexicon();
    char capS[DEEP_NLG_TOKEN_MAX];
    SafeCapitalize(p->subject, capS, sizeof(capS));

    if (p->relation[0] == '\0')
    {
        snprintf(out, out_size, lex->no_record, capS);
    }
    else
    {
        snprintf(out, out_size, lex->no_rel_record, capS, p->relation);
    }

    if (p->attribute[0] != '\0')
    {
        char buf[DEEP_NLG_BUF_MAX];
        snprintf(buf, sizeof(buf), " %s: \"%s\".", lex->hint_prefix, p->attribute);
        AppendSafe(out, out_size, buf);
    }
    else
    {
        AppendSafe(out, out_size, "\n");
    }

    return (uint32_t)strlen(out);
}

/* Realize a multi-hop reasoning chain */
static uint32_t RealizeChain(const NLG_PLAN *plan, char *out, size_t out_size)
{
    const DISCOURSE_LEXICON *lex = GetLexicon();
    out[0] = '\0';

    /* Find steps and conclusion */
    uint32_t step_indices[DEEP_NLG_PROPS_MAX];
    uint32_t nsteps = 0;
    const NLG_PROP *concl = NULL;

    for (uint32_t i = 0; i < plan->nprops; i++)
    {
        if (plan->props[i].role == NLG_ROLE_CHAIN_STEP)
            step_indices[nsteps++] = i;
        else if (plan->props[i].role == NLG_ROLE_CONCLUSION)
            concl = &plan->props[i];
    }

    if (nsteps == 0 && concl == NULL)
        return 0;

    char capFirst[DEEP_NLG_TOKEN_MAX];
    SafeCapitalize(plan->props[step_indices[0]].subject, capFirst, sizeof(capFirst));

    if (nsteps == 1)
    {
        /* Direct 1-hop */
        const NLG_PROP *p0 = &plan->props[step_indices[0]];
        char capObj[DEEP_NLG_TOKEN_MAX];
        SafeCapitalize(p0->object, capObj, sizeof(capObj));

        char buf[DEEP_NLG_BUF_MAX];
        snprintf(buf, sizeof(buf), "%s %s %s.", capFirst, lex->chain_step1, capObj);
        AppendSafe(out, out_size, buf);
    }
    else if (nsteps == 2)
    {
        /* 2-hop fluent chaining: A engendro a B, quien a su vez fue padre de C */
        const NLG_PROP *p0 = &plan->props[step_indices[0]];
        const NLG_PROP *p1 = &plan->props[step_indices[1]];
        char capMid[DEEP_NLG_TOKEN_MAX], capEnd[DEEP_NLG_TOKEN_MAX];
        SafeCapitalize(p0->object, capMid, sizeof(capMid));
        SafeCapitalize(p1->object, capEnd, sizeof(capEnd));

        char buf[DEEP_NLG_BUF_MAX];
        snprintf(buf, sizeof(buf), "%s %s %s, %s %s.",
                 capFirst, lex->chain_step1, capMid, lex->chain_step2, capEnd);
        AppendSafe(out, out_size, buf);
    }
    else
    {
        /* 3+ hops deep chaining with discourse flow */
        char buf[DEEP_NLG_BUF_MAX];
        const NLG_PROP *p0 = &plan->props[step_indices[0]];
        char capMid[DEEP_NLG_TOKEN_MAX];
        SafeCapitalize(p0->object, capMid, sizeof(capMid));

        snprintf(buf, sizeof(buf), "%s %s %s", capFirst, lex->chain_step1, capMid);
        AppendSafe(out, out_size, buf);

        for (uint32_t i = 1; i < nsteps; i++)
        {
            const NLG_PROP *pi = &plan->props[step_indices[i]];
            char capNext[DEEP_NLG_TOKEN_MAX];
            SafeCapitalize(pi->object, capNext, sizeof(capNext));

            if (i == nsteps - 1)
            {
                snprintf(buf, sizeof(buf), ", %s %s.", lex->chain_step_last, capNext);
            }
            else
            {
                snprintf(buf, sizeof(buf), ", %s %s", lex->chain_step2, capNext);
            }
            AppendSafe(out, out_size, buf);
        }
    }

    /* Append deduction conclusion if present */
    if (concl)
    {
        char capS[DEEP_NLG_TOKEN_MAX], capO[DEEP_NLG_TOKEN_MAX];
        SafeCapitalize(concl->subject, capS, sizeof(capS));
        SafeCapitalize(concl->object, capO, sizeof(capO));

        char cbuf[DEEP_NLG_BUF_MAX];
        snprintf(cbuf, sizeof(cbuf), " ");
        char concl_stmt[DEEP_NLG_BUF_MAX];
        snprintf(concl_stmt, sizeof(concl_stmt), lex->conclusion,
                 capS, concl->relation, capO);
        AppendSafe(out, out_size, " ");
        AppendSafe(out, out_size, concl_stmt);
    }
    AppendSafe(out, out_size, "\n");
    return (uint32_t)strlen(out);
}

/* Realize compound entity description (shared subject aggregation) */
static uint32_t RealizeCompoundEntity(const NLG_PLAN *plan, char *out, size_t out_size)
{
    const DISCOURSE_LEXICON *lex = GetLexicon();
    out[0] = '\0';

    if (plan->nprops == 0)
        return 0;

    char capSubj[DEEP_NLG_TOKEN_MAX];
    SafeCapitalize(plan->props[0].subject, capSubj, sizeof(capSubj));

    char buf[DEEP_NLG_BUF_MAX];
    snprintf(buf, sizeof(buf), "%s, ", lex->intro);
    AppendSafe(out, out_size, buf);

    /* First fact */
    const NLG_PROP *p0 = &plan->props[0];
    char capObj0[DEEP_NLG_TOKEN_MAX];
    SafeCapitalize(p0->object, capObj0, sizeof(capObj0));

    snprintf(buf, sizeof(buf), "%s %s %s %s %s",
             capSubj, lex->is_rel, p0->relation, lex->of_rel, capObj0);
    AppendSafe(out, out_size, buf);

    /* Subsequent facts with subject elision & discourse connectors */
    for (uint32_t i = 1; i < plan->nprops; i++)
    {
        const NLG_PROP *pi = &plan->props[i];
        char capObjI[DEEP_NLG_TOKEN_MAX];
        SafeCapitalize(pi->object, capObjI, sizeof(capObjI));

        const char *conn = (i == plan->nprops - 1) ? lex->coord_also : lex->coord_and;
        snprintf(buf, sizeof(buf), " %s %s %s %s",
                 conn, pi->relation, lex->of_rel, capObjI);
        AppendSafe(out, out_size, buf);
    }

    AppendSafe(out, out_size, ".\n");
    return (uint32_t)strlen(out);
}

uint32_t DeepNLG_Realize(const NLG_PLAN *plan, char *out, size_t out_size)
{
    if (!plan || !out || out_size == 0)
        return 0;

    out[0] = '\0';

    /* Route to appropriate microplanner based on plan topology */
    if (plan->nprops > 0 && plan->props[0].role == NLG_ROLE_ABSTENTION)
    {
        return RealizeAbstention(&plan->props[0], out, out_size);
    }

    int has_chain = 0;
    for (uint32_t i = 0; i < plan->nprops; i++)
    {
        if (plan->props[i].role == NLG_ROLE_CHAIN_STEP ||
            plan->props[i].role == NLG_ROLE_CONCLUSION)
        {
            has_chain = 1;
            break;
        }
    }

    if (has_chain)
        return RealizeChain(plan, out, out_size);

    return RealizeCompoundEntity(plan, out, out_size);
}

/* ---- Convenience Functions ---- */

uint32_t DeepNLG_GenerateChainText(const char *start,
                                   const char *step_rel,
                                   const char hops[][DEEP_NLG_TOKEN_MAX],
                                   uint32_t nhops,
                                   const char *conclusion_rel,
                                   const char *end_node,
                                   char *out,
                                   size_t out_size)
{
    NLG_PLAN plan;
    if (!DeepNLG_PlanChain(&plan, start, step_rel, hops, nhops,
                           conclusion_rel, end_node))
        return 0;
    return DeepNLG_Realize(&plan, out, out_size);
}

uint32_t DeepNLG_GenerateMultiFactText(const char *entity,
                                      const char relations[][DEEP_NLG_TOKEN_MAX],
                                      const char objects[][DEEP_NLG_TOKEN_MAX],
                                      uint32_t nfacts,
                                      char *out,
                                      size_t out_size)
{
    NLG_PLAN plan;
    if (!DeepNLG_PlanEntityOverview(&plan, entity, relations, objects, nfacts))
        return 0;
    return DeepNLG_Realize(&plan, out, out_size);
}

uint32_t DeepNLG_GenerateAbstainText(const char *entity,
                                    const char *relation,
                                    const char *context_hint,
                                    char *out,
                                    size_t out_size)
{
    NLG_PLAN plan;
    DeepNLG_PlanAbstain(&plan, entity, relation, context_hint);
    return DeepNLG_Realize(&plan, out, out_size);
}
