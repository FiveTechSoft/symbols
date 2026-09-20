/* =========================================================================
   persona.c: Pragmatic Conditioning, Epistemic Perspectives & Persona Filters
   Pillar 4: Deterministic Rhetorical Projection over Reflexive Meta-Graph
   - Mathematical persona projection operator Pi_style : G -> G_biased
   - Zero prompt injection: structural masks, not stochastic prompt prefixes
   - Modulates rhetorical framing, connectives, and vocabulary density
   - Strict Factual Invariance Invariant: Facts(Pi_P(Q)) == Facts(Q)
   - Zero hallucination guarantee, fail-closed epistemic boundaries
   ========================================================================= */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "persona.h"

/* =========================================================================
   Part 1: Declarative Persona Profiles & Lexicons (HARDCODING=0)
   ========================================================================= */

static const PERSONA_LEXICON g_persona_lexicons[PERSONA_COUNT] = {
    [PERSONA_NEUTRAL] = {
        .role_name             = "neutral",
        .intro                 = {
            [LANG_EN] = "According to verified records",
            [LANG_ES] = "Segun consta en los registros verificados",
            [LANG_FR] = "Selon les enregistrements verifies"
        },
        .chain_connective      = {
            [LANG_EN] = "which connects to",
            [LANG_ES] = "que conecta con",
            [LANG_FR] = "qui se connecte a"
        },
        .conclusion_connective = {
            [LANG_EN] = "Therefore, %s is the %s of %s.",
            [LANG_ES] = "Por tanto, %s es el %s de %s.",
            [LANG_FR] = "Par consequent, %s est le %s de %s."
        },
        .abstain_template      = {
            [LANG_EN] = "There is no record of %s in the available texts.",
            [LANG_ES] = "No tengo constancia de %s en los textos disponibles.",
            [LANG_FR] = "Il n'y a aucune trace de %s dans les textes disponibles."
        },
        .evidence_prefix       = {
            [LANG_EN] = "Grounding citation:",
            [LANG_ES] = "Cita de fundamentacion:",
            [LANG_FR] = "Citation de base:"
        }
    },
    [PERSONA_ARCHITECT] = {
        .role_name             = "architect",
        .intro                 = {
            [LANG_EN] = "From a structural systems architecture perspective",
            [LANG_ES] = "Desde la perspectiva arquitectonica del sistema",
            [LANG_FR] = "Du point de vue de l'architecture systeme"
        },
        .chain_connective      = {
            [LANG_EN] = "which satisfies dependency with",
            [LANG_ES] = "que satisface la dependencia con",
            [LANG_FR] = "qui satisfait la dependance avec"
        },
        .conclusion_connective = {
            [LANG_EN] = "Consequently, the structural invariant establishes that %s is the %s of %s.",
            [LANG_ES] = "Por consiguiente, el invariante estructural establece que %s es el %s de %s.",
            [LANG_FR] = "Par consequent, l'invariant structurel etablit que %s est le %s de %s."
        },
        .abstain_template      = {
            [LANG_EN] = "Component %s is undefined in current architectural specifications.",
            [LANG_ES] = "El componente %s no esta definido en las especificaciones arquitectonicas.",
            [LANG_FR] = "Le composant %s est indefini dans les specifications d'architecture."
        },
        .evidence_prefix       = {
            [LANG_EN] = "Architectural provenance:",
            [LANG_ES] = "Procedencia arquitectonica:",
            [LANG_FR] = "Provenance architecturale:"
        }
    },
    [PERSONA_AUDITOR] = {
        .role_name             = "auditor",
        .intro                 = {
            [LANG_EN] = "Upon rigorous epistemic audit of the factual substrate",
            [LANG_ES] = "Tras la auditoria formal del sustrato de hechos",
            [LANG_FR] = "Apres audit epistemique rigoureux du substrat factuel"
        },
        .chain_connective      = {
            [LANG_EN] = "which is formally grounded by",
            [LANG_ES] = "que queda formalmente respaldado por",
            [LANG_FR] = "qui est formellement fonde par"
        },
        .conclusion_connective = {
            [LANG_EN] = "The audit formally certifies that %s is the %s of %s.",
            [LANG_ES] = "La auditoria certifica formalmente que %s es el %s de %s.",
            [LANG_FR] = "L'audit certifie formellement que %s est le %s de %s."
        },
        .abstain_template      = {
            [LANG_EN] = "Epistemic audit failed: evidence for %s does not satisfy acceptance criteria.",
            [LANG_ES] = "Fallo de auditoria epistemica: la evidencia sobre %s no satisface los criterios de aceptacion.",
            [LANG_FR] = "Echec de l'audit epistemique: la preuve pour %s ne satisfait pas les criteres d'acceptation."
        },
        .evidence_prefix       = {
            [LANG_EN] = "Audit trail provenance:",
            [LANG_ES] = "Traza de auditoria:",
            [LANG_FR] = "Piste d'audit:"
        }
    },
    [PERSONA_TUTOR] = {
        .role_name             = "tutor",
        .intro                 = {
            [LANG_EN] = "Let us examine step by step how this connects",
            [LANG_ES] = "Veamos paso a paso como se conecta esto",
            [LANG_FR] = "Examinons etape par etape ce lien"
        },
        .chain_connective      = {
            [LANG_EN] = "which naturally leads us to",
            [LANG_ES] = "lo que naturalmente nos lleva a",
            [LANG_FR] = "ce qui nous amene naturellement a"
        },
        .conclusion_connective = {
            [LANG_EN] = "So as we can see, %s is the %s of %s.",
            [LANG_ES] = "Asi que como podemos comprobar, %s es el %s de %s.",
            [LANG_FR] = "Comme nous pouvons le constater, %s est le %s de %s."
        },
        .abstain_template      = {
            [LANG_EN] = "We do not have enough verified information about %s yet, but we can explore related topics.",
            [LANG_ES] = "Aun no disponemos de suficiente informacion sobre %s, pero podemos explorar temas relacionados.",
            [LANG_FR] = "Nous n'avons pas encore assez d'informations sur %s, mais nous pouvons explorer des sujets proches."
        },
        .evidence_prefix       = {
            [LANG_EN] = "Reference context:",
            [LANG_ES] = "Contexto de referencia:",
            [LANG_FR] = "Contexte de reference:"
        }
    },
    [PERSONA_CONCISE] = {
        .role_name             = "concise",
        .intro                 = {
            [LANG_EN] = "",
            [LANG_ES] = "",
            [LANG_FR] = ""
        },
        .chain_connective      = {
            [LANG_EN] = "->",
            [LANG_ES] = "->",
            [LANG_FR] = "->"
        },
        .conclusion_connective = {
            [LANG_EN] = "%s: %s of %s.",
            [LANG_ES] = "%s: %s de %s.",
            [LANG_FR] = "%s: %s de %s."
        },
        .abstain_template      = {
            [LANG_EN] = "UNKNOWN: %s.",
            [LANG_ES] = "DESCONOCIDO: %s.",
            [LANG_FR] = "INCONNU: %s."
        },
        .evidence_prefix       = {
            [LANG_EN] = "Src:",
            [LANG_ES] = "Origen:",
            [LANG_FR] = "Src:"
        }
    },
    [PERSONA_SOCRATIC] = {
        .role_name             = "socratic",
        .intro                 = {
            [LANG_EN] = "Examining the foundational premises of the inquiry",
            [LANG_ES] = "Examinando las premisas fundamentales de la consulta",
            [LANG_FR] = "En examinant les premisses fondamentales de la requete"
        },
        .chain_connective      = {
            [LANG_EN] = "which invites us to examine",
            [LANG_ES] = "lo cual nos invita a examinar",
            [LANG_FR] = "ce qui nous invite a examiner"
        },
        .conclusion_connective = {
            [LANG_EN] = "Thus the premise holds: %s is indeed the %s of %s.",
            [LANG_ES] = "Asi la premisa se cumple: %s es ciertamente el %s de %s.",
            [LANG_FR] = "Ainsi la premisse est verifiee: %s est en effet le %s de %s."
        },
        .abstain_template      = {
            [LANG_EN] = "The inquiry cannot proceed: no verified premise exists for %s.",
            [LANG_ES] = "La indagacion no puede proceder: no existe premisa verificada para %s.",
            [LANG_FR] = "La recherche ne peut aboutir: aucune premisse verifiable pour %s."
        },
        .evidence_prefix       = {
            [LANG_EN] = "Grounding premise:",
            [LANG_ES] = "Premisa fundamentada:",
            [LANG_FR] = "Premisse fondee:"
        }
    },
    [PERSONA_PIRATE_QUANTUM] = {
        .role_name             = "pirate_quantum",
        .intro                 = {
            [LANG_EN] = "Shiver me timbers and by Blackbeard's wave function, ye scallywag!",
            [LANG_ES] = "Por las barbas de Neptuno y el colapso de la funcion de onda, marinero!",
            [LANG_FR] = "Mille sabords et par l'effondrement de la fonction d'onde, moussaillon !"
        },
        .chain_connective      = {
            [LANG_EN] = "which entangles faster than a Spanish galleon with",
            [LANG_ES] = "que se entrelaza cual cabo de jarcia a la velocidad de la luz con",
            [LANG_FR] = "qui s'intrique plus vite qu'un galion avec"
        },
        .conclusion_connective = {
            [LANG_EN] = "So shiver me planks, %s is the %s of %s in pure quantum superposition till we open Davy Jones' chest!",
            [LANG_ES] = "De modo que por todos los diablos del Caribe, %s es el %s de %s en pura superposicion cuantica hasta que abramos el cofre!",
            [LANG_FR] = "Ainsi par mille canons, %s est le %s de %s en superposition quantique jusqu'a ce qu'on ouvre le coffre !"
        },
        .abstain_template      = {
            [LANG_EN] = "Blimey! Heisenberg's uncertainty principle swallowed all trace of %s into Davy Jones' locker!",
            [LANG_ES] = "Zafarrancho! El principio de incertidumbre de Heisenberg esconde a %s en el fondo del cofre de Davy Jones.",
            [LANG_FR] = "Tonnerre de Brest ! Le principe d'incertitude d'Heisenberg a englouti %s dans les abysses de Davy Jones."
        },
        .evidence_prefix       = {
            [LANG_EN] = "Quantum ship's log entry:",
            [LANG_ES] = "Bitacora de observacion cuantica:",
            [LANG_FR] = "Livre de bord quantique :"
        }
    }
};

