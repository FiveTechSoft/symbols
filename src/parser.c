#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "parser.h"
#include "learning.h"

/* ============================================================
   Common Spanish verbs (canonical forms)
   ============================================================ */

static const char *VERBS[] = {
    "ES", "SON", "ESTA", "ESTAN",
    "TIENE", "TIENEN", "TIENES",
    "HACE", "HACEN", "HACES",
    "COME", "COMEN", "COMES",
    "VIVE", "VIVEN", "VIVES",
    "DUERME", "DUERMEN",
    "NECESITA", "NECESITAN",
    "PUEDE", "PUEDEN",
    "TIENE_QUE",
    "HABITA", "HABITAN",
    "CONOCE", "CONOCEN",
    "USA", "USAN",
    "PARA", "SIRVE",
    "FUE", "ERA", "SERA",
    "HABIA", "HUBO",
    "EXISTE", "EXISTEN",
    "HAY",
    NULL
};

/* Prepositions and articles to strip from edges */
static const char *STRIP_WORDS[] = {
    "EL", "LA", "LOS", "LAS", "UN", "UNA", "UNOS", "UNAS",
    "DE", "DEL", "DELA", "DELOS", "DELAS",
    "EN", "EN_EL", "EN_LA", "EN_LOS", "EN_LAS",
    "POR", "PARA", "CON", "SIN", "SOBRE",
    "A", "AL", "ALA",
    "QUE", "COMO",
    "Y", "O",
    NULL
};

/* ============================================================
   Utility
   ============================================================ */

static void ToUpperCopy(const char *src, char *dst, size_t dst_size)
{
    size_t i;
    for (i = 0; src[i] && i < dst_size - 1; i++)
        dst[i] = (char)toupper((unsigned char)src[i]);
    dst[i] = '\0';
}

static int IsStripWord(const char *word)
{
    for (int i = 0; STRIP_WORDS[i]; i++)
        if (strcmp(word, STRIP_WORDS[i]) == 0)
            return 1;
    return 0;
}

static int IsVerb(const char *word)
{
    for (int i = 0; VERBS[i]; i++)
        if (strcmp(word, VERBS[i]) == 0)
            return 1;
    return 0;
}

static int IsArticle(const char *word)
{
    return (strcmp(word, "EL") == 0 || strcmp(word, "LA") == 0 ||
            strcmp(word, "LOS") == 0 || strcmp(word, "LAS") == 0 ||
            strcmp(word, "UN") == 0 || strcmp(word, "UNA") == 0);
}

static int IsPreposition(const char *word)
{
    return (strcmp(word, "DE") == 0 || strcmp(word, "DEL") == 0 ||
            strcmp(word, "EN") == 0 || strcmp(word, "POR") == 0 ||
            strcmp(word, "PARA") == 0 || strcmp(word, "CON") == 0 ||
            strcmp(word, "A") == 0 || strcmp(word, "AL") == 0 ||
            strcmp(word, "SOBRE") == 0);
}

/* ============================================================
   Tokenizer
   ============================================================ */

