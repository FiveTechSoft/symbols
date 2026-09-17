/* ============================================================
   tagged_term.c — Términos Prolog etiquetados de 64 bits.

   Especificación funcional: unificación Prolog sobre palabras
   de 8 bytes (ver include/tagged_term.h). Los Compound viven en
   un TERM_POOL de bump allocation; el backtracking libera
   restaurando top (coste O(0)) y deshace ligaduras vía TRAIL.
   ============================================================ */

#include <stdlib.h>
#include <string.h>
#include "tagged_term.h"

static int g_occurs_check = 0;

void tt_set_occurs_check(int enabled) { g_occurs_check = enabled; }
int  tt_occurs_check_enabled(void)    { return g_occurs_check; }

/* ---------- pool de términos (bump, alineado a 8) ---------- */

void tt_pool_init(TERM_POOL *pool, void *buffer, size_t capacity)
{
    pool->buffer = (uint8_t *)buffer;
    pool->top = 0;
    pool->capacity = capacity;
}

void *tt_pool_alloc(TERM_POOL *pool, size_t size)
{
    size_t aligned_top = (pool->top + 7) & ~(size_t)7;
    if (aligned_top + size > pool->capacity || aligned_top + size < aligned_top)
        return NULL;                       /* fail-closed: sin exit() */
    pool->top = aligned_top + size;
    return &pool->buffer[aligned_top];
}

Term tt_pool_var(TERM_POOL *pool)
{
    Term *cell = (Term *)tt_pool_alloc(pool, sizeof(Term));
    if (cell == NULL)
        return TERM_NULL;
    return tt_var(cell);
}

size_t tt_pool_mark(const TERM_POOL *pool) { return pool->top; }

void tt_pool_restore(TERM_POOL *pool, size_t mark)
{
    pool->top = mark;
}

Term tt_make_struct(TERM_POOL *pool, uint32_t functor_id,
                    uint32_t arity, const Term *args)
{
    if (arity > TT_MAX_ARITY)
        return TERM_NULL;
    size_t size = sizeof(Compound) + (size_t)arity * sizeof(Term);
    Compound *c = (Compound *)tt_pool_alloc(pool, size);
    if (c == NULL)
        return TERM_NULL;
    c->functor_id = functor_id;
    c->arity = arity;
    for (uint32_t i = 0; i < arity; i++)
        c->args[i] = args[i];
    return ((uintptr_t)c) | TAG_STRUCT;
}

/* ---------- trail ---------- */

int tt_trail_init(TRAIL *tr, uint32_t capacity)
{
    tr->cells = (Term **)malloc((size_t)capacity * sizeof(Term *));
    if (tr->cells == NULL)
        return 0;
    tr->top = 0;
    tr->capacity = capacity;
    return 1;
}

void tt_trail_free(TRAIL *tr)
{
    free(tr->cells);
    tr->cells = NULL;
    tr->top = 0;
    tr->capacity = 0;
}

uint32_t tt_trail_mark(const TRAIL *tr) { return tr->top; }

void tt_trail_restore(TRAIL *tr, uint32_t mark)
{
    while (tr->top > mark)
    {
        Term *cell = tr->cells[--tr->top];
        *cell = ((uintptr_t)cell) | TAG_VAR;   /* variable libre de nuevo */
    }
}

/* ---------- ocurs check ---------- */

int tt_occurs(Term var, Term term)
{
    /* var debe estar dereferenciada y libre */
    if (tt_tag(var) != TAG_VAR)
        return 0;

    Term stack[TT_MAX_ARITY * 2];
    int sp = 0;
    stack[sp++] = term;

    while (sp > 0)
    {
        Term t = tt_deref(stack[--sp], NULL);
        uintptr_t tag = tt_tag(t);
        if (tag == TAG_VAR)
        {
            if (t == var)
                return 1;
            continue;
        }
        if (tag == TAG_STRUCT)
        {
            Compound *c = tt_struct_ptr(t);
            /* poda de pila: si no cabe, falla el check conservador */
            if (sp + c->arity > (int)(sizeof(stack) / sizeof(Term)))
                return 1;
            for (uint32_t i = 0; i < c->arity; i++)
                stack[sp++] = c->args[i];
        }
    }
    return 0;
}

/* ---------- unificación iterativa ---------- */

Term tt_deref(Term t, Term **out_cell)
{
    Term *cell = NULL;
    while (tt_tag(t) == TAG_VAR)
    {
        cell = (Term *)tt_ptr(t);
        Term val = *cell;
        if (val == t)              /* libre: apunta a sí misma */
        {
            if (out_cell) *out_cell = cell;
            return t;
        }
        t = val;                   /* seguir la ligadura */
    }
    if (out_cell) *out_cell = cell;
    return t;
}

int tt_unify(Term a, Term b, TRAIL *tr)
{
    Term stack[TT_MAX_ARITY * 2];
    int sp = 0;

    stack[sp++] = a;
    stack[sp++] = b;

    while (sp > 0)
    {
        Term ta = stack[--sp];
        Term tb = stack[--sp];

        Term *cell_a = NULL;
        Term *cell_b = NULL;
        ta = tt_deref(ta, &cell_a);
        tb = tt_deref(tb, &cell_b);

        if (ta == tb)
            continue;              /* mismo término o misma variable */

        uintptr_t tag_a = tt_tag(ta);
        uintptr_t tag_b = tt_tag(tb);

        if (tag_a == TAG_VAR)
        {
            if (g_occurs_check && tt_occurs(ta, tb))
                return 0;
            if (tr->top >= tr->capacity)
            {
                /* trail lleno: fallo limpio SIN ligar (undo garantizado) */
                return 0;
            }
            *cell_a = tb;
            tr->cells[tr->top++] = cell_a;
            continue;
        }
        if (tag_b == TAG_VAR)
        {
            if (g_occurs_check && tt_occurs(tb, ta))
                return 0;
            if (tr->top >= tr->capacity)
                return 0;
            *cell_b = ta;
            tr->cells[tr->top++] = cell_b;
            continue;
        }

        if (tag_a != tag_b)
            return 0;              /* tipos incompatibles */

        if (tag_a == TAG_STRUCT)
        {
            Compound *ca = tt_struct_ptr(ta);
            Compound *cb = tt_struct_ptr(tb);
            if (ca->functor_id != cb->functor_id || ca->arity != cb->arity)
                return 0;

            uint32_t arity = ca->arity;
            if (sp + 2 * arity > (int)(sizeof(stack) / sizeof(Term)))
                return 0;          /* pila local llena: fallo limpio */
            for (uint32_t i = 0; i < arity; i++)
            {
                stack[sp++] = ca->args[i];
                stack[sp++] = cb->args[i];
            }
        }
        else
        {
            return 0;              /* átomos/enteros distintos */
        }
    }
    return 1;
}