/* ============================================================
   graph_reasoning: Inductive Rule Mining & Abductive Reasoning.
   Pure C99, zero tensors, zero backprop, fail-closed verification.

   English code comments (project rule); Spanish only in NLG literals.
   HARDCODING=0: Rules are mined structurally from graph topology.
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "graph_reasoning.h"

/* Structure to track candidate composition rules during mining */
typedef struct
{
    SYMBOL_ID r1;
    SYMBOL_ID r2;
    SYMBOL_ID head;
    uint32_t  support;
    uint32_t  premises;
} CANDIDATE_COMP;

#define MAX_CANDIDATES 512

void GraphRuleBaseInit(GRAPH_RULE_BASE *rb, float min_confidence, uint32_t min_support)
{
    if (!rb) return;
    memset(rb, 0, sizeof(*rb));
    rb->min_confidence = (min_confidence > 0.0f) ? min_confidence : 0.75f;
    rb->min_support = (min_support > 0) ? min_support : 2;
}

void GraphRuleFormat(const GRAPH *graph, const GRAPH_RULE *rule, char *out, size_t out_size)
{
    if (!graph || !rule || !out || out_size == 0) return;

    const SYMBOL *s_r1 = SymbolGet(graph->symbols, rule->r1);
    const SYMBOL *s_r2 = (rule->r2 != SYMBOL_INVALID) ? SymbolGet(graph->symbols, rule->r2) : NULL;
    const SYMBOL *s_head = SymbolGet(graph->symbols, rule->head);

    const char *n_r1 = s_r1 ? s_r1->name : "?";
    const char *n_r2 = s_r2 ? s_r2->name : "?";
    const char *n_head = s_head ? s_head->name : "?";

    switch (rule->type)
    {
    case RULE_TYPE_TRANSITIVE:
        snprintf(out, out_size, "%s o %s => %s [TRANSITIVE] (supp=%u, conf=%.2f)",
                 n_r1, n_r1, n_head, rule->support, rule->confidence);
        break;
    case RULE_TYPE_COMPOSITION:
        snprintf(out, out_size, "%s o %s => %s [COMPOSE] (supp=%u, conf=%.2f)",
                 n_r1, n_r2, n_head, rule->support, rule->confidence);
        break;
    case RULE_TYPE_INVERSE:
        snprintf(out, out_size, "%s(A,B) => %s(B,A) [INVERSE] (supp=%u, conf=%.2f)",
                 n_r1, n_head, rule->support, rule->confidence);
        break;
    case RULE_TYPE_SYMMETRIC:
        snprintf(out, out_size, "%s(A,B) => %s(B,A) [SYMMETRIC] (supp=%u, conf=%.2f)",
                 n_r1, n_head, rule->support, rule->confidence);
        break;
    }
}

/* Helper to record or update a candidate rule during mining */
static void RecordCandidate(CANDIDATE_COMP *cands, uint32_t *ncands,
                            SYMBOL_ID r1, SYMBOL_ID r2, SYMBOL_ID head,
                            int confirmed)
{
    for (uint32_t i = 0; i < *ncands; i++)
    {
        if (cands[i].r1 == r1 && cands[i].r2 == r2 && cands[i].head == head)
        {
            cands[i].premises++;
            if (confirmed) cands[i].support++;
            return;
        }
    }

    if (*ncands < MAX_CANDIDATES)
    {
        CANDIDATE_COMP *c = &cands[(*ncands)++];
        c->r1 = r1;
        c->r2 = r2;
        c->head = head;
        c->premises = 1;
        c->support = confirmed ? 1 : 0;
    }
}

