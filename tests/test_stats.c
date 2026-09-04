#include <stdio.h>
#include "model.h"
#include "stats.h"

int main(int argc, char **argv)
{
    const char *path = (argc > 1) ? argv[1] : "wiki_model.bin";

    printf("Loading model from %s...\n", path);
    MODEL *model = ModelLoad(path);
    if (!model)
    {
        printf("FAIL: Could not load model\n");
        return 1;
    }

    ModelPrintReport(model);
    ModelDestroy(model);
    return 0;
}
