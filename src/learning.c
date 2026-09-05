#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "learning.h"
#include "stem.h"
#include "parser.h"

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

/* ============================================================
   Sentence ingest: single syntax-tree path (see parser.c).
   Input → tree → symbols → relations. No patterns, no stop
   lists, no canonicalization tables: the map provides the
   vocabulary, positions provide the structure.
   ============================================================ */

int LearningSentence(GRAPH *graph, const char *sentence)
{
    return ParserIngestSentence(graph, sentence);
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
    SYMBOL_ID relation,
    PREDICTION *out_predictions,
    uint32_t max_predictions)
{
    if (graph == NULL || out_predictions == NULL || max_predictions == 0)
        return 0;

    RELATION *candidates[64];
    uint32_t count = GraphQuerySubjectRelation(
        (GRAPH *)graph,
        subject,
        relation,
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
    const char *relation_name,
    PREDICTION *out_predictions,
    uint32_t max_predictions)
{
    if (graph == NULL || subject_name == NULL || relation_name == NULL)
        return 0;

    char upper_s[64];
    char upper_p[64];
    StrToUpper(subject_name, upper_s, sizeof(upper_s));
    StrToUpper(relation_name, upper_p, sizeof(upper_p));

    SYMBOL_ID s = StemFindSymbol(graph->symbols, upper_s);
    SYMBOL_ID p = StemFindSymbol(graph->symbols, upper_p);

    if (s == SYMBOL_INVALID || p == SYMBOL_INVALID)
        return 0;

    return LearningPredict(graph, s, p, out_predictions, max_predictions);
}