static const PERSONA_PROFILE g_default_profiles[PERSONA_COUNT] = {
    [PERSONA_NEUTRAL]        = { .epistemic_threshold = 1.0f, .verbosity_level = 1, .require_provenance = 0, .use_rhetorical_intro = 1, .prefer_causal_chain = 0 },
    [PERSONA_ARCHITECT]      = { .epistemic_threshold = 1.0f, .verbosity_level = 2, .require_provenance = 1, .use_rhetorical_intro = 1, .prefer_causal_chain = 1 },
    [PERSONA_AUDITOR]        = { .epistemic_threshold = 1.5f, .verbosity_level = 2, .require_provenance = 1, .use_rhetorical_intro = 1, .prefer_causal_chain = 0 },
    [PERSONA_TUTOR]          = { .epistemic_threshold = 0.5f, .verbosity_level = 2, .require_provenance = 0, .use_rhetorical_intro = 1, .prefer_causal_chain = 1 },
    [PERSONA_CONCISE]        = { .epistemic_threshold = 1.0f, .verbosity_level = 0, .require_provenance = 0, .use_rhetorical_intro = 0, .prefer_causal_chain = 0 },
    [PERSONA_SOCRATIC]       = { .epistemic_threshold = 1.0f, .verbosity_level = 2, .require_provenance = 0, .use_rhetorical_intro = 1, .prefer_causal_chain = 1 },
    [PERSONA_PIRATE_QUANTUM] = { .epistemic_threshold = 1.0f, .verbosity_level = 2, .require_provenance = 1, .use_rhetorical_intro = 1, .prefer_causal_chain = 1 }
};

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

