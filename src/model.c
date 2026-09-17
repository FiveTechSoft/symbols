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
    model->numerics = NumericCreate(symbol_capacity);

    if (model->graph == NULL || model->embeddings == NULL ||
        model->numerics == NULL)
    {
        GraphDestroy(model->graph);
        EmbeddingTableDestroy(model->embeddings);
        NumericDestroy(model->numerics);
        free(model);
        return NULL;
    }

    GraphSetEmbeddingTable(model->graph, model->embeddings);
    GraphSetNumericTable(model->graph, model->numerics);
    model->config = LearningConfigDefault();
    return model;
}

void ModelDestroy(MODEL *model)
{
    if (model == NULL)
        return;

    EmbeddingTableDestroy(model->embeddings);
    NumericDestroy(model->numerics);
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

    /* 5. Numeric sidecar block (V5): typed measures hanging off
       symbols. Count + [id, value, unit]. Absent (count 0) when the
       model holds no measures; V4 and older readers stop before it. */
    {
        uint32_t num_count = 0;
        uint32_t i;
        if (model->numerics != NULL)
            num_count = model->numerics->count;
        if (fwrite(&num_count, sizeof(uint32_t), 1, f) != 1)
        {
            fclose(f);
            return 0;
        }
        for (i = 0; i < num_count; i++)
        {
            char unitbuf[NUMERIC_UNIT_MAX];
            memset(unitbuf, 0, sizeof(unitbuf));
            strncpy(unitbuf, model->numerics->items[i].unit,
                    NUMERIC_UNIT_MAX - 1);
            if (fwrite(&model->numerics->items[i].id, sizeof(SYMBOL_ID),
                       1, f) != 1 ||
                fwrite(&model->numerics->items[i].value, sizeof(double),
                       1, f) != 1 ||
                fwrite(unitbuf, 1, NUMERIC_UNIT_MAX, f) != NUMERIC_UNIT_MAX)
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

    /* Bulk read: one fread for the whole file, then parse in memory.
       The old per-field fread path (~10 reads/symbol) ran at 0.54 MB/s
       on a 32k-symbol model; a single buffered read removes that wall. */
    if (fseek(f, 0, SEEK_END) != 0)
    {
        fclose(f);
        return NULL;
    }
    long file_size = ftell(f);
    if (file_size < 24)
    {
        fclose(f);
        return NULL;
    }
    rewind(f);

    uint8_t *buf = (uint8_t *)malloc((size_t)file_size);
    if (buf == NULL)
    {
        fclose(f);
        return NULL;
    }
    if (fread(buf, 1, (size_t)file_size, f) != (size_t)file_size)
    {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);

    /* Parse with an explicit cursor; every read is bounds-checked. */
    const uint8_t *p = buf;
    const uint8_t *end = buf + file_size;
#define RD(dst, type)                                                     \
    do                                                                    \
    {                                                                     \
        if (end - p < (long)sizeof(type))                                 \
        {                                                                 \
            free(buf);                                                    \
            return NULL;                                                  \
        }                                                                 \
        memcpy((dst), p, sizeof(type));                                   \
        p += sizeof(type);                                                \
    } while (0)

    uint32_t magic, version, sym_count, rel_count;

    RD(&magic, uint32_t);
    if (magic != MODEL_MAGIC)
    {
        free(buf);
        return NULL;
    }

    RD(&version, uint32_t);

    RD(&sym_count, uint32_t);
    RD(&rel_count, uint32_t);

    /* V2: leer campos adicionales de embeddings */
    uint32_t emb_count = 0;
    uint32_t emb_dim = 0;

    if (version >= 2)
    {
        RD(&emb_count, uint32_t);
        RD(&emb_dim, uint32_t);
    }

    uint32_t sym_cap = sym_count < 16 ? 16 : sym_count * 2;
    uint32_t rel_cap = rel_count < 16 ? 16 : rel_count * 2;

    MODEL *model = ModelCreate(sym_cap, rel_cap);
    if (model == NULL)
    {
        free(buf);
        return NULL;
    }

    /* Load symbols */
    for (uint32_t i = 0; i < sym_count; i++)
    {
        SYMBOL_ID id;
        uint32_t name_len;
        uint64_t frequency;

        RD(&id, SYMBOL_ID);
        RD(&name_len, uint32_t);

        /* Hard cap: a corrupted file must not request huge allocations */
        if (name_len == 0 || name_len > 255 || end - p < (long)name_len)
        {
            ModelDestroy(model);
            free(buf);
            return NULL;
        }

        char name[256];
        memcpy(name, p, name_len);
        p += name_len;
        name[name_len] = '\0';

        RD(&frequency, uint64_t);

        SYMBOL_ID new_id = GraphAddSymbol(model->graph, name);

        if (new_id != SYMBOL_INVALID && new_id <= model->graph->symbols->count)
        {
            model->graph->symbols->items[new_id - 1].frequency = frequency;
        }
    }

    /* Load relations (V3 adds provenance; V1/V2 => unknown;
       V4 adds polarity; older => positive) */
    for (uint32_t i = 0; i < rel_count; i++)
    {
        SYMBOL_ID subj, rel, obj, src = SYMBOL_INVALID;
        uint64_t count;
        float weight;
        uint32_t pol = (uint32_t)POLARITY_POSITIVE;

        RD(&subj, SYMBOL_ID);
        RD(&rel, SYMBOL_ID);
        RD(&obj, SYMBOL_ID);
        RD(&count, uint64_t);
        RD(&weight, float);
        if (version >= 3)
            RD(&src, SYMBOL_ID);
        if (version >= 4)
            RD(&pol, uint32_t);

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

            RD(&emb_id, SYMBOL_ID);
            if (end - p < (long)sizeof(float) * EMBEDDING_DIM)
            {
                ModelDestroy(model);
                free(buf);
                return NULL;
            }
            memcpy(vector, p, sizeof(float) * EMBEDDING_DIM);
            p += sizeof(float) * EMBEDDING_DIM;

            EmbeddingSetVector(model->embeddings, emb_id, vector);
        }
    }

    /* Load numerics sidecar (V5 only; older files simply carry none) */
    if (version >= 5 && model->numerics != NULL)
    {
        uint32_t num_count = 0;
        uint32_t i;
        RD(&num_count, uint32_t);
        for (i = 0; i < num_count; i++)
        {
            SYMBOL_ID nid;
            double nval;
            char nunit[NUMERIC_UNIT_MAX];
            RD(&nid, SYMBOL_ID);
            RD(&nval, double);
            if (end - p < (long)NUMERIC_UNIT_MAX)
            {
                ModelDestroy(model);
                free(buf);
                return NULL;
            }
            memcpy(nunit, p, NUMERIC_UNIT_MAX);
            p += NUMERIC_UNIT_MAX;
            nunit[NUMERIC_UNIT_MAX - 1] = '\0';
            if (nid != SYMBOL_INVALID && nid <= sym_count)
                NumericSet(model->numerics, nid, nval, nunit);
        }
    }

    free(buf);
    return model;
#undef RD
}
