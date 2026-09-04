#include <stdio.h>
#include <string.h>
#include "transfer.h"

static GRAPH *g_graph = NULL;
static TRANSFER_RULE g_rules[MAX_TRANSFER_RULES];
static uint32_t g_rule_count = 0;

/* ============================================================
   Helper: check if entity has a specific predicate-object relation
   ============================================================ */
static int has_relation(GRAPH *g, SYMBOL_ID subj, SYMBOL_ID pred, SYMBOL_ID obj)
{
    RELATION *results[4];
    uint32_t n = GraphQuerySubjectPredicate(g, subj, pred, results, 4);
    for (uint32_t i = 0; i < n; i++)
    {
        if (obj == SYMBOL_INVALID || results[i]->object == obj)
            return 1;
    }
    return 0;
}

/* ============================================================
   Helper: get all objects for a given subject+predicate
   ============================================================ */
static uint32_t get_objects(GRAPH *g, SYMBOL_ID subj, SYMBOL_ID pred,
                            SYMBOL_ID *out, uint32_t max)
{
    RELATION *results[32];
    uint32_t n = GraphQuerySubjectPredicate(g, subj, pred, results, 32 > max ? max : 32);
    uint32_t count = 0;
    for (uint32_t i = 0; i < n && count < max; i++)
        out[count++] = results[i]->object;
    return count;
}

/* ============================================================
   Helper: get all predicates for a subject
   ============================================================ */
static uint32_t get_predicates(GRAPH *g, SYMBOL_ID subj,
                               SYMBOL_ID *out, uint32_t max)
{
    RELATION *results[32];
    uint32_t n = GraphQuerySubject(g, subj, results, 32 > max ? max : 32);
    uint32_t count = 0;
    for (uint32_t i = 0; i < n && count < max; i++)
    {
        /* Deduplicate predicates */
        int found = 0;
        for (uint32_t j = 0; j < count; j++)
        {
            if (out[j] == results[i]->predicate) { found = 1; break; }
        }
        if (!found) out[count++] = results[i]->predicate;
    }
    return count;
}

/* ============================================================
   Define all C programming transfer rules
   ============================================================ */
static void define_rules(void)
{
    g_rule_count = 0;

    /*
     * RULE 1: Memory write pattern
     * If function writes to memory AND has no size limit
     * → it probably REQUIRES careful buffer management
     */
    strncpy(g_rules[g_rule_count].name,
            "MEMWRITE_NEEDS_LIMITS", sizeof(g_rules[g_rule_count].name));
    g_rules[g_rule_count].required_pred[0] = 0; /* Will be resolved at runtime */
    g_rules[g_rule_count].required_count = 0;    /* Pattern-based, not literal */
    g_rules[g_rule_count].inferred_pred = 0;     /* Will be resolved at runtime */
    g_rules[g_rule_count].confidence = 0.85f;
    g_rule_count++;

    /*
     * RULE 2: Pointer return pattern
     * If function returns a pointer AND allocates memory
     * → it probably REQUIRES free()
     */
    strncpy(g_rules[g_rule_count].name,
            "ALLOC_NEEDS_FREE", sizeof(g_rules[g_rule_count].name));
    g_rules[g_rule_count].confidence = 0.95f;
    g_rule_count++;

    /*
     * RULE 3: String operation pattern
     * If function copies/modifies strings AND takes destination
     * → it probably can cause BUFFER_OVERFLOW if not bounded
     */
    strncpy(g_rules[g_rule_count].name,
            "STRCPY_UNBOUNDED_OVERFLOW", sizeof(g_rules[g_rule_count].name));
    g_rules[g_rule_count].confidence = 0.90f;
    g_rule_count++;

    /*
     * RULE 4: File operation pattern
     * If function opens a file
     * → it probably REQUIRES fclose()
     */
    strncpy(g_rules[g_rule_count].name,
            "FILE_NEEDS_CLOSE", sizeof(g_rules[g_rule_count].name));
    g_rules[g_rule_count].confidence = 0.95f;
    g_rule_count++;

    /*
     * RULE 5: Input parsing pattern
     * If function reads user input AND writes to buffer
     * → it probably VULNERABLE_TO format string attacks
     */
    strncpy(g_rules[g_rule_count].name,
            "INPUT_FORMAT_VULN", sizeof(g_rules[g_rule_count].name));
    g_rules[g_rule_count].confidence = 0.80f;
    g_rule_count++;

    /*
     * RULE 6: Numeric conversion pattern
     * If function converts strings to numbers
     * → it probably needs OVERFLOW_CHECK
     */
    strncpy(g_rules[g_rule_count].name,
            "ATOI_OVERFLOW", sizeof(g_rules[g_rule_count].name));
    g_rules[g_rule_count].confidence = 0.70f;
    g_rule_count++;
}

