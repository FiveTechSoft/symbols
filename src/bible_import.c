/* bible_import.c — ingest del corpus biblico al GraphEngine.

   Pipeline:
     bible_relations.tsv (S \t REL \t O \t verse)  -> grafo + provenance
     verses.tsv          (id \t book \t ref \t text) -> simbolos estructurales

   Salidas (formato V5 de model.c):
     bible_graph.dat   — modelo completo (simbolos + relaciones + embeddings ausentes)
     bible_index.dat   — copia binaria del mismo modelo V5 (los indices de
                         consulta son parte del grafo en memoria; V5 ya
                         persiste count/weight/source/polarity de cada relacion)

   Uso:
     bible_import <relations.tsv> <verses.tsv|-> [out_dir]
   */
#include "model.h"
#include "ingest.h"
#include "graph.h"
#include "symbol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
static double now(void)
{
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)f.QuadPart;
}
#else
#include <time.h>
static double now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}
#endif

/* Ingesta de simbolos estructurales desde verses.tsv: cada id de verso
   ("The Book of Esther 8:9") se internа como simbolo, y el libro como
   relacion (VERSE_IN_BOOK). Solo vocabulario + jerarquia, sin parsing
   linguistico del texto. Devuelve simbolos internados. */
static uint64_t IngestVerseSymbols(GRAPH *graph, const char *path)
{
    FILE *f = fopen(path, "r");
    if (f == NULL)
        return 0;

    char file_buf[1 << 16];
    setvbuf(f, file_buf, _IOFBF, sizeof(file_buf));

    char line[2048];
    uint64_t count = 0;
    SYMBOL_ID pred_cache = SYMBOL_INVALID;
    while (fgets(line, sizeof(line), f) != NULL)
    {
        char id[512], book[256], ref[64];
        if (sscanf(line, "%511[^\t]\t%255[^\t]\t%63[^\t]%*[\t]", id, book, ref) != 3)
            continue;

        SYMBOL_ID vid = GraphAddSymbol(graph, id);
        SYMBOL_ID bid = GraphAddSymbol(graph, book);
        if (vid != SYMBOL_INVALID && bid != SYMBOL_INVALID && vid != bid)
        {
            /* relacion estructural: verso pertenece a libro.
               GraphAddRelationPolar con predicado SYMBOL_INVALID no es
               valido (el predicado es un simbolo como cualquier otro). */
            SYMBOL_ID pred = pred_cache;
            if (pred == SYMBOL_INVALID)
                pred = pred_cache = GraphAddSymbol(graph, "VERSE_IN_BOOK");
            if (pred != SYMBOL_INVALID)
                (void)GraphAddRelationPolar(graph, vid, pred, bid,
                                            POLARITY_POSITIVE,
                                            CONFLICT_REJECT_NEW);
            count++;
        }
    }
    fclose(f);
    return count;
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        fprintf(stderr, "Uso: %s <relations.tsv> <verses.tsv|-> [out.dat]\n", argv[0]);
        return EXIT_FAILURE;
    }
    const char *rel_path = argv[1];
    const char *ver_path = argv[2];
    const char *out_path = (argc > 3) ? argv[3] : "bible_graph.dat";

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== bible_import ===\n");

    /* capacidades: 674 entidades + 66 libros + 31102 versos + margen */
    MODEL *model = ModelCreate(40000 + 2048, 20000);
    if (model == NULL)
    {
        fprintf(stderr, "ModelCreate fallo\n");
        return EXIT_FAILURE;
    }

    double t0 = now();

    /* 1. relaciones semanticas con provenance */
    FILE *f = fopen(rel_path, "r");
    if (f == NULL)
    {
        fprintf(stderr, "no se pudo abrir %s\n", rel_path);
        ModelDestroy(model);
        return EXIT_FAILURE;
    }
    char rel_buf[1 << 16];
    setvbuf(f, rel_buf, _IOFBF, sizeof(rel_buf));
    INGEST_STATS st = IngestTSVStreamSrc(model->graph, f, rel_path);
    fclose(f);
    double t1 = now();

    printf("relaciones: leidas=%llu insertadas=%llu actualizadas=%llu fallidas=%llu (%.3f s)\n",
           (unsigned long long)st.lines_read,
           (unsigned long long)st.relations_inserted,
           (unsigned long long)st.relations_updated,
           (unsigned long long)st.lines_failed, t1 - t0);

    /* 2. simbolos estructurales de versos (opcional, '-' para saltar) */
    uint64_t vcount = 0;
    if (ver_path[0] != '-' || ver_path[1] != '\0')
    {
        vcount = IngestVerseSymbols(model->graph, ver_path);
        if (vcount == 0)
            fprintf(stderr, "aviso: 0 versos desde %s\n", ver_path);
    }
    double t2 = now();
    printf("versos: %llu simbolos (%.3f s)\n",
           (unsigned long long)vcount, t2 - t1);

    /* 3. persistencia V5 */
    int ok = ModelSave(model, out_path);
    double t3 = now();
    if (!ok)
    {
        fprintf(stderr, "ModelSave fallo\n");
        ModelDestroy(model);
        return EXIT_FAILURE;
    }
    printf("guardado: %s (%.3f s)\n", out_path, t3 - t2);

    /* resumen */
    uint32_t nsym = SymbolCount(model->graph->symbols);
    uint32_t nrel = (uint32_t)model->graph->relations->count;
    printf("resumen: simbolos=%u relaciones=%u ingest=%.3fs total=%.3fs\n",
           nsym, nrel, t1 - t0, t3 - t0);

    ModelDestroy(model);
    return EXIT_SUCCESS;
}