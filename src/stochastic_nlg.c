/* ============================================================
   stochastic_nlg: Non-Deterministic Truth-Preserving Natural Language Generation.
   Pure C11, zero tensors, zero backprop, fail-closed truth preservation.

   English code comments (project rule); localized text only in templates.
   HARDCODING=0: Connectors and rhetorical frames are defined in
   declarative tables indexed by language, act, and confidence.
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "stochastic_nlg.h"

/* PRNG: Portable Xorshift32 for deterministic / stochastic generation */
static uint32_t NextRand(uint32_t *state)
{
    if (!state || *state == 0)
    {
        uint32_t dummy = 0x12345678;
        if (state) *state = dummy;
        else return dummy;
    }
    *state ^= *state << 13;
    *state ^= *state >> 17;
    *state ^= *state << 5;
    return *state;
}

static float NextFloat(uint32_t *state)
{
    return (float)(NextRand(state) & 0x00FFFFFF) / (float)0x01000000;
}

/* Safe token capitalization */
static void SafeCapitalize(const char *in, char *out, size_t out_size)
{
    if (!in || !out || out_size == 0) return;
    size_t i = 0;
    while (in[i] && i < out_size - 1)
    {
        out[i] = (i == 0) ? (char)toupper((unsigned char)in[i]) : in[i];
        i++;
    }
    out[i] = '\0';
}

/* Declarative rhetorical template group */
typedef struct
{
    const char *templates[STOCHASTIC_MAX_TEMPLATES];
    uint32_t   count;
} TEMPLATE_GROUP;

/* Complete rhetorical database for a given language */
typedef struct
{
    TEMPLATE_GROUP fact_assertions;
    TEMPLATE_GROUP inductive_high_conf; /* conf >= 0.90f */
    TEMPLATE_GROUP inductive_med_conf;  /* conf < 0.90f */
    TEMPLATE_GROUP abductive_single;
    TEMPLATE_GROUP abductive_multi_intro;
    TEMPLATE_GROUP active_inquiries;
    TEMPLATE_GROUP defeasible_contrast;
    const char     *abstain_unknown;
} RHETORICAL_DATABASE;

