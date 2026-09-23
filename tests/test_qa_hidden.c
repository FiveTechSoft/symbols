/* ============================================================
   test_qa_hidden.c: Hidden QA split runner (measure only).

   Loads tests/qa_hidden.tsv and the tracked golden wiki_model.bin,
   answers each question with the same judge as test_eval_qa
   (accent fold + substring), and prints:

     HIDDEN_QA pass=<n> total=<n> rate=<0..1>

   Accuracy is NEVER used as a development gate in this binary:
   the process exits 0 whenever the TSV is well-formed and the
   model loaded, so operators cannot red-team a threshold by
   editing the hidden set. Structural failures only:

     77  model or TSV missing (skip)
      2  TSV unreadable or zero data rows

   Do not edit tests/qa_hidden.tsv to raise the printed rate.
   ============================================================ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "graph.h"
#include "symbol.h"
#include "model.h"

static void TrimNL(char *s)
{
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r'))
        s[--n] = '\0';
}

static void FoldAccents(const char *src, char *dst, size_t dst_size)
{
    static const struct { const char *from; char to; } tab[] = {
        {"\xC3\xA1", 'A'}, {"\xC3\xA9", 'E'}, {"\xC3\xAD", 'I'},
        {"\xC3\xB3", 'O'}, {"\xC3\xBA", 'U'}, {"\xC3\xBC", 'U'},
        {"\xC3\x81", 'A'}, {"\xC3\x89", 'E'}, {"\xC3\x8D", 'I'},
        {"\xC3\x93", 'O'}, {"\xC3\x9A", 'U'}, {"\xC3\x9C", 'U'},
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
    const char *tsv_path = (argc > 1) ? argv[1] : "tests/qa_hidden.tsv";
    const char *model_path = (argc > 2) ? argv[2] : "wiki_model.bin";

    FILE *f = fopen(tsv_path, "r");
    if (!f)
    {
        printf("HIDDEN_QA pass=0 total=0 rate=0\n");
        return 77;
    }

    MODEL *m = ModelLoad(model_path);
    if (!m || !m->graph)
    {
        printf("HIDDEN_QA pass=0 total=0 rate=0\n");
        fclose(f);
        if (m)
            ModelDestroy(m);
        return 77;
    }
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
        if (found && strstr(fanswer, fexpected) != NULL)
            pass++;
    }
    fclose(f);

    if (total == 0)
    {
        printf("HIDDEN_QA pass=0 total=0 rate=0\n");
        ModelDestroy(m);
        return 2;
    }

    printf("HIDDEN_QA pass=%u total=%u rate=%.4f\n",
           pass, total, (double)pass / (double)total);
    ModelDestroy(m);
    return 0;
}
