#include <stdio.h>
#include "model.h"
#include "stats.h"

int main(int argc, char **argv)
{
    const char *path = (argc > 1) ? argv[1] : "data/wiki_model.bin";

    printf("Loading model from %s...\n", path);
    MODEL *model = ModelLoad(path);
    if (!model)
    {
        printf("Notice: '%s' not present, creating test model for stats report...\n", path);
        model = ModelCreate(16, 16);
        if (!model)
        {
            printf("FAIL: Could not create model\n");
            return 1;
        }
        SYMBOL_ID s1 = GraphAddSymbol(model->graph, "GATO");
        SYMBOL_ID s2 = GraphAddSymbol(model->graph, "COME");
        SYMBOL_ID s3 = GraphAddSymbol(model->graph, "PESCADO");
        GraphAddRelation(model->graph, s1, s2, s3);
    }

    ModelPrintReport(model);
    ModelDestroy(model);
    return 0;
}
