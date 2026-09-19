/* ============================================================
   metacognition: Working Memory, Activation Spreading & Epistemic Self-Auditing.
   Pure C11, zero tensors, zero backprop, fail-closed verification.

   English code comments (project rule); localized text in format strings.
   HARDCODING=0: All structural parameters and relational weights are dynamic.
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "metacognition.h"

/* Helper to resolve symbol names safely */
static const char *GetSymName(const GRAPH *graph, SYMBOL_ID id, const char *def)
{
    if (!graph || !graph->symbols) return def;
    const SYMBOL *s = SymbolGet(graph->symbols, id);
    if (!s || !s->name || s->name[0] == '\0') return def;
    return s->name;
}

/* ---- 1. WORKING MEMORY & ACTIVATION SPREADING ---- */

void WM_Init(WORKING_MEMORY *wm, float decay_rate, float threshold)
{
    if (!wm) return;
    memset(wm, 0, sizeof(*wm));
    wm->decay_rate = (decay_rate > 0.0f && decay_rate < 1.0f) ? decay_rate : 0.85f;
    wm->threshold  = (threshold > 0.0f) ? threshold : 0.10f;
    wm->current_turn = 0;
}

void WM_Stimulate(WORKING_MEMORY *wm, SYMBOL_ID symbol, float energy)
{
    if (!wm || symbol == SYMBOL_INVALID || energy <= 0.0f) return;

    /* Check if symbol already exists in working memory */
    for (uint32_t i = 0; i < wm->count; i++)
    {
        if (wm->nodes[i].symbol == symbol)
        {
            wm->nodes[i].activation += energy;
            if (wm->nodes[i].activation > 1.0f)
                wm->nodes[i].activation = 1.0f;
            wm->nodes[i].last_turn = wm->current_turn;
            return;
        }
    }

    /* Not found: insert new node if capacity allows */
    if (wm->count < MAX_WM_NODES)
    {
        wm->nodes[wm->count].symbol = symbol;
        wm->nodes[wm->count].activation = (energy > 1.0f) ? 1.0f : energy;
        wm->nodes[wm->count].last_turn = wm->current_turn;
        wm->count++;
        return;
    }

    /* Table full: replace the node with the lowest activation */
    uint32_t min_idx = 0;
    float min_act = wm->nodes[0].activation;
    for (uint32_t i = 1; i < wm->count; i++)
    {
        if (wm->nodes[i].activation < min_act)
        {
            min_act = wm->nodes[i].activation;
            min_idx = i;
        }
    }

    if (energy > min_act)
    {
        wm->nodes[min_idx].symbol = symbol;
        wm->nodes[min_idx].activation = (energy > 1.0f) ? 1.0f : energy;
        wm->nodes[min_idx].last_turn = wm->current_turn;
    }
}

