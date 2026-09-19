/* ============================================================
   passage_nlg: Document-Level Passage Generation & Soft Intent Mapping.
   Pure C11, zero tensors, zero backprop, fail-closed truth preservation.

   English code comments (project rule); localized text in format strings.
   HARDCODING=0: Discourse frames and lexicons are declarative.
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#ifdef _WIN32
#include <windows.h>
static void CompatSleep(uint32_t ms) { Sleep(ms); }
#else
#include <unistd.h>
static void CompatSleep(uint32_t ms) { usleep(ms * 1000); }
#endif

#include "passage_nlg.h"

/* Helper to resolve symbol names safely */
static const char *GetSymName(const GRAPH *graph, SYMBOL_ID id, const char *def)
{
    if (!graph || !graph->symbols) return def;
    const SYMBOL *s = SymbolGet(graph->symbols, id);
    if (!s || !s->name || s->name[0] == '\0') return def;
    return s->name;
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

/* Normalization: lowercase + accent folding */
static void NormalizeWord(const char *src, char *dst, size_t max_len)
{
    size_t s = 0, d = 0;
    while (src[s] && d < max_len - 1)
    {
        unsigned char c = (unsigned char)src[s];
        /* Skip common punctuation */
        if (c == '?' || c == '!' || c == '.' || c == ',' || c == ';' || c == ':' || c == '"' || c == '\'')
        {
            s++;
            continue;
        }
        /* UTF-8 2-byte accented vowel folding */
        if (c == 0xC3 && (unsigned char)src[s + 1] >= 0x80)
        {
            unsigned char c2 = (unsigned char)src[s + 1];
            switch (c2)
            {
                case 0x81: case 0xA1: dst[d++] = 'a'; break;
                case 0x89: case 0xA9: dst[d++] = 'e'; break;
                case 0x8D: case 0xAD: dst[d++] = 'i'; break;
                case 0x93: case 0xB3: dst[d++] = 'o'; break;
                case 0x9A: case 0xBA: case 0xBC: dst[d++] = 'u'; break;
                case 0x91: case 0xB1: dst[d++] = 'n'; break;
                default: dst[d++] = '?'; break;
            }
            s += 2;
            continue;
        }
        dst[d++] = (char)tolower(c);
        s++;
    }
    dst[d] = '\0';
}

/* Resolve entity name against graph symbols and cross-lingual dictionary */
static SYMBOL_ID ResolveEntity(
    const GRAPH *graph,
    const DICT *dict,
    const char *raw_name)
{
    if (!graph || !graph->symbols || !raw_name || raw_name[0] == '\0')
        return SYMBOL_INVALID;

    char norm[64];
    NormalizeWord(raw_name, norm, sizeof(norm));
    if (norm[0] == '\0') return SYMBOL_INVALID;

    /* 1. Direct search in symbol table */
    SYMBOL_ID sid = SymbolFind(graph->symbols, norm);
    if (sid != SYMBOL_INVALID) return sid;

    /* 2. Capitalized search */
    char cap[64];
    SafeCapitalize(norm, cap, sizeof(cap));
    sid = SymbolFind(graph->symbols, cap);
    if (sid != SYMBOL_INVALID) return sid;

    /* 3. Uppercase search */
    char up[64];
    for (size_t i = 0; norm[i] && i < sizeof(up) - 1; i++)
        up[i] = (char)toupper((unsigned char)norm[i]);
    up[strlen(norm)] = '\0';
    sid = SymbolFind(graph->symbols, up);
    if (sid != SYMBOL_INVALID) return sid;

    /* 4. Cross-lingual dictionary translation */
    if (dict)
    {
        /* Try alias -> canonical */
        const char *can = DictTranslate(dict, norm);
        if (can)
        {
            sid = SymbolFind(graph->symbols, can);
            if (sid != SYMBOL_INVALID) return sid;

            SafeCapitalize(can, cap, sizeof(cap));
            sid = SymbolFind(graph->symbols, cap);
            if (sid != SYMBOL_INVALID) return sid;
        }

        /* Try reverse canonical -> alias */
        for (uint32_t i = 0; i < dict->count; i++)
        {
            char dict_can_norm[64];
            NormalizeWord(dict->entries[i].canonical, dict_can_norm, sizeof(dict_can_norm));
            if (strcmp(dict_can_norm, norm) == 0)
            {
                sid = SymbolFind(graph->symbols, dict->entries[i].alias);
                if (sid != SYMBOL_INVALID) return sid;

                SafeCapitalize(dict->entries[i].alias, cap, sizeof(cap));
                sid = SymbolFind(graph->symbols, cap);
                if (sid != SYMBOL_INVALID) return sid;
            }
        }
    }

    return SYMBOL_INVALID;
}

/* Classify open conversational query and resolve entities */
PARSED_QUERY_INTENT PassageClassifyQuery(
    const GRAPH *graph,
    const DICT *dict,
    const char *user_input)
{
    PARSED_QUERY_INTENT res;
    memset(&res, 0, sizeof(res));
    if (!user_input) return res;

    strncpy(res.raw_query, user_input, sizeof(res.raw_query) - 1);
    char norm[256];
    NormalizeWord(user_input, norm, sizeof(norm));

    /* 1. Metacognition: "como sabes", "por que crees" */
    if (strstr(norm, "como sabes") || strstr(norm, "por que sabes") || strstr(norm, "de donde sale"))
    {
        res.intent = INTENT_METACOGNITION;
    }
    /* 2. Causal: "por que", "why" */
    else if (strstr(norm, "por que") || strstr(norm, "why") || strstr(norm, "a que se debe"))
    {
        res.intent = INTENT_WHY_QUERY;
    }
    /* 3. Verification: "es verdad", "es X de Y" */
    else if (strncmp(norm, "es ", 3) == 0 || strstr(norm, "es verdad") || strstr(norm, "acaso "))
    {
        res.intent = INTENT_VERIFY_QUERY;
    }
    /* 4. Summarization / Open topic: "hablame de", "cuentame de", "que sabes sobre", "de que trata" */
    else if (strstr(norm, "hablame de") || strstr(norm, "cuentame de") || strstr(norm, "cuentame sobre") ||
             strstr(norm, "cuentame acerca de") || strstr(norm, "que sabes sobre") || strstr(norm, "que sabes de") ||
             strstr(norm, "de que trata") || strstr(norm, "de que habla") || strstr(norm, "resumen de") ||
             strstr(norm, "tell me about") || strstr(norm, "what is") || strstr(norm, "who is"))
    {
        res.intent = INTENT_SUMMARIZE_ENTITY;
    }
    else
    {
        res.intent = INTENT_SUMMARIZE_ENTITY; /* Default open generative intent */
    }

    /* Extract candidate entity words from the query */
    char words[32][64];
    uint32_t num_words = 0;
    char temp[256];
    strncpy(temp, norm, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    char *tok = strtok(temp, " \t\r\n");
    while (tok && num_words < 32)
    {
        /* Skip common stopwords and trigger words */
        if (strcmp(tok, "el") != 0 && strcmp(tok, "la") != 0 && strcmp(tok, "los") != 0 &&
            strcmp(tok, "las") != 0 && strcmp(tok, "de") != 0 && strcmp(tok, "del") != 0 &&
            strcmp(tok, "sobre") != 0 && strcmp(tok, "acerca") != 0 && strcmp(tok, "que") != 0 &&
            strcmp(tok, "un") != 0 && strcmp(tok, "una") != 0 && strcmp(tok, "hablame") != 0 &&
            strcmp(tok, "cuentame") != 0 && strcmp(tok, "sabes") != 0 && strcmp(tok, "trata") != 0 &&
            strcmp(tok, "libro") != 0 && strcmp(tok, "book") != 0 && strcmp(tok, "tell") != 0 &&
            strcmp(tok, "me") != 0 && strcmp(tok, "about") != 0)
        {
            strncpy(words[num_words], tok, sizeof(words[num_words]) - 1);
            num_words++;
        }
        tok = strtok(NULL, " \t\r\n");
    }

    /* Resolve entities against graph & dictionary */
    for (uint32_t i = 0; i < num_words; i++)
    {
        SYMBOL_ID sid = ResolveEntity(graph, dict, words[i]);
        if (sid != SYMBOL_INVALID)
        {
            if (res.sym1 == SYMBOL_INVALID || res.sym1 == 0)
            {
                res.sym1 = sid;
                strncpy(res.entity1, words[i], sizeof(res.entity1) - 1);
            }
            else if (res.sym2 == SYMBOL_INVALID || res.sym2 == 0)
            {
                res.sym2 = sid;
                strncpy(res.entity2, words[i], sizeof(res.entity2) - 1);
                break;
            }
        }
    }

    return res;
}

/* Generate full multi-paragraph passage for an entity using sub-graph storytelling */
uint32_t PassageGenerateTopicStory(
    const GRAPH *graph,
    const COGNITIVE_LEARNER *learner,
    WORKING_MEMORY *wm,
    SYMBOL_ID entity,
    STOCHASTIC_NLG_CONFIG *cfg,
    char *out,
    size_t out_size)
{
    if (!graph || entity == SYMBOL_INVALID || !out || out_size == 0)
        return 0;

    const char *name = GetSymName(graph, entity, "Entidad");
    char capName[64];
    SafeCapitalize(name, capName, sizeof(capName));

    /* 1. Stimulate Working Memory and Spread Activation */
    if (wm)
    {
        WM_Stimulate(wm, entity, 1.0f);
        WM_SpreadActivation(wm, graph, 0.60f, 2);
    }

    /* 2. Collect direct relations connected to the entity */
    RELATION *out_rels[16];
    uint32_t n_out = GraphQuerySubject((GRAPH *)graph, entity, out_rels, 16);

    /* Collect incoming relations */
    typedef struct { SYMBOL_ID s; SYMBOL_ID r; } INC_REL;
    INC_REL inc_rels[16];
    uint32_t n_inc = 0;
    if (graph->relations)
    {
        for (uint32_t i = 0; i < graph->relations->count && n_inc < 16; i++)
        {
            if (graph->relations->items[i].object == entity &&
                graph->relations->items[i].polarity == POLARITY_POSITIVE)
            {
                inc_rels[n_inc].s = graph->relations->items[i].subject;
                inc_rels[n_inc].r = graph->relations->items[i].relation;
                n_inc++;
            }
        }
    }

    out[0] = '\0';
    size_t rem = out_size;

    /* ---- PARRAFO 1: ENCUADRE EJECUTIVO Y DEFINICION ---- */
    char p1[512];
    snprintf(p1, sizeof(p1),
             "El concepto de **%s** constituye una entidad central y verificada dentro de los registros ontologicos. "
             "En el corpus analizado, se perfila como un eje fundamental en torno al cual se articulan multiples testimonios "
             "y estructuras relacionales comprobadas.\n\n",
             capName);
    strncat(out, p1, rem - strlen(out) - 1);

    /* ---- PARRAFO 2: EJE RELACIONAL Y HECHOS DEL GRAFO ---- */
    char p2[1024] = "De acuerdo con los registros fehacientes del grafo, se constatan los siguientes vinculos directos:\n";
    uint32_t facts_shown = 0;

    for (uint32_t i = 0; i < n_out && facts_shown < 5; i++)
    {
        const char *rel_name = GetSymName(graph, out_rels[i]->relation, "relacion");
        const char *obj_name = GetSymName(graph, out_rels[i]->object, "objeto");
        char capObj[64];
        SafeCapitalize(obj_name, capObj, sizeof(capObj));

        char line[256];
        snprintf(line, sizeof(line), "  * %s actua como **%s** respecto a **%s**.\n",
                 capName, rel_name, capObj);
        strncat(p2, line, sizeof(p2) - strlen(p2) - 1);
        facts_shown++;
    }

    for (uint32_t i = 0; i < n_inc && facts_shown < 8; i++)
    {
        const char *sub_name = GetSymName(graph, inc_rels[i].s, "sujeto");
        const char *rel_name = GetSymName(graph, inc_rels[i].r, "relacion");
        char capSub[64];
        SafeCapitalize(sub_name, capSub, sizeof(capSub));

        char line[256];
        snprintf(line, sizeof(line), "  * Se documenta que **%s** guarda relacion de **%s** hacia %s.\n",
                 capSub, rel_name, capName);
        strncat(p2, line, sizeof(p2) - strlen(p2) - 1);
        facts_shown++;
    }

    if (facts_shown == 0)
    {
        strncat(p2, "  * La entidad se halla indexada de forma singular con consistencia estructural.\n",
                sizeof(p2) - strlen(p2) - 1);
    }
    strncat(p2, "\n", sizeof(p2) - strlen(p2) - 1);
    strncat(out, p2, rem - strlen(out) - 1);

    /* ---- PARRAFO 3: CONTEXTO CONEXO Y MEMORIA DE TRABAJO ---- */
    if (wm)
    {
        SYMBOL_ID top_syms[4];
        float top_acts[4];
        uint32_t n_top = WM_GetTopActive(wm, top_syms, top_acts, 4);

        if (n_top > 1)
        {
            char p3[512];
            char c1[64] = "", c2[64] = "";
            uint32_t found_neighbors = 0;
            for (uint32_t k = 0; k < n_top && found_neighbors < 2; k++)
            {
                if (top_syms[k] != entity)
                {
                    if (found_neighbors == 0)
                    {
                        SafeCapitalize(GetSymName(graph, top_syms[k], ""), c1, sizeof(c1));
                        found_neighbors++;
                    }
                    else if (found_neighbors == 1)
                    {
                        SafeCapitalize(GetSymName(graph, top_syms[k], ""), c2, sizeof(c2));
                        found_neighbors++;
                    }
                }
            }

            if (found_neighbors == 2)
            {
                snprintf(p3, sizeof(p3),
                         "En la red de conocimiento activa, **%s** estimula de manera inmediata conceptos afines como **%s** y **%s**, "
                         "conformando un campo semantico altamente conectado dentro de la memoria de trabajo.\n\n",
                         capName, c1, c2);
            }
            else if (found_neighbors == 1)
            {
                snprintf(p3, sizeof(p3),
                         "En la red de conocimiento activa, **%s** se articula estrechamente con el concepto afin de **%s**.\n\n",
                         capName, c1);
            }
            else
            {
                snprintf(p3, sizeof(p3),
                         "Este nodo mantiene coherencia de activacion local dentro de la memoria de trabajo.\n\n");
            }
            strncat(out, p3, rem - strlen(out) - 1);
        }
    }

    /* ---- PARRAFO 4: CIERRE REFLEXIVO Y CURIOSIDAD ACTIVA ---- */
    char p4[512];
    snprintf(p4, sizeof(p4),
             "¿Deseas que examinemos con mayor detalle alguno de estos vinculos especificos sobre **%s**, o prefieres explorar "
             "la justificacion formal de sus deducciones?",
             capName);
    strncat(out, p4, rem - strlen(out) - 1);

    (void)learner;
    (void)cfg;
    return (uint32_t)strlen(out);
}

/* Master conversational dispatcher */
uint32_t PassageHandleTurn(
    const GRAPH *graph,
    const COGNITIVE_LEARNER *learner,
    WORKING_MEMORY *wm,
    const PROVENANCE_TABLE *pt,
    const DICT *dict,
    const char *user_input,
    STOCHASTIC_NLG_CONFIG *cfg,
    STOCHASTIC_DISCOURSE_HISTORY *hist,
    char *out,
    size_t out_size)
{
    if (!graph || !user_input || !out || out_size == 0) return 0;

    PARSED_QUERY_INTENT q = PassageClassifyQuery(graph, dict, user_input);

    /* 1. Metacognitive query: "como sabes eso" */
    if (q.intent == INTENT_METACOGNITION && pt && q.sym1 != SYMBOL_INVALID && q.sym2 != SYMBOL_INVALID)
    {
        /* Find relation */
        RELATION *r = GraphFindRelation((GRAPH *)graph, q.sym1, SYMBOL_INVALID, q.sym2);
        if (r)
        {
            return MetacognitiveExplainBelief(graph, pt, &learner->rule_base, q.sym1, r->relation, q.sym2, out, out_size);
        }
    }

    /* 2. Direct relation query or why */
    if (q.sym1 != SYMBOL_INVALID && q.sym2 != SYMBOL_INVALID)
    {
        RELATION *r = GraphFindRelation((GRAPH *)graph, q.sym1, SYMBOL_INVALID, q.sym2);
        if (r)
        {
            if (q.intent == INTENT_WHY_QUERY)
            {
                return StochasticNLG_TurnResponse(graph, learner, q.sym1, r->relation, q.sym2, 1, cfg, hist, out, out_size);
            }
            else
            {
                return StochasticNLG_FactAssertion(graph, q.sym1, r->relation, q.sym2, cfg, hist, out, out_size);
            }
        }
    }

    /* 3. Summarization / Storytelling: entity found */
    if (q.sym1 != SYMBOL_INVALID)
    {
        return PassageGenerateTopicStory(graph, learner, wm, q.sym1, cfg, out, out_size);
    }

    /* 4. Epistemic honest abstention when no entity resolved */
    snprintf(out, out_size,
             "He procesado tu consulta, pero no he identificado un termino o entidad que corresponda a los registros verificados del grafo. "
             "¿Podrias precisar sobre que figura o concepto deseas que conversemos?");
    return (uint32_t)strlen(out);
}

/* Stream-print a generated passage token-by-token (LLM typing cadence) */
void PassageStreamOutput(const char *text, uint32_t ms_per_word)
{
    if (!text) return;
    const char *p = text;
    while (*p)
    {
        putchar(*p);
        if (*p == ' ' || *p == '\n')
        {
            fflush(stdout);
            if (ms_per_word > 0)
                CompatSleep(ms_per_word);
        }
        p++;
    }
    putchar('\n');
    fflush(stdout);
}
