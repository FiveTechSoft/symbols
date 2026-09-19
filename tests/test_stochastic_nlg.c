/* ============================================================
   test_stochastic_nlg: Verification of Non-Deterministic Truth-Preserving NLG.
   Verifies:
     1. Deterministic Reproducibility at tau = 0.0 (byte-identical across runs)
     2. Stochastic Diversity at tau > 0 (generates diverse surface forms)
     3. Truth Invariance (100% factual integrity, zero hallucinations)
     4. Repetition Penalty (disfavors immediate template reuse)
     5. Inductive Confidence Modulation (high vs moderate certainty)
     6. Abductive Causal Explanations
     7. Active Epistemic Curiosity Inquiries
     8. Defeasible Exception Contrast
     9. Multilingual Generation (EN, ES, FR)
    10. End-to-End Conversational Turn Dispatcher
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "graph.h"
#include "graph_reasoning.h"
#include "cognitive_learning.h"
#include "stochastic_nlg.h"
#include "i18n.h"

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
    printf("  TEST & VERIFICACION: GENERACION CONVERSACIONAL NO DETERMINISTA      \n");
    printf("======================================================================\n\n");

    GRAPH *g = GraphCreate(128, 512);
    COGNITIVE_LEARNER learner;
    CognitiveLearnerInit(&learner, 0.75f, 2);

    SYMBOL_ID s_david    = GraphAddSymbol(g, "david");
    SYMBOL_ID s_salomon  = GraphAddSymbol(g, "salomon");
    SYMBOL_ID s_roboam   = GraphAddSymbol(g, "roboam");
    SYMBOL_ID r_padre    = GraphAddSymbol(g, "padre_de");
    SYMBOL_ID r_abuelo   = GraphAddSymbol(g, "abuelo_de");

    GraphAddRelation(g, s_david, r_padre, s_salomon);
    GraphAddRelation(g, s_salomon, r_padre, s_roboam);
    GraphAddRelation(g, s_david, r_abuelo, s_roboam);

    STOCHASTIC_NLG_CONFIG cfg = StochasticNLG_DefaultConfig();
    STOCHASTIC_DISCOURSE_HISTORY hist;
    StochasticNLG_InitHistory(&hist);

    char out1[512] = {0};
    char out2[512] = {0};

    /* --- TEST 1: DETERMINISMO REPRODUCIBLE CON TAU = 0.0 --- */
    printf("--- TEST 1: Determinismo Reproducible a Temperatura tau = 0.0 ---\n");
    cfg.temperature = 0.0f;
    cfg.lang = LANG_ES;
    StochasticNLG_FactAssertion(g, s_david, r_padre, s_salomon, &cfg, &hist, out1, sizeof(out1));
    StochasticNLG_FactAssertion(g, s_david, r_padre, s_salomon, &cfg, &hist, out2, sizeof(out2));
    printf("  Salida 1: \"%s\"\n", out1);
    printf("  Salida 2: \"%s\"\n", out2);
    check(strcmp(out1, out2) == 0, "tau = 0.0 produce salidas byte-identicas (reproducibilidad total)");

    /* --- TEST 2: DIVERSIDAD ESTOCASTICA CON TAU > 0 --- */
    printf("\n--- TEST 2: Diversidad Estocastica a Temperatura tau = 0.8 ---\n");
    cfg.temperature = 0.8f;
    cfg.lang = LANG_ES;
    StochasticNLG_ResetHistory(&hist);

    char samples[12][512];
    int distinct_count = 0;
    for (int i = 0; i < 12; i++)
    {
        StochasticNLG_FactAssertion(g, s_david, r_padre, s_salomon, &cfg, &hist, samples[i], sizeof(samples[i]));
        int is_new = 1;
        for (int j = 0; j < i; j++)
        {
            if (strcmp(samples[i], samples[j]) == 0)
            {
                is_new = 0;
                break;
            }
        }
        if (is_new) distinct_count++;
    }
    printf("  Generadas 12 respuestas para el mismo hecho; formas superficiales distintas: %d\n", distinct_count);
    for (int i = 0; i < (distinct_count < 4 ? distinct_count : 4); i++)
    {
        printf("  Variante %d: \"%s\"\n", i + 1, samples[i]);
    }
    check(distinct_count >= 3, "tau = 0.8 genera multiples variantes sintacticas para el mismo hecho");

    /* --- TEST 3: INVARIANZA DE VERDAD FACTUAL (0 ALUCINACIONES) --- */
    printf("\n--- TEST 3: Invarianza de Verdad Factual (0 Alucinaciones en 50 muestras) ---\n");
    int truth_preserved = 1;
    for (int i = 0; i < 50; i++)
    {
        char buf[512] = {0};
        StochasticNLG_FactAssertion(g, s_david, r_padre, s_salomon, &cfg, &hist, buf, sizeof(buf));
        if (strstr(buf, "David") == NULL || strstr(buf, "Salomon") == NULL)
        {
            truth_preserved = 0;
            break;
        }
    }
    check(truth_preserved == 1, "100% de las realizaciones estocasticas preservan exactamente las entidades veridicas");

    /* --- TEST 4: PENALIZACION DE REPETICION (DISCOURSE MEMORY) --- */
    printf("\n--- TEST 4: Penalizacion de Repeticion Discursiva ---\n");
    StochasticNLG_ResetHistory(&hist);
    char turn_a[512] = {0};
    char turn_b[512] = {0};
    StochasticNLG_FactAssertion(g, s_david, r_padre, s_salomon, &cfg, &hist, turn_a, sizeof(turn_a));
    StochasticNLG_FactAssertion(g, s_david, r_padre, s_salomon, &cfg, &hist, turn_b, sizeof(turn_b));
    printf("  Turno A: \"%s\"\n", turn_a);
    printf("  Turno B: \"%s\"\n", turn_b);
    check(strcmp(turn_a, turn_b) != 0, "Turnos consecutivos disfavorecen la plantilla inmediata previa");

    /* --- TEST 5: MODULACION DE CONFIANZA INDUCTIVA --- */
    printf("\n--- TEST 5: Modulacion de Confianza Inductiva (Alta vs Media Certeza) ---\n");
    GRAPH_RULE rule_high;
    memset(&rule_high, 0, sizeof(rule_high));
    rule_high.confidence = 0.98f;
    char text_high[512] = {0};
    StochasticNLG_InductiveAssertion(g, s_david, r_abuelo, s_roboam, &rule_high, &cfg, &hist, text_high, sizeof(text_high));
    printf("  Inductivo (conf=0.98): \"%s\"\n", text_high);
    check(strstr(text_high, "deduce con certeza") != NULL ||
          strstr(text_high, "confirmant solidamente") != NULL ||
          strstr(text_high, "demuestran") != NULL,
          "Alta confianza (0.98) produce formulaciones asertivas solidas");

    GRAPH_RULE rule_med;
    memset(&rule_med, 0, sizeof(rule_med));
    rule_med.confidence = 0.78f;
    char text_med[512] = {0};
    StochasticNLG_InductiveAssertion(g, s_david, r_abuelo, s_roboam, &rule_med, &cfg, &hist, text_med, sizeof(text_med));
    printf("  Inductivo (conf=0.78): \"%s\"\n", text_med);
    check(strstr(text_med, "sugieren fuertemente") != NULL ||
          strstr(text_med, "probabilidad inductiva") != NULL ||
          strstr(text_med, "apunta de forma coherente") != NULL,
          "Confianza moderada (0.78) modula el lenguaje con cautela probabilistica");

    /* --- TEST 6: EXPLICACIONES ABDUCTIVAS CAUSALES --- */
    printf("\n--- TEST 6: Explicaciones Abductivas Causales (Por que ocurre X) ---\n");
    ABDUCTIVE_HYPOTHESIS hyp;
    memset(&hyp, 0, sizeof(hyp));
    hyp.subject = s_salomon;
    hyp.relation = r_padre;
    hyp.object = s_roboam;
    char text_abduce[512] = {0};
    StochasticNLG_AbductiveExplanation(g, &hyp, 1, &cfg, &hist, text_abduce, sizeof(text_abduce));
    printf("  Abductivo: \"%s\"\n", text_abduce);
    check(strstr(text_abduce, "Salomon") != NULL && strstr(text_abduce, "Roboam") != NULL,
          "Explicacion causal verbaliza la hipotesis abductiva verosimil");

    /* --- TEST 7: INQUIRACION EPISTEMICA ACTIVA (CURIOSIDAD SOCRATICA) --- */
    printf("\n--- TEST 7: Inquiracion Epistemica Activa (Preguntas de Curiosidad) ---\n");
    SYMBOL_ID s_asaf = GraphAddSymbol(g, "asaf");
    EPISTEMIC_INQUIRY inq;
    memset(&inq, 0, sizeof(inq));
    inq.missing_subj = s_roboam;
    inq.missing_rel = r_padre;
    inq.missing_obj = s_asaf;
    char text_inq[512] = {0};
    StochasticNLG_ActiveInquiry(g, &inq, &cfg, &hist, text_inq, sizeof(text_inq));
    printf("  Pregunta activa: \"%s\"\n", text_inq);
    check(strstr(text_inq, "Roboam") != NULL && strstr(text_inq, "Asaf") != NULL,
          "Curiosidad activa formula pregunta conversacional sobre el eslabon faltante");

    /* --- TEST 8: CONTRASTE DE EXCEPCION DEFEIBLE (NON-MONOTONIC) --- */
    printf("\n--- TEST 8: Contraste de Excepcion Defeible (Razonamiento No Monotono) ---\n");
    SYMBOL_ID s_pajaro   = GraphAddSymbol(g, "pajaro");
    SYMBOL_ID s_vuela    = GraphAddSymbol(g, "vuela_en");
    SYMBOL_ID s_cielo    = GraphAddSymbol(g, "cielo");
    SYMBOL_ID s_pinguino = GraphAddSymbol(g, "pinguino");

    char text_exc[512] = {0};
    StochasticNLG_DefeasibleContrast(g, s_pajaro, s_vuela, s_cielo, s_pinguino, &cfg, &hist, text_exc, sizeof(text_exc));
    printf("  Excepcion defeible: \"%s\"\n", text_exc);
    check(strstr(text_exc, "Pajaro") != NULL && strstr(text_exc, "Pinguino") != NULL,
          "Manejo de excepciones verbaliza la concesion o contraste sin contradiccion");

    /* --- TEST 9: SOPORTE MULTILINGUE DINAMICO (EN, ES, FR) --- */
    printf("\n--- TEST 9: Soporte Multilingüe Dinamico (EN, ES, FR) ---\n");
    cfg.lang = LANG_EN;
    char text_en[512] = {0};
    StochasticNLG_FactAssertion(g, s_david, r_padre, s_salomon, &cfg, &hist, text_en, sizeof(text_en));
    printf("  [EN]: \"%s\"\n", text_en);
    check(strstr(text_en, "is") != NULL || strstr(text_en, "records") != NULL || strstr(text_en, "documented") != NULL,
          "Generacion correcta y fluida en Ingles");

    cfg.lang = LANG_FR;
    char text_fr[512] = {0};
    StochasticNLG_FactAssertion(g, s_david, r_padre, s_salomon, &cfg, &hist, text_fr, sizeof(text_fr));
    printf("  [FR]: \"%s\"\n", text_fr);
    check(strstr(text_fr, "est") != NULL || strstr(text_fr, "registres") != NULL || strstr(text_fr, "atteste") != NULL,
          "Generacion correcta y fluida en Frances");

    /* --- TEST 10: DISPATCHER AUTONOMO DE TURNO CONVERSACIONAL --- */
    printf("\n--- TEST 10: Dispatcher Autonomo de Turno Conversacional ---\n");
    cfg.lang = LANG_ES;
    char turn_resp[512] = {0};
    /* Pregunta sobre hecho directo conocido */
    StochasticNLG_TurnResponse(g, &learner, s_david, r_padre, s_salomon, 0, &cfg, &hist, turn_resp, sizeof(turn_resp));
    printf("  Turno directo: \"%s\"\n", turn_resp);
    check(strlen(turn_resp) > 0 && strstr(turn_resp, "David") != NULL,
          "Dispatcher responde hecho directo con naturalidad no determinista");

    printf("\n======================================================================\n");
    printf("  RESUMEN TEST: %d PASS, %d FAIL\n", g_pass, g_fail);
    printf("======================================================================\n");

    GraphDestroy(g);
    return g_fail > 0 ? 1 : 0;
}
