#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symbol.h"
#include "embedding.h"

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
    printf("========================================\n");
    printf("     SYMBOLIC LLM - 32D EMBEDDINGS      \n");
    printf("========================================\n\n");

    SYMBOL_TABLE *symbols = SymbolTableCreate(32);
    EMBEDDING_TABLE *embeds = EmbeddingTableCreate(32);

    /* 1. Registrar simbolos */
    SYMBOL_ID s_gato   = SymbolAdd(symbols, "GATO");
    SYMBOL_ID s_felino = SymbolAdd(symbols, "FELINO");
    SYMBOL_ID s_perro  = SymbolAdd(symbols, "PERRO");
    SYMBOL_ID s_auto   = SymbolAdd(symbols, "AUTOMOVIL");

    /* 2. Inicializar con vectores aleatorios base (Random Indexing) */
    float v_gato[EMBEDDING_DIM];
    float v_felino[EMBEDDING_DIM];
    float v_perro[EMBEDDING_DIM];
    float v_auto[EMBEDDING_DIM];

    EmbeddingRandomInit(v_gato, 42);
    EmbeddingRandomInit(v_felino, 99);
    EmbeddingRandomInit(v_perro, 123);
    EmbeddingRandomInit(v_auto, 777);

    EmbeddingSetVector(embeds, s_gato, v_gato);
    EmbeddingSetVector(embeds, s_felino, v_felino);
    EmbeddingSetVector(embeds, s_perro, v_perro);
    EmbeddingSetVector(embeds, s_auto, v_auto);

    printf("Similitud inicial GATO vs FELINO (sin contexto compartido): %.3f\n",
           EmbeddingCosineSimilarity(EmbeddingGetVector(embeds, s_gato),
                                     EmbeddingGetVector(embeds, s_felino)));

    /* 3. Simular aprendizaje por coocurrencia semantica */
    printf("\nEntrenando coocurrencias contextuales...\n");
    const float *ctx_pescado = EmbeddingGetVector(embeds, s_gato);

    for (int step = 0; step < 10; step++)
    {
        float vec_gato_buf[EMBEDDING_DIM];
        float vec_felino_buf[EMBEDDING_DIM];
        const float *src_gato = EmbeddingGetVector(embeds, s_gato);
        const float *src_felino = EmbeddingGetVector(embeds, s_felino);
        memcpy(vec_gato_buf, src_gato, sizeof(vec_gato_buf));
        memcpy(vec_felino_buf, src_felino, sizeof(vec_felino_buf));

        EmbeddingCooccur(vec_gato_buf, ctx_pescado, 0.2f);
        EmbeddingCooccur(vec_felino_buf, ctx_pescado, 0.2f);

        EmbeddingSetVector(embeds, s_gato, vec_gato_buf);
        EmbeddingSetVector(embeds, s_felino, vec_felino_buf);
    }

    /* 4. Evaluar similitud */
    float final_sim = EmbeddingCosineSimilarity(
        EmbeddingGetVector(embeds, s_gato),
        EmbeddingGetVector(embeds, s_felino));

    printf("\nSimilitud final GATO vs FELINO: %.3f\n", final_sim);

    EMBEDDING_MATCH matches[4];
    uint32_t n = EmbeddingFindSimilar(embeds, s_gato, matches, 4);

    printf("\n--- Vecinos de GATO ---\n");
    for (uint32_t i = 0; i < n; i++)
    {
        const SYMBOL *sym = SymbolGet(symbols, matches[i].id);
        printf("  %-12s | coseno=%.3f\n",
               sym ? sym->name : "?", matches[i].score);
    }

    Assert(n > 0, "Debe encontrar vecinos cercanos");
    const SYMBOL *top_match = SymbolGet(symbols, matches[0].id);
    printf("\nSinonimo detectado: %s\n", top_match->name);
    Assert(strcmp(top_match->name, "FELINO") == 0, "FELINO debe ser el mas cercano");

    /* 5. Verificar que AUTOMOVIL queda lejos */
    float sim_auto = EmbeddingCosineSimilarity(
        EmbeddingGetVector(embeds, s_gato),
        EmbeddingGetVector(embeds, s_auto));
    printf("GATO vs AUTOMOVIL: %.3f (debe ser bajo)\n", sim_auto);

    EmbeddingTableDestroy(embeds);
    SymbolTableDestroy(symbols);

    printf("\n========================================\n");
    printf("Embeddings y similitud verificados OK.\n");
    printf("========================================\n");

    return EXIT_SUCCESS;
}
