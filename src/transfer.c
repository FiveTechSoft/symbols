#include <stdio.h>
#include <string.h>
#include "transfer.h"

/* ============================================================
   Helper: check if entity has a specific relation-object relation
   ============================================================ */
static int has_relation(GRAPH *g, SYMBOL_ID subj, SYMBOL_ID rel, SYMBOL_ID obj)
{
    RELATION *results[4];
    uint32_t n = GraphQuerySubjectRelation(g, subj, rel, results, 4);
    for (uint32_t i = 0; i < n; i++)
    {
        if (obj == SYMBOL_INVALID || results[i]->object == obj)
            return 1;
    }
    return 0;
}

/* ============================================================
   Helper: get all objects for a given subject+relation
   ============================================================ */
static uint32_t get_objects(GRAPH *g, SYMBOL_ID subj, SYMBOL_ID rel,
                            SYMBOL_ID *out, uint32_t max)
{
    RELATION *results[32];
    uint32_t n = GraphQuerySubjectRelation(g, subj, rel, results, 32 > max ? max : 32);
    uint32_t count = 0;
    for (uint32_t i = 0; i < n && count < max; i++)
        out[count++] = results[i]->object;
    return count;
}

/* ============================================================
   Helper: get all relations for a subject
   ============================================================ */
static uint32_t get_relations(GRAPH *g, SYMBOL_ID subj,
                               SYMBOL_ID *out, uint32_t max)
{
    RELATION *results[32];
    uint32_t n = GraphQuerySubject(g, subj, results, 32 > max ? max : 32);
    uint32_t count = 0;
    for (uint32_t i = 0; i < n && count < max; i++)
    {
        /* Deduplicate relations */
        int found = 0;
        for (uint32_t j = 0; j < count; j++)
        {
            if (out[j] == results[i]->relation) { found = 1; break; }
        }
        if (!found) out[count++] = results[i]->relation;
    }
    return count;
}

/* ============================================================
   TransferSimilarity: structural similarity between two entities
   ============================================================ */
float TransferSimilarity(GRAPH *graph, SYMBOL_ID a, SYMBOL_ID b)
{
    if (!graph || a == SYMBOL_INVALID || b == SYMBOL_INVALID)
        return 0.0f;

    /* Get relations for both */
    SYMBOL_ID rels_a[32], rels_b[32];
    uint32_t na = get_relations(graph, a, rels_a, 32);
    uint32_t nb = get_relations(graph, b, rels_b, 32);

    if (na == 0 || nb == 0) return 0.0f;

    /* Count shared relations */
    uint32_t shared = 0;
    for (uint32_t i = 0; i < na; i++)
    {
        for (uint32_t j = 0; j < nb; j++)
        {
            if (rels_a[i] == rels_b[j]) { shared++; break; }
        }
    }

    /* Jaccard similarity */
    uint32_t total = na + nb - shared;
    return total > 0 ? (float)shared / (float)total : 0.0f;
}

/* ============================================================
   TransferAnalogy: what A knows that B could learn by pattern
   ============================================================ */
uint32_t TransferAnalogy(GRAPH *graph, SYMBOL_ID source, SYMBOL_ID target,
                         TRANSFER_RESULT *results, uint32_t max_results)
{
    if (!graph) return 0;

    uint32_t found = 0;

    /* Find shared relations between source and target */
    SYMBOL_ID rels_s[32], rels_t[32];
    uint32_t ns = get_relations(graph, source, rels_s, 32);
    uint32_t nt = get_relations(graph, target, rels_t, 32);

    /* For each shared relation, compare objects */
    for (uint32_t i = 0; i < ns && found < max_results; i++)
    {
        for (uint32_t j = 0; j < nt; j++)
        {
            if (rels_s[i] != rels_t[j]) continue;

            /* Same relation — compare objects */
            SYMBOL_ID objs_s[8], objs_t[8];
            uint32_t os = get_objects(graph, source, rels_s[i], objs_s, 8);
            uint32_t ot = get_objects(graph, target, rels_t[j], objs_t, 8);

            /* Find objects that source has but target doesn't */
            for (uint32_t a = 0; a < os && found < max_results; a++)
            {
                int has = 0;
                for (uint32_t b = 0; b < ot; b++)
                {
                    if (objs_s[a] == objs_t[b]) { has = 1; break; }
                }

                if (!has)
                {
                    const SYMBOL *ps = SymbolGet(graph->symbols, rels_s[i]);
                    const SYMBOL *osym = SymbolGet(graph->symbols, objs_s[a]);
                    if (!ps || !osym) continue;

                    strncpy(results[found].rule_name, "ANALOGY_TRANSFER", 63);
                    results[found].source_entity = source;
                    results[found].target_entity = target;
                    results[found].inferred_rel = rels_s[i];
                    results[found].inferred_obj = objs_s[a];
                    results[found].confidence = 0.60f;
                    found++;
                }
            }
        }
    }

    return found;
}

/* ============================================================
   TransferPrintResults
   ============================================================ */
void TransferPrintResults(const GRAPH *graph, const TRANSFER_RESULT *results, uint32_t count)
{
    if (count == 0)
    {
        printf("  No new relations inferred.\n");
        return;
    }

    printf("  Inferred %u new relations:\n\n", count);
    for (uint32_t i = 0; i < count; i++)
    {
        const SYMBOL *src = SymbolGet(graph->symbols, results[i].source_entity);
        const SYMBOL *tgt = SymbolGet(graph->symbols, results[i].target_entity);
        const SYMBOL *rel = SymbolGet(graph->symbols, results[i].inferred_rel);
        const SYMBOL *obj = SymbolGet(graph->symbols, results[i].inferred_obj);

        printf("  [%s] (conf=%.0f%%)\n", results[i].rule_name, results[i].confidence * 100);
        if (src && rel && obj)
            printf("    %s --%s--> %s\n", src->name, rel->name, obj->name);
        if (tgt && tgt != src && rel && obj)
            printf("    (applied to: %s)\n", tgt->name);
        printf("\n");
    }
}
