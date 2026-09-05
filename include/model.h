#ifndef MODEL_H
#define MODEL_H

#include <stdint.h>
#include "graph.h"
#include "learning.h"
#include "embedding.h"

#define MODEL_MAGIC   0x53594D42
#define MODEL_VERSION 3 /* V3: procedencia (source) por relacion; V1/V2 leibles */

typedef struct
{
    GRAPH           *graph;
    EMBEDDING_TABLE *embeddings;
    LEARNING_CONFIG  config;
} MODEL;

MODEL *ModelCreate(uint32_t symbol_capacity, uint32_t relation_capacity);
void   ModelDestroy(MODEL *model);

int ModelSave(const MODEL *model, const char *filepath);
MODEL *ModelLoad(const char *filepath);

#endif