uint32_t GraphMineRules(const GRAPH *graph, GRAPH_RULE_BASE *rb)
{
    if (!graph || !graph->relations || !rb)
        return 0;

    rb->num_rules = 0;
    uint32_t total_rel = graph->relations->count;
    if (total_rel == 0)
        return 0;

    CANDIDATE_COMP candidates[MAX_CANDIDATES];
    uint32_t ncands = 0;

    /* 1. Mine Compositions & Transitivity: Path length 2 (A -> B -> C) */
    for (uint32_t i = 0; i < total_rel; i++)
    {
        const RELATION *rel1 = &graph->relations->items[i];
        SYMBOL_ID a = rel1->subject;
        SYMBOL_ID b = rel1->object;
        SYMBOL_ID r1 = rel1->relation;

        /* Find outgoing edges from b */
        RELATION *out_b[64];
        uint32_t n_out = GraphQuerySubject(graph, b, out_b, 64);

        for (uint32_t j = 0; j < n_out; j++)
        {
            const RELATION *rel2 = out_b[j];
            SYMBOL_ID c = rel2->object;
            SYMBOL_ID r2 = rel2->relation;

            /* Exclude degenerate loops (A == C or self-links) */
            if (a == c || a == b || b == c)
                continue;

            /* Check if direct edge (A -> C) exists for any relation */
            RELATION *direct_ac[32];
            uint32_t n_ac = GraphQuerySubject(graph, a, direct_ac, 32);

            int confirmed_any = 0;
            for (uint32_t k = 0; k < n_ac; k++)
            {
                if (direct_ac[k]->object == c)
                {
                    SYMBOL_ID head = direct_ac[k]->relation;
                    RecordCandidate(candidates, &ncands, r1, r2, head, 1);
                    confirmed_any = 1;
                }
            }

            /* If no edge (A -> C) exists at all, count as unconfirmed premise for (r1, r2) */
            if (!confirmed_any)
            {
                RecordCandidate(candidates, &ncands, r1, r2, SYMBOL_INVALID, 0);
            }
        }
    }

    /* Promote qualifying composition/transitive candidates to RuleBase */
    for (uint32_t i = 0; i < ncands; i++)
    {
        CANDIDATE_COMP *c = &candidates[i];
        if (c->head == SYMBOL_INVALID || c->support < rb->min_support)
            continue;

        float conf = (c->premises > 0) ? ((float)c->support / (float)c->premises) : 0.0f;
        if (conf >= rb->min_confidence && rb->num_rules < MAX_MINED_RULES)
        {
            GRAPH_RULE *r = &rb->rules[rb->num_rules++];
            r->r1 = c->r1;
            r->r2 = c->r2;
            r->head = c->head;
            r->support = c->support;
            r->premises = c->premises;
            r->confidence = conf;

            if (c->r1 == c->r2 && c->r1 == c->head)
                r->type = RULE_TYPE_TRANSITIVE;
            else
                r->type = RULE_TYPE_COMPOSITION;

            GraphRuleFormat(graph, r, r->name, sizeof(r->name));
        }
    }

    /* 2. Mine Inverses and Symmetry: (A -> B) vs (B -> A) */
    for (uint32_t i = 0; i < total_rel; i++)
    {
        const RELATION *rel = &graph->relations->items[i];
        SYMBOL_ID a = rel->subject;
        SYMBOL_ID b = rel->object;
        SYMBOL_ID r1 = rel->relation;

        if (a == b) continue;

        RELATION *rev[32];
        uint32_t n_rev = GraphQuerySubjectRelation(graph, b, r1, rev, 32);
        for (uint32_t k = 0; k < n_rev; k++)
        {
            if (rev[k]->object == a)
            {
                /* Check if already added as symmetric */
                int already = 0;
                for (uint32_t r = 0; r < rb->num_rules; r++)
                {
                    if (rb->rules[r].type == RULE_TYPE_SYMMETRIC && rb->rules[r].r1 == r1)
                    {
                        rb->rules[r].support++;
                        already = 1;
                        break;
                    }
                }
                if (!already && rb->num_rules < MAX_MINED_RULES)
                {
                    GRAPH_RULE *r = &rb->rules[rb->num_rules++];
                    r->type = RULE_TYPE_SYMMETRIC;
                    r->r1 = r1;
                    r->r2 = SYMBOL_INVALID;
                    r->head = r1;
                    r->support = 1;
                    r->premises = 1;
                    r->confidence = 1.0f;
                    GraphRuleFormat(graph, r, r->name, sizeof(r->name));
                }
            }
        }
    }

    return rb->num_rules;
}

uint32_t GraphApplyRules(GRAPH *graph, const GRAPH_RULE_BASE *rb)
{
    if (!graph || !graph->relations || !rb)
        return 0;

    uint32_t inferred_count = 0;
    uint32_t total_rel = graph->relations->count;

    for (uint32_t r = 0; r < rb->num_rules; r++)
    {
        const GRAPH_RULE *rule = &rb->rules[r];

        if (rule->type == RULE_TYPE_TRANSITIVE || rule->type == RULE_TYPE_COMPOSITION)
        {
            for (uint32_t i = 0; i < total_rel; i++)
            {
                const RELATION *rel1 = &graph->relations->items[i];
                if (rel1->relation != rule->r1)
                    continue;

                SYMBOL_ID a = rel1->subject;
                SYMBOL_ID b = rel1->object;

                RELATION *out_b[64];
                uint32_t n_out = GraphQuerySubjectRelation(graph, b, rule->r2, out_b, 64);

                for (uint32_t j = 0; j < n_out; j++)
                {
                    SYMBOL_ID c = out_b[j]->object;
                    if (a == c) continue;

                    /* Check if (A -> head -> C) already exists */
                    if (GraphFindRelation(graph, a, rule->head, c) == NULL)
                    {
                        if (GraphAddRelation(graph, a, rule->head, c) == 1)
                        {
                            inferred_count++;
                        }
                    }
                }
            }
        }
        else if (rule->type == RULE_TYPE_SYMMETRIC)
        {
            for (uint32_t i = 0; i < total_rel; i++)
            {
                const RELATION *rel = &graph->relations->items[i];
                if (rel->relation != rule->r1)
                    continue;

                SYMBOL_ID a = rel->subject;
                SYMBOL_ID b = rel->object;

                if (GraphFindRelation(graph, b, rule->head, a) == NULL)
                {
                    if (GraphAddRelation(graph, b, rule->head, a) == 1)
                    {
                        inferred_count++;
                    }
                }
            }
        }
    }

    return inferred_count;
}

