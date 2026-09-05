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

void NormalizeDiacritics(char *str)
{
    char *dst = str;
    for (char *p = str; *p; )
    {
        unsigned char c = (unsigned char)*p;
        char replacement = 0;

        if (c < 0x80) { *dst++ = *p++; continue; }

        if (c == 0xC2)
        {
            unsigned char next = (unsigned char)*(p + 1);
            if (next == 0x82) replacement = 'A';      /* Â */
            else if (next == 0xAA) replacement = 'A';  /* ª */
            else if (next == 0xB0) replacement = 'O';  /* º */
            if (replacement) { *dst++ = replacement; p += 2; continue; }
        }
        else if (c == 0xC3)
        {
            unsigned char next = (unsigned char)*(p + 1);
            if (next == 0x80) replacement = 'A';       else if (next == 0xA0) replacement = 'A';
            else if (next == 0x81) replacement = 'A';  else if (next == 0xA1) replacement = 'A';
            else if (next == 0x82) replacement = 'A';  else if (next == 0xA2) replacement = 'A';
            else if (next == 0x83) replacement = 'A';  else if (next == 0xA3) replacement = 'A';
            else if (next == 0x84) replacement = 'A';  else if (next == 0xA4) replacement = 'A';
            else if (next == 0x85) replacement = 'A';  else if (next == 0xA5) replacement = 'A';
            else if (next == 0x86) { *dst++ = 'A'; *dst++ = 'E'; p += 2; continue; }
            else if (next == 0xA6) { *dst++ = 'A'; *dst++ = 'E'; p += 2; continue; }
            else if (next == 0x87) replacement = 'C';  else if (next == 0xA7) replacement = 'C';
            else if (next == 0x88) replacement = 'E';  else if (next == 0xA8) replacement = 'E';
            else if (next == 0x89) replacement = 'E';  else if (next == 0xA9) replacement = 'E';
            else if (next == 0x8A) replacement = 'E';  else if (next == 0xAA) replacement = 'E';
            else if (next == 0x8B) replacement = 'E';  else if (next == 0xAB) replacement = 'E';
            else if (next == 0x8C) replacement = 'I';  else if (next == 0xAC) replacement = 'I';
            else if (next == 0x8D) replacement = 'I';  else if (next == 0xAD) replacement = 'I';
            else if (next == 0x8E) replacement = 'I';  else if (next == 0xAE) replacement = 'I';
            else if (next == 0x8F) replacement = 'I';  else if (next == 0xAF) replacement = 'I';
            else if (next == 0x90) replacement = 'D';  else if (next == 0xB0) replacement = 'D';
            else if (next == 0x91) replacement = 'N';  else if (next == 0xB1) replacement = 'N';
            else if (next == 0x92) replacement = 'O';  else if (next == 0xB2) replacement = 'O';
            else if (next == 0x93) replacement = 'O';  else if (next == 0xB3) replacement = 'O';
            else if (next == 0x94) replacement = 'O';  else if (next == 0xB4) replacement = 'O';
            else if (next == 0x95) replacement = 'O';  else if (next == 0xB5) replacement = 'O';
            else if (next == 0x96) replacement = 'O';  else if (next == 0xB6) replacement = 'O';
            else if (next == 0x98) replacement = 'O';  else if (next == 0xB8) replacement = 'O';
            else if (next == 0x99) replacement = 'U';  else if (next == 0xB9) replacement = 'U';
            else if (next == 0x9A) replacement = 'U';  else if (next == 0xBA) replacement = 'U';
            else if (next == 0x9B) replacement = 'U';  else if (next == 0xBB) replacement = 'U';
            else if (next == 0x9C) replacement = 'U';  else if (next == 0xBC) replacement = 'U';
            else if (next == 0x9D) replacement = 'Y';  else if (next == 0xBD) replacement = 'Y';
            else if (next == 0x9E) { *dst++ = 'T'; *dst++ = 'H'; p += 2; continue; }
            else if (next == 0x9F) { *dst++ = 'S'; *dst++ = 'S'; p += 2; continue; }
            else if (next == 0xBE) { *dst++ = 'T'; *dst++ = 'H'; p += 2; continue; }
            else if (next == 0xBF) replacement = 'Y';
            if (replacement) { *dst++ = replacement; p += 2; continue; }
        }
        else if (c == 0xC4)
        {
            unsigned char next = (unsigned char)*(p + 1);
            if (next == 0x80 || next == 0x81) replacement = 'A';
            else if (next == 0x84 || next == 0x85 || next == 0x86 || next == 0x87) replacement = 'C';
            else if (next == 0x8C) replacement = 'D';
            else if (next == 0x8E || next == 0x90 || next == 0x92) replacement = 'E';
            else if (next == 0x98 || next == 0x9A || next == 0x9B) replacement = 'G';
            else if (next == 0xA3 || next == 0xA4 || next == 0xA6) replacement = 'I';
            else if (next == 0xB0) replacement = 'I';
            else if (next == 0xA7 || next == 0xA8) replacement = 'K';
            else if (next == 0xA9 || next == 0xAA || next == 0xAB || next == 0xAC) replacement = 'L';
            else if (next == 0xAF || next == 0xB1 || next == 0xB2 || next == 0xB3) replacement = 'N';
            else if (next == 0xB4 || next == 0xB5) replacement = 'O';
            else if (next == 0xB9 || next == 0xBA || next == 0xBB) replacement = 'R';
            else if (next == 0xBC || next == 0xBD || next == 0xBE || next == 0xBF) replacement = 'S';
            if (replacement) { *dst++ = replacement; p += 2; continue; }
        }
        else if (c == 0xC5)
        {
            unsigned char next = (unsigned char)*(p + 1);
            if (next == 0x82) replacement = 'S';
            else if (next == 0x92) replacement = 'Z';
            else if (next == 0x94) replacement = 'Z';
            if (replacement) { *dst++ = replacement; p += 2; continue; }
        }
        else if (c == 0xC6)
        {
            unsigned char next = (unsigned char)*(p + 1);
            if (next == 0x8E) { *dst++ = 'A'; *dst++ = 'E'; p += 2; continue; }
            else if (next == 0x92) { *dst++ = 'O'; *dst++ = 'E'; p += 2; continue; }
        }
        else if (c == 0xC9)
        {
            unsigned char next = (unsigned char)*(p + 1);
            if (next == 0x91 || next == 0x93 || next == 0x94 || next == 0x96) replacement = 'A';
            else if (next == 0x9B || next == 0x9D) replacement = 'E';
            else if (next == 0xA5) replacement = 'I';
            else if (next == 0xB2 || next == 0xB4) replacement = 'O';
            else if (next == 0xBE || next == 0xC0) replacement = 'U';
            if (replacement) { *dst++ = replacement; p += 2; continue; }
        }

        *dst++ = *p++;
    }
    *dst = '\0';
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
             strcmp(w, "ERA") == 0 || strcmp(w, "SER") == 0 ||
             strcmp(w, "FUE") == 0 || strcmp(w, "ESO") == 0)
    {
        strcpy(w, "ES");
    }
    else if (strcmp(w, "TIENE") == 0 || strcmp(w, "TIENEN") == 0 ||
             strcmp(w, "TENER") == 0 || strcmp(w, "TENIA") == 0)
    {
        strcpy(w, "TIENE");
    }
    else if (strcmp(w, "HACE") == 0 || strcmp(w, "HACEN") == 0 ||
             strcmp(w, "HACER") == 0 || strcmp(w, "HACIENDO") == 0)
    {
        strcpy(w, "HACE");
    }
    else if (strcmp(w, "NECESITA") == 0 || strcmp(w, "NECESITAN") == 0 ||
             strcmp(w, "NECESITAR") == 0)
    {
        strcpy(w, "NECESITA");
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

    /* ---- Pattern matching for natural Spanish ---- */
    /* Patterns: "el X es Y", "X es Y", "X tiene Y", "X esta en Y" */
    {
        char upper_buf[512];
        size_t slen = strlen(buffer);
        for (size_t i = 0; i < slen; i++)
            upper_buf[i] = (char)toupper((unsigned char)buffer[i]);
        upper_buf[slen] = '\0';

        /* Pattern: "EL X ES Y" or "LA X ES Y" */
        const char *pref_el = "EL ";
        const char *pref_la = "LA ";
        const char *pref_los = "LOS ";
        const char *pref_las = "LAS ";
        const char *rest = NULL;

        if (strncmp(upper_buf, pref_el, 3) == 0) rest = upper_buf + 3;
        else if (strncmp(upper_buf, pref_la, 3) == 0) rest = upper_buf + 3;
        else if (strncmp(upper_buf, pref_los, 4) == 0) rest = upper_buf + 4;
        else if (strncmp(upper_buf, pref_las, 4) == 0) rest = upper_buf + 4;

        if (rest)
        {
            /* "X ES Y" */
            char subj[64] = {0}, pred[64] = {0}, obj[64] = {0};
            if (sscanf(rest, "%63s ES %63[^\n]", subj, obj) == 2 && obj[0])
            {
                strcpy(pred, "ES");
                SYMBOL_ID s = GraphAddSymbol(graph, subj);
                SYMBOL_ID p = GraphAddSymbol(graph, pred);
                SYMBOL_ID o = GraphAddSymbol(graph, obj);
                return GraphAddRelation(graph, s, p, o);
            }
            /* "X TIENE Y" */
            if (sscanf(rest, "%63s TIENE %63[^\n]", subj, obj) == 2 && obj[0])
            {
                strcpy(pred, "TIENE");
                SYMBOL_ID s = GraphAddSymbol(graph, subj);
                SYMBOL_ID p = GraphAddSymbol(graph, pred);
                SYMBOL_ID o = GraphAddSymbol(graph, obj);
                return GraphAddRelation(graph, s, p, o);
            }
            /* "X ESTA EN Y" */
            if (sscanf(rest, "%63s ESTA EN %63[^\n]", subj, obj) == 2 && obj[0])
            {
                strcpy(pred, "ESTA_EN");
                SYMBOL_ID s = GraphAddSymbol(graph, subj);
                SYMBOL_ID p = GraphAddSymbol(graph, pred);
                SYMBOL_ID o = GraphAddSymbol(graph, obj);
                return GraphAddRelation(graph, s, p, o);
            }
            /* "X HACE Y" */
            if (sscanf(rest, "%63s HACE %63[^\n]", subj, obj) == 2 && obj[0])
            {
                strcpy(pred, "HACE");
                SYMBOL_ID s = GraphAddSymbol(graph, subj);
                SYMBOL_ID p = GraphAddSymbol(graph, pred);
                SYMBOL_ID o = GraphAddSymbol(graph, obj);
                return GraphAddRelation(graph, s, p, o);
            }
        }

        /* Pattern without article: "X ES Y", "X TIENE Y" */
        {
            char subj[64] = {0}, pred[64] = {0}, obj[64] = {0};
            if (sscanf(upper_buf, "%63s ES %63[^\n]", subj, obj) == 2 && obj[0])
            {
                strcpy(pred, "ES");
                SYMBOL_ID s = GraphAddSymbol(graph, subj);
                SYMBOL_ID p = GraphAddSymbol(graph, pred);
                SYMBOL_ID o = GraphAddSymbol(graph, obj);
                return GraphAddRelation(graph, s, p, o);
            }
            if (sscanf(upper_buf, "%63s TIENE %63[^\n]", subj, obj) == 2 && obj[0])
            {
                strcpy(pred, "TIENE");
                SYMBOL_ID s = GraphAddSymbol(graph, subj);
                SYMBOL_ID p = GraphAddSymbol(graph, pred);
                SYMBOL_ID o = GraphAddSymbol(graph, obj);
                return GraphAddRelation(graph, s, p, o);
            }
            if (sscanf(upper_buf, "%63s ESTA EN %63[^\n]", subj, obj) == 2 && obj[0])
            {
                strcpy(pred, "ESTA_EN");
                SYMBOL_ID s = GraphAddSymbol(graph, subj);
                SYMBOL_ID p = GraphAddSymbol(graph, pred);
                SYMBOL_ID o = GraphAddSymbol(graph, obj);
                return GraphAddRelation(graph, s, p, o);
            }
            if (sscanf(upper_buf, "%63s HACE %63[^\n]", subj, obj) == 2 && obj[0])
            {
                strcpy(pred, "HACE");
                SYMBOL_ID s = GraphAddSymbol(graph, subj);
                SYMBOL_ID p = GraphAddSymbol(graph, pred);
                SYMBOL_ID o = GraphAddSymbol(graph, obj);
                return GraphAddRelation(graph, s, p, o);
            }
        }
    }

    /* ---- Fallback: original S-P-O token extraction ---- */

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