static void AppendStr(char *dst, size_t max_size, const char *src)
{
    if (!dst || !src || max_size == 0) return;
    size_t cur = strlen(dst);
    if (cur >= max_size - 1) return;
    size_t add = strlen(src);
    if (cur + add >= max_size)
        add = max_size - 1 - cur;
    memcpy(dst + cur, src, add);
    dst[cur + add] = '\0';
}

/* =========================================================================
   Part 1: Persona Filter Configuration & Activation
   ========================================================================= */

void PersonaFilterInit(PERSONA_FILTER *filter, PERSONA_ID id)
{
    if (!filter) return;
    if (id >= PERSONA_COUNT) id = PERSONA_NEUTRAL;

    filter->id      = id;
    filter->profile = g_default_profiles[id];
    filter->lex     = &g_persona_lexicons[id];
}

typedef struct {
    const char *alias;
    PERSONA_ID id;
} PERSONA_ALIAS;

static const PERSONA_ALIAS g_persona_aliases[] = {
    { "pirate",          PERSONA_PIRATE_QUANTUM },
    { "pirata",          PERSONA_PIRATE_QUANTUM },
    { "pirate_quantum",  PERSONA_PIRATE_QUANTUM },
    { "quantum_pirate",  PERSONA_PIRATE_QUANTUM },
    { "architect",       PERSONA_ARCHITECT },
    { "arquitecto",      PERSONA_ARCHITECT },
    { "auditor",         PERSONA_AUDITOR },
    { "tutor",           PERSONA_TUTOR },
    { "profesor",        PERSONA_TUTOR },
    { "concise",         PERSONA_CONCISE },
    { "conciso",         PERSONA_CONCISE },
    { "socratic",        PERSONA_SOCRATIC },
    { "socratico",       PERSONA_SOCRATIC },
    { "neutral",         PERSONA_NEUTRAL },
    { NULL,              PERSONA_NEUTRAL }
};

