#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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
    printf("==================================================\n");
    printf("  SYMBOLIC LLM - MODEL PERSISTENCE V2 (WITH 32D)  \n");
    printf("==================================================\n\n");

    const char *filepath = "test_v2.bin";

    /* 1. Entrenar Grafo y Asignar Embeddings */
    printf("1. Inicializando modelo y vectores en memoria...\n");
    MODEL *original = ModelCreate(32, 64);
    Assert(original != NULL, "ModelCreate");

    LearningSentence(original->graph, "Gato come pescado.");
    LearningSentence(original->graph, "Gato come carne.");

    SYMBOL_ID gato   = SymbolFind(original->graph->symbols, "GATO");
    SYMBOL_ID felino = GraphAddSymbol(original->graph, "FELINO");

    float v_gato[EMBEDDING_DIM];
    float v_felino[EMBEDDING_DIM];

    EmbeddingRandomInit(v_gato, 42);
    memcpy(v_felino, v_gato, sizeof(v_gato));
    v_felino[0] += 0.01f;
    EmbeddingNormalize(v_felino);

    EmbeddingSetVector(original->embeddings, gato, v_gato);
    EmbeddingSetVector(original->embeddings, felino, v_felino);

    float sim_orig = EmbeddingCosineSimilarity(
        EmbeddingGetVector(original->embeddings, gato),
        EmbeddingGetVector(original->embeddings, felino));
    printf("   Similitud original GATO vs FELINO: %.4f\n", sim_orig);

    /* 2. Guardar Modelo V2 a Disco */
    printf("\n2. Serializando modelo V2 a '%s'...\n", filepath);
    Assert(ModelSave(original, filepath) == 1, "ModelSave V2");

    FILE *f = fopen(filepath, "rb");
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fclose(f);
    printf("   Tamano total en disco (incluyendo vectores 32D): %ld bytes\n", fsize);

    /* 3. Destruir Modelo Original */
    printf("\n3. Destruyendo modelo original de RAM...\n");
    ModelDestroy(original);
    original = NULL;

    /* 4. Deserializar Modelo V2 desde Disco */
    printf("\n4. Deserializando modelo desde disco...\n");
    MODEL *loaded = ModelLoad(filepath);
    Assert(loaded != NULL, "ModelLoad V2");

    printf("   Simbolos restaurados  : %u\n", SymbolCount(loaded->graph->symbols));
    printf("   Relaciones restauradas: %u\n", RelationCount(loaded->graph->relations));

    /* 5. Validar Vectores e Inferencia Hibrida */
    printf("\n5. Verificando integridad de los embeddings cargados:\n");

    SYMBOL_ID l_gato   = SymbolFind(loaded->graph->symbols, "GATO");
    SYMBOL_ID l_felino = SymbolFind(loaded->graph->symbols, "FELINO");
    SYMBOL_ID l_come   = SymbolFind(loaded->graph->symbols, "COME");

    Assert(l_gato != SYMBOL_INVALID && l_felino != SYMBOL_INVALID, "Simbolos encontrados");

    const float *lv_gato   = EmbeddingGetVector(loaded->embeddings, l_gato);
    const float *lv_felino = EmbeddingGetVector(loaded->embeddings, l_felino);

    Assert(lv_gato != NULL && lv_felino != NULL, "Vectores presentes");

    float sim_loaded = EmbeddingCosineSimilarity(lv_gato, lv_felino);
    printf("   Similitud restaurada GATO vs FELINO: %.4f (esperado: %.4f)\n",
           sim_loaded, sim_orig);
    Assert(fabsf(sim_loaded - sim_orig) < 1e-5f, "Similitud identica");

    RELATION *fuzzy_results[8];
    SYMBOL_ID resolved = SYMBOL_INVALID;

    uint32_t n = GraphQuerySubjectRelationFuzzy(
        loaded->graph, l_felino, l_come,
        fuzzy_results, 8, 0.75f, &resolved);

    Assert(n > 0, "Debe resolver la consulta difusa");
    Assert(resolved == l_gato, "FELINO debe resolver a GATO");

    printf("\n   Consulta Fuzzy: 'Que come el FELINO?'\n");
    char output[256];
    GENERATOR_CONFIG cfg = GeneratorConfigDefault();
    GeneratorAggregateRelations(loaded->graph, (const RELATION **)fuzzy_results, n, &cfg, output, sizeof(output));
    printf("   Respuesta: \"%s\"\n\n", output);

    ModelDestroy(loaded);
    remove(filepath);

    printf("==================================================\n");
    printf("Persistencia binaria V2 (Grafo + 32D) validada OK.\n");
    printf("==================================================\n");

    return EXIT_SUCCESS;
}
