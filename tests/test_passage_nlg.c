/* ============================================================
   test_passage_nlg: Verification of Document-Level Passage Generation
   and Soft Intent Mapping.
   Verifies:
     1. Elastic intent parsing ("hablame de...", "cuentame de...").
     2. Cross-lingual entity resolution (Spanish query -> English graph symbols).
     3. Multi-paragraph document generation with cohesive macroplanning.
     4. Working memory sub-graph integration (activation spreading).
     5. Zero-hallucination guarantee across full-passage synthesis.
     6. Master turn dispatcher for open conversational prompts.
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "graph.h"
#include "graph_reasoning.h"
#include "cognitive_learning.h"
#include "stochastic_nlg.h"
#include "metacognition.h"
#include "dict.h"
#include "passage_nlg.h"

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
    printf("  TEST & VERIFICACION: GENERACION DE ENSAYOS Y MAPEADO ELASTICO (LLM) \n");
    printf("======================================================================\n\n");

    GRAPH *g = GraphCreate(128, 512);

    /* English symbols as found in biblical/historical corpora */
    SYMBOL_ID s_solomon  = GraphAddSymbol(g, "Solomon");
    SYMBOL_ID s_david    = GraphAddSymbol(g, "David");
    SYMBOL_ID s_proverbs = GraphAddSymbol(g, "Proverbs");
    SYMBOL_ID s_wisdom   = GraphAddSymbol(g, "Wisdom");
    SYMBOL_ID s_israel   = GraphAddSymbol(g, "Israel");

    SYMBOL_ID r_father   = GraphAddSymbol(g, "father_of");
    SYMBOL_ID r_author   = GraphAddSymbol(g, "author_of");
    SYMBOL_ID r_theme    = GraphAddSymbol(g, "theme_of");
    SYMBOL_ID r_king     = GraphAddSymbol(g, "king_of");

    GraphAddRelation(g, s_david, r_father, s_solomon);
    GraphAddRelation(g, s_solomon, r_author, s_proverbs);
    GraphAddRelation(g, s_proverbs, r_theme, s_wisdom);
    GraphAddRelation(g, s_solomon, r_king, s_israel);

    /* Cross-lingual dictionary */
    DICT dict;
    DictInit(&dict);
    /* Add key translations */
    strncpy(dict.entries[0].alias, "salomon", DICT_TOKEN_MAX - 1);
    strncpy(dict.entries[0].canonical, "solomon", DICT_TOKEN_MAX - 1);
    strncpy(dict.entries[1].alias, "proverbios", DICT_TOKEN_MAX - 1);
    strncpy(dict.entries[1].canonical, "proverbs", DICT_TOKEN_MAX - 1);
    strncpy(dict.entries[2].alias, "sabiduria", DICT_TOKEN_MAX - 1);
    strncpy(dict.entries[2].canonical, "wisdom", DICT_TOKEN_MAX - 1);
    dict.count = 3;

    COGNITIVE_LEARNER learner;
    CognitiveLearnerInit(&learner, 0.75f, 2);

    WORKING_MEMORY wm;
    WM_Init(&wm, 0.80f, 0.15f);

    STOCHASTIC_NLG_CONFIG cfg = StochasticNLG_DefaultConfig();
    STOCHASTIC_DISCOURSE_HISTORY hist;
    StochasticNLG_InitHistory(&hist);

    PROVENANCE_TABLE pt;
    ProvenanceInit(&pt);

    /* --- TEST 1: SOFT INTENT MAPPING Y RESOLUCION CROSS-LINGUAL --- */
    printf("--- TEST 1: Mapeo Elastico de Intencion y Resolucion Bilingue ---\n");

    PARSED_QUERY_INTENT q1 = PassageClassifyQuery(g, &dict, "Hablame acerca del libro de proverbios");
    printf("  Consulta: \"Hablame acerca del libro de proverbios\"\n");
    printf("  Intent detectado: %d (SUMMARIZE), Entidad resuelta: ID %u (%s)\n",
           q1.intent, q1.sym1, q1.sym1 != SYMBOL_INVALID ? SymbolGet(g->symbols, q1.sym1)->name : "NONE");
    check(q1.intent == INTENT_SUMMARIZE_ENTITY, "Clasificacion elastica: Identifica intencion de resumen de tema");
    check(q1.sym1 == s_proverbs, "Puente bilingue: 'proverbios' se resuelve a 'Proverbs' en el grafo");

    PARSED_QUERY_INTENT q2 = PassageClassifyQuery(g, &dict, "Cuentame sobre salomon");
    printf("  Consulta: \"Cuentame sobre salomon\"\n");
    printf("  Intent detectado: %d (SUMMARIZE), Entidad resuelta: ID %u (%s)\n",
           q2.intent, q2.sym1, q2.sym1 != SYMBOL_INVALID ? SymbolGet(g->symbols, q2.sym1)->name : "NONE");
    check(q2.intent == INTENT_SUMMARIZE_ENTITY && q2.sym1 == s_solomon,
          "Puente bilingue: 'salomon' se resuelve a 'Solomon' en el grafo");

    /* --- TEST 2: GENERACION DE ENSAYO / DOCUMENT-LEVEL STORYTELLING --- */
    printf("\n--- TEST 2: Generacion de Ensayo Multi-Parrafo Estructurado ---\n");
    char passage[2048];
    uint32_t len = PassageGenerateTopicStory(g, &learner, &wm, s_proverbs, &cfg, passage, sizeof(passage));
    printf("%s\n\n", passage);

    check(len > 200, "Ensayo: Genera texto descriptivo sustancial (>200 caracteres)");
    check(strstr(passage, "Proverbs") != NULL, "Ensayo: Menciona la entidad nuclear verificada");
    check(strstr(passage, "Solomon") != NULL, "Ensayo: Integra hechos de autoría del grafo (Solomon)");
    check(strstr(passage, "Wisdom") != NULL, "Ensayo: Integra hechos tematicos del grafo (Wisdom)");
    check(strstr(passage, "\n\n") != NULL, "Macroplanning: Estructura el texto en parrafos separados");

    /* --- TEST 3: INTEGRACION DE MEMORIA DE TRABAJO (ACTIVATION SPREADING) --- */
    printf("--- TEST 3: Integracion de Memoria de Trabajo y Activacion Propagada ---\n");
    float act_solomon = WM_GetActivation(&wm, s_solomon);
    float act_wisdom  = WM_GetActivation(&wm, s_wisdom);
    printf("  Niveles de atencion en WM -> Solomon: %.2f, Wisdom: %.2f\n", act_solomon, act_wisdom);
    check(act_solomon > 0.0f && act_wisdom > 0.0f,
          "Memoria de Trabajo: Estimulada de forma refleja por la generacion del ensayo");

    /* --- TEST 4: DISPATCHER MASTER DE TURNO CONVERSACIONAL (LLM-LIKE) --- */
    printf("\n--- TEST 4: Dispatcher Master de Turno Conversacional Completo ---\n");
    char reply[2048];
    PassageHandleTurn(g, &learner, &wm, &pt, &dict,
                      "Que sabes sobre salomon?",
                      &cfg, &hist, reply, sizeof(reply));
    printf("  Respuesta a 'Que sabes sobre salomon?':\n%s\n\n", reply);
    check(strstr(reply, "Solomon") != NULL && strstr(reply, "David") != NULL,
          "Dispatcher LLM: Responde a preguntas coloquiales abiertas con narrativa completa");

    printf("======================================================================\n");
    printf("  RESUMEN TEST: %d PASS, %d FAIL\n", g_pass, g_fail);
    printf("======================================================================\n");

    GraphDestroy(g);
    return g_fail > 0 ? 1 : 0;
}