uint32_t WM_SpreadActivation(
    WORKING_MEMORY *wm,
    const GRAPH *graph,
    float spread_factor,
    uint32_t max_hops)
{
    if (!wm || !graph || wm->count == 0 || spread_factor <= 0.0f || max_hops == 0)
        return 0;

    uint32_t total_activated = 0;

    for (uint32_t hop = 0; hop < max_hops; hop++)
    {
        /* Buffer new activations to prevent in-place feedback loops during traversal */
        typedef struct { SYMBOL_ID sym; float energy; } DELTA_ENERGY;
        DELTA_ENERGY deltas[MAX_WM_NODES * 4];
        uint32_t num_deltas = 0;

        uint32_t cur_count = wm->count;
        for (uint32_t i = 0; i < cur_count; i++)
        {
            float src_act = wm->nodes[i].activation;
            if (src_act < wm->threshold) continue;

            RELATION *out_rels[16];
            uint32_t n = GraphQuerySubject((GRAPH *)graph, wm->nodes[i].symbol, out_rels, 16);
            for (uint32_t r = 0; r < n; r++)
            {
                if (out_rels[r]->polarity == POLARITY_NEGATIVE) continue;
                SYMBOL_ID target = out_rels[r]->object;
                if (target == SYMBOL_INVALID) continue;

                float rel_w = (out_rels[r]->weight > 0.0f) ? out_rels[r]->weight : 1.0f;
                float delta = src_act * spread_factor * rel_w;

                if (delta >= wm->threshold && num_deltas < (MAX_WM_NODES * 4))
                {
                    deltas[num_deltas].sym = target;
                    deltas[num_deltas].energy = delta;
                    num_deltas++;
                }
            }

            /* Also spread activation along incoming relations (bidirectional cognitive association) */
            if (graph->relations)
            {
                for (uint32_t r = 0; r < graph->relations->count; r++)
                {
                    if (graph->relations->items[r].object == wm->nodes[i].symbol &&
                        graph->relations->items[r].polarity == POLARITY_POSITIVE)
                    {
                        SYMBOL_ID target = graph->relations->items[r].subject;
                        if (target == SYMBOL_INVALID) continue;

                        float rel_w = (graph->relations->items[r].weight > 0.0f) ? graph->relations->items[r].weight : 1.0f;
                        float delta = src_act * spread_factor * rel_w;

                        if (delta >= wm->threshold && num_deltas < (MAX_WM_NODES * 4))
                        {
                            deltas[num_deltas].sym = target;
                            deltas[num_deltas].energy = delta;
                            num_deltas++;
                        }
                    }
                }
            }
        }

        /* Apply deltas */
        for (uint32_t d = 0; d < num_deltas; d++)
        {
            WM_Stimulate(wm, deltas[d].sym, deltas[d].energy);
            total_activated++;
        }
    }

    return total_activated;
}

void WM_StepTurn(WORKING_MEMORY *wm)
{
    if (!wm) return;
    wm->current_turn++;

    /* Apply decay and filter out sub-threshold nodes */
    uint32_t write_idx = 0;
    for (uint32_t i = 0; i < wm->count; i++)
    {
        wm->nodes[i].activation *= wm->decay_rate;
        if (wm->nodes[i].activation >= wm->threshold)
        {
            if (write_idx != i)
            {
                wm->nodes[write_idx] = wm->nodes[i];
            }
            write_idx++;
        }
    }
    wm->count = write_idx;
}

uint32_t WM_GetTopActive(
    const WORKING_MEMORY *wm,
    SYMBOL_ID *out_symbols,
    float *out_activations,
    uint32_t max_k)
{
    if (!wm || !out_symbols || max_k == 0) return 0;

    /* Copy active nodes */
    WM_NODE sorted[MAX_WM_NODES];
    uint32_t n = wm->count;
    memcpy(sorted, wm->nodes, n * sizeof(WM_NODE));

    /* Simple selection sort (n <= 64) */
    for (uint32_t i = 0; i < n; i++)
    {
        uint32_t max_i = i;
        for (uint32_t j = i + 1; j < n; j++)
        {
            if (sorted[j].activation > sorted[max_i].activation)
                max_i = j;
        }
        if (max_i != i)
        {
            WM_NODE tmp = sorted[i];
            sorted[i] = sorted[max_i];
            sorted[max_i] = tmp;
        }
    }

    uint32_t count = (n < max_k) ? n : max_k;
    for (uint32_t i = 0; i < count; i++)
    {
        out_symbols[i] = sorted[i].symbol;
        if (out_activations)
            out_activations[i] = sorted[i].activation;
    }
    return count;
}

float WM_GetActivation(const WORKING_MEMORY *wm, SYMBOL_ID symbol)
{
    if (!wm || symbol == SYMBOL_INVALID) return 0.0f;
    for (uint32_t i = 0; i < wm->count; i++)
    {
        if (wm->nodes[i].symbol == symbol)
            return wm->nodes[i].activation;
    }
    return 0.0f;
}

/* ---- 2. EPISTEMIC PROVENANCE & METACOGNITION ---- */

void ProvenanceInit(PROVENANCE_TABLE *pt)
{
    if (!pt) return;
    memset(pt, 0, sizeof(*pt));
}

int32_t ProvenanceFind(
    const PROVENANCE_TABLE *pt,
    SYMBOL_ID s, SYMBOL_ID r, SYMBOL_ID o)
{
    if (!pt) return -1;
    for (uint32_t i = 0; i < pt->count; i++)
    {
        if (pt->entries[i].subject == s &&
            pt->entries[i].relation == r &&
            pt->entries[i].object == o)
        {
            return (int32_t)i;
        }
    }
    return -1;
}

