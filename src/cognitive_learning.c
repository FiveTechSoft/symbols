/* ============================================================
   cognitive_learning: Advanced Learning Paradigms on the Peircean Triad.
   Pure C99, zero tensors, zero backprop, fail-closed verification.

   English code comments (project rule); Spanish only in NLG literals.
   HARDCODING=0: Dynamic rule induction and inquiry synthesis.
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cognitive_learning.h"

void CognitiveLearnerInit(COGNITIVE_LEARNER *learner, float min_confidence, uint32_t min_support)
{
    if (!learner) return;
    memset(learner, 0, sizeof(*learner));
    GraphRuleBaseInit(&learner->rule_base, min_confidence, min_support);
    learner->num_exceptions = 0;
    learner->cycle_count = 0;
}

uint32_t CognitiveFormulateInquiry(const GRAPH *graph,
                                   const COGNITIVE_LEARNER *learner,
                                   SYMBOL_ID subject,
                                   SYMBOL_ID relation,
                                   SYMBOL_ID object,
                                   EPISTEMIC_INQUIRY *inquiries,
                                   uint32_t max_inquiries)
{
    if (!graph || !learner || !inquiries || max_inquiries == 0)
        return 0;

    ABDUCTIVE_HYPOTHESIS hyps[MAX_ABDUCTIVE_HYPS];
    uint32_t nh = GraphAbduce(graph, &learner->rule_base, subject, relation, object,
                              hyps, max_inquiries);

    for (uint32_t i = 0; i < nh; i++)
    {
        EPISTEMIC_INQUIRY *inq = &inquiries[i];
        inq->target_subj = subject;
        inq->target_rel  = relation;
        inq->target_obj  = object;
        inq->missing_subj = hyps[i].subject;
        inq->missing_rel  = hyps[i].relation;
        inq->missing_obj  = hyps[i].object;
        inq->priority     = hyps[i].plausibility;

        const SYMBOL *s_ms = SymbolGet(graph->symbols, hyps[i].subject);
        const SYMBOL *s_mr = SymbolGet(graph->symbols, hyps[i].relation);
        const SYMBOL *s_mo = SymbolGet(graph->symbols, hyps[i].object);

        const SYMBOL *s_ts = SymbolGet(graph->symbols, subject);
        const SYMBOL *s_tr = SymbolGet(graph->symbols, relation);
        const SYMBOL *s_to = SymbolGet(graph->symbols, object);

        const char *ms_n = s_ms ? s_ms->name : "?";
        const char *mr_n = s_mr ? s_mr->name : "?";
        const char *mo_n = s_mo ? s_mo->name : "?";

        const char *ts_n = s_ts ? s_ts->name : "?";
        const char *tr_n = s_tr ? s_tr->name : "?";
        const char *to_n = s_to ? s_to->name : "?";

        snprintf(inq->question, sizeof(inq->question),
                 "Falta verificar si [%s %s %s] para confirmar que [%s %s %s].",
                 ms_n, mr_n, mo_n, ts_n, tr_n, to_n);

        snprintf(inq->search_term, sizeof(inq->search_term),
                 "%s %s %s", ms_n, mr_n, mo_n);
    }

    return nh;
}

int CognitiveAssimilateEvidence(GRAPH *graph,
                                COGNITIVE_LEARNER *learner,
                                SYMBOL_ID subject,
                                SYMBOL_ID relation,
                                SYMBOL_ID object)
{
    if (!graph || !learner)
        return 0;

    /* Add the confirmed piece of evidence */
    if (GraphAddRelation(graph, subject, relation, object) != 1)
        return 0;

    /* Immediately trigger forward deductive propagation */
    CognitiveRunInquiryCycle(graph, learner);
    return 1;
}

int CognitiveRegisterException(COGNITIVE_LEARNER *learner,
                               uint32_t rule_idx,
                               SYMBOL_ID exception_entity,
                               const char *rationale)
{
    if (!learner || learner->num_exceptions >= MAX_EXCEPTIONS)
        return 0;

    BELIEF_EXCEPTION *ex = &learner->exceptions[learner->num_exceptions++];
    ex->base_rule_idx = rule_idx;
    ex->exception_entity = exception_entity;
    if (rationale)
    {
        strncpy(ex->rationale, rationale, sizeof(ex->rationale) - 1);
        ex->rationale[sizeof(ex->rationale) - 1] = '\0';
    }
    else
    {
        ex->rationale[0] = '\0';
    }
    return 1;
}

int CognitiveIsException(const COGNITIVE_LEARNER *learner,
                         uint32_t rule_idx,
                         SYMBOL_ID entity)
{
    if (!learner) return 0;

    for (uint32_t i = 0; i < learner->num_exceptions; i++)
    {
        const BELIEF_EXCEPTION *ex = &learner->exceptions[i];
        if (ex->base_rule_idx == rule_idx && ex->exception_entity == entity)
            return 1;
    }
    return 0;
}

