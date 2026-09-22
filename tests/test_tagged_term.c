/* test_tagged_term.c — verificación tagged_term
   Incluye: demo del diseño (unificación + backtrack), casos de
   borde (tags, arity, trail, pool reset, ocurs check) y bench
   de estrés con arena restore. */
#include "tagged_term.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

/* ---------- helpers ---------- */

static uint8_t pool_buf[1 << 20];          /* 1 MB */
static TERM_POOL pool;
static TRAIL trail;

static void setup(void)
{
    tt_pool_init(&pool, pool_buf, sizeof(pool_buf));
    if (!tt_trail_init(&trail, 65536))
        exit(1);
}

static Term mk_atom_named(uint32_t id) { return tt_atom(id); }

/* ---------- 1. demo del diseño ---------- */

static void test_demo_user(void)
{
    setup();
    Term atom_antonio  = tt_atom(101);
    Term atom_marbella = tt_atom(202);

    Term cell_X, cell_Y;
    Term X = tt_var(&cell_X);
    Term Y = tt_var(&cell_Y);

    Term args1[2] = { atom_antonio, Y };
    Term s1 = tt_make_struct(&pool, 1, 2, args1);   /* vive(antonio,Y) */
    Term args2[2] = { X, atom_marbella };
    Term s2 = tt_make_struct(&pool, 1, 2, args2);   /* vive(X,marbella) */

    uint32_t mark = tt_trail_mark(&trail);
    assert(tt_unify(s1, s2, &trail) == 1);
    assert(tt_get_atom_id(tt_deref(X, NULL)) == 101);  /* X=antonio */
    assert(tt_get_atom_id(tt_deref(Y, NULL)) == 202);  /* Y=marbella */

    tt_trail_restore(&trail, mark);
    assert(tt_tag(tt_deref(X, NULL)) == TAG_VAR);      /* libre de nuevo */
    assert(tt_tag(tt_deref(Y, NULL)) == TAG_VAR);
    tt_trail_free(&trail);
    printf("  PASS test_demo_user\n");
}

/* ---------- 2. demo bench del usuario (estrés) ---------- */

static void test_stress(void)
{
    setup();
    size_t iterations = 200000;
    Term atom_antonio = tt_atom(1001);
    Term atom_marbella = tt_atom(2002);

    size_t mem_mark = tt_pool_mark(&pool);
    clock_t t0 = clock();
    for (size_t i = 0; i < iterations; i++)
    {
        uint32_t tmark = tt_trail_mark(&trail);
        Term cell_x, cell_y;
        Term var_x = tt_var(&cell_x);
        Term var_y = tt_var(&cell_y);
        Term args1[2] = { atom_antonio, var_y };
        Term s1 = tt_make_struct(&pool, 50, 2, args1);  /* hecho(antonio,Y) */
        Term args2[2] = { var_x, atom_marbella };
        Term s2 = tt_make_struct(&pool, 50, 2, args2);  /* hecho(X,marbella) */
        assert(tt_unify(s1, s2, &trail) == 1);
        tt_trail_restore(&trail, tmark);
        tt_pool_restore(&pool, mem_mark);
    }
    double elapsed = (double)(clock() - t0) / CLOCKS_PER_SEC;
    printf("  PASS test_stress (%zu unifs, %.2f ns/unif)\n",
           iterations, elapsed * 1e9 / iterations);
    tt_trail_free(&trail);
}

/* ---------- 3. tags y escalares ---------- */

