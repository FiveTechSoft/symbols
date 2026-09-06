#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "graph.h"
#include "symbol.h"
#include "model.h"

/* Puerta de regresion QA: exige EVAL_QA_THRESHOLD % de aciertos.
   El set vive en tests/qa_eval.tsv (pregunta \t respuesta).
   No modifica el modelo: solo ModelLoad + consultas. */
#ifndef EVAL_QA_THRESHOLD
#define EVAL_QA_THRESHOLD 90
#endif

static void TrimNL(char *s)
{
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r'))
        s[--n] = '\0';
}

/* Fold Spanish diacritics (UTF-8) to ASCII base, both sides of the
   comparison: the map stores HÚNGARO, humans write HUNGRARO — same
   word. Ñ is a letter, not an accent: preserved. Measurement-only;
   the sets are never touched. */
static void FoldAccents(const char *src, char *dst, size_t dst_size)
{
    static const struct { const char *from; char to; } tab[] = {
        {"\xC3\xA1", 'A'}, {"\xC3\xA9", 'E'}, {"\xC3\xAD", 'I'},
        {"\xC3\xB3", 'O'}, {"\xC3\xBA", 'U'}, {"\xC3\xBC", 'U'},
        {"\xC3\x81", 'A'}, {"\xC3\x89", 'E'}, {"\xC3\x8D", 'I'},
        {"\xC3\x93", 'O'}, {"\xC3\x9A", 'U'}, {"\xC3\x9C", 'U'},
        /* Mirrors NormalizeDiacritics (engine folds Ñ→N everywhere,
           subjects included): measurement must judge likewise. */
        {"\xC3\x91", 'N'}, {"\xC3\xB1", 'N'},
    };
    size_t o = 0;
    while (*src && o + 1 < dst_size)
    {
        int hit = 0;
        for (size_t t = 0; t < sizeof(tab) / sizeof(tab[0]); t++)
        {
            if (strncmp(src, tab[t].from, 2) == 0)
            {
                dst[o++] = tab[t].to;
                src += 2;
                hit = 1;
                break;
            }
        }
        if (!hit)
            dst[o++] = *src++;
    }
    dst[o] = '\0';
}

int main(int argc, char **argv)
{
    const char *tsv_path = (argc > 1) ? argv[1] : "tests/qa_eval.tsv";
    const char *model_path = (argc > 2) ? argv[2] : "wiki_model.bin";

    FILE *f = fopen(tsv_path, "r");
    if (!f) { printf("FAIL: no puedo abrir %s\n", tsv_path); return 2; }

    MODEL *m = ModelLoad(model_path);
    if (!m || !m->graph) { printf("FAIL: no model %s\n", model_path); fclose(f); return 2; }
    GRAPH *graph = m->graph;

    char line[1024];
    uint32_t total = 0, pass = 0;
    while (fgets(line, sizeof(line), f))
    {
        TrimNL(line);
        if (line[0] == '\0' || line[0] == '#')
            continue;
        char *tab = strchr(line, '\t');
        if (!tab)
            continue;
        *tab = '\0';
        const char *question = line;
        const char *expected = tab + 1;
        if (expected[0] == '\0')
            continue;

        total++;
        QUESTION q = ParserDetectQuestion(graph, question);
        char answer[256] = {0};
        int found = (q.valid && q.is_question) ?
            ParserAnswerQuestion(graph, &q, answer, sizeof(answer)) : 0;
        char fanswer[256] = {0}, fexpected[256] = {0};
        FoldAccents(found ? answer : "", fanswer, sizeof(fanswer));
        FoldAccents(expected, fexpected, sizeof(fexpected));
        int ok = (found && strstr(fanswer, fexpected) != NULL);
        if (ok)
            pass++;
        else
            printf("  MISS #%u: '%s' -> '%s' (esperaba '%s', subj='%s' rel='%s')\n",
                   total, question, found ? answer : "<sin respuesta>",
                   expected, q.subject, q.relation);
    }
    fclose(f);

    double acc = total ? (100.0 * pass / total) : 0.0;
    printf("QA eval: %u/%u = %.1f%% (umbral %d%%)\n",
           pass, total, acc, EVAL_QA_THRESHOLD);

    ModelDestroy(m);
    return (total > 0 && acc >= EVAL_QA_THRESHOLD) ? EXIT_SUCCESS : EXIT_FAILURE;
}
