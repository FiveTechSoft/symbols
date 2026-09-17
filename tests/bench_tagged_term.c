/* bench_tagged_term.c — MLIPS del motor tagged_term.
   Replica el bucle de estres de fast_prolog.c del usuario:
   hecho(antonio,Y) = hecho(X,marbella), trail undo + pool restore
   por iteracion. Reporta ns/unif y MLIPS. */
#include "tagged_term.h"
#include <stdio.h>
#include <stdlib.h>
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

int main(int argc, char **argv)
{
    size_t iterations = (argc > 1) ? (size_t)atoll(argv[1]) : 10000000;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== bench_tagged_term ===\n");
    printf("Iteraciones: %zu | sizeof(Term)=%zu\n", iterations, sizeof(Term));

    size_t pool_cap = 64u * 1024 * 1024;
    uint8_t *pool_buf = (uint8_t *)malloc(pool_cap);
    TERM_POOL pool;
    tt_pool_init(&pool, pool_buf, pool_cap);
    TRAIL trail;
    if (!tt_trail_init(&trail, 65536))
        return 1;

    Term atom_a = tt_atom(1001);
    Term atom_m = tt_atom(2002);

    double t0 = now();
    size_t mem_mark = tt_pool_mark(&pool);
    for (size_t i = 0; i < iterations; i++)
    {
        uint32_t tmark = tt_trail_mark(&trail);
        Term cell_x, cell_y;
        Term vx = tt_var(&cell_x);
        Term vy = tt_var(&cell_y);
        Term a1[2] = { atom_a, vy };
        Term s1 = tt_make_struct(&pool, 50, 2, a1);
        Term a2[2] = { vx, atom_m };
        Term s2 = tt_make_struct(&pool, 50, 2, a2);
        if (!tt_unify(s1, s2, &trail))
        {
            fprintf(stderr, "FAIL en iter %zu\n", i);
            return 1;
        }
        tt_trail_restore(&trail, tmark);
        tt_pool_restore(&pool, mem_mark);
    }
    double dt = now() - t0;

    printf("Tiempo: %.3f s\n", dt);
    printf("Velocidad: %.2f MLIPS (%.2f ns/unif)\n",
           (double)iterations / dt / 1e6, dt * 1e9 / (double)iterations);

    tt_trail_free(&trail);
    free(pool_buf);
    return 0;
}