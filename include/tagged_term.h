#ifndef TAGGED_TERM_H
#define TAGGED_TERM_H

/* ============================================================
   tagged_term.h — Términos Prolog etiquetados en una palabra.

   Layout de 64 bits (x86_64/ARM64, malloc devuelve >= 8-byte
   alignment, los 3 bits bajos de cualquier puntero son 000):

     63                                                        3  2 1 0
    +----------------------------------------------------------+-------+
    |                direccion / carga util                    |  tag  |
    +----------------------------------------------------------+-------+
      000 -> VAR    (puntero a celda Term; libre si *cell == t)
      001 -> ATOM   (id de 29 bits desplazado << 3, inmediato)
      010 -> NUM    (entero 61 bits con signo, inmediato)
      011 -> STRUCT (puntero a Compound en el pool de terminos)

   Memoria: los Compound viven en un TERM_POOL de bump allocation
   (1 ciclo por termino); backtracking libera restaurando top.
   ============================================================ */

#include <stdint.h>
#include <stddef.h>

_Static_assert(sizeof(uintptr_t) == 8, "tagged_term requiere 64 bits");

typedef uintptr_t Term;

#define TAG_MASK    ((uintptr_t)0x7)
#define PTR_MASK    (~((uintptr_t)0x7))

#define TAG_VAR     ((uintptr_t)0x0)  /* 000 */
#define TAG_ATOM    ((uintptr_t)0x1)  /* 001 */
#define TAG_NUM     ((uintptr_t)0x2)  /* 010 */
#define TAG_STRUCT  ((uintptr_t)0x3)  /* 011 */

#define TERM_NULL   ((Term)0)

typedef struct
{
    uint32_t functor_id;
    uint32_t arity;
    Term     args[];   /* flexible array member */
} Compound;

/* Bump allocator de terminos: backtracking = restaurar top (O(0)) */
typedef struct
{
    uint8_t *buffer;
    size_t   top;
    size_t   capacity;
} TERM_POOL;

/* Trail: solo direcciones de celdas de variables ligadas.
   Capacidad fija definida en la creacion; desbordarse = fallo
   de unificacion limpio (fail-closed), nunca corrupcion. */
typedef struct
{
    Term   **cells;
    uint32_t top;
    uint32_t capacity;
} TRAIL;

#define TT_MAX_ARITY 100  /* unify necesita 2*arity slots en su pila local */

/* ---------- extractores / constructores (hot path, inline) ---------- */

static inline uintptr_t tt_tag(Term t) { return t & TAG_MASK; }
static inline void *tt_ptr(Term t)     { return (void *)(t & PTR_MASK); }

static inline Term tt_var(Term *cell)
{
    *cell = ((uintptr_t)cell) | TAG_VAR;
    return *cell;
}

static inline Term tt_atom(uint32_t atom_id)
{
    return (((uintptr_t)atom_id) << 3) | TAG_ATOM;
}

static inline uint32_t tt_get_atom_id(Term t)
{
    return (uint32_t)(t >> 3);
}

static inline Term tt_int(int64_t val)
{
    return (((uintptr_t)val) << 3) | TAG_NUM;
}

static inline int64_t tt_get_int(Term t)
{
    return ((int64_t)t) >> 3;   /* aritmetico: conserva signo */
}

static inline Compound *tt_struct_ptr(Term t)
{
    return (Compound *)(t & PTR_MASK);
}

/* ---------- pool de terminos ---------- */

void  tt_pool_init(TERM_POOL *pool, void *buffer, size_t capacity);
void *tt_pool_alloc(TERM_POOL *pool, size_t size);
Term  tt_pool_var(TERM_POOL *pool);   /* celda de variable en el pool */
Term  tt_make_struct(TERM_POOL *pool, uint32_t functor_id,
                     uint32_t arity, const Term *args);
/* Marca para backtrack de memoria: tt_pool_restore(mark) */
size_t tt_pool_mark(const TERM_POOL *pool);
void   tt_pool_restore(TERM_POOL *pool, size_t mark);

/* ---------- trail ---------- */

int       tt_trail_init(TRAIL *tr, uint32_t capacity);
void      tt_trail_free(TRAIL *tr);
uint32_t  tt_trail_mark(const TRAIL *tr);
void      tt_trail_restore(TRAIL *tr, uint32_t mark);

/* ---------- deref / unificacion ---------- */

Term tt_deref(Term t, Term **out_cell);

/* Unificacion iterativa (sin recursion en C). Devuelve 1 = ok,
   0 = fallo. El trail registra todo binding para undo. */
int tt_unify(Term a, Term b, TRAIL *tr);

/* Ocurs check opcional (off por defecto, como Prolog):
   activado, unify(X, f(X)) falla en vez de crear un ciclo. */
void tt_set_occurs_check(int enabled);
int  tt_occurs_check_enabled(void);
int  tt_occurs(Term var, Term term);

#endif