/* Declarative multilingual template database (HARDCODING=0) */
static const RHETORICAL_DATABASE g_rhetoric[LANG_COUNT] = {
    [LANG_EN] = {
        .fact_assertions = {
            .templates = {
                "%s is %s of %s.",
                "According to verified records, %s is %s of %s.",
                "It is directly documented that %s acts as %s regarding %s.",
                "Indeed, records confirm that %s is %s of %s."
            },
            .count = 4
        },
        .inductive_high_conf = {
            .templates = {
                "From the induced rules, it is deduced with certainty that %s %s %s.",
                "Observed relational patterns solidly confirm that %s %s %s.",
                "The structural laws of the graph establish that %s %s %s."
            },
            .count = 3
        },
        .inductive_med_conf = {
            .templates = {
                "Recurring evidence strongly suggests that %s %s %s.",
                "There is a high inductive likelihood that %s %s %s.",
                "Everything consistently points to %s %s %s."
            },
            .count = 3
        },
        .abductive_single = {
            .templates = {
                "The most direct causal explanation is that %s %s %s.",
                "This is grounded on the plausible hypothesis that %s %s %s.",
                "For this observation to hold, it is necessarily required that %s %s %s."
            },
            .count = 3
        },
        .abductive_multi_intro = {
            .templates = {
                "Multiple plausible hypotheses explain this goal: primarily that %s %s %s, or alternatively that %s %s %s.",
                "Evidence reveals alternative explanations: either %s %s %s, or otherwise %s %s %s."
            },
            .count = 2
        },
        .active_inquiries = {
            .templates = {
                "To delve deeper into this: is there any record of whether %s %s %s?",
                "It would be valuable to verify if %s %s %s to complete the deduction chain.",
                "An epistemic gap was identified: it would help to clarify whether %s %s %s.",
                "Could you confirm whether %s %s %s?"
            },
            .count = 4
        },
        .defeasible_contrast = {
            .templates = {
                "While generally %s %s %s, %s constitutes a documented exception.",
                "Unlike the standard rule for %s regarding %s %s, for %s it does not apply.",
                "For the general rule of %s (%s %s), it is verified that %s is a confirmed exception."
            },
            .count = 3
        },
        .abstain_unknown = "There is insufficient evidence in the records to answer regarding %s %s %s."
    },
    [LANG_ES] = {
        .fact_assertions = {
            .templates = {
                "%s es %s de %s.",
                "Segun los registros verificados, %s es %s de %s.",
                "Consta fehacientemente que %s actua como %s respecto a %s.",
                "Efectivamente, se confirma que %s es %s de %s."
            },
            .count = 4
        },
        .inductive_high_conf = {
            .templates = {
                "A partir de las reglas inducidas, se deduce con certeza que %s %s %s.",
                "Los patrones observados confirman solidamente que %s %s %s.",
                "Las leyes estructurales del grafo demuestran que %s %s %s."
            },
            .count = 3
        },
        .inductive_med_conf = {
            .templates = {
                "Las evidencias recurrentes sugieren fuertemente que %s %s %s.",
                "Existe una alta probabilidad inductiva de que %s %s %s.",
                "Todo apunta de forma coherente a que %s %s %s."
            },
            .count = 3
        },
        .abductive_single = {
            .templates = {
                "La explicacion causal mas directa es que %s %s %s.",
                "Esto se fundamenta en la hipotesis verosimil de que %s %s %s.",
                "Para que esto se cumpla, se requiere necesariamente que %s %s %s."
            },
            .count = 3
        },
        .abductive_multi_intro = {
            .templates = {
                "Existen varias hipotesis plausibles: primordialmente que %s %s %s, o alternativamente que %s %s %s.",
                "Se aprecian explicaciones alternativas: o bien que %s %s %s, o por otra parte que %s %s %s."
            },
            .count = 2
        },
        .active_inquiries = {
            .templates = {
                "Para profundizar en este punto: ¿se tiene constancia de si %s %s %s?",
                "Resultaria valioso verificar si %s %s %s para completar la cadena de deduccion.",
                "He identificado una laguna epistemica: convendria esclarecer si %s %s %s.",
                "¿Podrias confirmar si %s %s %s?"
            },
            .count = 4
        },
        .defeasible_contrast = {
            .templates = {
                "Si bien por norma general %s %s %s, %s constituye una excepcion documentada.",
                "A diferencia de la regla habitual para %s respecto a %s %s, en el caso de %s no se aplica.",
                "Para la regla general de %s (%s %s), se verifica que %s es una excepcion confirmada."
            },
            .count = 3
        },
        .abstain_unknown = "No tengo constancia suficiente en los registros para responder sobre %s %s %s."
    },
    [LANG_FR] = {
        .fact_assertions = {
            .templates = {
                "%s est %s de %s.",
                "Selon les registres verifies, %s est %s de %s.",
                "Il est directement atteste que %s agit comme %s concernant %s.",
                "En effet, les faits confirment que %s est %s de %s."
            },
            .count = 4
        },
        .inductive_high_conf = {
            .templates = {
                "A partir des regles induites, il est deduit avec certitude que %s %s %s.",
                "Les motifs observes confirment solidement que %s %s %s.",
                "Les lois structurelles du graphe etablissent que %s %s %s."
            },
            .count = 3
        },
        .inductive_med_conf = {
            .templates = {
                "Les preuves recurrentes suggerent fortement que %s %s %s.",
                "Il existe une forte probabilite inductive que %s %s %s.",
                "Tout converge de maniere coherente vers le fait que %s %s %s."
            },
            .count = 3
        },
        .abductive_single = {
            .templates = {
                "L'explication causale la plus directe est que %s %s %s.",
                "Ceci repose sur l'hypothese plausible que %s %s %s.",
                "Pour que cela soit vrai, il est necessairement requis que %s %s %s."
            },
            .count = 3
        },
        .abductive_multi_intro = {
            .templates = {
                "Plusieurs hypotheses plausibles expliquent ceci: principalement que %s %s %s, ou alternativement que %s %s %s.",
                "Des explications alternatives se presentent: soit %s %s %s, soit d'autre part %s %s %s."
            },
            .count = 2
        },
        .active_inquiries = {
            .templates = {
                "Pour approfondir ce point: existe-t-il une trace indiquant si %s %s %s?",
                "Il serait precieux de verifier si %s %s %s pour completer la chaine de deduction.",
                "Une lacune epistemique a ete identifiee: il conviendrait de clarifier si %s %s %s.",
                "Pourriez-vous confirmer si %s %s %s?"
            },
            .count = 4
        },
        .defeasible_contrast = {
            .templates = {
                "Bien que generalement %s %s %s, %s constitue une exception documentee.",
                "Contrairement a la regle habituelle pour %s concernant %s %s, pour %s cela ne s'applique pas.",
                "Pour la regle generale de %s (%s %s), il est verifie que %s est une exception confirmee."
            },
            .count = 3
        },
        .abstain_unknown = "Il n'y a pas suffisamment de preuves dans les registres pour repondre concernant %s %s %s."
    }
};