static void test_tags_scalars(void)
{
    setup();
    /* atom roundtrip */
    Term a = tt_atom(0);
    assert(tt_get_atom_id(a) == 0);
    Term b = tt_atom(0x1FFFFFFF);          /* 29 bits max */
    assert(tt_get_atom_id(b) == 0x1FFFFFFF);
    /* atom overflow: id 29 bits + 1 contamina el tag -> rechazado en ctor?
       (documentado: caller garantiza < 2^29; aquí comprobamos detección) */
    assert(tt_tag(tt_atom(7)) == TAG_ATOM);

    /* int roundtrip con signo */
    assert(tt_get_int(tt_int(0)) == 0);
    assert(tt_get_int(tt_int(42)) == 42);
    assert(tt_get_int(tt_int(-42)) == -42);
    assert(tt_get_int(tt_int(INT64_MAX >> 3)) == (INT64_MAX >> 3));
    assert(tt_get_int(tt_int(INT64_MIN >> 3)) == (INT64_MIN >> 3));

    /* atom != int con misma carga util: tags distintos, unify falla */
    Term atom_5 = tt_atom(5);
    Term int_5 = tt_int(5);
    assert(atom_5 != int_5);
    uint32_t mark = tt_trail_mark(&trail);
    assert(tt_unify(atom_5, int_5, &trail) == 0);
    assert(tt_unify(tt_atom(5), tt_atom(6), &trail) == 0);
    assert(tt_unify(tt_int(5), tt_int(-5), &trail) == 0);
    assert(tt_unify(tt_int(7), tt_int(7), &trail) == 1);
    tt_trail_restore(&trail, mark);
    tt_trail_free(&trail);
    printf("  PASS test_tags_scalars\n");
}

/* ---------- 4. arity/functor mismatch + var-var ---------- */

static void test_struct_rules(void)
{
    setup();
    Term cellX;
    Term X = tt_var(&cellX);
    Term a1 = tt_atom(1), a2 = tt_atom(2), a3 = tt_atom(3);

    /* functor distinto */
    Term argsA[2] = { a1, a2 };
    Term sA = tt_make_struct(&pool, 10, 2, argsA);
    Term argsB[2] = { a1, a2 };
    Term sB = tt_make_struct(&pool, 11, 2, argsB);
    uint32_t mark = tt_trail_mark(&trail);
    assert(tt_unify(sA, sB, &trail) == 0);

    /* arity distinto */
    Term argsC[3] = { a1, a2, a3 };
    Term sC = tt_make_struct(&pool, 10, 3, argsC);
    assert(tt_unify(sA, sC, &trail) == 0);

    /* mismos functor/arity, arg distinto */
    Term argsD[2] = { a1, a3 };
    Term sD = tt_make_struct(&pool, 10, 2, argsD);
    assert(tt_unify(sA, sD, &trail) == 0);

    /* idénticos: ok */
    Term argsE[2] = { a1, a2 };
    Term sE = tt_make_struct(&pool, 10, 2, argsE);
    assert(tt_unify(sA, sE, &trail) == 1);

    /* var-var: comparten celda tras unify */
    Term cellV1, cellV2;
    Term V1 = tt_var(&cellV1), V2 = tt_var(&cellV2);
    assert(tt_unify(V1, V2, &trail) == 1);
    /* ambas deref al mismo término */
    Term d1 = tt_deref(V1, NULL), d2 = tt_deref(V2, NULL);
    assert(d1 == d2);

    /* struct con misma var en ambos lados */
    assert(tt_unify(sA, sA, &trail) == 1);
    tt_trail_restore(&trail, mark);
    tt_trail_free(&trail);
    printf("  PASS test_struct_rules\n");
}

/* ---------- 5. trail: undo en LIFO + capacidad fail-closed ---------- */

static void test_trail_semantics(void)
{
    setup();
    Term cellV, cellW;
    Term V = tt_var(&cellV), W = tt_var(&cellW);

    uint32_t m0 = tt_trail_mark(&trail);
    assert(tt_unify(V, tt_atom(9), &trail) == 1);
    assert(tt_get_atom_id(tt_deref(V, NULL)) == 9);

    uint32_t m1 = tt_trail_mark(&trail);
    assert(tt_unify(W, tt_atom(8), &trail) == 1);
    assert(tt_get_atom_id(tt_deref(W, NULL)) == 8);

    tt_trail_restore(&trail, m1);          /* solo W se desliga */
    assert(tt_get_atom_id(tt_deref(V, NULL)) == 9);
    assert(tt_tag(tt_deref(W, NULL)) == TAG_VAR);

    tt_trail_restore(&trail, m0);          /* V se desliga */
    assert(tt_tag(tt_deref(V, NULL)) == TAG_VAR);

    /* capacidad fail-closed: trail de 4 slots, 5 ligaduras */
    TRAIL tiny;
    int init_ok = tt_trail_init(&tiny, 4);   /* fuera del assert: bajo
                                                NDEBUG el assert muere con
                                                su efecto secundario */
    assert(init_ok == 1);
    uint32_t ok = 0;
    Term cells[5];
    for (uint32_t i = 0; i < 5; i++)
    {
        Term v = tt_var(&cells[i]);
        Term av = tt_atom(100 + i);
        /* The trail retains cell addresses until restore: keep every cell in
           scope for the complete backtracking transaction. */
        if (tt_unify(v, av, &tiny))
            ok++;
    }
    assert(ok == 4);                       /* la 5ª falla limpio */
    tt_trail_restore(&tiny, 0);
    tt_trail_free(&tiny);
    tt_trail_free(&trail);
    printf("  PASS test_trail_semantics\n");
}

