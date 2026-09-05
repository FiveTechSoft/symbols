#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "graph.h"
#include "symbol.h"
#include "relation.h"
#include "ingest.h"
#include "model.h"

static void Assert(int cond, const char *msg)
{
    if (!cond)
    {
        printf("FAIL: %s\n", msg);
        exit(EXIT_FAILURE);
    }
}

/* Hand-written minimal V2: header + 1 symbol + 1 relation, no source.
   Verifies V3 reads V1/V2 with unknown provenance. */
static void WriteV2Fixture(const char *path)
{
    FILE *f = fopen(path, "wb");
    Assert(f != NULL, "abrir fixture V2");
    uint32_t magic = MODEL_MAGIC, version = 2;
    uint32_t sym_count = 3, rel_count = 1, emb_count = 0, emb_dim = 32;
    uint32_t zero = 0;
    fwrite(&magic, 4, 1, f);
    fwrite(&version, 4, 1, f);
    fwrite(&sym_count, 4, 1, f);
    fwrite(&rel_count, 4, 1, f);
    fwrite(&emb_count, 4, 1, f);
    fwrite(&emb_dim, 4, 1, f);
    const char *names[] = {"ADAN", "PADRE_DE", "SET"};
    for (int i = 0; i < 3; i++)
    {
        uint32_t id = (uint32_t)(i + 1);
        uint32_t nlen = (uint32_t)strlen(names[i]);
        uint64_t freq = 1;
        fwrite(&id, 4, 1, f);
        fwrite(&nlen, 4, 1, f);
        fwrite(names[i], 1, nlen, f);
        fwrite(&freq, 8, 1, f);
    }
    {
        uint32_t s = 1, p = 2, o = 3;
        uint64_t count = 1;
        float weight = 1.0f;
        fwrite(&s, 4, 1, f);
        fwrite(&p, 4, 1, f);
        fwrite(&o, 4, 1, f);
        fwrite(&count, 8, 1, f);
        fwrite(&weight, 4, 1, f);
        (void)zero;
    }
    fclose(f);
}

int main(void)
{
    printf("========================================\n");
    printf("     SYMBOLIC LLM - PROCEDENCIA        \n");
    printf("========================================\n\n");

    /* 1. Ingest with explicit source */
    GRAPH *g = GraphCreate(64, 64);
    Assert(g != NULL, "GraphCreate");
    Assert(IngestTripleSource(g, "ADAN", "PADRE_DE", "SET", "GEN 5:3") == 1,
           "ingesta con fuente");
    Assert(IngestTriple(g, "EVA", "MADRE_DE", "SET") == 1,
           "ingesta sin fuente");
    /* Re-ingest keeps the original source */
    Assert(IngestTripleSource(g, "ADAN", "PADRE_DE", "SET", "OTRO") == 2,
           "update detectado");
    RELATION *r = GraphFindRelation(g,
        SymbolFind(g->symbols, "ADAN"),
        SymbolFind(g->symbols, "PADRE_DE"),
        SymbolFind(g->symbols, "SET"));
    Assert(r != NULL, "relacion encontrada");
    const SYMBOL *src = SymbolGet(g->symbols, r->source);
    Assert(src != NULL && strcmp(src->name, "GEN 5:3") == 0,
           "fuente original conservada tras update");
    RELATION *r2 = GraphFindRelation(g,
        SymbolFind(g->symbols, "EVA"),
        SymbolFind(g->symbols, "MADRE_DE"),
        SymbolFind(g->symbols, "SET"));
    Assert(r2 != NULL && r2->source == SYMBOL_INVALID,
           "sin fuente => SYMBOL_INVALID");
    GraphDestroy(g);
    printf("  ingesta + first-writer-wins OK\n");

    /* 2. Roundtrip V3: la fuente sobrevive a disco */
    {
        MODEL *m = ModelCreate(64, 64);
        Assert(m != NULL, "ModelCreate");
        Assert(IngestTripleSource(m->graph, "ADAN", "PADRE_DE", "SET", "GEN 5:3") == 1,
               "ingesta modelo");
        Assert(ModelSave(m, "test_provenance_tmp.bin") == 1, "ModelSave V3");
        ModelDestroy(m);

        MODEL *m2 = ModelLoad("test_provenance_tmp.bin");
        Assert(m2 != NULL && m2->graph != NULL, "ModelLoad V3");
        RELATION *rl = GraphFindRelation(m2->graph,
            SymbolFind(m2->graph->symbols, "ADAN"),
            SymbolFind(m2->graph->symbols, "PADRE_DE"),
            SymbolFind(m2->graph->symbols, "SET"));
        Assert(rl != NULL, "relacion tras load");
        const SYMBOL *sl = SymbolGet(m2->graph->symbols, rl->source);
        Assert(sl != NULL && strcmp(sl->name, "GEN 5:3") == 0,
               "fuente sobrevive roundtrip V3");
        ModelDestroy(m2);
        remove("test_provenance_tmp.bin");
    }
    printf("  roundtrip V3 OK\n");

    /* 3. Compat V2: fixture sin campo source => desconocida */
    {
        WriteV2Fixture("test_provenance_v2.bin");
        MODEL *m = ModelLoad("test_provenance_v2.bin");
        Assert(m != NULL && m->graph != NULL, "ModelLoad V2");
        RELATION *rl = GraphFindRelation(m->graph,
            SymbolFind(m->graph->symbols, "ADAN"),
            SymbolFind(m->graph->symbols, "PADRE_DE"),
            SymbolFind(m->graph->symbols, "SET"));
        Assert(rl != NULL, "relacion V2 encontrada");
        Assert(rl->source == SYMBOL_INVALID, "V2 => fuente desconocida");
        ModelDestroy(m);
        remove("test_provenance_v2.bin");
    }
    printf("  compat V2 OK\n");

    printf("\n========================================\n");
    printf("Procedencia verificada OK.\n");
    printf("========================================\n");
    return EXIT_SUCCESS;
}