/* ============================================================
   TransferApply: Apply pattern-based rules to an entity
   ============================================================ */
uint32_t TransferApply(GRAPH *graph, SYMBOL_ID entity,
                       TRANSFER_RESULT *results, uint32_t max_results)
{
    if (!graph) return 0;
    g_graph = graph;

    if (g_rule_count == 0) define_rules();

    uint32_t found = 0;

    /* Get entity's predicates and objects */
    SYMBOL_ID preds[32];
    uint32_t npreds = get_predicates(graph, entity, preds, 32);

    SYMBOL_ID objs[32][8];
    uint32_t nobjs[32] = {0};
    for (uint32_t i = 0; i < npreds; i++)
        nobjs[i] = get_objects(graph, entity, preds[i], objs[i], 8);

    /* Resolve symbol IDs for pattern matching */
    SYMBOL_ID SID_RETORNA  = SymbolFind(graph->symbols, "RETORNA");
    SYMBOL_ID SID_PTR      = SymbolFind(graph->symbols, "PUNTERO_VOID");
    SYMBOL_ID SID_PTR2     = SymbolFind(graph->symbols, "PUNTERO");
    SYMBOL_ID SID_REQUIERE = SymbolFind(graph->symbols, "REQUIERE");
    SYMBOL_ID SID_FREE     = SymbolFind(graph->symbols, "FREE");
    SYMBOL_ID SID_COPIA    = SymbolFind(graph->symbols, "COPIA");
    SYMBOL_ID SID_DESTINO  = SymbolFind(graph->symbols, "DESTINO");
    SYMBOL_ID SID_ABRIR    = SymbolFind(graph->symbols, "ABRE");
    SYMBOL_ID SID_LEE      = SymbolFind(graph->symbols, "LEE");
    SYMBOL_ID SID_CONVIERTE = SymbolFind(graph->symbols, "CONVIERTE");
    SYMBOL_ID SID_ESCRIBE2 = SymbolFind(graph->symbols, "ESCRIBE");

    /* RULE 2: ALLOC_NEEDS_FREE
     * If RETORNA PUNTERO_VOID and doesn't already have REQUIERE FREE */
    {
        int has_alloc = 0;
        int has_require_free = 0;

        for (uint32_t i = 0; i < npreds; i++)
        {
            if (preds[i] == SID_RETORNA)
            {
                for (uint32_t j = 0; j < nobjs[i]; j++)
                {
                    if (objs[i][j] == SID_PTR || objs[i][j] == SID_PTR2)
                        has_alloc = 1;
                }
            }
            if (preds[i] == SID_REQUIERE)
            {
                for (uint32_t j = 0; j < nobjs[i]; j++)
                {
                    if (objs[i][j] == SID_FREE)
                        has_require_free = 1;
                }
            }
        }

        if (has_alloc && !has_require_free && found < max_results)
        {
            strncpy(results[found].rule_name, "ALLOC_NEEDS_FREE", 63);
            results[found].source_entity = entity;
            results[found].target_entity = entity;
            results[found].inferred_pred = SID_REQUIERE;
            results[found].inferred_obj = SID_FREE;
            results[found].confidence = 0.95f;
            found++;
        }
    }

    /* RULE 3: STRCPY_UNBOUNDED_OVERFLOW
     * If COPIA CADENA with DESTINO and no LIMITA */
    {
        int has_copia = 0;
        int has_destino = 0;
        int has_limita = 0;

        for (uint32_t i = 0; i < npreds; i++)
        {
            if (preds[i] == SID_COPIA) has_copia = 1;
            if (preds[i] == SID_DESTINO) has_destino = 1;

            /* Check for LIMITA predicate */
            const char *pname = "";
            const SYMBOL *ps = SymbolGet(graph->symbols, preds[i]);
            if (ps) pname = ps->name;
            if (strstr(pname, "LIMITA")) has_limita = 1;
        }

        if (has_copia && has_destino && !has_limita && found < max_results)
        {
            strncpy(results[found].rule_name, "STRCPY_UNBOUNDED_OVERFLOW", 63);
            results[found].source_entity = entity;
            results[found].target_entity = entity;
            results[found].inferred_pred = SymbolFind(graph->symbols, "VULNERABLE_A");
            results[found].inferred_obj = SymbolFind(graph->symbols, "BUFFER_OVERFLOW");
            results[found].confidence = 0.90f;
            found++;
        }
    }

    /* RULE 4: FILE_NEEDS_CLOSE
     * If ABRE ARCHIVO and no CIERRA */
    {
        int has_abre = 0;
        int has_cierra = 0;

        for (uint32_t i = 0; i < npreds; i++)
        {
            if (preds[i] == SID_ABRIR) has_abre = 1;

            const char *pname = "";
            const SYMBOL *ps = SymbolGet(graph->symbols, preds[i]);
            if (ps) pname = ps->name;
            if (strstr(pname, "CIERRA")) has_cierra = 1;
        }

        if (has_abre && !has_cierra && found < max_results)
        {
            strncpy(results[found].rule_name, "FILE_NEEDS_CLOSE", 63);
            results[found].source_entity = entity;
            results[found].target_entity = entity;
            results[found].inferred_pred = SID_REQUIERE;
            results[found].inferred_obj = SymbolFind(graph->symbols, "FCLOSE");
            results[found].confidence = 0.95f;
            found++;
        }
    }

    /* RULE 5: INPUT_FORMAT_VULN
     * If LEE ENTRADA and ESCRIBE to buffer */
    {
        int has_input = 0;
        int has_write_buf = 0;

        for (uint32_t i = 0; i < npreds; i++)
        {
            if (preds[i] == SID_LEE)
            {
                for (uint32_t j = 0; j < nobjs[i]; j++)
                {
                    const char *oname = "";
                    const SYMBOL *os = SymbolGet(graph->symbols, objs[i][j]);
                    if (os) oname = os->name;
                    if (strstr(oname, "ENTRADA") || strstr(oname, "STDIN"))
                        has_input = 1;
                }
            }
            if (preds[i] == SID_ESCRIBE2)
            {
                for (uint32_t j = 0; j < nobjs[i]; j++)
                {
                    const char *oname = "";
                    const SYMBOL *os = SymbolGet(graph->symbols, objs[i][j]);
                    if (os) oname = os->name;
                    if (strstr(oname, "BUFFER"))
                        has_write_buf = 1;
                }
            }
        }

        if (has_input && has_write_buf && found < max_results)
        {
            strncpy(results[found].rule_name, "INPUT_FORMAT_VULN", 63);
            results[found].source_entity = entity;
            results[found].target_entity = entity;
            results[found].inferred_pred = SymbolFind(graph->symbols, "VULNERABLE_A");
            results[found].inferred_obj = SymbolFind(graph->symbols, "FORMAT_STRING");
            results[found].confidence = 0.80f;
            found++;
        }
    }

    /* RULE 6: ATOI_OVERFLOW
     * If CONVIERTE CADENA_A_ENTERO */
    {
        int has_conv = 0;

        for (uint32_t i = 0; i < npreds; i++)
        {
            if (preds[i] == SID_CONVIERTE)
            {
                for (uint32_t j = 0; j < nobjs[i]; j++)
                {
                    const char *oname = "";
                    const SYMBOL *os = SymbolGet(graph->symbols, objs[i][j]);
                    if (os) oname = os->name;
                    if (strstr(oname, "ENTERO") || strstr(oname, "FLOAT"))
                        has_conv = 1;
                }
            }
        }

        if (has_conv && found < max_results)
        {
            strncpy(results[found].rule_name, "ATOI_OVERFLOW", 63);
            results[found].source_entity = entity;
            results[found].target_entity = entity;
            results[found].inferred_pred = SymbolFind(graph->symbols, "VULNERABLE_A");
            results[found].inferred_obj = SymbolFind(graph->symbols, "INTEGER_OVERFLOW");
            results[found].confidence = 0.70f;
            found++;
        }
    }

    return found;
}

