/* ============================================================
   demo_conversation: Interactive & Staged Multi-Turn Conversation
   evaluating the full cognitive stack:
     - Knowledge retrieval & Ingestion
     - Spreading activation working memory
     - Non-deterministic natural language generation
     - Active epistemic inquiry (curiosity)
     - Metacognitive self-auditing ("Why do you believe this?")
     - Counterfactual vulnerability analysis
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "graph.h"
#include "graph_reasoning.h"
#include "cognitive_learning.h"
#include "stochastic_nlg.h"
#include "metacognition.h"

int main(void)
{
    printf("================================================================================\n");
    printf("     SESION CONVERSACIONAL CON SYMBOLIC COGNITIVE ENGINE (C11 PURO)            \n");
    printf("================================================================================\n\n");

    /* 1. Inicializar grafo y base de conocimiento */
    GRAPH *g = GraphCreate(256, 1024);
    COGNITIVE_LEARNER learner;
    CognitiveLearnerInit(&learner, 0.75f, 2);

    WORKING_MEMORY wm;
    WM_Init(&wm, 0.80f, 0.15f);

    PROVENANCE_TABLE pt;
    ProvenanceInit(&pt);

    STOCHASTIC_NLG_CONFIG cfg = StochasticNLG_DefaultConfig();
    cfg.temperature = 0.75f;
    cfg.lang = LANG_ES;

    STOCHASTIC_DISCOURSE_HISTORY hist;
    StochasticNLG_InitHistory(&hist);

    /* Hechos iniciales: Historia del Reino de Israel y Biologia */
    SYMBOL_ID s_david     = GraphAddSymbol(g, "David");
    SYMBOL_ID s_salomon   = GraphAddSymbol(g, "Salomon");
    SYMBOL_ID s_roboam    = GraphAddSymbol(g, "Roboam");
    SYMBOL_ID s_israel    = GraphAddSymbol(g, "Israel");

    SYMBOL_ID s_ave       = GraphAddSymbol(g, "Ave");
    SYMBOL_ID s_halcon    = GraphAddSymbol(g, "Halcon");
    SYMBOL_ID s_pinguino  = GraphAddSymbol(g, "Pinguino");
    SYMBOL_ID s_cielo     = GraphAddSymbol(g, "Cielo");

    SYMBOL_ID r_padre     = GraphAddSymbol(g, "padre_de");
    SYMBOL_ID r_abuelo    = GraphAddSymbol(g, "abuelo_de");
    SYMBOL_ID r_rey       = GraphAddSymbol(g, "rey_de");
    SYMBOL_ID r_es_un     = GraphAddSymbol(g, "es_un");
    SYMBOL_ID r_vuela     = GraphAddSymbol(g, "vuela_en");

    /* Axiomas iniciales */
    GraphAddRelation(g, s_david, r_padre, s_salomon);
    ProvenanceRecordAxiom(&pt, s_david, r_padre, s_salomon, 1.0f);

    GraphAddRelation(g, s_salomon, r_padre, s_roboam);
    ProvenanceRecordAxiom(&pt, s_salomon, r_padre, s_roboam, 1.0f);

    GraphAddRelation(g, s_david, r_rey, s_israel);
    ProvenanceRecordAxiom(&pt, s_david, r_rey, s_israel, 1.0f);

    GraphAddRelation(g, s_halcon, r_es_un, s_ave);
    ProvenanceRecordAxiom(&pt, s_halcon, r_es_un, s_ave, 1.0f);

    GraphAddRelation(g, s_pinguino, r_es_un, s_ave);
    ProvenanceRecordAxiom(&pt, s_pinguino, r_es_un, s_ave, 1.0f);

    GraphAddRelation(g, s_ave, r_vuela, s_cielo);
    ProvenanceRecordAxiom(&pt, s_ave, r_vuela, s_cielo, 0.95f);

    /* Regla inducida: padre_de o padre_de => abuelo_de (confianza 0.98) */
    GRAPH_RULE rule_abuelo;
    memset(&rule_abuelo, 0, sizeof(rule_abuelo));
    rule_abuelo.type = RULE_TYPE_COMPOSITION;
    rule_abuelo.r1 = r_padre;
    rule_abuelo.r2 = r_padre;
    rule_abuelo.head = r_abuelo;
    rule_abuelo.confidence = 0.98f;
    strncpy(rule_abuelo.name, "padre_de o padre_de => abuelo_de", sizeof(rule_abuelo.name) - 1);
    learner.rule_base.rules[0] = rule_abuelo;
    learner.rule_base.num_rules = 1;

    /* Deduccion automatica: David abuelo_de Roboam */
    GraphAddRelation(g, s_david, r_abuelo, s_roboam);
    int32_t p_abuelo[2] = {
        ProvenanceFind(&pt, s_david, r_padre, s_salomon),
        ProvenanceFind(&pt, s_salomon, r_padre, s_roboam)
    };
    ProvenanceRecordDeduction(&pt, s_david, r_abuelo, s_roboam, p_abuelo, 2, 0, 0.98f);

    /* Excepcion registrada: Pinguino no vuela en cielo */
    CognitiveRegisterException(&learner, 0, s_pinguino, "Pinguino carece de adaptacion anatomica para vuelo aereo");

    /* Conversacion de 8 turnos */
    struct {
        const char *user_msg;
        int action_type; /* 1: query fact, 2: why, 3: curiosity query, 4: exception, 5: counterfactual */
        SYMBOL_ID s, r, o;
    } script[] = {
        { "Hola! Que relacion existe entre David y Salomon?", 1, s_david, r_padre, s_salomon },
        { "Confirmame de nuevo esa relacion.", 1, s_david, r_padre, s_salomon },
        { "Y que sabemos acerca de David y Roboam?", 1, s_david, r_abuelo, s_roboam },
        { "Por que sabes que David es abuelo de Roboam?", 2, s_david, r_abuelo, s_roboam },
        { "Que pasaria si el registro sobre Salomon y Roboam fuera falso?", 5, s_salomon, r_padre, s_roboam },
        { "Sabemos quien es nieto de Roboam?", 3, s_roboam, r_abuelo, GraphAddSymbol(g, "Abia") },
        { "Los pinguinos vuelan en el cielo?", 4, s_pinguino, r_vuela, s_cielo },
        { "Cual es el foco actual de tu memoria de trabajo?", 0, SYMBOL_INVALID, SYMBOL_INVALID, SYMBOL_INVALID }
    };

    char response[1024];

    for (int turn = 0; turn < 8; turn++)
    {
        printf("--------------------------------------------------------------------------------\n");
        printf("[TURNO %d] USUARIO: \"%s\"\n", turn + 1, script[turn].user_msg);

        /* 1. Actualizar memoria de trabajo (estimulacion del sujeto si aplica) */
        if (script[turn].s != SYMBOL_INVALID)
        {
            WM_Stimulate(&wm, script[turn].s, 1.0f);
            WM_SpreadActivation(&wm, g, 0.50f, 1);
        }

        /* 2. Generar respuesta segun el tipo cognitivo */
        response[0] = '\0';
        if (script[turn].action_type == 1)
        {
            /* Consulta directa con generacion estocastica no repetitiva */
            if (script[turn].r == r_abuelo)
            {
                StochasticNLG_InductiveAssertion(g, script[turn].s, script[turn].r, script[turn].o,
                                                 &rule_abuelo, &cfg, &hist, response, sizeof(response));
            }
            else
            {
                StochasticNLG_FactAssertion(g, script[turn].s, script[turn].r, script[turn].o,
                                            &cfg, &hist, response, sizeof(response));
            }
        }
        else if (script[turn].action_type == 2)
        {
            /* Metacognicion: Justificacion de creencia */
            MetacognitiveExplainBelief(g, &pt, &learner.rule_base, script[turn].s, script[turn].r, script[turn].o,
                                       response, sizeof(response));
        }
        else if (script[turn].action_type == 3)
        {
            /* Inquiracion epistemica activa (curiosidad) */
            EPISTEMIC_INQUIRY inq;
            memset(&inq, 0, sizeof(inq));
            inq.missing_subj = script[turn].s;
            inq.missing_rel = r_padre;
            inq.missing_obj = script[turn].o;
            StochasticNLG_ActiveInquiry(g, &inq, &cfg, &hist, response, sizeof(response));
        }
        else if (script[turn].action_type == 4)
        {
            /* Excepcion defeible */
            StochasticNLG_DefeasibleContrast(g, s_ave, r_vuela, s_cielo, script[turn].s,
                                             &cfg, &hist, response, sizeof(response));
        }
        else if (script[turn].action_type == 5)
        {
            /* Cascada contrafactual */
            int32_t impacted[8];
            uint32_t n = MetacognitiveAuditHypotheticalLoss(&pt, script[turn].s, script[turn].r, script[turn].o,
                                                            impacted, 8);
            char *w = response;
            size_t rem = sizeof(response);
            snprintf(w, rem, "Analisis contrafactual: Si refutamos [%s %s %s], colapsan %u deducciones dependientes:\n",
                     SymbolGet(g->symbols, script[turn].s)->name,
                     SymbolGet(g->symbols, script[turn].r)->name,
                     SymbolGet(g->symbols, script[turn].o)->name, n);
            for (uint32_t k = 0; k < n; k++)
            {
                PROVENANCE_ENTRY *pe = &pt.entries[impacted[k]];
                char line[128];
                snprintf(line, sizeof(line), "  -> Perderia sustento: [%s %s %s]\n",
                         SymbolGet(g->symbols, pe->subject)->name,
                         SymbolGet(g->symbols, pe->relation)->name,
                         SymbolGet(g->symbols, pe->object)->name);
                strncat(response, line, rem - strlen(response) - 1);
            }
        }
        else if (script[turn].action_type == 0)
        {
            /* Inspeccion de memoria de trabajo */
            SYMBOL_ID top_s[4];
            float top_a[4];
            uint32_t n = WM_GetTopActive(&wm, top_s, top_a, 4);
            char *w = response;
            size_t rem = sizeof(response);
            snprintf(w, rem, "Foco actual de atencion en memoria de trabajo (%u conceptos activos):\n", n);
            for (uint32_t k = 0; k < n; k++)
            {
                char line[128];
                snprintf(line, sizeof(line), "  - %s (energia: %.2f)\n",
                         SymbolGet(g->symbols, top_s[k])->name, top_a[k]);
                strncat(response, line, rem - strlen(response) - 1);
            }
        }

        printf("MOTOR: %s\n\n", response);

        /* Avanzar turno en memoria de trabajo (decaimiento) */
        WM_StepTurn(&wm);
    }

    GraphDestroy(g);
    return 0;
}
