#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "graph.h"
#include "symbol.h"
#include "model.h"

/* QA answer hygiene: no answer may contain corpus junk markers
   (markdown leftovers, wiki braces, mojibake, unbalanced parens).
   Reads the same set as test_eval_qa. Only judges answered
   questions; misses are covered by the test_eval_qa threshold. */
static const char *JUNK[] = {
    "}}", "{{", "[[", "]]", "*", "Ã", "Â", "_(", "(_", NULL
};

static int HasUnbalancedParens(const char *s)
{
    int open = 0, close = 0;
    for (; *s; s++)
    {
        if (*s == '(') open++;
        if (*s == ')') close++;
    }
    return open != close;
}

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
    uint32_t checked = 0, dirty = 0;
    while (fgets(line, sizeof(line), f))
    {
        TrimNL(line);
        if (line[0] == '\0' || line[0] == '#')
            continue;
        char *tab = strchr(line, '\t');
        if (!tab)
            continue;
        *tab = '\0';
        QUESTION q = ParserDetectQuestion(graph, line);
        char answer[256] = {0};
        int found = (q.valid && q.is_question) ?
            ParserAnswerQuestion(graph, &q, answer, sizeof(answer)) : 0;
        if (!found)
            continue;
        checked++;
        const char *hit = NULL;
        for (int i = 0; JUNK[i]; i++)
            if (strstr(answer, JUNK[i]) != NULL) { hit = JUNK[i]; break; }
        if (!hit && HasUnbalancedParens(answer))
            hit = "paren-desbalanceado";
        if (hit)
        {
            dirty++;
            printf("  SUCIA: '%s' -> '%s' (marca: %s)\n", line, answer, hit);
        }
    }
    fclose(f);

    printf("QA hygiene: %u/%u limpias\n", checked - dirty, checked);
    ModelDestroy(m);
    return (checked > 0 && dirty == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