int32_t ProvenanceRecordAxiom(
    PROVENANCE_TABLE *pt,
    SYMBOL_ID s, SYMBOL_ID r, SYMBOL_ID o,
    float initial_confidence)
{
    if (!pt) return -1;
    int32_t idx = ProvenanceFind(pt, s, r, o);
    if (idx >= 0)
    {
        pt->entries[idx].robustness = initial_confidence;
        return idx;
    }

    if (pt->count >= MAX_PROV_ENTRIES) return -1;

    idx = (int32_t)pt->count++;
    PROVENANCE_ENTRY *e = &pt->entries[idx];
    memset(e, 0, sizeof(*e));
    e->subject = s;
    e->relation = r;
    e->object = o;
    e->origin = PROV_AXIOM;
    e->robustness = (initial_confidence > 1.0f) ? 1.0f : (initial_confidence < 0.0f ? 0.0f : initial_confidence);
    e->rule_idx = -1;
    e->rule_conf = 1.0f;
    return idx;
}

int32_t ProvenanceRecordDeduction(
    PROVENANCE_TABLE *pt,
    SYMBOL_ID s, SYMBOL_ID r, SYMBOL_ID o,
    const int32_t *premise_indices,
    uint32_t num_premises,
    int32_t rule_idx,
    float rule_confidence)
{
    if (!pt) return -1;
    int32_t idx = ProvenanceFind(pt, s, r, o);
    if (idx < 0)
    {
        if (pt->count >= MAX_PROV_ENTRIES) return -1;
        idx = (int32_t)pt->count++;
    }

    PROVENANCE_ENTRY *e = &pt->entries[idx];
    memset(e, 0, sizeof(*e));
    e->subject = s;
    e->relation = r;
    e->object = o;
    e->origin = PROV_DEDUCED;
    e->rule_idx = rule_idx;
    e->rule_conf = rule_confidence;

    float r_cum = rule_confidence;
    uint32_t n = (num_premises < MAX_PREMISES) ? num_premises : MAX_PREMISES;
    e->num_premises = n;

    for (uint32_t p = 0; p < n; p++)
    {
        e->premises[p] = (uint32_t)premise_indices[p];
        if (premise_indices[p] >= 0 && (uint32_t)premise_indices[p] < pt->count)
        {
            r_cum *= pt->entries[premise_indices[p]].robustness;
        }
    }
    e->robustness = r_cum;
    return idx;
}

int32_t ProvenanceRecordAssimilated(
    PROVENANCE_TABLE *pt,
    SYMBOL_ID s, SYMBOL_ID r, SYMBOL_ID o,
    float user_confidence)
{
    if (!pt) return -1;
    int32_t idx = ProvenanceFind(pt, s, r, o);
    if (idx < 0)
    {
        if (pt->count >= MAX_PROV_ENTRIES) return -1;
        idx = (int32_t)pt->count++;
    }

    PROVENANCE_ENTRY *e = &pt->entries[idx];
    memset(e, 0, sizeof(*e));
    e->subject = s;
    e->relation = r;
    e->object = o;
    e->origin = PROV_ASSIMILATED;
    e->robustness = user_confidence;
    e->rule_idx = -1;
    e->rule_conf = user_confidence;
    return idx;
}

