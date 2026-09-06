#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "model.h"

MODEL *ModelCreate(uint32_t symbol_capacity, uint32_t relation_capacity)
{
    MODEL *model = (MODEL *)malloc(sizeof(MODEL));
    if (model == NULL)
        return NULL;

    model->graph = GraphCreate(symbol_capacity, relation_capacity);
    model->embeddings = EmbeddingTableCreate(symbol_capacity);

    if (model->graph == NULL || model->embeddings == NULL)
    {
        GraphDestroy(model->graph);
        EmbeddingTableDestroy(model->embeddings);
        free(model);
        return NULL;
    }

    GraphSetEmbeddingTable(model->graph, model->embeddings);
    model->config = LearningConfigDefault();
    return model;
}

void ModelDestroy(MODEL *model)
{
    if (model == NULL)
        return;

    EmbeddingTableDestroy(model->embeddings);
    GraphDestroy(model->graph);
    free(model);
}

/* ============================================================
   Serializacion binaria V2
   ============================================================ */

int ModelSave(const MODEL *model, const char *filepath)
{
    if (model == NULL || model->graph == NULL || filepath == NULL)
        return 0;

    FILE *f = fopen(filepath, "wb");
    if (f == NULL)
        return 0;

    uint32_t magic = MODEL_MAGIC;
    uint32_t version = MODEL_VERSION;
    uint32_t sym_count = SymbolCount(model->graph->symbols);
    uint32_t rel_count = RelationCount(model->graph->relations);
    uint32_t emb_count = 0;
    uint32_t emb_dim = EMBEDDING_DIM;

    /* Contar embeddings inicializados */
    if (model->embeddings != NULL)
    {
        for (uint32_t i = 0; i < model->embeddings->count; i++)
        {
            if (model->embeddings->items[i].initialized)
                emb_count++;
        }
    }

    /* 1. Header V2 */
    if (fwrite(&magic,     sizeof(uint32_t), 1, f) != 1 ||
        fwrite(&version,   sizeof(uint32_t), 1, f) != 1 ||
        fwrite(&sym_count, sizeof(uint32_t), 1, f) != 1 ||
        fwrite(&rel_count, sizeof(uint32_t), 1, f) != 1 ||
        fwrite(&emb_count, sizeof(uint32_t), 1, f) != 1 ||
        fwrite(&emb_dim,   sizeof(uint32_t), 1, f) != 1)
    {
        fclose(f);
        return 0;
    }

    /* 2. Symbol block */
    for (uint32_t i = 0; i < sym_count; i++)
    {
        const SYMBOL *s = SymbolGet(model->graph->symbols, i + 1);
        if (s == NULL || s->name == NULL)
        {
            fclose(f);
            return 0;
        }

        uint32_t name_len = (uint32_t)strlen(s->name);

        if (fwrite(&s->id,       sizeof(SYMBOL_ID), 1, f) != 1 ||
            fwrite(&name_len,    sizeof(uint32_t), 1, f) != 1 ||
            fwrite(s->name,      1, name_len, f) != name_len ||
            fwrite(&s->frequency, sizeof(uint64_t), 1, f) != 1)
        {
            fclose(f);
            return 0;
        }
    }

    /* 3. Relation block */
    for (uint32_t i = 0; i < rel_count; i++)
    {
        const RELATION *r = RelationGet(model->graph->relations, i);
        if (r == NULL)
        {
            fclose(f);
            return 0;
        }

        if (fwrite(&r->subject,   sizeof(SYMBOL_ID), 1, f) != 1 ||
            fwrite(&r->relation, sizeof(SYMBOL_ID), 1, f) != 1 ||
            fwrite(&r->object,    sizeof(SYMBOL_ID), 1, f) != 1 ||
            fwrite(&r->count,     sizeof(uint64_t), 1, f) != 1 ||
            fwrite(&r->weight,    sizeof(float), 1, f) != 1 ||
            fwrite(&r->source,    sizeof(SYMBOL_ID), 1, f) != 1)
        {
            fclose(f);
            return 0;
        }
        {
            /* V4: polarity rides along (older readers stop above). */
            uint32_t pol = (uint32_t)r->polarity;
            if (fwrite(&pol, sizeof(uint32_t), 1, f) != 1)
            {
                fclose(f);
                return 0;
            }
        }
    }

    /* 4. Embeddings block (32D) */
    if (model->embeddings != NULL)
    {
        for (uint32_t i = 0; i < model->embeddings->count; i++)
        {
            const SYMBOL_EMBEDDING *e = &model->embeddings->items[i];
            if (!e->initialized)
                continue;

            if (fwrite(&e->id, sizeof(SYMBOL_ID), 1, f) != 1 ||
                fwrite(e->vector, sizeof(float), EMBEDDING_DIM, f) != EMBEDDING_DIM)
            {
                fclose(f);
                return 0;
            }
        }
    }

    fclose(f);
    return 1;
}

/* ============================================================
   Deserializacion binaria (compatible V1 y V2)
   ============================================================ */

