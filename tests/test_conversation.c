#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "graph.h"
#include "context.h"
#include "learning.h"
#include "dialog.h"

int main(void)
{
    srand((unsigned int)time(NULL));

    GRAPH *graph = GraphCreate(128, 256);
    CONTEXT *ctx = ContextCreate();
    char reply[512];

    printf("========================================================\n");
    printf("     SYMBOLIC LLM - TEST CONVERSACIONAL NATURAL         \n");
    printf("========================================================\n\n");

    const char *conversation[] = {
        "Hola! Buenos dias",
        "Antonio programa en Harbour.",
        "Harbour es un lenguaje xBase.",
        "Que programa Antonio?",
        "El compila con hbmk2.",
        "Con que compila Antonio?",
        "Que come el perro?",
        "Muchas gracias por la ayuda!",
        "Hasta luego"
    };

    uint32_t num_turns = sizeof(conversation) / sizeof(conversation[0]);

    for (uint32_t i = 0; i < num_turns; i++)
    {
        printf("Usuario > %s\n", conversation[i]);
        DialogGenerateResponse(graph, ctx, conversation[i], reply, sizeof(reply));
        printf("Modelo  > %s\n\n", reply);
    }

    ContextDestroy(ctx);
    GraphDestroy(graph);

    return 0;
}