uint32_t MetacognitiveExplainBelief(
    const GRAPH *graph,
    const PROVENANCE_TABLE *pt,
    const GRAPH_RULE_BASE *rb,
    SYMBOL_ID subject,
    SYMBOL_ID relation,
    SYMBOL_ID object,
    char *out,
    size_t out_size)
{
    if (!graph || !pt || !out || out_size == 0) return 0;

    int32_t idx = ProvenanceFind(pt, subject, relation, object);
    if (idx < 0)
    {
        snprintf(out, out_size, "No existe constancia ni registro de procedencia para este hecho.");
        return (uint32_t)strlen(out);
    }

    const PROVENANCE_ENTRY *e = &pt->entries[idx];
    const char *s = GetSymName(graph, e->subject, "S");
    const char *r = GetSymName(graph, e->relation, "R");
    const char *o = GetSymName(graph, e->object, "O");

    if (e->origin == PROV_AXIOM)
    {
        snprintf(out, out_size,
                 "El hecho [%s %s %s] es un axioma observado directamente en el corpus (robustez: %.2f).",
                 s, r, o, e->robustness);
    }
    else if (e->origin == PROV_ASSIMILATED)
    {
        snprintf(out, out_size,
                 "El hecho [%s %s %s] fue asimilado como evidencia externa tras indagacion activa (robustez: %.2f).",
                 s, r, o, e->robustness);
    }
    else if (e->origin == PROV_DEDUCED)
    {
        char rule_desc[128] = "regla deductiva";
        if (rb && e->rule_idx >= 0 && (uint32_t)e->rule_idx < rb->num_rules)
        {
            GraphRuleFormat(graph, &rb->rules[e->rule_idx], rule_desc, sizeof(rule_desc));
        }

        size_t written = (size_t)snprintf(
            out, out_size,
            "El hecho [%s %s %s] se dedujo mediante la regla [%s] (confianza: %.2f) a partir de %u premisas comprobadas:\n",
            s, r, o, rule_desc, e->rule_conf, e->num_premises);

        for (uint32_t p = 0; p < e->num_premises; p++)
        {
            uint32_t p_idx = e->premises[p];
            if (p_idx < pt->count && written < out_size - 1)
            {
                const PROVENANCE_ENTRY *pe = &pt->entries[p_idx];
                const char *ps = GetSymName(graph, pe->subject, "pS");
                const char *pr = GetSymName(graph, pe->relation, "pR");
                const char *po = GetSymName(graph, pe->object, "pO");

                char p_buf[256];
                snprintf(p_buf, sizeof(p_buf),
                         "  - Premisa %u: [%s %s %s] (origen: %s, robustez: %.2f)\n",
                         p + 1, ps, pr, po,
                         (pe->origin == PROV_AXIOM) ? "observacion" :
                         (pe->origin == PROV_ASSIMILATED ? "asimilacion" : "deduccion"),
                         pe->robustness);

                size_t add = strlen(p_buf);
                if (written + add < out_size - 1)
                {
                    memcpy(out + written, p_buf, add);
                    written += add;
                    out[written] = '\0';
                }
            }
        }

        char footer[128];
        snprintf(footer, sizeof(footer), "Robustez epistemica global acumulada: %.2f.", e->robustness);
        size_t f_len = strlen(footer);
        if (written + f_len < out_size - 1)
        {
            memcpy(out + written, footer, f_len);
            written += f_len;
            out[written] = '\0';
        }
    }
    else
    {
        snprintf(out, out_size, "El hecho [%s %s %s] es una hipotesis abductiva (robustez: %.2f).",
                 s, r, o, e->robustness);
    }

    return (uint32_t)strlen(out);
}

/* Recursive helper to search for the weakest premise/rule in the derivation tree */
static void FindWeakestRec(
    const PROVENANCE_TABLE *pt,
    uint32_t entry_idx,
    uint8_t *visited,
    float *min_score,
    int32_t *weakest_entry)
{
    if (entry_idx >= pt->count || visited[entry_idx]) return;
    visited[entry_idx] = 1;

    const PROVENANCE_ENTRY *e = &pt->entries[entry_idx];

    /* Compare current entry */
    if (e->robustness < *min_score)
    {
        *min_score = e->robustness;
        *weakest_entry = (int32_t)entry_idx;
    }

    /* Compare rule confidence if deduced */
    if (e->origin == PROV_DEDUCED && e->rule_conf < *min_score)
    {
        *min_score = e->rule_conf;
        *weakest_entry = (int32_t)entry_idx;
    }

    /* Recurse into premises */
    for (uint32_t p = 0; p < e->num_premises; p++)
    {
        FindWeakestRec(pt, e->premises[p], visited, min_score, weakest_entry);
    }
}