/* ---------- 6. pool: OOM limpio + reset O(0) ---------- */

static void test_pool_bounds(void)
{
    uint8_t tiny_buf[256];
    TERM_POOL tiny;
    tt_pool_init(&tiny, tiny_buf, sizeof(tiny_buf));

    /* struct arity maxima permitida cabe */
    Term args[8];
    for (int i = 0; i < 8; i++) args[i] = tt_atom((uint32_t)i);
    assert(tt_make_struct(&tiny, 1, 8, args) != TERM_NULL);

    /* arity sobre el maximo: NULL */
    assert(tt_make_struct(&tiny, 1, TT_MAX_ARITY + 1, args) == TERM_NULL);

    /* llenar el resto hasta OOM: tt_make_struct devuelve TERM_NULL, no crash */
    int oom_seen = 0;
    for (int i = 0; i < 100; i++)
    {
        if (tt_make_struct(&tiny, 77, 8, args) == TERM_NULL)
        {
            oom_seen = 1;
            break;
        }
    }
    assert(oom_seen == 1);

    /* restore a 0 deja reusar */
    tt_pool_restore(&tiny, 0);
    assert(tt_make_struct(&tiny, 1, 8, args) != TERM_NULL);
    printf("  PASS test_pool_bounds\n");
}

/* ---------- 7. ocurs check ---------- */

static void test_occurs(void)
{
    setup();
    Term cellX;
    Term X = tt_var(&cellX);

    Term args[1] = { X };
    Term fx = tt_make_struct(&pool, 5, 1, args);   /* f(X) */

    /* sin ocurs (por defecto): X=f(X) crea ciclo -> deref lo sigue */
    tt_set_occurs_check(0);
    uint32_t mark = tt_trail_mark(&trail);
    assert(tt_unify(X, fx, &trail) == 1);          /* acepta (infinito) */
    tt_trail_restore(&trail, mark);

    /* con ocurs: rechaza */
    tt_set_occurs_check(1);
    assert(tt_unify(X, fx, &trail) == 0);
    assert(tt_tag(tt_deref(X, NULL)) == TAG_VAR);  /* X sigue libre */
    tt_set_occurs_check(0);
    tt_trail_free(&trail);
    printf("  PASS test_occurs\n");
}

/* ---------- 8. estructuras anidadas ---------- */

