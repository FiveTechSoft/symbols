#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "parser.h"
#include "compat.h"
#include "stem.h"
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

static void AppendToken(char *dst, size_t dst_size, const char *token)
{
    size_t used = strlen(dst);
    if (used + 1 >= dst_size)
        return;

    if (used > 0)
    {
        dst[used++] = '_';
        dst[used] = '\0';
    }

    size_t space = dst_size - used - 1;
    size_t len = strlen(token);
    if (len > space)
        len = space;

    memcpy(dst + used, token, len);
    dst[used + len] = '\0';
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
            for (uint32_t j = 0; j < i; j++)
            {
                if (!IsArticle(tokens->tokens[j]))
                    AppendToken(subj, sizeof(subj), tokens->tokens[j]);
            }

            /* Verb = predicate */
            char pred[64];
            strcpy(pred, tokens->tokens[i]);

            /* Everything after verb = object */
            char obj[128] = {0};
            for (uint32_t j = i + 1; j < tokens->count; j++)
            {
                if (!IsArticle(tokens->tokens[j]))
                    AppendToken(obj, sizeof(obj), tokens->tokens[j]);
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
                    AppendToken(subj, sizeof(subj), tokens->tokens[j]);
            }

            char obj[128] = {0};
            for (uint32_t j = i + 1; j < tokens->count; j++)
            {
                if (!IsArticle(tokens->tokens[j]) && !IsPreposition(tokens->tokens[j]))
                    AppendToken(obj, sizeof(obj), tokens->tokens[j]);
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
            AppendToken(obj, sizeof(obj), tokens->tokens[j]);

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
    int starts_with_cuantos = (strcmp(tokens.tokens[0], "CUANTOS") == 0 ||
                               strcmp(tokens.tokens[0], "CUANTAS") == 0 ||
                               strcmp(tokens.tokens[0], "CUÁNTOS") == 0 ||
                               strcmp(tokens.tokens[0], "CUÁNTAS") == 0);

    q.is_question = (has_question_mark || ends_with_es ||
                     starts_with_que || starts_with_donde ||
                     starts_with_cuantos);

    if (!q.is_question)
        return q;

    /* Strategy 0: kinship 2-hop "ABUELO DE X" (antes que la 1 y la 2,
       que lo destrozarian: ABUELO no esta en PREDICATE_MAP).
       e.g. "quien es el abuelo de David" / "el abuelo de David es"
       → subject=DAVID, predicate=ABUELO */
    {
        int has_abuelo = 0, de_pos = -1;
        for (uint32_t i = 0; i < tokens.count; i++)
        {
            if (strcmp(tokens.tokens[i], "ABUELO") == 0 ||
                strcmp(tokens.tokens[i], "ABUELA") == 0 ||
                strcmp(tokens.tokens[i], "ABUELOS") == 0 ||
                strcmp(tokens.tokens[i], "ABUELAS") == 0)
                has_abuelo = 1;
            if ((strcmp(tokens.tokens[i], "DE") == 0 ||
                 strcmp(tokens.tokens[i], "DEL") == 0) && de_pos < 0)
                de_pos = (int)i;
        }
        if (has_abuelo && de_pos > 0)
        {
            /* Entidad: ultimo token tras DE sin articulos ni ES final */
            char subj[128] = {0};
            for (uint32_t i = (uint32_t)de_pos + 1; i < tokens.count; i++)
            {
                if (IsArticle(tokens.tokens[i]))
                    continue;
                if (i == tokens.count - 1 &&
                    strcmp(tokens.tokens[i], "ES") == 0)
                    continue;
                strcpy(subj, tokens.tokens[i]);
            }
            if (strlen(subj) > 0)
            {
                strcpy(q.subject, subj);
                strcpy(q.predicate, "ABUELO");
                q.valid = 1;
                return q;
            }
        }
    }

    /* Strategy 0b: conteo "CUANTOS HIJOS TIENE X"
       → subject=X, predicate=CUENTA_HIJOS */
    if (starts_with_cuantos)
    {
        int has_hijos = 0, tiene_pos = -1;
        for (uint32_t i = 0; i < tokens.count; i++)
        {
            if (strcmp(tokens.tokens[i], "HIJOS") == 0 ||
                strcmp(tokens.tokens[i], "HIJAS") == 0 ||
                strcmp(tokens.tokens[i], "HIJO") == 0 ||
                strcmp(tokens.tokens[i], "HIJA") == 0)
                has_hijos = 1;
            if (strcmp(tokens.tokens[i], "TIENE") == 0 ||
                strcmp(tokens.tokens[i], "TIENEN") == 0)
                tiene_pos = (int)i;
        }
        if (has_hijos && tiene_pos > 0)
        {
            char subj[128] = {0};
            for (uint32_t i = (uint32_t)tiene_pos + 1; i < tokens.count; i++)
            {
                if (IsArticle(tokens.tokens[i]))
                    continue;
                strcpy(subj, tokens.tokens[i]);
            }
            if (strlen(subj) > 0)
            {
                strcpy(q.subject, subj);
                strcpy(q.predicate, "CUENTA_HIJOS");
                q.valid = 1;
                return q;
            }
        }
    }

    /* Strategy 1: "la PREDICATE de ENTITY es" pattern
     * e.g. "la CAPITAL de FRANCIA es" → subject=FRANCIA, predicate=CAPITAL
     * Find "DE" separator: tokens before DE = predicate keywords, after DE = entity */
    if (ends_with_es && tokens.count >= 4)
    {
        /* Look for "DE" token */
        int de_pos = -1;
        for (uint32_t i = 0; i < tokens.count - 1; i++)
        {
            if (strcmp(tokens.tokens[i], "DE") == 0 ||
                strcmp(tokens.tokens[i], "DEL") == 0)
            {
                de_pos = (int)i;
                break;
            }
        }

        if (de_pos > 0)
        {
            /* Predicate = keyword(s) between articles and DE.
               Exact keyword first, then stemmed form (MONEDAS->MONEDA). */
            for (uint32_t i = 0; i < (uint32_t)de_pos; i++)
            {
                char stem[64];
                StemWord(tokens.tokens[i], stem, sizeof(stem));
                for (int k = 0; PREDICATE_MAP[k].keyword; k++)
                {
                    if (strcmp(tokens.tokens[i], PREDICATE_MAP[k].keyword) == 0 ||
                        strcmp(stem, PREDICATE_MAP[k].keyword) == 0)
                    {
                        strcpy(q.predicate, PREDICATE_MAP[k].predicate);
                        break;
                    }
                }
                if (strlen(q.predicate) > 0) break;
            }

            /* Subject = tokens after DE, before ES */
            char subj[128] = {0};
            for (uint32_t i = (uint32_t)de_pos + 1; i < tokens.count - 1; i++)
            {
                if (!IsArticle(tokens.tokens[i]))
                    AppendToken(subj, sizeof(subj), tokens.tokens[i]);
            }

            if (strlen(subj) > 0 && strlen(q.predicate) > 0)
            {
                strcpy(q.subject, subj);
                q.valid = 1;
                return q;
            }
        }
    }

    /* Strategy 2: "X es" at end → search for concept X */
    if (ends_with_es && tokens.count >= 3)
    {
        /* Try to find a matching predicate keyword (exact, then stemmed) */
        for (uint32_t i = 0; i < tokens.count - 1; i++)
        {
            char stem[64];
            StemWord(tokens.tokens[i], stem, sizeof(stem));
            for (int k = 0; PREDICATE_MAP[k].keyword; k++)
            {
                if (strcmp(tokens.tokens[i], PREDICATE_MAP[k].keyword) == 0 ||
                    strcmp(stem, PREDICATE_MAP[k].keyword) == 0)
                {
                    strcpy(q.predicate, PREDICATE_MAP[k].predicate);
                    break;
                }
            }
            if (strlen(q.predicate) > 0) break;
        }

        if (strlen(q.predicate) == 0)
            strcpy(q.predicate, "ES");

        /* Build subject: last non-ES token, skip articles */
        char subj[128] = {0};
        for (uint32_t i = tokens.count - 2; i > 0; i--)
        {
            if (!IsArticle(tokens.tokens[i]))
            {
                strcpy(subj, tokens.tokens[i]);
                break;
            }
        }
        if (strlen(subj) == 0)
            strcpy(subj, tokens.tokens[0]);

        strcpy(q.subject, subj);
        q.valid = 1;
        return q;
    }

    /* Strategy 3: "donde esta X" → ESTA as predicate */
    if (starts_with_donde && tokens.count >= 3)
    {
        for (uint32_t i = 1; i < tokens.count; i++)
        {
            if (strcmp(tokens.tokens[i], "ESTA") == 0 ||
                strcmp(tokens.tokens[i], "VIVE") == 0)
            {
                strcpy(q.predicate, tokens.tokens[i]);
                /* Subject = tokens after verb */
                for (uint32_t j = i + 1; j < tokens.count; j++)
                {
                    if (!IsArticle(tokens.tokens[j]))
                    {
                        strcpy(q.subject, tokens.tokens[j]);
                        break;
                    }
                }
                if (strlen(q.subject) > 0)
                {
                    q.valid = 1;
                    return q;
                }
            }
        }
    }

    /* Strategy 4: "que es X" / "quien es X" */
    if (starts_with_que || starts_with_donde)
    {
        for (uint32_t i = 0; i < tokens.count; i++)
        {
            if (strcmp(tokens.tokens[i], "ES") == 0 && i + 1 < tokens.count)
            {
                char subj[128] = {0};
                for (uint32_t j = i + 1; j < tokens.count; j++)
                {
                    if (!IsArticle(tokens.tokens[j]))
                        AppendToken(subj, sizeof(subj), tokens.tokens[j]);
                }

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

/* Padres de X por ambas direcciones de la tabla:
   (X,HIJO_DE,P) y (P,PADRE_DE,X). Sin duplicados. */
static uint32_t FindParents(const GRAPH *graph, SYMBOL_ID xid,
                            SYMBOL_ID *out, uint32_t max)
{
    uint32_t n = 0;
    if (graph == NULL || xid == SYMBOL_INVALID || out == NULL || max == 0)
        return 0;

    SYMBOL_ID hijo = StemFindSymbol(graph->symbols, "HIJO_DE");
    SYMBOL_ID padre = StemFindSymbol(graph->symbols, "PADRE_DE");
    if (hijo == SYMBOL_INVALID && padre == SYMBOL_INVALID)
        return 0;

    RELATION *rels[64];
    if (hijo != SYMBOL_INVALID)
    {
        uint32_t m = GraphQuerySubject(graph, xid, rels, 64);
        for (uint32_t i = 0; i < m && n < max; i++)
            if (rels[i]->predicate == hijo)
                out[n++] = rels[i]->object;
    }
    if (padre != SYMBOL_INVALID)
    {
        uint32_t m = GraphQueryObject(graph, xid, rels, 64);
        for (uint32_t i = 0; i < m && n < max; i++)
        {
            if (rels[i]->predicate != padre)
                continue;
            int dup = 0;
            for (uint32_t k = 0; k < n; k++)
                if (out[k] == rels[i]->subject) { dup = 1; break; }
            if (!dup)
                out[n++] = rels[i]->subject;
        }
    }
    return n;
}

/* Hijos de X: (C,HIJO_DE,X) y (X,PADRE_DE,C). Sin duplicados. */
static uint32_t FindChildren(const GRAPH *graph, SYMBOL_ID xid,
                             SYMBOL_ID *out, uint32_t max)
{
    uint32_t n = 0;
    if (graph == NULL || xid == SYMBOL_INVALID || out == NULL || max == 0)
        return 0;

    SYMBOL_ID hijo = StemFindSymbol(graph->symbols, "HIJO_DE");
    SYMBOL_ID padre = StemFindSymbol(graph->symbols, "PADRE_DE");
    if (hijo == SYMBOL_INVALID && padre == SYMBOL_INVALID)
        return 0;

    RELATION *rels[64];
    if (hijo != SYMBOL_INVALID)
    {
        uint32_t m = GraphQueryObject(graph, xid, rels, 64);
        for (uint32_t i = 0; i < m && n < max; i++)
            if (rels[i]->predicate == hijo)
                out[n++] = rels[i]->subject;
    }
    if (padre != SYMBOL_INVALID)
    {
        uint32_t m = GraphQuerySubject(graph, xid, rels, 64);
        for (uint32_t i = 0; i < m && n < max; i++)
        {
            if (rels[i]->predicate != padre)
                continue;
            int dup = 0;
            for (uint32_t k = 0; k < n; k++)
                if (out[k] == rels[i]->object) { dup = 1; break; }
            if (!dup)
                out[n++] = rels[i]->object;
        }
    }
    return n;
}

static uint32_t AppendName(const GRAPH *graph, SYMBOL_ID id,
                           char *out, uint32_t pos, uint32_t max_len)
{
    const SYMBOL *sym = SymbolGet(graph->symbols, id);
    if (!sym || !sym->name)
        return pos;
    uint32_t name_len = (uint32_t)strlen(sym->name);
    if (pos + name_len < max_len)
    {
        memcpy(out + pos, sym->name, name_len);
        pos += name_len;
    }
    return pos;
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

    /* Parentesco 2-hop: abuelos = padres(padres(X)) */
    if (strcmp(q->predicate, "ABUELO") == 0)
    {
        SYMBOL_ID xid = StemFindSymbol(graph->symbols, q->subject);
        if (xid == SYMBOL_INVALID)
            return 0;
        SYMBOL_ID pars[8], grans[8];
        uint32_t np = FindParents(graph, xid, pars, 8);
        uint32_t ng = 0;
        for (uint32_t i = 0; i < np && ng < 8; i++)
        {
            SYMBOL_ID gp[8];
            uint32_t n = FindParents(graph, pars[i], gp, 8);
            for (uint32_t j = 0; j < n && ng < 8; j++)
            {
                int dup = 0;
                for (uint32_t k = 0; k < ng; k++)
                    if (grans[k] == gp[j]) { dup = 1; break; }
                if (!dup)
                    grans[ng++] = gp[j];
            }
        }
        if (ng == 0)
            return 0;
        uint32_t pos = 0;
        for (uint32_t i = 0; i < ng; i++)
        {
            if (i > 0 && pos + 2 < max_len)
            {
                out_answer[pos++] = ',';
                out_answer[pos++] = ' ';
            }
            pos = AppendName(graph, grans[i], out_answer, pos, max_len);
        }
        out_answer[pos] = '\0';
        return 1;
    }

    /* Conteo: "N: nombre1, nombre2, ..." (N=0 solo si X existe) */
    if (strcmp(q->predicate, "CUENTA_HIJOS") == 0)
    {
        SYMBOL_ID xid = StemFindSymbol(graph->symbols, q->subject);
        if (xid == SYMBOL_INVALID)
            return 0;
        SYMBOL_ID kids[16];
        uint32_t n = FindChildren(graph, xid, kids, 16);
        uint32_t pos = 0;
        char num[16];
        snprintf(num, sizeof(num), "%u", n);
        uint32_t nlen = (uint32_t)strlen(num);
        if (pos + nlen + 2 < max_len)
        {
            memcpy(out_answer + pos, num, nlen);
            pos += nlen;
            out_answer[pos++] = ':';
            out_answer[pos++] = ' ';
        }
        uint32_t shown = (n > 8) ? 8 : n;
        for (uint32_t i = 0; i < shown; i++)
        {
            if (i > 0 && pos + 2 < max_len)
            {
                out_answer[pos++] = ',';
                out_answer[pos++] = ' ';
            }
            pos = AppendName(graph, kids[i], out_answer, pos, max_len);
        }
        out_answer[pos] = '\0';
        return 1;
    }

    /* Find subject symbol (exact first, then morphological fallback) */
    SYMBOL_ID subj_id = StemFindSymbol(graph->symbols, q->subject);

    /* Try direct match first */
    if (subj_id != SYMBOL_INVALID)
    {
        /* Find predicate */
        SYMBOL_ID pred_id = StemFindSymbol(graph->symbols, q->predicate);

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