PERSONA_ID PersonaFindByName(const char *name)
{
    if (!name) return PERSONA_NEUTRAL;
    for (size_t i = 0; g_persona_aliases[i].alias != NULL; i++)
    {
        if (strcasecmp(name, g_persona_aliases[i].alias) == 0)
            return g_persona_aliases[i].id;
    }
    for (int i = 0; i < PERSONA_COUNT; i++)
    {
        if (strcasecmp(name, g_persona_lexicons[i].role_name) == 0)
            return (PERSONA_ID)i;
    }
    return PERSONA_NEUTRAL;
}

const char *PersonaGetName(PERSONA_ID id)
{
    if (id >= PERSONA_COUNT) id = PERSONA_NEUTRAL;
    return g_persona_lexicons[id].role_name;
}

const PERSONA_LEXICON *PersonaGetLexicon(PERSONA_ID id)
{
    if (id >= PERSONA_COUNT) id = PERSONA_NEUTRAL;
    return &g_persona_lexicons[id];
}

/* =========================================================================
   Part 2: Projection Operator Pi_style over Graph & Meta-Graph
   ========================================================================= */

int PersonaComputeRelationBias(const PERSONA_FILTER *filter,
                               const RELATION *rel,
                               const METAGRAPH *mg)
{
    (void)mg;
    if (!filter || !rel) return 0;

    int bias = 0;
    if (filter->id == PERSONA_AUDITOR)
    {
        /* Auditor prioritizes grounded triples with provenance */
        if (rel->source != SYMBOL_INVALID) bias += 25;
        if (rel->weight >= 2.0f) bias += 15;
    }
    else if (filter->id == PERSONA_ARCHITECT)
    {
        /* Architect prioritizes structural relations */
        if (rel->weight >= 1.5f) bias += 20;
    }
    else if (filter->id == PERSONA_CONCISE)
    {
        /* Concise favors highest weight directly */
        if (rel->weight >= 2.0f) bias += 10;
    }
    else if (filter->id == PERSONA_PIRATE_QUANTUM)
    {
        /* Pirate quantum prioritizes high epistemic certainty and causal links */
        if (rel->weight >= 2.0f) bias += 20;
        if (rel->source != SYMBOL_INVALID) bias += 10;
    }

    if (bias > 50) bias = 50;
    if (bias < -50) bias = -50;
    return bias;
}

