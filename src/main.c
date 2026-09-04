#include <stdio.h>
#include <stdlib.h>

#include "graph.h"


static void Assert(int condition, const char *message)
{
    if (!condition)
    {
        printf("FAIL: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static void PrintRelation(
    const RELATION *r,
    const SYMBOL_TABLE *symbols)
{
    const SYMBOL *s;
    const SYMBOL *p;
    const SYMBOL *o;

    if (r == NULL)
        return;

    s = SymbolGet(symbols, r->subject);
    p = SymbolGet(symbols, r->predicate);
    o = SymbolGet(symbols, r->object);

    printf(
        "  %s --%s--> %s\n",
        s ? s->name : "?",
        p ? p->name : "?",
        o ? o->name : "?"
    );
}


int main(void)
{
    KNOWLEDGE_GRAPH *graph;
    RELATION *results[32];
    uint32_t n;
    uint32_t i;


    printf("========================================\n");
    printf("       SYMBOLIC LLM - GRAPH QUERY\n");
    printf("========================================\n\n");


    /* --------------------------------------------------------
       Crear grafo
       -------------------------------------------------------- */

    graph = GraphCreate(64, 64);
    Assert(graph != NULL, "GraphCreate");


    /* --------------------------------------------------------
       Cargar conocimiento
       --------------------------------------------------------

       GATO  --ES---> ANIMAL
       PERRO --ES---> ANIMAL

       GATO  --COME-> PEZ
       GATO  --COME-> CARNE
       PERRO --COME-> CARNE

       -------------------------------------------------------- */

    GraphAddRelation(graph, "GATO", "ES", "ANIMAL");
    GraphAddRelation(graph, "PERRO", "ES", "ANIMAL");

    GraphAddRelation(graph, "GATO", "COME", "PEZ");
    GraphAddRelation(graph, "GATO", "COME", "CARNE");
    GraphAddRelation(graph, "PERRO", "COME", "CARNE");

    printf("Conocimiento cargado: %u relaciones\n\n",
           RelationCount(graph->relations));


    /* --------------------------------------------------------
       Query: ¿Qué es GATO?
       -------------------------------------------------------- */

    printf("--- Que es GATO? ---\n\n");

    n = GraphQuerySubjectPredicate(graph, "GATO", "ES", results, 32);

    for (i = 0; i < n; i++)
        PrintRelation(results[i], graph->symbols);

    Assert(n == 1, "GATO deberia tener 1 relacion ES");


    /* --------------------------------------------------------
       Query: ¿Qué come GATO?
       -------------------------------------------------------- */

    printf("\n--- Que come GATO? ---\n\n");

    n = GraphQuerySubjectPredicate(graph, "GATO", "COME", results, 32);

    for (i = 0; i < n; i++)
        PrintRelation(results[i], graph->symbols);

    Assert(n == 2, "GATO deberia tener 2 relaciones COME");


    /* --------------------------------------------------------
       Query: ¿Qué come PERRO?
       -------------------------------------------------------- */

    printf("\n--- Que come PERRO? ---\n\n");

    n = GraphQuerySubjectPredicate(graph, "PERRO", "COME", results, 32);

    for (i = 0; i < n; i++)
        PrintRelation(results[i], graph->symbols);

    Assert(n == 1, "PERRO deberia tener 1 relacion COME");


    /* --------------------------------------------------------
       Query: ¿Qué animales conocemos?
         (¿Qué tiene ES → ANIMAL?)
       -------------------------------------------------------- */

    printf("\n--- Que animales conocemos? ---\n\n");

    n = GraphQueryPredicateObject(graph, "ES", "ANIMAL", results, 32);

    for (i = 0; i < n; i++)
        PrintRelation(results[i], graph->symbols);

    Assert(n == 2, "Deberiamos conocer 2 animales");


    /* --------------------------------------------------------
       Query: ¿Quién come CARNE?
       -------------------------------------------------------- */

    printf("\n--- Quien come CARNE? ---\n\n");

    n = GraphQueryPredicateObject(graph, "COME", "CARNE", results, 32);

    for (i = 0; i < n; i++)
        PrintRelation(results[i], graph->symbols);

    Assert(n == 2, "2 especies comen carne");


    /* --------------------------------------------------------
       Query: Todo lo que sabemos de GATO
       -------------------------------------------------------- */

    printf("\n--- Todo lo que sabemos de GATO ---\n\n");

    n = GraphQuerySubject(graph, "GATO", results, 32);

    for (i = 0; i < n; i++)
        PrintRelation(results[i], graph->symbols);

    Assert(n == 3, "GATO deberia tener 3 relaciones en total");


    /* --------------------------------------------------------
       Query: relación exacta
       -------------------------------------------------------- */

    printf("\n--- Verificacion exacta ---\n\n");

    Assert(
        GraphFindRelation(graph, "GATO", "ES", "ANIMAL") != NULL,
        "GATO --ES--> ANIMAL debe existir"
    );

    Assert(
        GraphFindRelation(graph, "GATO", "COME", "PEZ") != NULL,
        "GATO --COME--> PEZ debe existir"
    );

    Assert(
        GraphFindRelation(graph, "GATO", "COME", "CARNE") != NULL,
        "GATO --COME--> CARNE debe existir"
    );

    Assert(
        GraphFindRelation(graph, "PERRO", "ES", "ANIMAL") != NULL,
        "PERRO --ES--> ANIMAL debe existir"
    );

    Assert(
        GraphFindRelation(graph, "PERRO", "COME", "CARNE") != NULL,
        "PERRO --COME--> CARNE debe existir"
    );

    Assert(
        GraphFindRelation(graph, "PERRO", "COME", "PEZ") == NULL,
        "PERRO --COME--> PEZ NO debe existir"
    );

    printf("Todas las relaciones verificadas OK\n");


    /* --------------------------------------------------------
       Liberar memoria
       -------------------------------------------------------- */

    GraphDestroy(graph);


    printf("\n========================================\n");
    printf("All graph tests passed.\n");
    printf("========================================\n");

    return EXIT_SUCCESS;
}