uint32_t GraphAbduce(const GRAPH *graph,
                     const GRAPH_RULE_BASE *rb,
                     SYMBOL_ID subject,
                     SYMBOL_ID relation,
                     SYMBOL_ID object,
                     ABDUCTIVE_HYPOTHESIS *hyps,
                     uint32_t max_hyps)
{
    if (!graph || !rb || !hyps || max_hyps == 0)
        return 0;

    /* If the relation is already present, no hypothesis needed */
    if (GraphFindRelation((GRAPH *)graph, subject, relation, object) != NULL)
        return 0;

    uint32_t nhyps = 0;

    for (uint32_t r = 0; r < rb->num_rules && nhyps < max_hyps; r++)
    {
        const GRAPH_RULE *rule = &rb->rules[r];
        if (rule->head != relation)
            continue;

        if (rule->type == RULE_TYPE_TRANSITIVE || rule->type == RULE_TYPE_COMPOSITION)
        {
            /* Case 1: We know (subject -> r1 -> pivot).
               Hypothesis needed to entail (subject -> head -> object): (pivot -> r2 -> object) */
            RELATION *out_s[64];
            uint32_t n_s = GraphQuerySubjectRelation(graph, subject, rule->r1, out_s, 64);

            for (uint32_t i = 0; i < n_s && nhyps < max_hyps; i++)
            {
                SYMBOL_ID pivot = out_s[i]->object;
                if (pivot == object || pivot == subject)
                    continue;

                /* Check if (pivot -> r2 -> object) is missing */
                if (GraphFindRelation((GRAPH *)graph, pivot, rule->r2, object) == NULL)
                {
                    ABDUCTIVE_HYPOTHESIS *h = &hyps[nhyps++];
                    h->subject = pivot;
                    h->relation = rule->r2;
                    h->object = object;
                    h->target_subj = subject;
                    h->target_rel = relation;
                    h->target_obj = object;
                    h->plausibility = rule->confidence;

                    const SYMBOL *s_p = SymbolGet(graph->symbols, pivot);
                    const SYMBOL *s_r2 = SymbolGet(graph->symbols, rule->r2);
                    const SYMBOL *s_obj = SymbolGet(graph->symbols, object);
                    const SYMBOL *s_sub = SymbolGet(graph->symbols, subject);
                    const SYMBOL *s_head = SymbolGet(graph->symbols, rule->head);

                    snprintf(h->explanation, sizeof(h->explanation),
                             "Si [%s %s %s] fuese cierto, se deduciria que [%s %s %s] (conf=%.2f)",
                             s_p ? s_p->name : "?", s_r2 ? s_r2->name : "?", s_obj ? s_obj->name : "?",
                             s_sub ? s_sub->name : "?", s_head ? s_head->name : "?", s_obj ? s_obj->name : "?",
                             rule->confidence);
                }
            }

            /* Case 2: We know (pivot -> r2 -> object).
               Hypothesis needed: (subject -> r1 -> pivot) */
            RELATION *in_o[64];
            uint32_t n_o = GraphQueryObject(graph, object, in_o, 64);

            for (uint32_t i = 0; i < n_o && nhyps < max_hyps; i++)
            {
                if (in_o[i]->relation != rule->r2)
                    continue;

                SYMBOL_ID pivot = in_o[i]->subject;
                if (pivot == subject || pivot == object)
                    continue;

                if (GraphFindRelation((GRAPH *)graph, subject, rule->r1, pivot) == NULL)
                {
                    /* Avoid duplicate hypothesis */
                    int dup = 0;
                    for (uint32_t k = 0; k < nhyps; k++)
                    {
                        if (hyps[k].subject == subject &&
                            hyps[k].relation == rule->r1 &&
                            hyps[k].object == pivot)
                        {
                            dup = 1;
                            break;
                        }
                    }
                    if (dup) continue;

                    ABDUCTIVE_HYPOTHESIS *h = &hyps[nhyps++];
                    h->subject = subject;
                    h->relation = rule->r1;
                    h->object = pivot;
                    h->target_subj = subject;
                    h->target_rel = relation;
                    h->target_obj = object;
                    h->plausibility = rule->confidence;

                    const SYMBOL *s_sub = SymbolGet(graph->symbols, subject);
                    const SYMBOL *s_r1 = SymbolGet(graph->symbols, rule->r1);
                    const SYMBOL *s_p = SymbolGet(graph->symbols, pivot);
                    const SYMBOL *s_obj = SymbolGet(graph->symbols, object);
                    const SYMBOL *s_head = SymbolGet(graph->symbols, rule->head);

                    snprintf(h->explanation, sizeof(h->explanation),
                             "Si [%s %s %s] fuese cierto, se deduciria que [%s %s %s] (conf=%.2f)",
                             s_sub ? s_sub->name : "?", s_r1 ? s_r1->name : "?", s_p ? s_p->name : "?",
                             s_sub ? s_sub->name : "?", s_head ? s_head->name : "?", s_obj ? s_obj->name : "?",
                             rule->confidence);
                }
            }
        }
    }

    return nhyps;
}