int ParserTokenize(const char *input, PARSED_SENTENCE *out)
{
    if (input == NULL || out == NULL)
        return 0;

    memset(out, 0, sizeof(*out));

    char buffer[1024];
    strncpy(buffer, input, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    NormalizeDiacritics(buffer);

    char *saveptr = NULL;
    char *token = strtok_r(buffer, " \t\n,;:!?.()\"'¿¡", &saveptr);

    while (token != NULL && out->count < PARSER_MAX_TOKENS)
    {
        if (strlen(token) > 0)
        {
            ToUpperCopy(token, out->tokens[out->count], 64);
            out->count++;
        }
        token = strtok_r(NULL, " \t\n,;:!?.()\"'¿¡", &saveptr);
    }

    return (int)out->count;
}

/* ============================================================
   S-P-O Extraction

   Strategies (tried in order):
   1. "X ES Y" / "X TIENE Y" — verb as pivot
   2. "X VERB Y" — any known verb
   3. Capital letter detection (proper nouns)
   4. First token = subject, rest = predicate + object
   ============================================================ */

PARSE_RESULT ParserExtractSPO(const PARSED_SENTENCE *tokens)
{
    PARSE_RESULT result;
    memset(&result, 0, sizeof(result));

    if (tokens == NULL || tokens->count < 2)
        return result;

    /* Strategy 1: Find verb pivot */
    for (uint32_t i = 0; i < tokens->count; i++)
    {
        if (IsVerb(tokens->tokens[i]))
        {
            /* Everything before verb = subject */
            char subj[128] = {0};
            int first_real = -1;
            for (uint32_t j = 0; j < i; j++)
            {
                if (!IsArticle(tokens->tokens[j]))
                {
                    if (first_real >= 0)
                        strcat(subj, "_");
                    strcat(subj, tokens->tokens[j]);
                    if (first_real < 0) first_real = (int)j;
                }
            }

            /* Verb = predicate */
            char pred[64];
            strcpy(pred, tokens->tokens[i]);

            /* Everything after verb = object */
            char obj[128] = {0};
            int last_real = -1;
            for (uint32_t j = i + 1; j < tokens->count; j++)
            {
                if (!IsArticle(tokens->tokens[j]))
                {
                    if (last_real >= 0)
                        strcat(obj, "_");
                    strcat(obj, tokens->tokens[j]);
                    last_real = (int)j;
                }
            }

            if (strlen(subj) > 0 && strlen(obj) > 0)
            {
                strcpy(result.subject, subj);
                strcpy(result.predicate, pred);
                strcpy(result.object, obj);
                result.valid = 1;
                return result;
            }
        }
    }

    /* Strategy 2: "X DE Y" pattern (e.g., "capital de España") */
    for (uint32_t i = 1; i < tokens->count; i++)
    {
        if (strcmp(tokens->tokens[i], "DE") == 0 && i > 0 && i + 1 < tokens->count)
        {
            char subj[128] = {0};
            for (uint32_t j = 0; j < i; j++)
            {
                if (!IsArticle(tokens->tokens[j]))
                {
                    if (strlen(subj) > 0) strcat(subj, "_");
                    strcat(subj, tokens->tokens[j]);
                }
            }

            char obj[128] = {0};
            for (uint32_t j = i + 1; j < tokens->count; j++)
            {
                if (!IsArticle(tokens->tokens[j]) && !IsPreposition(tokens->tokens[j]))
                {
                    if (strlen(obj) > 0) strcat(obj, "_");
                    strcat(obj, tokens->tokens[j]);
                }
            }

            if (strlen(subj) > 0 && strlen(obj) > 0)
            {
                strcpy(result.subject, subj);
                strcpy(result.predicate, "ES");
                strcpy(result.object, obj);
                result.valid = 1;
                return result;
            }
        }
    }

    /* Strategy 3: first noun = subject, remaining = object */
    if (tokens->count >= 3)
    {
        uint32_t start = 0;
        if (IsArticle(tokens->tokens[0]))
            start = 1;

        char subj[128] = {0};
        strcpy(subj, tokens->tokens[start]);

        char obj[128] = {0};
        for (uint32_t j = start + 1; j < tokens->count; j++)
        {
            if (strlen(obj) > 0) strcat(obj, "_");
            strcat(obj, tokens->tokens[j]);
        }

        strcpy(result.subject, subj);
        strcpy(result.predicate, "ES");
        strcpy(result.object, obj);
        result.valid = 1;
    }

    return result;
}

/* ============================================================
   Full pipeline: sentence → graph
   ============================================================ */

int ParserIngestSentence(GRAPH *graph, const char *sentence)
{
    if (graph == NULL || sentence == NULL)
        return 0;

    PARSED_SENTENCE tokens;
    int n_tokens = ParserTokenize(sentence, &tokens);

    if (n_tokens < 2)
        return 0;

    PARSE_RESULT spo = ParserExtractSPO(&tokens);

    if (!spo.valid)
        return 0;

    /* Add to graph */
    SYMBOL_ID s = GraphAddSymbol(graph, spo.subject);
    SYMBOL_ID p = GraphAddSymbol(graph, spo.predicate);
    SYMBOL_ID o = GraphAddSymbol(graph, spo.object);

    if (s == SYMBOL_INVALID || p == SYMBOL_INVALID || o == SYMBOL_INVALID)
        return 0;

    int added = GraphAddRelation(graph, s, p, o);

    /* Update embeddings on every co-occurrence */
    if (graph->embeddings != NULL)
    {
        EMBEDDING_TABLE *emb = graph->embeddings;

        if (EmbeddingGetVector(emb, s) == NULL)
        {
            float v[EMBEDDING_DIM];
            EmbeddingRandomInit(v, (uint32_t)s * 2654435761u);
            EmbeddingSetVector(emb, s, v);
        }
        if (EmbeddingGetVector(emb, o) == NULL)
        {
            float v[EMBEDDING_DIM];
            EmbeddingRandomInit(v, (uint32_t)o * 2654435761u);
            EmbeddingSetVector(emb, o, v);
        }

        float *target  = (float *)EmbeddingGetVector(emb, s);
        float *context = (float *)EmbeddingGetVector(emb, o);
        if (target && context)
        {
            EmbeddingCooccur(target, context, 0.1f);
            EmbeddingCooccur(context, target, 0.1f);
            EmbeddingNormalize(target);
            EmbeddingNormalize(context);
        }
    }

    return added;
}

/* ============================================================
   Question Detection & Answering
   ============================================================ */

/* Predicate keyword mapping: map question words to graph predicates */
typedef struct
{
    const char *keyword;    /* word in the question */
    const char *predicate;  /* graph predicate to search */
} KEYWORD_MAP;

static const KEYWORD_MAP PREDICATE_MAP[] = {
    {"CAPITAL",    "CAPITAL"},
    {"CAPITALES",  "CAPITAL"},
    {"IDIOMA",     "IDIOMA"},
    {"IDIOMAS",    "IDIOMA"},
    {"MONEDA",     "MONEDA"},
    {"GOBIERNO",   "GOBIERNO"},
    {"POBLACION",  "POBLACION"},
    {"SUPERFICIE", "SUPERFICIE"},
    {"CONTINENTE", "CONTINENTE"},
    {"OCUPACION",  "OCUPACION"},
    {"NACIMIENTO", "NACIMIENTO"},
    {"MUERTE",     "MUERTE"},
    {"PREMIO",     "PREMIO"},
    {"AUTOR",      "AUTOR"},
    {"CREADO_POR", "CREADO_POR"},
    {"EDITORIAL",  "EDITORIAL"},
    {"PAIS",       "PAIS"},
    {"GENTILICIO", "GENTILICIO"},
    {"RELIGION",   "RELIGION"},
    {"PADRE",      "PADRE"},
    {"MADRE",      "MADRE"},
    {"HIJO",       "HIJO"},
    {"ESPOSA",     "ESPOSA"},
    {"ESPOSO",     "ESPOSO"},
    {"COME",       "COME"},
    {"VIVE",       "VIVE"},
    {"DUERME",     "DUERME"},
    {"TIENE",      "TIENE"},
    {"USO",        "USA"},
    {"CONOCIDO_POR", "CONOCIDO_POR"},
    {NULL, NULL}
};

QUESTION ParserDetectQuestion(const char *input)
{
    QUESTION q;
    memset(&q, 0, sizeof(q));

    if (input == NULL)
        return q;

    /* Tokenize */
    PARSED_SENTENCE tokens;
    ParserTokenize(input, &tokens);

    if (tokens.count < 2)
        return q;

    /* Check if it's a question */
    int has_question_mark = (strstr(input, "?") != NULL ||
                             strstr(input, "¿") != NULL);
    int ends_with_es = (strcmp(tokens.tokens[tokens.count - 1], "ES") == 0);
    int starts_with_que = (strcmp(tokens.tokens[0], "QUE") == 0 ||
                           strcmp(tokens.tokens[0], "QUÉ") == 0 ||
                           strcmp(tokens.tokens[0], "QUIEN") == 0 ||
                           strcmp(tokens.tokens[0], "QUIÉN") == 0);
    int starts_with_donde = (strcmp(tokens.tokens[0], "DONDE") == 0 ||
                             strcmp(tokens.tokens[0], "DÓNDE") == 0);

    q.is_question = (has_question_mark || ends_with_es ||
                     starts_with_que || starts_with_donde);

    if (!q.is_question)
        return q;

    /* Strategy 1: "X es" at end → search for concept X */
    if (ends_with_es && tokens.count >= 3)
    {
        /* Build subject from tokens before "ES", skip articles */
        char subj[128] = {0};
        for (uint32_t i = 0; i < tokens.count - 1; i++)
        {
            if (!IsArticle(tokens.tokens[i]))
            {
                if (strlen(subj) > 0) strcat(subj, "_");
                strcat(subj, tokens.tokens[i]);
            }
        }

        /* Try to find a matching predicate keyword */
        for (uint32_t i = 0; i < tokens.count - 1; i++)
        {
            for (int k = 0; PREDICATE_MAP[k].keyword; k++)
            {
                if (strcmp(tokens.tokens[i], PREDICATE_MAP[k].keyword) == 0)
                {
                    strcpy(q.predicate, PREDICATE_MAP[k].predicate);
                    break;
                }
            }
            if (strlen(q.predicate) > 0) break;
        }

        /* If no predicate found, use "ES" */
        if (strlen(q.predicate) == 0)
            strcpy(q.predicate, "ES");

        strcpy(q.subject, subj);
        q.valid = 1;
        return q;
    }

    /* Strategy 2: "que es X" / "quien es X" */
    if (starts_with_que || starts_with_donde)
    {
        /* Find "ES" position */
        for (uint32_t i = 0; i < tokens.count; i++)
        {
            if (strcmp(tokens.tokens[i], "ES") == 0 && i + 1 < tokens.count)
            {
                char subj[128] = {0};
                for (uint32_t j = i + 1; j < tokens.count; j++)
                {
                    if (!IsArticle(tokens.tokens[j]))
                    {
                        if (strlen(subj) > 0) strcat(subj, "_");
                        strcat(subj, tokens.tokens[j]);
                    }
                }

                /* Check if question word maps to a predicate */
                for (int k = 0; PREDICATE_MAP[k].keyword; k++)
                {
                    if (strcmp(tokens.tokens[0], PREDICATE_MAP[k].keyword) == 0)
                    {
                        strcpy(q.predicate, PREDICATE_MAP[k].predicate);
                        break;
                    }
                }

                if (strlen(q.predicate) == 0)
                    strcpy(q.predicate, "ES");

                strcpy(q.subject, subj);
                q.valid = 1;
                return q;
            }
        }
    }

    return q;
}

int ParserAnswerQuestion(
    const GRAPH *graph,
    const QUESTION *q,
    char *out_answer,
    uint32_t max_len)
{
    if (graph == NULL || q == NULL || !q->valid || out_answer == NULL)
        return 0;

    out_answer[0] = '\0';

    /* Find subject symbol */
    SYMBOL_ID subj_id = SymbolFind(graph->symbols, q->subject);

    /* Try direct match first */
    if (subj_id != SYMBOL_INVALID)
    {
        /* Find predicate */
        SYMBOL_ID pred_id = SymbolFind(graph->symbols, q->predicate);

        if (pred_id != SYMBOL_INVALID)
        {
            RELATION *results[8];
            uint32_t n = GraphQuerySubjectPredicate(
                graph, subj_id, pred_id, results, 8);

            if (n > 0)
            {
                /* Build answer from objects */
                uint32_t pos = 0;
                for (uint32_t i = 0; i < n && i < 5; i++)
                {
                    const SYMBOL *obj = SymbolGet(graph->symbols, results[i]->object);
                    if (obj)
                    {
                        if (i > 0 && pos + 2 < max_len)
                        {
                            out_answer[pos++] = ',';
                            out_answer[pos++] = ' ';
                        }
                        uint32_t name_len = (uint32_t)strlen(obj->name);
                        if (pos + name_len < max_len)
                        {
                            memcpy(out_answer + pos, obj->name, name_len);
                            pos += name_len;
                        }
                    }
                }
                out_answer[pos] = '\0';
                return 1;
            }
        }

        /* Fallback: get all relations of subject */
        RELATION *all[32];
        uint32_t n = GraphQuerySubject(graph, subj_id, all, 32);

        if (n > 0)
        {
            /* Find the most relevant predicate */
            for (uint32_t i = 0; i < n; i++)
            {
                const SYMBOL *pred = SymbolGet(graph->symbols, all[i]->predicate);
                const SYMBOL *obj = SymbolGet(graph->symbols, all[i]->object);
                if (pred && obj)
                {
                    /* Check if predicate matches what we're looking for */
                    if (strlen(q->predicate) > 0 &&
                        strstr(pred->name, q->predicate) != NULL)
                    {
                        snprintf(out_answer, max_len, "%s", obj->name);
                        return 1;
                    }
                }
            }

            /* If no predicate match, return first relation */
            const SYMBOL *p = SymbolGet(graph->symbols, all[0]->predicate);
            const SYMBOL *o = SymbolGet(graph->symbols, all[0]->object);
            if (p && o)
            {
                snprintf(out_answer, max_len, "%s", o->name);
                return 1;
            }
        }
    }

    /* Fallback: partial match */
    for (uint32_t i = 0; i < graph->symbols->count; i++)
    {
        const SYMBOL *sym = &graph->symbols->items[i];
        if (sym->name && strstr(sym->name, q->subject) != NULL)
        {
            RELATION *all[16];
            uint32_t n = GraphQuerySubject(graph, sym->id, all, 16);
            if (n > 0)
            {
                const SYMBOL *o = SymbolGet(graph->symbols, all[0]->object);
                if (o)
                {
                    snprintf(out_answer, max_len, "%s", o->name);
                    return 1;
                }
            }
        }
    }

    return 0;
}