/* Default configuration */
STOCHASTIC_NLG_CONFIG StochasticNLG_DefaultConfig(void)
{
    STOCHASTIC_NLG_CONFIG cfg;
    cfg.temperature = 0.70f;
    cfg.repetition_penalty = 0.25f;
    cfg.rng_state = 0x5EED1234;
    cfg.lang = LangGet();
    return cfg;
}

void StochasticNLG_InitHistory(STOCHASTIC_DISCOURSE_HISTORY *hist)
{
    if (!hist) return;
    memset(hist, 0, sizeof(*hist));
}

void StochasticNLG_ResetHistory(STOCHASTIC_DISCOURSE_HISTORY *hist)
{
    StochasticNLG_InitHistory(hist);
}

/* Stochastic template sampling with repetition penalty */
static uint32_t SampleTemplateIndex(
    const TEMPLATE_GROUP *group,
    COG_DIALOGUE_ACT act,
    STOCHASTIC_NLG_CONFIG *cfg,
    STOCHASTIC_DISCOURSE_HISTORY *hist)
{
    if (!group || group->count <= 1) return 0;

    /* When temperature is 0, return deterministic argmax */
    if (cfg->temperature <= 0.001f)
    {
        return 0;
    }

    float weights[STOCHASTIC_MAX_TEMPLATES];
    float sum_weights = 0.0f;

    for (uint32_t i = 0; i < group->count; i++)
    {
        uint32_t recent_occurrences = 0;
        if (hist)
        {
            uint32_t limit = hist->counts[act] < STOCHASTIC_HISTORY_SIZE ?
                             hist->counts[act] : STOCHASTIC_HISTORY_SIZE;
            for (uint32_t h = 0; h < limit; h++)
            {
                if (hist->recent_templates[act][h] == (uint8_t)i)
                    recent_occurrences++;
            }
        }

        float penalty = 1.0f;
        for (uint32_t r = 0; r < recent_occurrences; r++)
            penalty *= cfg->repetition_penalty;

        float inv_tau = 1.0f / (cfg->temperature > 0.05f ? cfg->temperature : 0.05f);
        weights[i] = powf(penalty, inv_tau);
        sum_weights += weights[i];
    }

    if (sum_weights <= 0.00001f) return 0;

    float r = NextFloat(&cfg->rng_state) * sum_weights;
    float cum = 0.0f;
    uint32_t chosen = 0;
    for (uint32_t i = 0; i < group->count; i++)
    {
        cum += weights[i];
        if (r <= cum || i == group->count - 1)
        {
            chosen = i;
            break;
        }
    }

    if (hist)
    {
        uint32_t pos = hist->counts[act] % STOCHASTIC_HISTORY_SIZE;
        hist->recent_templates[act][pos] = (uint8_t)chosen;
        hist->counts[act]++;
    }

    return chosen;
}

