#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "inference.h"

INFERENCE_CONFIG InferenceConfigDefault(void)
{
    INFERENCE_CONFIG cfg;
    cfg.max_depth = 5;
    cfg.min_confidence = 0.25f;
    cfg.decay_factor = 0.90f;
    return cfg;
}

static int IsInPath(const SYMBOL_ID *nodes, uint32_t depth, SYMBOL_ID node)
{
    for (uint32_t i = 0; i < depth; i++)
    {
        if (nodes[i] == node)
            return 1;
    }
    return 0;
}

static int DFSProve(
    GRAPH *graph,
    SYMBOL_ID current_node,
    SYMBOL_ID target_pred,
    SYMBOL_ID target_obj,
    float current_confidence,
    uint32_t depth,
    const INFERENCE_CONFIG *config,
    INFERENCE_PATH *path)
{
    RELATION *direct = GraphFindRelation(graph, current_node, target_pred, target_obj);
    if (direct != NULL)
    {
        path->step_nodes[depth] = current_node;
        path->step_predicates[depth] = target_pred;
        path->step_nodes[depth + 1] = target_obj;
        path->depth = depth + 1;
        path->accumulated_confidence = current_confidence * direct->weight;
        return 1;
    }

    if (depth >= config->max_depth)
        return 0;

    if (current_confidence < config->min_confidence)
        return 0;

    path->step_nodes[depth] = current_node;

    SYMBOL_ID es_sym = SymbolFind(graph->symbols, "ES");
    if (es_sym == SYMBOL_INVALID)
        return 0;

    RELATION *candidates[32];
    uint32_t count = GraphQuerySubjectPredicate(graph, current_node, es_sym, candidates, 32);

    for (uint32_t i = 0; i < count; i++)
    {
        SYMBOL_ID next_node = candidates[i]->object;

        if (IsInPath(path->step_nodes, depth + 1, next_node))
            continue;

        path->step_predicates[depth] = es_sym;

        float next_conf = current_confidence * candidates[i]->weight * config->decay_factor;

        if (DFSProve(graph, next_node, target_pred, target_obj, next_conf, depth + 1, config, path))
            return 1;
    }

    return 0;
}

int InferenceProve(
    GRAPH *graph,
    SYMBOL_ID subject,
    SYMBOL_ID target_predicate,
    SYMBOL_ID target_object,
    const INFERENCE_CONFIG *config,
    INFERENCE_PATH *out_path)
{
    if (!graph || subject == SYMBOL_INVALID || target_predicate == SYMBOL_INVALID ||
        target_object == SYMBOL_INVALID || !out_path)
        return 0;

    INFERENCE_CONFIG cfg = config ? *config : InferenceConfigDefault();
    memset(out_path, 0, sizeof(INFERENCE_PATH));

    return DFSProve(graph, subject, target_predicate, target_object, 1.0f, 0, &cfg, out_path);
}

uint32_t InferenceApplyCompositionRule(
    GRAPH *graph,
    const COMPOSITION_RULE *rule,
    const INFERENCE_CONFIG *config)
{
    if (!graph || !rule)
        return 0;

    INFERENCE_CONFIG cfg = config ? *config : InferenceConfigDefault();
    uint32_t count = RelationCount(graph->relations);
    uint32_t inferred_total = 0;

    for (uint32_t i = 0; i < count; i++)
    {
        const RELATION *r1 = RelationGet(graph->relations, i);
        if (!r1 || r1->predicate != rule->pred_first)
            continue;

        SYMBOL_ID a = r1->subject;
        SYMBOL_ID b = r1->object;

        RELATION *second_hop[32];
        uint32_t n2 = RelationFindBySubjectPredicate(graph->relations, b, rule->pred_second, second_hop, 32);

        for (uint32_t j = 0; j < n2; j++)
        {
            SYMBOL_ID c = second_hop[j]->object;
            if (a == c)
                continue;

            RELATION *existing = RelationFind(graph->relations, a, rule->pred_result, c);
            if (existing == NULL)
            {
                float conf = r1->weight * second_hop[j]->weight * rule->rule_weight * cfg.decay_factor;
                if (conf >= cfg.min_confidence)
                {
                    GraphAddRelation(graph, a, rule->pred_result, c);
                    RELATION *new_r = RelationFind(graph->relations, a, rule->pred_result, c);
                    if (new_r)
                        new_r->weight = conf;
                    inferred_total++;
                }
            }
        }
    }

    return inferred_total;
}

uint32_t InferenceMaterializeTransitive(
    GRAPH *graph,
    SYMBOL_ID predicate,
    const INFERENCE_CONFIG *config)
{
    COMPOSITION_RULE rule;
    rule.pred_first = predicate;
    rule.pred_second = predicate;
    rule.pred_result = predicate;
    rule.rule_weight = 0.95f;

    uint32_t total = 0;
    uint32_t iter = 0;
    INFERENCE_CONFIG cfg = config ? *config : InferenceConfigDefault();

    while (iter < cfg.max_depth)
    {
        uint32_t added = InferenceApplyCompositionRule(graph, &rule, &cfg);
        if (added == 0)
            break;
        total += added;
        iter++;
    }

    return total;
}

void InferencePrintExplanation(const GRAPH *graph, const INFERENCE_PATH *path)
{
    if (!graph || !path || path->depth == 0)
    {
        printf("Sin traza de inferencia disponible.\n");
        return;
    }

    printf("Demostracion logica (Saltos: %u, Confianza: %.1f%%):\n",
           path->depth, path->accumulated_confidence * 100.0f);

    for (uint32_t i = 0; i < path->depth; i++)
    {
        const SYMBOL *s = SymbolGet(graph->symbols, path->step_nodes[i]);
        const SYMBOL *p = SymbolGet(graph->symbols, path->step_predicates[i]);
        const SYMBOL *o = SymbolGet(graph->symbols, path->step_nodes[i + 1]);

        printf("  Paso %u: %s --%s--> %s\n",
               i + 1,
               s ? s->name : "?",
               p ? p->name : "?",
               o ? o->name : "?");
    }
}
