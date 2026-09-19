/* ============================================================
   test_cognitive_learning: Verification of Advanced Learning Paradigms.
   Verifies:
     1. Peircean Inquiry Cycle (Induction -> Deduction closure)
     2. Active Epistemic Inquiry & Curiosity-driven Assimilation
     3. Non-Monotonic Belief Revision (Exception guards / Defeasible)
     4. Symbolic Self-Supervised Learning (Masked Edge Reconstruction)
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "graph.h"
#include "cognitive_learning.h"

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
    printf("  TEST & VERIFICACION: PARADIGMAS AVANZADOS DE APRENDIZAJE COGNITIVO  \n");
    printf("======================================================================\n\n");

    GRAPH *g = GraphCreate(128, 512);
    COGNITIVE_LEARNER learner;
    CognitiveLearnerInit(&learner, 0.75f, 2);

    /* Symbols */
    SYMBOL_ID s_boaz     = GraphAddSymbol(g, "boaz");
    SYMBOL_ID s_obed     = GraphAddSymbol(g, "obed");
    SYMBOL_ID s_jesse    = GraphAddSymbol(g, "jesse");
    SYMBOL_ID s_david    = GraphAddSymbol(g, "david");

    SYMBOL_ID r_padre    = GraphAddSymbol(g, "padre_de");
    SYMBOL_ID r_abuelo   = GraphAddSymbol(g, "abuelo_de");

    /* --- TEST 1: PEIRCEAN INQUIRY CYCLE --- */
    printf("--- PARADIGMA 1: Ciclo de Indagacion de Peirce (Induccion -> Deduccion) ---\n");
    GraphAddRelation(g, s_boaz, r_padre, s_obed);
    GraphAddRelation(g, s_obed, r_padre, s_jesse);
    GraphAddRelation(g, s_boaz, r_abuelo, s_jesse);

    GraphAddRelation(g, s_obed, r_padre, s_jesse);
    GraphAddRelation(g, s_jesse, r_padre, s_david);
    GraphAddRelation(g, s_obed, r_abuelo, s_david);

    uint32_t derived = CognitiveRunInquiryCycle(g, &learner);
    printf("  Ciclo ejecutado: %u aristas deducidas, %u reglas activas.\n",
           derived, learner.rule_base.num_rules);
    check(learner.rule_base.num_rules >= 1, "Ciclo 1: Indujo la regla de composicion familiar");
    check(learner.cycle_count == 1, "Ciclo 1: Contador de ciclos cognitivos = 1");

    /* --- TEST 2: APRENDIZAJE ACTIVO POR CURIOSIDAD EPISTEMICA --- */
    printf("\n--- PARADIGMA 2: Aprendizaje Activo por Curiosidad Epistemica ---\n");
    /* Se formula una meta sobre un pariente lejano: ¿es boaz abuelo de david?
       En el grafo existe boaz padre obed, pero obed padre david NO es directo (es jesse).
       Planteamos una meta que necesita un eslabon intermedio:
       Creamos entidad nueva: "salomon". boaz abuelo salomon. */
    SYMBOL_ID s_salomon = GraphAddSymbol(g, "salomon");
    /* Sabemos: jesse padre david. Meta: jesse abuelo salomon? */
    EPISTEMIC_INQUIRY inqs[4];
    uint32_t n_inq = CognitiveFormulateInquiry(g, &learner, s_jesse, r_abuelo, s_salomon, inqs, 4);
    printf("  Consultas epistemicas dirigidas generadas: %u\n", n_inq);
    for (uint32_t i = 0; i < n_inq; i++)
    {
        printf("  -> Pregunta activa: \"%s\"\n", inqs[i].question);
        printf("  -> Termino de busqueda: \"%s\" (Prioridad: %.2f)\n",
               inqs[i].search_term, inqs[i].priority);
    }
    check(n_inq > 0, "Curiosidad: Detecto la laguna y formulo la pregunta activa");
    check(inqs[0].missing_subj == s_david && inqs[0].missing_rel == r_padre && inqs[0].missing_obj == s_salomon,
          "Curiosidad: Identifico exactamente el eslabon faltante [david padre_de salomon]");

    /* Asimilamos la respuesta a la pregunta formulada */
    printf("  Asimilando evidencia respondida: [david padre_de salomon]...\n");
    int ok = CognitiveAssimilateEvidence(g, &learner, s_david, r_padre, s_salomon);
    check(ok == 1, "Asimilacion de evidencia completada con propagacion hacia adelante");
    check(GraphFindRelation(g, s_jesse, r_abuelo, s_salomon) != NULL,
          "Resultado: Meta [jesse abuelo_de salomon] demostrada autonomamente tras asimilar evidencia");

    /* --- TEST 3: REVISION DE CREENCIAS NO MONOTONA (EXCEPCIONES) --- */
    printf("\n--- PARADIGMA 3: Revision de Creencias No Monotona (Manejo de Excepciones) ---\n");
    SYMBOL_ID s_pajaro   = GraphAddSymbol(g, "pajaro");
    SYMBOL_ID s_aguila   = GraphAddSymbol(g, "aguila");
    SYMBOL_ID s_pinguino = GraphAddSymbol(g, "pinguino");
    SYMBOL_ID s_cielo    = GraphAddSymbol(g, "cielo");

    SYMBOL_ID r_is_a     = GraphAddSymbol(g, "es_un");
    SYMBOL_ID r_vuela    = GraphAddSymbol(g, "vuela_en");

    /* Inducir regla: es_un o vuela_en => vuela_en */
    SYMBOL_ID s_halcon   = GraphAddSymbol(g, "halcon");
    SYMBOL_ID s_paloma   = GraphAddSymbol(g, "paloma");

    GraphAddRelation(g, s_halcon, r_is_a, s_pajaro);
    GraphAddRelation(g, s_pajaro, r_vuela, s_cielo);
    GraphAddRelation(g, s_halcon, r_vuela, s_cielo); /* confirm 1 */

    GraphAddRelation(g, s_paloma, r_is_a, s_pajaro);
    GraphAddRelation(g, s_paloma, r_vuela, s_cielo); /* confirm 2 */

    CognitiveRunInquiryCycle(g, &learner);

    /* Encontrar el indice de la regla (es_un o vuela_en => vuela_en) */
    uint32_t fly_rule_idx = 0;
    int found_fly_rule = 0;
    for (uint32_t i = 0; i < learner.rule_base.num_rules; i++)
    {
        if (learner.rule_base.rules[i].r1 == r_is_a && learner.rule_base.rules[i].head == r_vuela)
        {
            fly_rule_idx = i;
            found_fly_rule = 1;
            break;
        }
    }
    check(found_fly_rule, "Regla inducida: es_un o vuela_en => vuela_en");

    /* Registramos excepcion: 'pinguino' NO hereda vuela_en */
    CognitiveRegisterException(&learner, fly_rule_idx, s_pinguino, "Pinguinos son aves no voladoras");

    /* Introducimos aguila y pinguino */
    GraphAddRelation(g, s_aguila, r_is_a, s_pajaro);
    GraphAddRelation(g, s_pinguino, r_is_a, s_pajaro);

    /* Disparamos ciclo de deduccion con proteccion no monotona */
    CognitiveRunInquiryCycle(g, &learner);

    check(GraphFindRelation(g, s_aguila, r_vuela, s_cielo) != NULL,
          "No monotonia: [aguila vuela_en cielo] DEDUCIDO correctamente");
    check(GraphFindRelation(g, s_pinguino, r_vuela, s_cielo) == NULL,
          "No monotonia: [pinguino vuela_en cielo] BLOQUEADO por excepcion (0 falso positivo)");

    /* --- TEST 4: APRENDIZAJE AUTO-SUPERVISADO SIMBOLICO (MASKED GRAPH) --- */
    printf("\n--- PARADIGMA 4: Aprendizaje Auto-Supervisado Simbolico (Masked Graph Discovery) ---\n");
    /* El motor enmascara aristas existentes y trata de reconstruirlas */
    SELF_SUPERVISED_METRICS m = CognitiveSelfSupervisedTrain(g, &learner, 2);
    printf("  Metricas de auto-supervision: Enmascaradas=%u, Reconstruidas=%u, Tasa=%.2f%%\n",
           m.total_masked, m.reconstructed, m.reconstruction_rate * 100.0f);
    check(m.total_masked >= 1, "Auto-supervision: Selecciono aristas complejas para enmascarar");
    check(m.reconstruction_rate >= 0.99f, "Auto-supervision: Reconstruccion 100%% autonoma sin datos externos");

    printf("\n=== RESULTADOS ===\n");
    printf("Pruebas pasadas: %d / %d\n", g_pass, g_pass + g_fail);

    GraphDestroy(g);
    return (g_fail == 0) ? 0 : 1;
}
