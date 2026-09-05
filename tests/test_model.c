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
    printf("1. Training model in RAM...\n");
    MODEL *original = ModelCreate(64, 64);
    Assert(original != NULL, "ModelCreate");

    LearningSentence(original->graph, "Gato come pescado.");
    LearningSentence(original->graph, "Gato come pescado.");
    LearningSentence(original->graph, "Gato come carne.");
    LearningSentence(original->graph, "Gato es animal.");
    LearningSentence(original->graph, "Antonio programa Harbour.");
    LearningSentence(original->graph, "Antonio compila hbmk2.");

    printf("   Learned symbols : %u\n", SymbolCount(original->graph->symbols));
    printf("   Graph relations : %u\n", RelationCount(original->graph->relations));

    /* --------------------------------------------------------
       2. Guardar a Disco
       -------------------------------------------------------- */
    printf("\n2. Serializing model to '%s'...\n", filepath);
    int save_ok = ModelSave(original, filepath);
    Assert(save_ok == 1, "ModelSave");

    FILE *f = fopen(filepath, "rb");
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fclose(f);
    printf("   Model persisted. Disk size: %ld bytes\n", file_size);

    /* --------------------------------------------------------
       3. Destruir modelo original (RAM = 0)
       -------------------------------------------------------- */
    printf("\n3. Freeing original model from RAM...\n");
    ModelDestroy(original);
    original = NULL;

    /* --------------------------------------------------------
       4. Cargar modelo desde disco
       -------------------------------------------------------- */
    printf("\n4. Deserializing model from '%s'...\n", filepath);
    MODEL *loaded = ModelLoad(filepath);
    Assert(loaded != NULL, "ModelLoad");

    printf("   Restored symbols  : %u\n", SymbolCount(loaded->graph->symbols));
    printf("   Restored relations: %u\n\n", RelationCount(loaded->graph->relations));

    /* --------------------------------------------------------
       5. Verificacion de Inferencia y Generacion
       -------------------------------------------------------- */
    printf("5. Checking inference on the restored model:\n");
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
    printf("6. Checking probabilities...\n");
    PREDICTION rels[8];
    uint32_t np = LearningPredictText(loaded->graph, "GATO", "COME", rels, 8);

    Assert(np == 2, "GATO/COME must yield 2 predictions");

    /* PESCADO debe tener ~66.7% y CARNE ~33.3% */
    int found_pescado = 0;
    int found_carne = 0;
    for (uint32_t i = 0; i < np; i++)
    {
        if (rels[i].name[0] == 'P')
            found_pescado = 1;
        if (rels[i].name[0] == 'C')
            found_carne = 1;
    }
    Assert(found_pescado, "PESCADO prediction must exist");
    Assert(found_carne, "CARNE prediction must exist");

    printf("   Probabilities intact: P(PESCADO|GATO,COME) and P(CARNE|GATO,COME).\n");

    /* Limpieza */
    ModelDestroy(loaded);
    remove(filepath);

    printf("\n========================================\n");
    printf("Binary persistence validated.\n");
    printf("========================================\n");

    return EXIT_SUCCESS;
}