float MetacognitiveFindWeakestLink(
    const GRAPH *graph,
    const PROVENANCE_TABLE *pt,
    SYMBOL_ID subject,
    SYMBOL_ID relation,
    SYMBOL_ID object,
    char *out_weakest_desc,
    size_t desc_size)
{
    if (!graph || !pt) return 0.0f;

    int32_t idx = ProvenanceFind(pt, subject, relation, object);
    if (idx < 0)
    {
        if (out_weakest_desc && desc_size > 0)
            snprintf(out_weakest_desc, desc_size, "Hecho no registrado.");
        return 0.0f;
    }

    uint8_t visited[MAX_PROV_ENTRIES] = {0};
    float min_score = 1.0f;
    int32_t weakest_idx = idx;

    FindWeakestRec(pt, (uint32_t)idx, visited, &min_score, &weakest_idx);

    if (out_weakest_desc && desc_size > 0 && weakest_idx >= 0 && (uint32_t)weakest_idx < pt->count)
    {
        const PROVENANCE_ENTRY *we = &pt->entries[weakest_idx];
        const char *ws = GetSymName(graph, we->subject, "S");
        const char *wr = GetSymName(graph, we->relation, "R");
        const char *wo = GetSymName(graph, we->object, "O");

        if (weakest_idx == idx && we->origin == PROV_DEDUCED)
        {
            snprintf(out_weakest_desc, desc_size,
                     "La regla de derivacion para [%s %s %s] con confianza %.2f",
                     ws, wr, wo, we->rule_conf);
        }
        else
        {
            snprintf(out_weakest_desc, desc_size,
                     "Premisa [%s %s %s] con robustez %.2f",
                     ws, wr, wo, we->robustness);
        }
    }

    return min_score;
}

uint32_t MetacognitiveAuditHypotheticalLoss(
    const PROVENANCE_TABLE *pt,
    SYMBOL_ID subject,
    SYMBOL_ID relation,
    SYMBOL_ID object,
    int32_t *out_impacted_indices,
    uint32_t max_impacted)
{
    if (!pt) return 0;

    int32_t target_idx = ProvenanceFind(pt, subject, relation, object);
    if (target_idx < 0) return 0;

    uint8_t collapsed[MAX_PROV_ENTRIES] = {0};
    collapsed[target_idx] = 1;

    /* Multi-pass cascade to identify all forward-dependent deductions */
    int changed = 1;
    while (changed)
    {
        changed = 0;
        for (uint32_t i = 0; i < pt->count; i++)
        {
            if (collapsed[i]) continue;
            if (pt->entries[i].origin != PROV_DEDUCED) continue;

            for (uint32_t p = 0; p < pt->entries[i].num_premises; p++)
            {
                uint32_t parent_idx = pt->entries[i].premises[p];
                if (parent_idx < pt->count && collapsed[parent_idx])
                {
                    collapsed[i] = 1;
                    changed = 1;
                    break;
                }
            }
        }
    }

    /* Collect all impacted entries (excluding the target itself) */
    uint32_t count = 0;
    for (uint32_t i = 0; i < pt->count; i++)
    {
        if (collapsed[i] && (int32_t)i != target_idx)
        {
            if (out_impacted_indices && count < max_impacted)
            {
                out_impacted_indices[count] = (int32_t)i;
            }
            count++;
        }
    }

    return count;
}

EPISTEMIC_HEALTH_REPORT MetacognitiveAuditGraphHealth(const PROVENANCE_TABLE *pt)
{
    EPISTEMIC_HEALTH_REPORT report;
    memset(&report, 0, sizeof(report));

    if (!pt || pt->count == 0) return report;

    report.total_beliefs = pt->count;
    float sum_rob = 0.0f;

    for (uint32_t i = 0; i < pt->count; i++)
    {
        const PROVENANCE_ENTRY *e = &pt->entries[i];
        sum_rob += e->robustness;

        switch (e->origin)
        {
            case PROV_AXIOM:        report.observed_axioms++; break;
            case PROV_DEDUCED:      report.deduced_beliefs++; break;
            case PROV_ASSIMILATED:  report.assimilated_beliefs++; break;
            default: break;
        }

        if (e->robustness < 0.70f)
        {
            report.high_vulnerability_count++;
        }
    }

    report.avg_robustness = sum_rob / (float)pt->count;
    return report;
}