/* Helper to resolve symbol names safely */
static const char *GetSymName(const GRAPH *graph, SYMBOL_ID id, const char *def)
{
    if (!graph || !graph->symbols) return def;
    const SYMBOL *s = SymbolGet(graph->symbols, id);
    if (!s || !s->name || s->name[0] == '\0') return def;
    return s->name;
}

/* 1. Fact Assertion */
uint32_t StochasticNLG_FactAssertion(
    const GRAPH *graph,
    SYMBOL_ID subject,
    SYMBOL_ID relation,
    SYMBOL_ID object,
    STOCHASTIC_NLG_CONFIG *cfg,
    STOCHASTIC_DISCOURSE_HISTORY *hist,
    char *out,
    size_t out_size)
{
    if (!graph || !cfg || !out || out_size == 0) return 0;
    LANG_ID lang = cfg->lang < LANG_COUNT ? cfg->lang : LANG_EN;
    const RHETORICAL_DATABASE *db = &g_rhetoric[lang];

    const char *s_name = GetSymName(graph, subject, "Sujeto");
    const char *r_name = GetSymName(graph, relation, "relacion");
    const char *o_name = GetSymName(graph, object, "Objeto");

    char capS[64], capO[64];
    SafeCapitalize(s_name, capS, sizeof(capS));
    SafeCapitalize(o_name, capO, sizeof(capO));

    uint32_t idx = SampleTemplateIndex(&db->fact_assertions, COG_ACT_FACT_ASSERTION, cfg, hist);
    const char *fmt = db->fact_assertions.templates[idx];

    snprintf(out, out_size, fmt, capS, r_name, capO);
    return (uint32_t)strlen(out);
}

/* 2. Inductive Reasoning */
uint32_t StochasticNLG_InductiveAssertion(
    const GRAPH *graph,
    SYMBOL_ID subject,
    SYMBOL_ID relation,
    SYMBOL_ID object,
    const GRAPH_RULE *rule,
    STOCHASTIC_NLG_CONFIG *cfg,
    STOCHASTIC_DISCOURSE_HISTORY *hist,
    char *out,
    size_t out_size)
{
    if (!graph || !cfg || !out || out_size == 0) return 0;
    LANG_ID lang = cfg->lang < LANG_COUNT ? cfg->lang : LANG_EN;
    const RHETORICAL_DATABASE *db = &g_rhetoric[lang];

    const char *s_name = GetSymName(graph, subject, "Sujeto");
    const char *r_name = GetSymName(graph, relation, "relacion");
    const char *o_name = GetSymName(graph, object, "Objeto");

    char capS[64], capO[64];
    SafeCapitalize(s_name, capS, sizeof(capS));
    SafeCapitalize(o_name, capO, sizeof(capO));

    float conf = rule ? rule->confidence : 1.0f;
    const TEMPLATE_GROUP *group = (conf >= 0.90f) ?
                                  &db->inductive_high_conf :
                                  &db->inductive_med_conf;

    uint32_t idx = SampleTemplateIndex(group, COG_ACT_INDUCTIVE_REASONING, cfg, hist);
    const char *fmt = group->templates[idx];

    snprintf(out, out_size, fmt, capS, r_name, capO);
    return (uint32_t)strlen(out);
}

