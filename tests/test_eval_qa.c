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
        QUESTION q = ParserDetectQuestion(question);
        char answer[256] = {0};
        int found = (q.valid && q.is_question) ?
            ParserAnswerQuestion(graph, &q, answer, sizeof(answer)) : 0;
        int ok = (found && strstr(answer, expected) != NULL);
        if (ok)
            pass++;
        else
            printf("  MISS #%u: '%s' -> '%s' (esperaba '%s', subj='%s' pred='%s')\n",
                   total, question, found ? answer : "<sin respuesta>",
                   expected, q.subject, q.predicate);
    }
    fclose(f);

    double acc = total ? (100.0 * pass / total) : 0.0;
    printf("QA eval: %u/%u = %.1f%% (umbral %d%%)\n",
           pass, total, acc, EVAL_QA_THRESHOLD);

    ModelDestroy(m);
    return (total > 0 && acc >= EVAL_QA_THRESHOLD) ? EXIT_SUCCESS : EXIT_FAILURE;
}
