#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "learning.h"

LEARNING_CONFIG LearningConfigDefault(void)
{
    LEARNING_CONFIG cfg;
    cfg.min_count_threshold = 1;
    cfg.smoothing_epsilon = 0.0f;
    return cfg;
}

/* ============================================================
   Normalizacion y Lexer auxiliar
   ============================================================ */

static void StrToUpper(const char *src, char *dst, size_t max_len)
{
    size_t i = 0;
    while (src[i] && i < max_len - 1)
    {
        dst[i] = (char)toupper((unsigned char)src[i]);
        i++;
    }
    dst[i] = '\0';
}

static int IsStopWord(const char *w)
{
    static const char *stopwords[] = {
        "EL", "LA", "LOS", "LAS", "UN", "UNA", "UNOS", "UNAS",
        "DE", "DEL", "A", "AL", "EN", "CON", "POR", "PARA",
        "Y", "O", "QUE", "SE", "SU", "SUS", NULL
    };

    for (int i = 0; stopwords[i] != NULL; i++)
    {
        if (strcmp(w, stopwords[i]) == 0)
            return 1;
    }
    return 0;
}

static void CanonicalizePredicate(char *w)
{
    if (strcmp(w, "COME") == 0 || strcmp(w, "COMEN") == 0 ||
        strcmp(w, "COMIENDO") == 0 || strcmp(w, "COMER") == 0)
    {
        strcpy(w, "COME");
    }
    else if (strcmp(w, "ES") == 0 || strcmp(w, "SON") == 0 ||
             strcmp(w, "ERA") == 0 || strcmp(w, "SER") == 0)
    {
        strcpy(w, "ES");
    }
    else if (strcmp(w, "VIVE") == 0 || strcmp(w, "VIVEN") == 0 ||
             strcmp(w, "VIVIENDO") == 0 || strcmp(w, "VIVIR") == 0)
    {
        strcpy(w, "VIVE_EN");
    }
    else if (strcmp(w, "DUERME") == 0 || strcmp(w, "DUERMEN") == 0 ||
             strcmp(w, "DORMIR") == 0)
    {
        strcpy(w, "DUERME_EN");
    }
    else if (strcmp(w, "NECESITA") == 0 || strcmp(w, "NECESITAN") == 0 ||
             strcmp(w, "NECESITAR") == 0)
    {
        strcpy(w, "NECESITA");
    }
}

/* ============================================================
   Extraccion sintactica simple S - P - O
   ============================================================ */

int LearningSentence(GRAPH *graph, const char *sentence)
{
    if (graph == NULL || sentence == NULL)
        return 0;

    char buffer[512];
    strncpy(buffer, sentence, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    size_t len = strlen(buffer);
    for (size_t i = 0; i < len; i++)
    {
        if (buffer[i] == '.' || buffer[i] == ',' || buffer[i] == ';' ||
            buffer[i] == '!' || buffer[i] == '?' || buffer[i] == '\n')
        {
            buffer[i] = ' ';
        }
    }

    char tokens[16][64];
    uint32_t token_count = 0;

    char *token = strtok(buffer, " \t\r");
    while (token != NULL && token_count < 16)
    {
        char upper[64];
        StrToUpper(token, upper, sizeof(upper));

        if (!IsStopWord(upper) && strlen(upper) > 0)
        {
            strncpy(tokens[token_count], upper, 63);
            tokens[token_count][63] = '\0';
            token_count++;
        }
        token = strtok(NULL, " \t\r");
    }

    if (token_count < 3)
        return 0;

    char subject[64];
    char predicate[64];
    char object[64];

    strncpy(subject, tokens[0], 64);
    strncpy(predicate, tokens[1], 64);
    strncpy(object, tokens[2], 64);

    CanonicalizePredicate(predicate);

    SYMBOL_ID s = GraphAddSymbol(graph, subject);
    SYMBOL_ID p = GraphAddSymbol(graph, predicate);
    SYMBOL_ID o = GraphAddSymbol(graph, object);

    return GraphAddRelation(graph, s, p, o);
}

uint32_t LearningCorpus(GRAPH *graph, const char **sentences, uint32_t count)
{
    uint32_t learned = 0;
    for (uint32_t i = 0; i < count; i++)
    {
        if (LearningSentence(graph, sentences[i]))
            learned++;
    }
    return learned;
}

/* ============================================================
   Motor Probabilistico de Prediccion
   ============================================================ */

static int ComparePredictions(const void *a, const void *b)
{
    const PREDICTION *p1 = (const PREDICTION *)a;
    const PREDICTION *p2 = (const PREDICTION *)b;
    if (p2->probability > p1->probability) return 1;
    if (p2->probability < p1->probability) return -1;
    return 0;
}

uint32_t LearningPredict(
    const GRAPH *graph,
    SYMBOL_ID subject,
    SYMBOL_ID predicate,
    PREDICTION *out_predictions,
    uint32_t max_predictions)
{
    if (graph == NULL || out_predictions == NULL || max_predictions == 0)
        return 0;

    RELATION *candidates[64];
    uint32_t count = GraphQuerySubjectPredicate(
        (GRAPH *)graph,
        subject,
        predicate,
        candidates,
        64
    );

    if (count == 0)
        return 0;

    uint64_t total_count = 0;
    for (uint32_t i = 0; i < count; i++)
    {
        total_count += candidates[i]->count;
    }

    if (total_count == 0)
        return 0;

    uint32_t n = (count < max_predictions) ? count : max_predictions;

    for (uint32_t i = 0; i < n; i++)
    {
        out_predictions[i].object = candidates[i]->object;
        out_predictions[i].count  = candidates[i]->count;
        out_predictions[i].probability =
            (float)candidates[i]->count / (float)total_count;

        const SYMBOL *sym = SymbolGet(graph->symbols, candidates[i]->object);
        if (sym && sym->name)
            strncpy(out_predictions[i].name, sym->name, 63);
        else
            strcpy(out_predictions[i].name, "?");
        out_predictions[i].name[63] = '\0';
    }

    qsort(out_predictions, n, sizeof(PREDICTION), ComparePredictions);

    return n;
}

uint32_t LearningPredictText(
    const GRAPH *graph,
    const char *subject_name,
    const char *predicate_name,
    PREDICTION *out_predictions,
    uint32_t max_predictions)
{
    if (graph == NULL || subject_name == NULL || predicate_name == NULL)
        return 0;

    char upper_s[64];
    char upper_p[64];
    StrToUpper(subject_name, upper_s, sizeof(upper_s));
    StrToUpper(predicate_name, upper_p, sizeof(upper_p));
    CanonicalizePredicate(upper_p);

    SYMBOL_ID s = SymbolFind(graph->symbols, upper_s);
    SYMBOL_ID p = SymbolFind(graph->symbols, upper_p);

    if (s == SYMBOL_INVALID || p == SYMBOL_INVALID)
        return 0;

    return LearningPredict(graph, s, p, out_predictions, max_predictions);
}