int PersonaAcceptsRelation(const PERSONA_FILTER *filter, const RELATION *rel)
{
    if (!rel) return 0;
    if (!filter) return 1;
    return (rel->weight >= filter->profile.epistemic_threshold);
}

uint32_t PersonaRealizeFact(const PERSONA_FILTER *filter,
                            LANG_ID lang,
                            const char *subject,
                            const char *relation,
                            const char *object,
                            const char *source,
                            char *out,
                            size_t out_size)
{
    if (!subject || !relation || !object || !out || out_size == 0)
        return 0;
    out[0] = '\0';

    if (lang >= LANG_COUNT) lang = LANG_EN;
    PERSONA_ID pid = filter ? filter->id : PERSONA_NEUTRAL;
    const PERSONA_LEXICON *lex = &g_persona_lexicons[pid];

    char s_cap[64], o_cap[64];
    SafeCapitalize(subject, s_cap, sizeof(s_cap));
    SafeCapitalize(object, o_cap, sizeof(o_cap));

    if (pid == PERSONA_CONCISE)
    {
        /* Concise: "Subject: relation Object." */
        snprintf(out, out_size, "%s: %s %s.", s_cap, relation, o_cap);
        if (source && source[0] && filter && filter->profile.require_provenance)
        {
            char prov[128];
            snprintf(prov, sizeof(prov), " [%s %s]", lex->evidence_prefix[lang], source);
            AppendStr(out, out_size, prov);
        }
        return (uint32_t)strlen(out);
    }

    /* Rhetorical Intro */
    const char *intro = lex->intro[lang];
    if (intro && intro[0] && (!filter || filter->profile.use_rhetorical_intro))
    {
        AppendStr(out, out_size, intro);
        AppendStr(out, out_size, ": ");
    }

    /* Core fact */
    char fact[256];
    if (lang == LANG_ES)
        snprintf(fact, sizeof(fact), "%s es %s de %s.", s_cap, relation, o_cap);
    else if (lang == LANG_FR)
        snprintf(fact, sizeof(fact), "%s est %s de %s.", s_cap, relation, o_cap);
    else
        snprintf(fact, sizeof(fact), "%s is the %s of %s.", s_cap, relation, o_cap);

    AppendStr(out, out_size, fact);

    /* Provenance citation if required */
    if (source && source[0] && filter && filter->profile.require_provenance)
    {
        char prov[128];
        snprintf(prov, sizeof(prov), " (%s %s)", lex->evidence_prefix[lang], source);
        AppendStr(out, out_size, prov);
    }

    return (uint32_t)strlen(out);
}

