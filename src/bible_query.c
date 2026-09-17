/* bible_query.c — consultas de humo + medicion sobre bible_graph.dat.

   Uso:
     bible_query <model.dat> <comando> [args...]
   Comandos:
     facts <sujeto>            — todas las relaciones con ese sujeto
     rel <sujeto> <predicado>  — objetos de (S, P, ?)
     rev <predicado> <objeto>  — sujetos de (?, P, O)
     bench <n>                 — SymbolFind + GraphQuerySubject x n sobre simbolos reales
   */
#include "model.h"
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

static const char *sym_name(const SYMBOL_TABLE *t, SYMBOL_ID id)
{
    const SYMBOL *s = SymbolGet(t, id);
    return (s != NULL) ? s->name : NULL;
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        fprintf(stderr, "Uso: %s <model.dat> <facts|rel|rev|bench> [args]\n", argv[0]);
        return EXIT_FAILURE;
    }
    setvbuf(stdout, NULL, _IONBF, 0);

    double t0 = now();
    MODEL *m = ModelLoad(argv[1]);
    if (m == NULL)
    {
        fprintf(stderr, "ModelLoad fallo\n");
        return EXIT_FAILURE;
    }
    double t1 = now();
    printf("carga: %u simbolos, %u relaciones (%.3f s, %.2f MB/s)\n",
           SymbolCount(m->graph->symbols),
           (unsigned)m->graph->relations->count,
           t1 - t0, 2.87 / (t1 - t0));

    const char *cmd = argv[2];
    if (strcmp(cmd, "bench") == 0)
    {
        long n = (argc > 3) ? atol(argv[3]) : 100000;
        /* sample determinista: nombres reales de simbolos del modelo */
        uint32_t nsym = SymbolCount(m->graph->symbols);
        double tstart = now();
        uint64_t hits = 0, rels = 0;
        for (long i = 0; i < n; i++)
        {
            uint32_t idx = 1 + (uint32_t)(i % nsym);
            const SYMBOL *sym = SymbolGet(m->graph->symbols, idx);
            if (sym == NULL)
                continue;
            SYMBOL_ID s = SymbolFind(m->graph->symbols, sym->name);
            if (s != SYMBOL_INVALID)
            {
                hits++;
                /* grado: chain scan del subject index */
                const RELATION_TABLE *t = m->graph->relations;
                if (t->subj_capacity > 0)
                {
                    uint32_t i = t->subj_heads[s & t->subj_mask];
                    while (i != 0xFFFFFFFF)
                    {
                        if (t->items[i].subject == s)
                            rels++;
                        i = t->subj_next[i];
                    }
                }
            }
        }
        double dt = now() - tstart;
        printf("bench: %ld lookups+grado, %llu hits, %llu aristas vistas, %.3f s, %.0f ops/s\n",
               n, (unsigned long long)hits, (unsigned long long)rels, dt, n / dt);
        ModelDestroy(m);
        return EXIT_SUCCESS;
    }

    if (argc < 4)
    {
        fprintf(stderr, "comando requiere sujeto\n");
        ModelDestroy(m);
        return EXIT_FAILURE;
    }
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s", argv[3]);
    /* a mayusculas, igual que IngestTriple normaliza */
    for (char *p = buf; *p; p++)
        if (*p >= 'a' && *p <= 'z')
            *p = (char)(*p - 'a' + 'A');

    SYMBOL_ID s = SymbolFind(m->graph->symbols, buf);
    if (s == SYMBOL_INVALID)
    {
        printf("no existe simbolo: %s\n", buf);
        ModelDestroy(m);
        return EXIT_FAILURE;
    }

    if (strcmp(cmd, "facts") == 0)
    {
        RELATION *res[256];
        uint32_t n = GraphQuerySubject(m->graph, s, res, 256);
        printf("%s: %u relaciones\n", buf, n);
        for (uint32_t i = 0; i < n && i < 40; i++)
        {
            const char *rn = sym_name(m->graph->symbols, res[i]->relation);
            const char *on = sym_name(m->graph->symbols, res[i]->object);
            const char *sn = sym_name(m->graph->symbols, res[i]->source);
            printf("  %s -> %s [%s x%llu]\n", rn ? rn : "?", on ? on : "?",
                   sn ? sn : "-", (unsigned long long)res[i]->count);
        }
    }
    else if (strcmp(cmd, "rel") == 0 && argc > 4)
    {
        char rbuf[256];
        snprintf(rbuf, sizeof(rbuf), "%s", argv[4]);
        for (char *p = rbuf; *p; p++)
            if (*p >= 'a' && *p <= 'z')
                *p = (char)(*p - 'a' + 'A');
        SYMBOL_ID r = SymbolFind(m->graph->symbols, rbuf);
        if (r == SYMBOL_INVALID)
        {
            printf("no existe predicado: %s\n", rbuf);
            ModelDestroy(m);
            return EXIT_FAILURE;
        }
        RELATION *res2[256];
        uint32_t n = GraphQuerySubjectRelation(m->graph, s, r, res2, 256);
        printf("%s -%s-> %u objetos:\n", buf, rbuf, n);
        for (uint32_t i = 0; i < n && i < 40; i++)
        {
            const char *on = sym_name(m->graph->symbols, res2[i]->object);
            printf("  %s\n", on ? on : "?");
        }
    }
    else if (strcmp(cmd, "rev") == 0)
    {
        /* forma A: rev <obj> -> cualquier pred | forma B: rev <pred> <obj> */
        char rbuf[256];
        rbuf[0] = '\0';
        SYMBOL_ID r = SYMBOL_INVALID;
        if (argc > 5)
        {
            snprintf(rbuf, sizeof(rbuf), "%s", argv[4]);
            for (char *p = rbuf; *p; p++)
                if (*p >= 'a' && *p <= 'z')
                    *p = (char)(*p - 'a' + 'A');
            r = SymbolFind(m->graph->symbols, rbuf);
            if (r == SYMBOL_INVALID)
            {
                printf("no existe predicado: %s\n", rbuf);
                ModelDestroy(m);
                return EXIT_FAILURE;
            }
        }
        /* reverse: sin indice de objeto => escaneo lineal completo */
        uint32_t n = 0;
        RELATION *res3[256];
        /* escaneo directo sobre la tabla (RelationFindByObject con NULL aborta) */
        {
            RELATION_TABLE *t = m->graph->relations;
            for (uint32_t i = 0; i < t->count && n < 256; i++)
            {
                if (t->items[i].object == s &&
                    (r == SYMBOL_INVALID || t->items[i].relation == r))
                    res3[n++] = &t->items[i];
            }
        }
        printf("sujetos con -%s-> %s: %u\n", rbuf, buf, n);
        for (uint32_t i = 0; i < n && i < 40; i++)
        {
            const char *sn2 = sym_name(m->graph->symbols, res3[i]->subject);
            const char *rn2 = sym_name(m->graph->symbols, res3[i]->relation);
            printf("  %s -%s-> %s\n", sn2 ? sn2 : "?", rn2 ? rn2 : "?", buf);
        }
    }
    else
    {
        fprintf(stderr, "comando desconocido o args insuficientes\n");
        ModelDestroy(m);
        return EXIT_FAILURE;
    }

    ModelDestroy(m);
    return EXIT_SUCCESS;
}
