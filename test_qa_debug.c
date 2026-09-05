#include <stdio.h>
#include <string.h>
#include "parser.h"
#include "graph.h"
#include "symbol.h"
#include "embedding.h"
#include "model.h"

int main(void)
{
    /* Load model */
    MODEL *m = ModelLoad("wiki_model.bin");
    if (!m || !m->graph) { printf("FAIL: no model\n"); return 1; }

    GRAPH *graph = m->graph;

    /* Test question detection */
    const char *tests[] = {
        "la capital de Francia es",
        "quien es Paris",
        "donde esta Berlin",
        "que tierra es",
        NULL
    };

    for (int i = 0; tests[i]; i++)
    {
        printf("Input: \"%s\"\n", tests[i]);
        QUESTION q = ParserDetectQuestion(graph, tests[i]);
        printf("  is_question=%d valid=%d subject='%s' relation='%s'\n",
            q.is_question, q.valid, q.subject, q.relation);

        if (q.valid && q.is_question)
        {
            char answer[256] = {0};
            int found = ParserAnswerQuestion(graph, &q, answer, sizeof(answer));
            printf("  found=%d answer='%s'\n", found, answer);
        }
        printf("\n");
    }

    ModelDestroy(m);
    return 0;
}