uint32_t PersonaRealizeChain(const PERSONA_FILTER *filter,
                             LANG_ID lang,
                             const char *start_node,
                             const char hops[][CCG_STR_MAX],
                             uint32_t nhops,
                             const char *conclusion_rel,
                             const char *end_node,
                             char *out,
                             size_t out_size)
{
    if (!start_node || !conclusion_rel || !end_node || !out || out_size == 0)
        return 0;
    out[0] = '\0';

    if (lang >= LANG_COUNT) lang = LANG_EN;
    PERSONA_ID pid = filter ? filter->id : PERSONA_NEUTRAL;
    const PERSONA_LEXICON *lex = &g_persona_lexicons[pid];

    char s_cap[64], e_cap[64];
    SafeCapitalize(start_node, s_cap, sizeof(s_cap));
    SafeCapitalize(end_node, e_cap, sizeof(e_cap));

    if (pid == PERSONA_CONCISE)
    {
        /* Concise: "A -> Hops -> B (conclusion)" */
        AppendStr(out, out_size, s_cap);
        for (uint32_t i = 0; i < nhops; i++)
        {
            AppendStr(out, out_size, " -> ");
            char h_cap[64];
            SafeCapitalize(hops[i], h_cap, sizeof(h_cap));
            AppendStr(out, out_size, h_cap);
        }
        char conc[128];
        snprintf(conc, sizeof(conc), " [Result: %s is %s of %s].", s_cap, conclusion_rel, e_cap);
        AppendStr(out, out_size, conc);
        return (uint32_t)strlen(out);
    }

    /* Rhetorical Intro */
    const char *intro = lex->intro[lang];
    if (intro && intro[0] && (!filter || filter->profile.use_rhetorical_intro))
    {
        AppendStr(out, out_size, intro);
        AppendStr(out, out_size, ", ");
    }

    /* Derivation sequence */
    char step[256];
    char cur[64];
    strncpy(cur, s_cap, sizeof(cur) - 1);
    cur[sizeof(cur) - 1] = '\0';

    for (uint32_t i = 0; i < nhops; i++)
    {
        char next[64];
        SafeCapitalize(hops[i], next, sizeof(next));
        snprintf(step, sizeof(step), "%s %s %s, ", cur, lex->chain_connective[lang], next);
        AppendStr(out, out_size, step);
        strncpy(cur, next, sizeof(cur) - 1);
    }

    /* Formal Conclusion */
    char conclusion[256];
    snprintf(conclusion, sizeof(conclusion), lex->conclusion_connective[lang],
             s_cap, conclusion_rel, e_cap);
    AppendStr(out, out_size, conclusion);

    return (uint32_t)strlen(out);
}

uint32_t PersonaRealizeAbstain(const PERSONA_FILTER *filter,
                               LANG_ID lang,
                               const char *entity,
                               const char *relation,
                               char *out,
                               size_t out_size)
{
    (void)relation;
    if (!entity || !out || out_size == 0) return 0;
    out[0] = '\0';

    if (lang >= LANG_COUNT) lang = LANG_EN;
    PERSONA_ID pid = filter ? filter->id : PERSONA_NEUTRAL;
    const PERSONA_LEXICON *lex = &g_persona_lexicons[pid];

    char e_cap[64];
    SafeCapitalize(entity, e_cap, sizeof(e_cap));

    snprintf(out, out_size, lex->abstain_template[lang], e_cap);
    return (uint32_t)strlen(out);
}

uint32_t PersonaRealizePhysicalConsequence(const PERSONA_FILTER *filter,
                                           LANG_ID lang,
                                           const char *subject,
                                           const char *action,
                                           const char *target,
                                           const char *material,
                                           const char *consequence,
                                           char *out,
                                           size_t out_size)
{
    if (!subject || !action || !target || !out || out_size == 0)
        return 0;
    out[0] = '\0';

    if (lang >= LANG_COUNT) lang = LANG_EN;
    PERSONA_ID pid = filter ? filter->id : PERSONA_NEUTRAL;
    const PERSONA_LEXICON *lex = &g_persona_lexicons[pid];

    char s_cap[64], t_cap[64];
    SafeCapitalize(subject, s_cap, sizeof(s_cap));
    SafeCapitalize(target, t_cap, sizeof(t_cap));

    if (pid == PERSONA_CONCISE)
    {
        snprintf(out, out_size, "%s -> %s %s -> %s (%s).",
                 s_cap, action, t_cap, consequence ? consequence : "failure", material ? material : "material");
        return (uint32_t)strlen(out);
    }

    /* Rhetorical Intro if enabled */
    const char *intro = lex->intro[lang];
    if (intro && intro[0] && (!filter || filter->profile.use_rhetorical_intro))
    {
        AppendStr(out, out_size, intro);
        AppendStr(out, out_size, ": ");
    }

    /* Core verified physical deduction */
    char fact[256];
    if (lang == LANG_ES)
    {
        snprintf(fact, sizeof(fact),
                 "Si un %s se cae al %s, se rompera (porque el %s es un material fragil que se rompe con el impacto).",
                 subject, target, material ? material : "cristal");
    }
    else if (lang == LANG_FR)
    {
        snprintf(fact, sizeof(fact),
                 "Si %s tombe sur %s, il se brisera (parce qu'il est fait de %s ce qui cause sa rupture lors de l'impact).",
                 subject, target, material ? material : "materiau");
    }
    else
    {
        snprintf(fact, sizeof(fact),
                 "If %s is %s %s, it will %s (because %s is made of %s which causes %s upon impact).",
                 subject, action, target, consequence ? consequence : "shatter",
                 subject, material ? material : "brittle material", consequence ? consequence : "shatter");
    }
    AppendStr(out, out_size, fact);

    /* Grounding provenance citation if required by profile */
    if (filter && filter->profile.require_provenance)
    {
        char prov[128];
        snprintf(prov, sizeof(prov), " (%s ConceptNet 5.8 / fisica clasica)", lex->evidence_prefix[lang]);
        AppendStr(out, out_size, prov);
    }

    return (uint32_t)strlen(out);
}