/* 3. Abductive Explanation */
uint32_t StochasticNLG_AbductiveExplanation(
    const GRAPH *graph,
    const ABDUCTIVE_HYPOTHESIS *hyps,
    uint32_t num_hyps,
    STOCHASTIC_NLG_CONFIG *cfg,
    STOCHASTIC_DISCOURSE_HISTORY *hist,
    char *out,
    size_t out_size)
{
    if (!graph || !cfg || !out || out_size == 0) return 0;
    LANG_ID lang = cfg->lang < LANG_COUNT ? cfg->lang : LANG_EN;
    const RHETORICAL_DATABASE *db = &g_rhetoric[lang];

    if (!hyps || num_hyps == 0)
    {
        snprintf(out, out_size, "%s", "No plausible abductive hypothesis found.");
        return (uint32_t)strlen(out);
    }

    if (num_hyps == 1)
    {
        const char *s = GetSymName(graph, hyps[0].subject, "S");
        const char *r = GetSymName(graph, hyps[0].relation, "R");
        const char *o = GetSymName(graph, hyps[0].object, "O");
        char capS[64], capO[64];
        SafeCapitalize(s, capS, sizeof(capS));
        SafeCapitalize(o, capO, sizeof(capO));

        uint32_t idx = SampleTemplateIndex(&db->abductive_single, COG_ACT_ABDUCTIVE_EXPLANATION, cfg, hist);
        const char *fmt = db->abductive_single.templates[idx];
        snprintf(out, out_size, fmt, capS, r, capO);
    }
    else
    {
        const char *s1 = GetSymName(graph, hyps[0].subject, "S1");
        const char *r1 = GetSymName(graph, hyps[0].relation, "R1");
        const char *o1 = GetSymName(graph, hyps[0].object, "O1");
        const char *s2 = GetSymName(graph, hyps[1].subject, "S2");
        const char *r2 = GetSymName(graph, hyps[1].relation, "R2");
        const char *o2 = GetSymName(graph, hyps[1].object, "O2");

        char capS1[64], capO1[64], capS2[64], capO2[64];
        SafeCapitalize(s1, capS1, sizeof(capS1));
        SafeCapitalize(o1, capO1, sizeof(capO1));
        SafeCapitalize(s2, capS2, sizeof(capS2));
        SafeCapitalize(o2, capO2, sizeof(capO2));

        uint32_t idx = SampleTemplateIndex(&db->abductive_multi_intro, COG_ACT_ABDUCTIVE_EXPLANATION, cfg, hist);
        const char *fmt = db->abductive_multi_intro.templates[idx];
        snprintf(out, out_size, fmt, capS1, r1, capO1, capS2, r2, capO2);
    }

    return (uint32_t)strlen(out);
}

/* 4. Active Epistemic Inquiry */
uint32_t StochasticNLG_ActiveInquiry(
    const GRAPH *graph,
    const EPISTEMIC_INQUIRY *inquiry,
    STOCHASTIC_NLG_CONFIG *cfg,
    STOCHASTIC_DISCOURSE_HISTORY *hist,
    char *out,
    size_t out_size)
{
    if (!graph || !cfg || !out || out_size == 0 || !inquiry) return 0;
    LANG_ID lang = cfg->lang < LANG_COUNT ? cfg->lang : LANG_EN;
    const RHETORICAL_DATABASE *db = &g_rhetoric[lang];

    const char *s = GetSymName(graph, inquiry->missing_subj, "S");
    const char *r = GetSymName(graph, inquiry->missing_rel, "R");
    const char *o = GetSymName(graph, inquiry->missing_obj, "O");

    char capS[64], capO[64];
    SafeCapitalize(s, capS, sizeof(capS));
    SafeCapitalize(o, capO, sizeof(capO));

    uint32_t idx = SampleTemplateIndex(&db->active_inquiries, COG_ACT_ACTIVE_INQUIRY, cfg, hist);
    const char *fmt = db->active_inquiries.templates[idx];

    snprintf(out, out_size, fmt, capS, r, capO);
    return (uint32_t)strlen(out);
}

/* 5. Defeasible Contrast */
uint32_t StochasticNLG_DefeasibleContrast(
    const GRAPH *graph,
    SYMBOL_ID general_class,
    SYMBOL_ID relation,
    SYMBOL_ID property,
    SYMBOL_ID exception_entity,
    STOCHASTIC_NLG_CONFIG *cfg,
    STOCHASTIC_DISCOURSE_HISTORY *hist,
    char *out,
    size_t out_size)
{
    if (!graph || !cfg || !out || out_size == 0) return 0;
    LANG_ID lang = cfg->lang < LANG_COUNT ? cfg->lang : LANG_EN;
    const RHETORICAL_DATABASE *db = &g_rhetoric[lang];

    const char *cls = GetSymName(graph, general_class, "Clase");
    const char *rel = GetSymName(graph, relation, "relacion");
    const char *prp = GetSymName(graph, property, "propiedad");
    const char *exc = GetSymName(graph, exception_entity, "Excepcion");

    char capCls[64], capExc[64];
    SafeCapitalize(cls, capCls, sizeof(capCls));
    SafeCapitalize(exc, capExc, sizeof(capExc));

    uint32_t idx = SampleTemplateIndex(&db->defeasible_contrast, COG_ACT_DEFEASIBLE_CONTRAST, cfg, hist);
    const char *fmt = db->defeasible_contrast.templates[idx];

    snprintf(out, out_size, fmt, capCls, rel, prp, capExc);
    return (uint32_t)strlen(out);
}