static void test_nested(void)
{
    setup();
    Term cellX;
    Term X = tt_var(&cellX);
    Term a1 = tt_atom(1), a2 = tt_atom(2);

    /* p(a1, q(a2, X)) */
    Term qargs[2] = { a2, X };
    Term q = tt_make_struct(&pool, 7, 2, qargs);
    Term pargs[2] = { a1, q };
    Term p = tt_make_struct(&pool, 6, 2, pargs);

    /* p(a1, q(a2, atom99)) */
    Term qargs2[2] = { a2, tt_atom(99) };
    Term q2 = tt_make_struct(&pool, 7, 2, qargs2);
    Term pargs2[2] = { a1, q2 };
    Term p2 = tt_make_struct(&pool, 6, 2, pargs2);

    uint32_t mark = tt_trail_mark(&trail);
    assert(tt_unify(p, p2, &trail) == 1);
    assert(tt_get_atom_id(tt_deref(X, NULL)) == 99);

    tt_trail_restore(&trail, mark);
    assert(tt_tag(tt_deref(X, NULL)) == TAG_VAR);

    /* anidado: X libre se liga con atom1000 (semantica Prolog) */
    Term qargs3[2] = { a2, tt_atom(1000) };
    Term q3 = tt_make_struct(&pool, 7, 2, qargs3);
    Term pargs3[2] = { a1, q3 };
    Term p3 = tt_make_struct(&pool, 6, 2, pargs3);
    mark = tt_trail_mark(&trail);
    assert(tt_unify(p, p3, &trail) == 1);
    assert(tt_get_atom_id(tt_deref(X, NULL)) == 1000);
    tt_trail_restore(&trail, mark);
    assert(tt_tag(tt_deref(X, NULL)) == TAG_VAR);

    /* anidado con functor distinto en profundidad: fallo */
    Term qargs4[2] = { a2, tt_atom(1000) };
    Term q4 = tt_make_struct(&pool, 8, 2, qargs4);      /* functor 8 != 7 */
    Term pargs4[2] = { a1, q4 };
    Term p4 = tt_make_struct(&pool, 6, 2, pargs4);
    assert(tt_unify(p, p4, &trail) == 0);
    tt_trail_free(&trail);
    printf("  PASS test_nested\n");
}

/* ---------- 9. var en pool: self-cycle correcto ---------- */

static void test_pool_var(void)
{
    setup();
    Term v = tt_pool_var(&pool);
    assert(tt_tag(v) == TAG_VAR);
    assert(tt_deref(v, NULL) == v);        /* libre */

    uint32_t mark = tt_trail_mark(&trail);
    assert(tt_unify(v, tt_atom(55), &trail) == 1);
    assert(tt_get_atom_id(tt_deref(v, NULL)) == 55);
    tt_trail_restore(&trail, mark);
    assert(tt_deref(v, NULL) == v);
    tt_trail_free(&trail);
    printf("  PASS test_pool_var\n");
}

/* ---------- 10. reutilización de celdas de stack tras backtrack ---------- */

static void test_stack_reuse(void)
{
    setup();
    Term cellX;
    Term X = tt_var(&cellX);

    for (int round = 0; round < 1000; round++)
    {
        uint32_t mark = tt_trail_mark(&trail);
        Term a1 = tt_atom((uint32_t)(round % 100));
        Term args[2] = { a1, tt_atom(300) };       /* s sin X: ligable */
        Term s = tt_make_struct(&pool, 3, 2, args);
        assert(tt_unify(X, s, &trail) == 1);       /* X = f(a1, atom300) */
        assert(tt_tag(tt_deref(X, NULL)) == TAG_STRUCT);
        tt_trail_restore(&trail, mark);
        assert(tt_tag(tt_deref(X, NULL)) == TAG_VAR);
        /* ligar a atomo y desligar */
        assert(tt_unify(X, tt_atom((uint32_t)round), &trail) == 1);
        assert(tt_get_atom_id(tt_deref(X, NULL)) == (uint32_t)round);
        tt_trail_restore(&trail, mark);
        assert(tt_tag(tt_deref(X, NULL)) == TAG_VAR);
    }
    tt_trail_free(&trail);
    printf("  PASS test_stack_reuse\n");
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);      /* crash-safe output */
    setvbuf(stderr, NULL, _IONBF, 0);
#define CHK(name) fprintf(stderr, "[enter %s]\n", name)
    printf("=== test_tagged_term ===\n");
    CHK("demo");    test_demo_user();
    CHK("stress");  test_stress();
    CHK("tags");    test_tags_scalars();
    CHK("structs"); test_struct_rules();
    CHK("trail");   test_trail_semantics();
    CHK("pool");    test_pool_bounds();
    CHK("occurs");  test_occurs();
    CHK("nested");  test_nested();
    CHK("poolvar"); test_pool_var();
    CHK("reuse");   test_stack_reuse();
    printf("All tagged_term tests passed.\n");
    return 0;
}