uint32_t CognitiveRunInquiryCycle(GRAPH *graph, COGNITIVE_LEARNER *learner)
{
    if (!graph || !learner)
        return 0;

    /* Phase 1: Inductive Rule Mining */
    GraphMineRules(graph, &learner->rule_base);

    /* Phase 2: Forward Deductive Chaining with Defeasible Exception Guards */
    uint32_t new_edges = 0;
    uint32_t total_rel = graph->relations->count;

    for (uint32_t r = 0; r < learner->rule_base.num_rules; r++)
    {
        const GRAPH_RULE *rule = &learner->rule_base.rules[r];

        if (rule->type == RULE_TYPE_TRANSITIVE || rule->type == RULE_TYPE_COMPOSITION)
        {
            for (uint32_t i = 0; i < total_rel; i++)
            {
                const RELATION *rel1 = &graph->relations->items[i];
                if (rel1->relation != rule->r1)
                    continue;

                SYMBOL_ID a = rel1->subject;
                SYMBOL_ID b = rel1->object;

                /* Check non-monotonic exception on subject */
                if (CognitiveIsException(learner, r, a))
                    continue;

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
                            new_edges++;
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

                if (CognitiveIsException(learner, r, b))
                    continue;

                if (GraphFindRelation(graph, b, rule->head, a) == NULL)
                {
                    if (GraphAddRelation(graph, b, rule->head, a) == 1)
                    {
                        new_edges++;
                    }
                }
            }
        }
    }

    learner->cycle_count++;
    return new_edges;
}

SELF_SUPERVISED_METRICS CognitiveSelfSupervisedTrain(GRAPH *graph,
                                                    COGNITIVE_LEARNER *learner,
                                                    uint32_t mask_count)
{
    SELF_SUPERVISED_METRICS metrics;
    memset(&metrics, 0, sizeof(metrics));

    if (!graph || !learner || mask_count == 0)
        return metrics;

    /* Ensure rule base is populated */
    GraphMineRules(graph, &learner->rule_base);

    /* Find candidate composite edges (A -> head -> C) supported by (A -> r1 -> B) and (B -> r2 -> C) */
    typedef struct { SYMBOL_ID s; SYMBOL_ID r; SYMBOL_ID o; } TRIPLE;
    TRIPLE masked[32];
    uint32_t n_masked = 0;

    for (uint32_t r = 0; r < learner->rule_base.num_rules && n_masked < mask_count && n_masked < 32; r++)
    {
        const GRAPH_RULE *rule = &learner->rule_base.rules[r];
        if (rule->type != RULE_TYPE_COMPOSITION && rule->type != RULE_TYPE_TRANSITIVE)
            continue;

        for (uint32_t i = 0; i < graph->relations->count && n_masked < mask_count && n_masked < 32; i++)
        {
            const RELATION *rel = &graph->relations->items[i];
            if (rel->relation == rule->head)
            {
                /* Check if premises exist */
                RELATION *out_a[32];
                uint32_t na = GraphQuerySubjectRelation(graph, rel->subject, rule->r1, out_a, 32);
                for (uint32_t j = 0; j < na; j++)
                {
                    SYMBOL_ID b = out_a[j]->object;
                    if (GraphFindRelation(graph, b, rule->r2, rel->object) != NULL)
                    {
                        /* Found a reconstructible edge to mask! */
                        masked[n_masked].s = rel->subject;
                        masked[n_masked].r = rel->relation;
                        masked[n_masked].o = rel->object;
                        n_masked++;
                        break;
                    }
                }
            }
        }
    }

    if (n_masked == 0)
        return metrics;

    metrics.total_masked = n_masked;

    /* Mask the edges by temporarily zeroing their relation entry */
    for (uint32_t m = 0; m < n_masked; m++)
    {
        for (uint32_t i = 0; i < graph->relations->count; i++)
        {
            RELATION *r = &graph->relations->items[i];
            if (r->subject == masked[m].s && r->relation == masked[m].r && r->object == masked[m].o)
            {
                /* Swap with last and decrement count to remove edge */
                *r = graph->relations->items[graph->relations->count - 1];
                graph->relations->count--;
                break;
            }
        }
    }

    /* Rebuild relation index after masking */
    extern void RelationIndexRebuild(RELATION_TABLE *table);
    RelationIndexRebuild(graph->relations);

    /* Run forward deductive reconstruction using the active learned rules */
    GraphApplyRules(graph, &learner->rule_base);

    /* Verify how many masked edges were reconstructed */
    for (uint32_t m = 0; m < n_masked; m++)
    {
        if (GraphFindRelation(graph, masked[m].s, masked[m].r, masked[m].o) != NULL)
        {
            metrics.reconstructed++;
        }
        else
        {
            /* Re-add if not reconstructed */
            GraphAddRelation(graph, masked[m].s, masked[m].r, masked[m].o);
        }
    }

    metrics.reconstruction_rate = (metrics.total_masked > 0)
        ? ((float)metrics.reconstructed / (float)metrics.total_masked)
        : 0.0f;

    return metrics;
}