/* ============================================================
   TransferSimilarity: structural similarity between two entities
   ============================================================ */
float TransferSimilarity(GRAPH *graph, SYMBOL_ID a, SYMBOL_ID b)
{
    if (!graph || a == SYMBOL_INVALID || b == SYMBOL_INVALID)
        return 0.0f;

    /* Get predicates for both */
    SYMBOL_ID preds_a[32], preds_b[32];
    uint32_t na = get_predicates(graph, a, preds_a, 32);
    uint32_t nb = get_predicates(graph, b, preds_b, 32);

    if (na == 0 || nb == 0) return 0.0f;

    /* Count shared predicates */
    uint32_t shared = 0;
    for (uint32_t i = 0; i < na; i++)
    {
        for (uint32_t j = 0; j < nb; j++)
        {
            if (preds_a[i] == preds_b[j]) { shared++; break; }
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

    /* Find shared predicates between source and target */
    SYMBOL_ID preds_s[32], preds_t[32];
    uint32_t ns = get_predicates(graph, source, preds_s, 32);
    uint32_t nt = get_predicates(graph, target, preds_t, 32);

    /* For each shared predicate, compare objects */
    for (uint32_t i = 0; i < ns && found < max_results; i++)
    {
        for (uint32_t j = 0; j < nt; j++)
        {
            if (preds_s[i] != preds_t[j]) continue;

            /* Same predicate — compare objects */
            SYMBOL_ID objs_s[8], objs_t[8];
            uint32_t os = get_objects(graph, source, preds_s[i], objs_s, 8);
            uint32_t ot = get_objects(graph, target, preds_t[j], objs_t, 8);

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
                    const SYMBOL *ps = SymbolGet(graph->symbols, preds_s[i]);
                    const SYMBOL *osym = SymbolGet(graph->symbols, objs_s[a]);
                    if (!ps || !osym) continue;

                    strncpy(results[found].rule_name, "ANALOGY_TRANSFER", 63);
                    results[found].source_entity = source;
                    results[found].target_entity = target;
                    results[found].inferred_pred = preds_s[i];
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
        printf("  No se pudieron inferir nuevas relaciones.\n");
        return;
    }

    printf("  Se infirieron %u relaciones nuevas:\n\n", count);
    for (uint32_t i = 0; i < count; i++)
    {
        const SYMBOL *src = SymbolGet(graph->symbols, results[i].source_entity);
        const SYMBOL *tgt = SymbolGet(graph->symbols, results[i].target_entity);
        const SYMBOL *pred = SymbolGet(graph->symbols, results[i].inferred_pred);
        const SYMBOL *obj = SymbolGet(graph->symbols, results[i].inferred_obj);

        printf("  [%s] (conf=%.0f%%)\n", results[i].rule_name, results[i].confidence * 100);
        if (src && pred && obj)
            printf("    %s --%s--> %s\n", src->name, pred->name, obj->name);
        if (tgt && tgt != src && pred && obj)
            printf("    (aplicado a: %s)\n", tgt->name);
        printf("\n");
    }
}