/* Autonomous multi-turn conversational turn dispatcher */
uint32_t StochasticNLG_TurnResponse(
    const GRAPH *graph,
    const COGNITIVE_LEARNER *learner,
    SYMBOL_ID subject,
    SYMBOL_ID relation,
    SYMBOL_ID object,
    int is_why_question,
    STOCHASTIC_NLG_CONFIG *cfg,
    STOCHASTIC_DISCOURSE_HISTORY *hist,
    char *out,
    size_t out_size)
{
    if (!graph || !cfg || !out || out_size == 0) return 0;

    /* 1. Check for exception guards */
    if (learner)
    {
        for (uint32_t e = 0; e < learner->num_exceptions; e++)
        {
            if (learner->exceptions[e].exception_entity == subject)
            {
                uint32_t ridx = learner->exceptions[e].base_rule_idx;
                if (ridx < learner->rule_base.num_rules)
                {
                    const GRAPH_RULE *r = &learner->rule_base.rules[ridx];
                    return StochasticNLG_DefeasibleContrast(
                        graph, r->r1, r->head, object, subject, cfg, hist, out, out_size);
                }
            }
        }
    }

    /* 2. Causal "Why" question */
    if (is_why_question && learner)
    {
        ABDUCTIVE_HYPOTHESIS hyps[4];
        uint32_t nhyps = GraphAbduce(graph, &learner->rule_base, subject, relation, object, hyps, 4);
        if (nhyps > 0)
        {
            return StochasticNLG_AbductiveExplanation(graph, hyps, nhyps, cfg, hist, out, out_size);
        }
    }

    /* 3. Direct or derived fact in the graph */
    if (GraphFindRelation((GRAPH *)graph, subject, relation, object) != NULL)
    {
        /* Check if rule exists for this head relation */
        if (learner)
        {
            for (uint32_t r = 0; r < learner->rule_base.num_rules; r++)
            {
                if (learner->rule_base.rules[r].head == relation)
                {
                    return StochasticNLG_InductiveAssertion(
                        graph, subject, relation, object, &learner->rule_base.rules[r], cfg, hist, out, out_size);
                }
            }
        }
        return StochasticNLG_FactAssertion(graph, subject, relation, object, cfg, hist, out, out_size);
    }

    /* 4. Missing knowledge: Trigger Active Epistemic Inquiry */
    if (learner)
    {
        EPISTEMIC_INQUIRY inqs[2];
        uint32_t ninq = CognitiveFormulateInquiry(graph, learner, subject, relation, object, inqs, 2);
        if (ninq > 0)
        {
            return StochasticNLG_ActiveInquiry(graph, &inqs[0], cfg, hist, out, out_size);
        }
    }

    /* 5. Epistemic abstention */
    LANG_ID lang = cfg->lang < LANG_COUNT ? cfg->lang : LANG_EN;
    const RHETORICAL_DATABASE *db = &g_rhetoric[lang];
    const char *s = GetSymName(graph, subject, "S");
    const char *r = GetSymName(graph, relation, "R");
    const char *o = GetSymName(graph, object, "O");
    char capS[64], capO[64];
    SafeCapitalize(s, capS, sizeof(capS));
    SafeCapitalize(o, capO, sizeof(capO));

    snprintf(out, out_size, db->abstain_unknown, capS, r, capO);
    return (uint32_t)strlen(out);
}
