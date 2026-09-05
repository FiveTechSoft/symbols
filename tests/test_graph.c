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
    p = SymbolGet(symbols, r->relation);
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
    GRAPH *graph;
    SYMBOL_ID gato, perro, animal, ser_vivo;
    SYMBOL_ID es, come;
    SYMBOL_ID pez, carne;
    RELATION *results[32];
    uint32_t n;


    printf("========================================\n");
    printf("       SYMBOLIC LLM - GRAPH TEST\n");
    printf("========================================\n\n");


    /* --------------------------------------------------------
       Crear grafo
       -------------------------------------------------------- */

    graph = GraphCreate(64, 64);
    Assert(graph != NULL, "GraphCreate");


    /* --------------------------------------------------------
       Crear símbolos
       -------------------------------------------------------- */

    gato     = GraphAddSymbol(graph, "GATO");
    perro    = GraphAddSymbol(graph, "PERRO");
    animal   = GraphAddSymbol(graph, "ANIMAL");
    ser_vivo = GraphAddSymbol(graph, "SER_VIVO");

    es       = GraphAddSymbol(graph, "ES");
    come     = GraphAddSymbol(graph, "COME");

    pez      = GraphAddSymbol(graph, "PEZ");
    carne    = GraphAddSymbol(graph, "CARNE");

    Assert(gato != SYMBOL_INVALID, "GATO");
    Assert(perro != SYMBOL_INVALID, "PERRO");
    Assert(animal != SYMBOL_INVALID, "ANIMAL");
    Assert(ser_vivo != SYMBOL_INVALID, "SER_VIVO");
    Assert(es != SYMBOL_INVALID, "ES");
    Assert(come != SYMBOL_INVALID, "COME");
    Assert(pez != SYMBOL_INVALID, "PEZ");
    Assert(carne != SYMBOL_INVALID, "CARNE");

    printf("Simbolos creados:\n");
    printf("  GATO=%u  PERRO=%u  ANIMAL=%u  SER_VIVO=%u\n",
           gato, perro, animal, ser_vivo);
    printf("  ES=%u  COME=%u  PEZ=%u  CARNE=%u\n\n",
           es, come, pez, carne);


    /* --------------------------------------------------------
       Cargar conocimiento

       GATO  --ES---> ANIMAL
       PERRO --ES---> ANIMAL
       ANIMAL --ES--> SER_VIVO

       GATO  --COME-> PEZ
       GATO  --COME-> CARNE
       PERRO --COME-> CARNE
       -------------------------------------------------------- */

    GraphAddRelation(graph, gato, es, animal);
    GraphAddRelation(graph, perro, es, animal);
    GraphAddRelation(graph, animal, es, ser_vivo);

    GraphAddRelation(graph, gato, come, pez);
    GraphAddRelation(graph, gato, come, carne);
    GraphAddRelation(graph, perro, come, carne);

    printf("Conocimiento base: %u relaciones\n\n",
           RelationCount(graph->relations));


    /* --------------------------------------------------------
       Query: Qué es GATO?
       -------------------------------------------------------- */

    printf("--- Que es GATO? ---\n\n");

    n = GraphQuerySubjectRelation(graph, gato, es, results, 32);
    Assert(n == 1, "GATO tiene 1 relacion ES");

    for (uint32_t i = 0; i < n; i++)
        PrintRelation(results[i], graph->symbols);


    /* --------------------------------------------------------
       Query: Qué come GATO?
       -------------------------------------------------------- */

    printf("\n--- Que come GATO? ---\n\n");

    n = GraphQuerySubjectRelation(graph, gato, come, results, 32);
    Assert(n == 2, "GATO tiene 2 relaciones COME");

    for (uint32_t i = 0; i < n; i++)
        PrintRelation(results[i], graph->symbols);


    /* --------------------------------------------------------
       Query: Qué animales conocemos?
       -------------------------------------------------------- */

    printf("\n--- Que animales conocemos? ---\n\n");

    n = GraphQueryObject(graph, animal, results, 32);
    Assert(n >= 2, "Al menos 2 animales");

    for (uint32_t i = 0; i < n; i++)
        PrintRelation(results[i], graph->symbols);


    /* --------------------------------------------------------
       Query: Todo lo de GATO
       -------------------------------------------------------- */

    printf("\n--- Todo lo que sabemos de GATO ---\n\n");

    n = GraphQuerySubject(graph, gato, results, 32);
    Assert(n == 3, "GATO tiene 3 relaciones");

    for (uint32_t i = 0; i < n; i++)
        PrintRelation(results[i], graph->symbols);


    /* --------------------------------------------------------
       INFERENCIA

       Cadena:  GATO --ES--> ANIMAL --ES--> SER_VIVO
       Resultado: GATO --ES--> SER_VIVO
       -------------------------------------------------------- */

    printf("\n--- Inferencia transitiva ---\n\n");

    printf("Relaciones antes: %u\n", RelationCount(graph->relations));

    int ok = GraphInferTransitive(graph, gato, es, ser_vivo);

    printf("Inferencia GATO->SER_VIVO: %s\n", ok ? "SI" : "NO");
    printf("Relaciones despues: %u\n\n", RelationCount(graph->relations));

    Assert(ok == 1, "Debe inferir GATO --ES--> SER_VIVO");

    Assert(
        GraphFindRelation(graph, gato, es, ser_vivo) != NULL,
        "GATO --ES--> SER_VIVO existe"
    );

    printf("Inferencia verificada:\n");
    PrintRelation(
        GraphFindRelation(graph, gato, es, ser_vivo),
        graph->symbols
    );


    /* --------------------------------------------------------
       Inferir PERRO tambien
       -------------------------------------------------------- */

    printf("\n--- Inferir PERRO --ES--> SER_VIVO ---\n\n");

    ok = GraphInferTransitive(graph, perro, es, ser_vivo);
    Assert(ok == 1, "Debe inferir PERRO --ES--> SER_VIVO");

    printf("Inferencia verificada:\n");
    PrintRelation(
        GraphFindRelation(graph, perro, es, ser_vivo),
        graph->symbols
    );


    /* --------------------------------------------------------
       Segunda inferencia: no crear redundancia
       -------------------------------------------------------- */

    printf("\n--- Segunda inferencia (sin cambios) ---\n\n");

    ok = GraphInferTransitive(graph, gato, es, ser_vivo);
    Assert(ok == 0, "No debe crear redundancia");

    printf("Resultado: %s (correcto)\n\n",
           ok ? "CREO ALGO" : "NADA NUEVO");


    /* --------------------------------------------------------
       Query final: Qué es GATO ahora?
       -------------------------------------------------------- */

    printf("--- Que es GATO ahora? (con inferencia) ---\n\n");

    n = GraphQuerySubjectRelation(graph, gato, es, results, 32);
    Assert(n == 2, "GATO ahora tiene 2 relaciones ES");

    for (uint32_t i = 0; i < n; i++)
        PrintRelation(results[i], graph->symbols);


    /* --------------------------------------------------------
       Query: Qué es SER_VIVO?
       -------------------------------------------------------- */

    printf("\n--- Que es SER_VIVO? (quien lo es) ---\n\n");

    n = GraphQueryObject(graph, ser_vivo, results, 32);
    Assert(n >= 2, "Al menos 2 seres vivos");

    for (uint32_t i = 0; i < n; i++)
        PrintRelation(results[i], graph->symbols);


    /* --------------------------------------------------------
       Liberar
       -------------------------------------------------------- */

    GraphDestroy(graph);


    printf("\n========================================\n");
    printf("All graph tests passed.\n");
    printf("========================================\n");

    return EXIT_SUCCESS;
}
