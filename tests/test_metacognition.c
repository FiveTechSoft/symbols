/* ============================================================
   test_metacognition: Verification of Working Memory & Epistemic Self-Auditing.
   Verifies:
     1. Working memory stimulation and capacity bounds.
     2. Multi-hop activation spreading along relational topology.
     3. Temporal decay and attention focus pruning.
     4. Provenance tracking with mathematical robustness propagation.
     5. Metacognitive self-justification ("Why do I believe this?").
     6. Weakest link analysis in multi-step deduction trees.
     7. Hypothetical loss & counterfactual collapse cascade.
     8. Global epistemic health audit.
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "graph.h"
#include "graph_reasoning.h"
#include "metacognition.h"

static int g_pass = 0;
static int g_fail = 0;

static void check(int condition, const char *name)
{
    if (condition)
    {
        printf("  [PASS] %s\n", name);
        g_pass++;
    }
    else
    {
        printf("  [FAIL] %s\n", name);
        g_fail++;
    }
}

int main(void)
{
    printf("======================================================================\n");
    printf("  TEST & VERIFICACION: MEMORIA DE TRABAJO Y AUTO-AUDITORIA METACOGNITIVA\n");
    printf("======================================================================\n\n");

    GRAPH *g = GraphCreate(128, 512);

    SYMBOL_ID s_david    = GraphAddSymbol(g, "david");
    SYMBOL_ID s_betsabe  = GraphAddSymbol(g, "betsabe");
    SYMBOL_ID s_salomon  = GraphAddSymbol(g, "salomon");
    SYMBOL_ID s_roboam   = GraphAddSymbol(g, "roboam");
    SYMBOL_ID s_abia     = GraphAddSymbol(g, "abia");
    SYMBOL_ID s_egipto   = GraphAddSymbol(g, "egipto");

    SYMBOL_ID r_esposa   = GraphAddSymbol(g, "esposa_de");
    SYMBOL_ID r_padre    = GraphAddSymbol(g, "padre_de");
    SYMBOL_ID r_abuelo   = GraphAddSymbol(g, "abuelo_de");
    SYMBOL_ID r_antepas  = GraphAddSymbol(g, "antepasado_de");

    /* Graph topology */
    GraphAddRelation(g, s_david, r_esposa, s_betsabe);
    GraphAddRelation(g, s_david, r_padre, s_salomon);
    GraphAddRelation(g, s_salomon, r_padre, s_roboam);
    GraphAddRelation(g, s_roboam, r_padre, s_abia);

    /* --- FASE 1: MEMORIA DE TRABAJO Y PROPAGACION DE ACTIVACION --- */
    printf("--- FASE 1: Memoria de Trabajo y Propagacion de Activacion (Spreading Activation) ---\n");
    WORKING_MEMORY wm;
    WM_Init(&wm, 0.80f, 0.15f);

    /* 1.1 Estimulacion inicial */
    WM_Stimulate(&wm, s_david, 1.0f);
    check(WM_GetActivation(&wm, s_david) == 1.0f, "Estimulacion: David recibe 1.0 de energia en memoria de trabajo");
    check(WM_GetActivation(&wm, s_salomon) == 0.0f, "Salomon inicialmente inactivo (0.0)");

    /* 1.2 Propagacion de energia a lo largo del grafo relacional */
    uint32_t act_count = WM_SpreadActivation(&wm, g, 0.60f, 2);
    printf("  Activacion propagada: %u eventos de transmision relacional.\n", act_count);

    float act_betsabe = WM_GetActivation(&wm, s_betsabe);
    float act_salomon = WM_GetActivation(&wm, s_salomon);
    float act_roboam  = WM_GetActivation(&wm, s_roboam);
    float act_egipto  = WM_GetActivation(&wm, s_egipto);

    printf("  Nivel de activacion -> Betsabe (1 hop): %.2f\n", act_betsabe);
    printf("  Nivel de activacion -> Salomon (1 hop): %.2f\n", act_salomon);
    printf("  Nivel de activacion -> Roboam  (2 hops): %.2f\n", act_roboam);
    printf("  Nivel de activacion -> Egipto (desconectado): %.2f\n", act_egipto);

    check(act_salomon >= 0.50f, "1-hop: Salomon activado por conexion directa con David");
    check(act_betsabe >= 0.50f, "1-hop: Betsabe activada por conexion directa con David");
    check(act_roboam >= 0.30f, "2-hops: Roboam activado por propagacion en segundo orden");
    check(act_egipto == 0.0f, "Aislamiento: Egipto desconectado permanece con 0.0 de activacion");

    /* 1.3 Foco de atencion y decaimiento temporal */
    SYMBOL_ID top_syms[4];
    float top_acts[4];
    uint32_t n_top = WM_GetTopActive(&wm, top_syms, top_acts, 4);
    check(n_top >= 3 && top_syms[0] == s_david, "Foco de atencion: David es el concepto dominante en memoria de trabajo");

    WM_StepTurn(&wm);
    check(WM_GetActivation(&wm, s_david) == 0.80f, "Decaimiento: Energia decae segun decay_rate (1.00 -> 0.80)");
    WM_StepTurn(&wm); /* 0.64 */
    WM_StepTurn(&wm); /* 0.512 */
    WM_StepTurn(&wm); /* 0.4096 */
    WM_StepTurn(&wm); /* 0.327 */
    WM_StepTurn(&wm); /* 0.262 */
    WM_StepTurn(&wm); /* 0.209 */
    WM_StepTurn(&wm); /* 0.167 */
    WM_StepTurn(&wm); /* 0.134 < threshold(0.15) -> Pruned! */
    check(WM_GetActivation(&wm, s_roboam) == 0.0f, "Poda: Nodos que caen bajo el umbral son desalojados de la memoria de trabajo");

    /* --- FASE 2: PROCEDENCIA EPISTEMICA Y METACOGNICION --- */
    printf("\n--- FASE 2: Procedencia Epistemica y Auto-Auditoria Metacognitiva ---\n");
    PROVENANCE_TABLE pt;
    ProvenanceInit(&pt);

    /* 2.1 Registro de axiomas base observados */
    int32_t idx_ax1 = ProvenanceRecordAxiom(&pt, s_david, r_padre, s_salomon, 1.0f);
    int32_t idx_ax2 = ProvenanceRecordAxiom(&pt, s_salomon, r_padre, s_roboam, 1.0f);
    int32_t idx_ax3 = ProvenanceRecordAxiom(&pt, s_roboam, r_padre, s_abia, 0.85f); /* Premisa con menor certeza */

    check(idx_ax1 >= 0 && idx_ax2 >= 0 && idx_ax3 >= 0, "Registro de axiomas base con procedencia formal");

    /* 2.2 Deduccion hacia adelante con propagacion de robustez */
    /* Regla 1: padre o padre => abuelo (confianza 0.95) */
    GRAPH_RULE_BASE rb;
    GraphRuleBaseInit(&rb, 0.70f, 1);
    rb.rules[0].type = RULE_TYPE_COMPOSITION;
    rb.rules[0].r1 = r_padre;
    rb.rules[0].r2 = r_padre;
    rb.rules[0].head = r_abuelo;
    rb.rules[0].confidence = 0.95f;
    strncpy(rb.rules[0].name, "padre_de o padre_de => abuelo_de", sizeof(rb.rules[0].name) - 1);
    rb.num_rules = 1;

    int32_t p1[2] = { idx_ax1, idx_ax2 };
    int32_t idx_ded1 = ProvenanceRecordDeduction(&pt, s_david, r_abuelo, s_roboam, p1, 2, 0, 0.95f);
    /* Robustez esperada: 0.95 * 1.0 * 1.0 = 0.95 */
    check(idx_ded1 >= 0 && pt.entries[idx_ded1].robustness == 0.95f,
          "Deduccion 1: Robustez calculada con rigor matematico (0.95)");

    /* Deduccion 2 (multi-hop profundo): abuelo o padre => antepasado (confianza 0.90) */
    int32_t p2[2] = { idx_ded1, idx_ax3 };
    int32_t idx_ded2 = ProvenanceRecordDeduction(&pt, s_david, r_antepas, s_abia, p2, 2, 0, 0.90f);
    /* Robustez esperada: 0.90 * 0.95 * 0.85 = 0.72675 */
    float expected_rob2 = 0.90f * 0.95f * 0.85f;
    check(fabs(pt.entries[idx_ded2].robustness - expected_rob2) < 0.001f,
          "Deduccion 2 (multi-hop): Propagacion multiplicativa exacta a traves del DAG");

    /* 2.3 Auto-Justificacion Metacognitiva: ¿Por que creo esto? */
    printf("\n--- Justificacion Metacognitiva de Creencia ---\n");
    char justification[1024];
    MetacognitiveExplainBelief(g, &pt, &rb, s_david, r_abuelo, s_roboam, justification, sizeof(justification));
    printf("%s\n\n", justification);
    check(strstr(justification, "se dedujo mediante la regla") != NULL &&
          (strstr(justification, "david") != NULL || strstr(justification, "David") != NULL) &&
          (strstr(justification, "roboam") != NULL || strstr(justification, "Roboam") != NULL) &&
          strstr(justification, "0.95") != NULL,
          "Metacognicion: Genera el arbol recursivo de justificacion formal con axiomas");

    /* 2.4 Analisis del Eslabon Mas Debil */
    char weakest_desc[256];
    float min_score = MetacognitiveFindWeakestLink(g, &pt, s_david, r_antepas, s_abia, weakest_desc, sizeof(weakest_desc));
    printf("  Eslabon mas debil detectado: \"%s\" (Score: %.2f)\n", weakest_desc, min_score);
    check(fabs(min_score - 0.72675f) < 0.01f || strstr(weakest_desc, "Roboam") != NULL || strstr(weakest_desc, "regla") != NULL,
          "Metacognicion: Identifica la vulnerabilidad critica en cadenas complejas de deduccion");

    /* 2.5 Analisis de Impacto Contrafactual: ¿Que colapsa si refuto X? */
    printf("\n--- Analisis de Perdida Hipotetica / Cascada Contrafactual ---\n");
    int32_t impacted[16];
    /* Si refutamos el axioma 1 [David padre Salomon]: deben colapsar ded1 (abuelo) y ded2 (antepasado) */
    uint32_t n_impacted = MetacognitiveAuditHypotheticalLoss(&pt, s_david, r_padre, s_salomon, impacted, 16);
    printf("  Refutando hipoteticamente [David padre_de Salomon]...\n");
    printf("  Creencias deducidas que colapsan en cascada: %u\n", n_impacted);
    for (uint32_t i = 0; i < n_impacted; i++)
    {
        const PROVENANCE_ENTRY *ie = &pt.entries[impacted[i]];
        const char *is = SymbolGet(g->symbols, ie->subject)->name;
        const char *ir = SymbolGet(g->symbols, ie->relation)->name;
        const char *io = SymbolGet(g->symbols, ie->object)->name;
        printf("    -> Colapsa: [%s %s %s] (Robustez previa: %.2f)\n", is, ir, io, ie->robustness);
    }
    check(n_impacted == 2, "Cascada contrafactual: Detecta exactamente las 2 creencias dependientes que colapsan");

    /* 2.6 Auditoria de Salud Epistemica Global */
    EPISTEMIC_HEALTH_REPORT health = MetacognitiveAuditGraphHealth(&pt);
    printf("\n--- Auditoria de Salud Epistemica Global ---\n");
    printf("  Total creencias: %u\n", health.total_beliefs);
    printf("  Axiomas directos: %u\n", health.observed_axioms);
    printf("  Deducciones activas: %u\n", health.deduced_beliefs);
    printf("  Robustez promedio: %.2f\n", health.avg_robustness);
    printf("  Creencias vulnerables (<0.70): %u\n", health.high_vulnerability_count);
    check(health.total_beliefs == 5 && health.observed_axioms == 3 && health.deduced_beliefs == 2,
          "Auditoria de salud: Cuantifica con precision los componentes ontologicos del sistema");

    printf("\n======================================================================\n");
    printf("  RESUMEN TEST: %d PASS, %d FAIL\n", g_pass, g_fail);
    printf("======================================================================\n");

    GraphDestroy(g);
    return g_fail > 0 ? 1 : 0;
}
