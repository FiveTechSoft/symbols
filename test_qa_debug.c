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
    MODEL *m = ModelLoad("data/wiki_model.bin");
    if (!m || !m->graph)
    {
        printf("Notice: wiki_model.bin not found, creating synthetic model for qa_debug test...\n");
        m = ModelCreate(32, 32);
        if (!m || !m->graph) { printf("FAIL: no model\n"); return 1; }
        SYMBOL_ID fr = GraphAddSymbol(m->graph, "FRANCIA");
        SYMBOL_ID cap = GraphAddSymbol(m->graph, "CAPITAL");
        SYMBOL_ID pa = GraphAddSymbol(m->graph, "PARIS");
        GraphAddRelation(m->graph, fr, cap, pa);
    }

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