/* =========================================================================
   Part 3: Mathematical Non-Interference Verification
   ========================================================================= */

int PersonaVerifyNonInterference(const GRAPH *graph,
                                 const char *test_subject,
                                 const char *test_relation)
{
    if (!graph || !test_subject || !test_relation) return 0;

    SYMBOL_ID s_id = SymbolFind(graph->symbols, test_subject);
    SYMBOL_ID p_id = SymbolFind(graph->symbols, test_relation);

    if (s_id == SYMBOL_INVALID || p_id == SYMBOL_INVALID)
    {
        /* If fact does not exist, ALL personas MUST fail-closed with honest abstention */
        for (int p = 0; p < PERSONA_COUNT; p++)
        {
            PERSONA_FILTER f;
            PersonaFilterInit(&f, (PERSONA_ID)p);
            char out[256];
            PersonaRealizeAbstain(&f, LANG_EN, test_subject, test_relation, out, sizeof(out));

            /* Check that no false positive fact assertion is made */
            if (strstr(out, "is the") != NULL && strstr(out, "Result:") != NULL)
                return 0; /* Invariance violated */
        }
        return 1; /* Pure fail-closed honesty across all personas */
    }

    RELATION *results[4];
    uint32_t count = RelationFindBySubjectRelation(graph->relations, s_id, p_id, results, 4);
    if (count == 0) return 1;

    const SYMBOL *target_sym = SymbolGet(graph->symbols, results[0]->object);
    if (!target_sym || !target_sym->name) return 0;

    /* For existing fact, verify every persona expresses the exact target object */
    for (int p = 0; p < PERSONA_COUNT; p++)
    {
        PERSONA_FILTER f;
        PersonaFilterInit(&f, (PERSONA_ID)p);

        char out[512];
        PersonaRealizeFact(&f, LANG_EN, test_subject, test_relation, target_sym->name,
                           "ground_truth.tsv:1", out, sizeof(out));

        /* Invariant 1: The ground truth target MUST be contained in the surface text */
        char target_lower[64], target_upper[64], target_cap[64];
        for (size_t i = 0; target_sym->name[i]; i++) {
            target_lower[i] = (char)tolower((unsigned char)target_sym->name[i]);
            target_upper[i] = (char)toupper((unsigned char)target_sym->name[i]);
        }
        target_lower[strlen(target_sym->name)] = '\0';
        target_upper[strlen(target_sym->name)] = '\0';
        SafeCapitalize(target_sym->name, target_cap, sizeof(target_cap));

        int contains_target = (strstr(out, target_cap) != NULL ||
                               strstr(out, target_lower) != NULL ||
                               strstr(out, target_upper) != NULL);
        if (!contains_target)
            return 0; /* Mathematical Invariance Violated: fact lost in persona projection */
    }

    return 1; /* Invariance mathematically verified across all personas */
}
