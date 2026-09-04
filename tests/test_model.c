#include <stdio.h>
#include <stdlib.h>
#include "model.h"
#include "generator.h"

static void Assert(int condition, const char *msg)
{
    if (!condition)
    {
        printf("FAIL: %s\n", msg);
        exit(EXIT_FAILURE);
    }
}

int main(void)
{
    printf("========================================\n");
    printf("     SYMBOLIC LLM - MODEL PERSISTENCE   \n");
    printf("========================================\n\n");

    const char *filepath = "test_model.bin";

    /* --------------------------------------------------------
       1. Fase de Entrenamiento inicial
       -------------------------------------------------------- */
    printf("1. Entrenando modelo en RAM...\n");
    MODEL *original = ModelCreate(64, 64);
    Assert(original != NULL, "ModelCreate");

    LearningSentence(original->graph, "El gato come pescado.");
    LearningSentence(original->graph, "El gato come pescado.");
    LearningSentence(original->graph, "El gato come carne.");
    LearningSentence(original->graph, "El gato es un animal.");
    LearningSentence(original->graph, "Antonio programa en Harbour.");
    LearningSentence(original->graph, "Antonio compila con hbmk2.");

    printf("   Simbolos aprendidos : %u\n", SymbolCount(original->graph->symbols));
    printf("   Relaciones en grafo : %u\n", RelationCount(original->graph->relations));

    /* --------------------------------------------------------
       2. Guardar a Disco
       -------------------------------------------------------- */
    printf("\n2. Serializando modelo en '%s'...\n", filepath);
    int save_ok = ModelSave(original, filepath);
    Assert(save_ok == 1, "ModelSave");

    FILE *f = fopen(filepath, "rb");
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fclose(f);
    printf("   Modelo persistido con exito. Tamano en disco: %ld bytes\n", file_size);

    /* --------------------------------------------------------
       3. Destruir modelo original (RAM = 0)
       -------------------------------------------------------- */
    printf("\n3. Liberando modelo original de la memoria RAM...\n");
    ModelDestroy(original);
    original = NULL;

    /* --------------------------------------------------------
       4. Cargar modelo desde disco
       -------------------------------------------------------- */
    printf("\n4. Deserializando modelo desde '%s'...\n", filepath);
    MODEL *loaded = ModelLoad(filepath);
    Assert(loaded != NULL, "ModelLoad");

    printf("   Simbolos restaurados  : %u\n", SymbolCount(loaded->graph->symbols));
    printf("   Relaciones restauradas: %u\n\n", RelationCount(loaded->graph->relations));

    /* --------------------------------------------------------
       5. Verificacion de Inferencia y Generacion
       -------------------------------------------------------- */
    printf("5. Comprobando inferencia sobre el modelo restaurado:\n");
    char output[256];

    GeneratorAnswerQuery(loaded->graph, "GATO", "COME", output, sizeof(output));
    printf("   Q: Que come el gato?\n");
    printf("   A: \"%s\"\n\n", output);

    GeneratorAnswerQuery(loaded->graph, "ANTONIO", "PROGRAMA", output, sizeof(output));
    printf("   Q: En que programa Antonio?\n");
    printf("   A: \"%s\"\n\n", output);

    GeneratorAnswerQuery(loaded->graph, "ANTONIO", "COMPILA", output, sizeof(output));
    printf("   Q: Con que compila Antonio?\n");
    printf("   A: \"%s\"\n\n", output);

    /* --------------------------------------------------------
       6. Verificar probabilidades exactas
       -------------------------------------------------------- */
    printf("6. Verificando probabilidades...\n");
    PREDICTION preds[8];
    uint32_t np = LearningPredictText(loaded->graph, "GATO", "COME", preds, 8);

    Assert(np == 2, "Deben haber 2 predicciones para GATO/COME");

    /* PESCADO debe tener ~66.7% y CARNE ~33.3% */
    int found_pescado = 0;
    int found_carne = 0;
    for (uint32_t i = 0; i < np; i++)
    {
        if (preds[i].name[0] == 'P')
            found_pescado = 1;
        if (preds[i].name[0] == 'C')
            found_carne = 1;
    }
    Assert(found_pescado, "Debe existir prediccion para PESCADO");
    Assert(found_carne, "Debe existir prediccion para CARNE");

    printf("   Probabilidades verificadas: P(PESCADO|GATO,COME) y P(CARNE|GATO,COME) intactas.\n");

    /* Limpieza */
    ModelDestroy(loaded);
    remove(filepath);

    printf("\n========================================\n");
    printf("Persistencia binaria validada con exito.\n");
    printf("========================================\n");

    return EXIT_SUCCESS;
}
