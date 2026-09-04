#include <stdio.h>
#include <stdlib.h>
#include "graph.h"

static void Assert(int cond, const char *msg)
{
    if (!cond)
    {
        printf("FAIL: %s\n", msg);
        exit(EXIT_FAILURE);
    }
}

int main(void)
{
    printf("========================================================\n");
    printf("    SYMBOLIC LLM - CONTRADICCIONES Y POLARIDAD (FASE 24)\n");
    printf("========================================================\n\n");

    GRAPH *graph = GraphCreate(32, 64);

    SYMBOL_ID pez     = GraphAddSymbol(graph, "PEZ");
    SYMBOL_ID vuela   = GraphAddSymbol(graph, "VUELA");
    SYMBOL_ID cielo   = GraphAddSymbol(graph, "CIELO");

    SYMBOL_ID gato    = GraphAddSymbol(graph, "GATO");
    SYMBOL_ID come    = GraphAddSymbol(graph, "COME");
    SYMBOL_ID veneno  = GraphAddSymbol(graph, "VENENO");
    SYMBOL_ID carne   = GraphAddSymbol(graph, "CARNE");

    printf("1. Registrando hecho negativo explicito:\n");
    printf("   'PEZ --NO_VUELA--> CIELO'\n");

    int ok = GraphAddRelationPolar(graph, pez, vuela, cielo, POLARITY_NEGATIVE, CONFLICT_REJECT_NEW);
    Assert(ok == 1, "Debe registrar hecho negativo");

    RELATION *r_neg = RelationFindPolar(graph->relations, pez, vuela, cielo, POLARITY_NEGATIVE);
    Assert(r_neg != NULL, "Debe encontrarse con polaridad negativa");
    Assert(r_neg->polarity == POLARITY_NEGATIVE, "Polaridad debe ser negativa");

    RELATION *r_pos = RelationFindPolar(graph->relations, pez, vuela, cielo, POLARITY_POSITIVE);
    Assert(r_pos == NULL, "No debe existir como hecho positivo");
    printf("   -> Hecho negativo verificado con exito.\n\n");

    printf("2. Intentando insertar contradiccion en modo estricto (REJECT_NEW):\n");
    printf("   Intentando afirmar: 'PEZ --VUELA--> CIELO'\n");

    int inserted = GraphAddRelationPolar(graph, pez, vuela, cielo, POLARITY_POSITIVE, CONFLICT_REJECT_NEW);
    Assert(inserted == 0, "Debe rechazar la contradiccion positiva frente al hecho negativo previo");
    printf("   -> Bloqueo automatico en O(1): la afirmacion contradictoria fue rechazada.\n\n");

    printf("3. Probando resolucion por volumen de evidencia acumulada:\n");

    GraphAddRelationPolar(graph, gato, come, carne, POLARITY_POSITIVE, CONFLICT_EVIDENCE_WINS);
    GraphAddRelationPolar(graph, gato, come, carne, POLARITY_POSITIVE, CONFLICT_EVIDENCE_WINS);
    GraphAddRelationPolar(graph, gato, come, carne, POLARITY_POSITIVE, CONFLICT_EVIDENCE_WINS);

    RELATION *r_carne = RelationFindPolar(graph->relations, gato, come, carne, POLARITY_POSITIVE);
    printf("   Evidencia inicial: GATO COME CARNE (count = %llu)\n", (unsigned long long)r_carne->count);
    Assert(r_carne->count == 3, "Debe tener 3 observaciones");

    printf("   Entra contradiccion: 'GATO NO_COME CARNE'...\n");
    GraphAddRelationPolar(graph, gato, come, carne, POLARITY_NEGATIVE, CONFLICT_EVIDENCE_WINS);

    r_carne = RelationFindPolar(graph->relations, gato, come, carne, POLARITY_POSITIVE);
    Assert(r_carne != NULL, "La afirmacion mayoritaria debe seguir viva");
    printf("   Evidencia tras contradiccion: count atenuado a %llu (la verdad mayoritaria prevalece)\n\n",
           (unsigned long long)r_carne->count);
    Assert(r_carne->count == 2, "El conteo debe reducirse a 2");

    printf("4. Probando modo de coexistencia para auditoria logica (ALLOW_BOTH):\n");
    GraphAddRelationPolar(graph, gato, come, veneno, POLARITY_POSITIVE, CONFLICT_ALLOW_BOTH);
    GraphAddRelationPolar(graph, gato, come, veneno, POLARITY_NEGATIVE, CONFLICT_ALLOW_BOTH);

    CONTRADICTION_REPORT rep = GraphCheckContradiction(graph, gato, come, veneno);
    Assert(rep.has_conflict == 1, "Debe detectar el conflicto activo");

    printf("   Alerta de contradiccion detectada en O(1)!\n");
    printf("   Sujeto: GATO | Accion: COME | Objeto: VENENO\n");
    printf("   - Evidencia a favor : %llu (peso: %.2f)\n", (unsigned long long)rep.positive_evidence, rep.positive_weight);
    printf("   - Evidencia en contra: %llu (peso: %.2f)\n", (unsigned long long)rep.negative_evidence, rep.negative_weight);

    GraphDestroy(graph);

    printf("\n========================================================\n");
    printf("Manejo de polaridad y contradicciones validado con exito.\n");
    printf("========================================================\n");

    return EXIT_SUCCESS;
}