MODEL *ModelLoad(const char *filepath)
{
    if (filepath == NULL)
        return NULL;

    FILE *f = fopen(filepath, "rb");
    if (f == NULL)
        return NULL;

    uint32_t magic, version, sym_count, rel_count;

    if (fread(&magic, sizeof(uint32_t), 1, f) != 1 || magic != MODEL_MAGIC)
    {
        fclose(f);
        return NULL;
    }

    if (fread(&version, sizeof(uint32_t), 1, f) != 1)
    {
        fclose(f);
        return NULL;
    }

    if (fread(&sym_count, sizeof(uint32_t), 1, f) != 1 ||
        fread(&rel_count, sizeof(uint32_t), 1, f) != 1)
    {
        fclose(f);
        return NULL;
    }

    /* V2: leer campos adicionales de embeddings */
    uint32_t emb_count = 0;
    uint32_t emb_dim = 0;

    if (version >= 2)
    {
        if (fread(&emb_count, sizeof(uint32_t), 1, f) != 1 ||
            fread(&emb_dim,   sizeof(uint32_t), 1, f) != 1)
        {
            fclose(f);
            return NULL;
        }
    }

    uint32_t sym_cap = sym_count < 16 ? 16 : sym_count * 2;
    uint32_t rel_cap = rel_count < 16 ? 16 : rel_count * 2;

    MODEL *model = ModelCreate(sym_cap, rel_cap);
    if (model == NULL)
    {
        fclose(f);
        return NULL;
    }

    /* Load symbols */
    for (uint32_t i = 0; i < sym_count; i++)
    {
        SYMBOL_ID id;
        uint32_t name_len;
        uint64_t frequency;

        if (fread(&id,       sizeof(SYMBOL_ID), 1, f) != 1 ||
            fread(&name_len, sizeof(uint32_t), 1, f) != 1)
        {
            ModelDestroy(model);
            fclose(f);
            return NULL;
        }

        /* Hard cap: a corrupted file must not request huge allocations */
        if (name_len == 0 || name_len > 255)
        {
            ModelDestroy(model);
            fclose(f);
            return NULL;
        }

        char *name = (char *)malloc(name_len + 1);
        if (name == NULL)
        {
            ModelDestroy(model);
            fclose(f);
            return NULL;
        }

        if (fread(name, 1, name_len, f) != name_len ||
            fread(&frequency, sizeof(uint64_t), 1, f) != 1)
        {
            free(name);
            ModelDestroy(model);
            fclose(f);
            return NULL;
        }
        name[name_len] = '\0';

        SYMBOL_ID new_id = GraphAddSymbol(model->graph, name);

        if (new_id != SYMBOL_INVALID && new_id <= model->graph->symbols->count)
        {
            model->graph->symbols->items[new_id - 1].frequency = frequency;
        }

        free(name);
    }

    /* Load relations (V3 adds provenance; V1/V2 => unknown;
       V4 adds polarity; older => positive) */
    for (uint32_t i = 0; i < rel_count; i++)
    {
        SYMBOL_ID subj, rel, obj, src = SYMBOL_INVALID;
        uint64_t count;
        float weight;
        uint32_t pol = (uint32_t)POLARITY_POSITIVE;

        if (fread(&subj,   sizeof(SYMBOL_ID), 1, f) != 1 ||
            fread(&rel,   sizeof(SYMBOL_ID), 1, f) != 1 ||
            fread(&obj,    sizeof(SYMBOL_ID), 1, f) != 1 ||
            fread(&count,     sizeof(uint64_t), 1, f) != 1 ||
            fread(&weight, sizeof(float), 1, f) != 1 ||
            (version >= 3 && fread(&src, sizeof(SYMBOL_ID), 1, f) != 1) ||
            (version >= 4 && fread(&pol, sizeof(uint32_t), 1, f) != 1))
        {
            ModelDestroy(model);
            fclose(f);
            return NULL;
        }

        /* Exact restore: direct polar add + field copy. Re-running
           conflict policies here would corrupt stored weights
           (halving twice); policies run at ingest, never at load. */
        if (RelationAddPolar(model->graph->relations, subj, rel, obj,
                             (RELATION_POLARITY)pol))
        {
            RELATION *r = RelationFindPolar(model->graph->relations,
                                            subj, rel, obj,
                                            (RELATION_POLARITY)pol);
            if (r != NULL)
            {
                r->count = count;
                r->weight = weight;
                /* src counts only if it points at a symbol from the file */
                if (version >= 3 && src != SYMBOL_INVALID && src <= sym_count)
                    r->source = src;
            }
        }
    }

    /* Load embeddings (V2 only) */
    if (version >= 2 && emb_count > 0 && model->embeddings != NULL)
    {
        for (uint32_t i = 0; i < emb_count; i++)
        {
            SYMBOL_ID emb_id;
            float vector[EMBEDDING_DIM];

            if (fread(&emb_id, sizeof(SYMBOL_ID), 1, f) != 1 ||
                fread(vector,  sizeof(float), EMBEDDING_DIM, f) != EMBEDDING_DIM)
            {
                ModelDestroy(model);
                fclose(f);
                return NULL;
            }

            EmbeddingSetVector(model->embeddings, emb_id, vector);
        }
    }

    fclose(f);
    return model;
}
