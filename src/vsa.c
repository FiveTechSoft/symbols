#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vsa.h"

/* SplitMix64 pseudo-random generator for high-dispersion orthogonal seeds */
static uint64_t SplitMix64(uint64_t *state)
{
    uint64_t z = (*state += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

void VsaZero(VSA_VECTOR *v)
{
    if (v != NULL)
        memset(v->words, 0, sizeof(v->words));
}

void VsaSetAll(VSA_VECTOR *v)
{
    if (v != NULL)
        memset(v->words, 0xFF, sizeof(v->words));
}

void VsaRandom(VSA_VECTOR *v, uint64_t seed)
{
    if (v == NULL)
        return;
    uint64_t s = seed ? seed : 0x123456789ABCDEF0ULL;
    for (int i = 0; i < VSA_WORDS; i++)
        v->words[i] = SplitMix64(&s);
}

void VsaBind(const VSA_VECTOR *a, const VSA_VECTOR *b, VSA_VECTOR *out)
{
    if (a == NULL || b == NULL || out == NULL)
        return;
    out->words[0] = a->words[0] ^ b->words[0];
    out->words[1] = a->words[1] ^ b->words[1];
    out->words[2] = a->words[2] ^ b->words[2];
    out->words[3] = a->words[3] ^ b->words[3];
}

void VsaUnbind(const VSA_VECTOR *bound, const VSA_VECTOR *key, VSA_VECTOR *out)
{
    /* In binary VSA (XOR algebra), binding is its own inverse:
       (a ^ b) ^ b = a */
    VsaBind(bound, key, out);
}

void VsaBundle(const VSA_VECTOR **vectors, uint32_t count, VSA_VECTOR *out)
{
    if (vectors == NULL || count == 0 || out == NULL)
    {
        if (out != NULL) VsaZero(out);
        return;
    }
    if (count == 1)
    {
        *out = *vectors[0];
        return;
    }

    uint32_t threshold = count / 2;
    VsaZero(out);

    /* Process word by word for maximum cache locality */
    for (int w = 0; w < VSA_WORDS; w++)
    {
        uint32_t bit_counts[64];
        memset(bit_counts, 0, sizeof(bit_counts));

        for (uint32_t k = 0; k < count; k++)
        {
            uint64_t val = vectors[k]->words[w];
            for (int b = 0; b < 64; b++)
            {
                if ((val >> b) & 1ULL)
                    bit_counts[b]++;
            }
        }

        uint64_t res = 0;
        for (int b = 0; b < 64; b++)
        {
            if (bit_counts[b] > threshold)
            {
                res |= (1ULL << b);
            }
            else if (bit_counts[b] == threshold && (count % 2 == 0))
            {
                /* Tie breaking: use bit from the first vector */
                if ((vectors[0]->words[w] >> b) & 1ULL)
                    res |= (1ULL << b);
            }
        }
        out->words[w] = res;
    }
}

void VsaBundle2(const VSA_VECTOR *a, const VSA_VECTOR *b, uint64_t tie_breaker, VSA_VECTOR *out)
{
    if (a == NULL || b == NULL || out == NULL)
        return;
    for (int w = 0; w < VSA_WORDS; w++)
    {
        /* Agreement keeps the bit; disagreement takes tie_breaker */
        out->words[w] = (a->words[w] & b->words[w]) |
                        ((a->words[w] ^ b->words[w]) & tie_breaker);
    }
}

void VsaRotate(const VSA_VECTOR *in, int shift, VSA_VECTOR *out)
{
    if (in == NULL || out == NULL)
        return;

    int s = ((shift % VSA_DIM) + VSA_DIM) % VSA_DIM;
    if (s == 0)
    {
        *out = *in;
        return;
    }

    VsaZero(out);
    for (int i = 0; i < VSA_DIM; i++)
    {
        int src_w = i / 64;
        int src_b = i % 64;
        if ((in->words[src_w] >> src_b) & 1ULL)
        {
            int dst_i = (i + s) % VSA_DIM;
            int dst_w = dst_i / 64;
            int dst_b = dst_i % 64;
            out->words[dst_w] |= (1ULL << dst_b);
        }
    }
}

uint32_t VsaHammingDistance(const VSA_VECTOR *a, const VSA_VECTOR *b)
{
    if (a == NULL || b == NULL)
        return VSA_DIM;
    uint32_t d = 0;
    d += VsaPopcount64(a->words[0] ^ b->words[0]);
    d += VsaPopcount64(a->words[1] ^ b->words[1]);
    d += VsaPopcount64(a->words[2] ^ b->words[2]);
    d += VsaPopcount64(a->words[3] ^ b->words[3]);
    return d;
}

float VsaSimilarity(const VSA_VECTOR *a, const VSA_VECTOR *b)
{
    uint32_t dist = VsaHammingDistance(a, b);
    return 1.0f - (2.0f * (float)dist) / (float)VSA_DIM;
}

/* =========================================================================
   VSA Item Memory (Clean-up Memory)
   ========================================================================= */

static uint32_t VsaNextPow2(uint32_t n)
{
    if (n < 16) return 16;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    return n + 1;
}

static int VsaEnsureId(VSA_ITEM_MEMORY *mem, SYMBOL_ID id)
{
    if (id < mem->id_capacity)
        return 1;

    uint32_t new_cap = VsaNextPow2(id + 1);
    uint32_t *new_by_id = (uint32_t *)realloc(mem->by_id, new_cap * sizeof(uint32_t));
    if (new_by_id == NULL)
        return 0;

    memset(new_by_id + mem->id_capacity, 0, (new_cap - mem->id_capacity) * sizeof(uint32_t));
    mem->by_id = new_by_id;
    mem->id_capacity = new_cap;
    return 1;
}

VSA_ITEM_MEMORY *VsaMemoryCreate(uint32_t capacity)
{
    if (capacity == 0) capacity = 16;
    VSA_ITEM_MEMORY *mem = (VSA_ITEM_MEMORY *)malloc(sizeof(VSA_ITEM_MEMORY));
    if (mem == NULL) return NULL;

    mem->items = (VSA_ITEM *)calloc(capacity, sizeof(VSA_ITEM));
    if (mem->items == NULL)
    {
        free(mem);
        return NULL;
    }
    mem->count = 0;
    mem->capacity = capacity;
    mem->by_id = NULL;
    mem->id_capacity = 0;
    return mem;
}

void VsaMemoryDestroy(VSA_ITEM_MEMORY *mem)
{
    if (mem == NULL) return;
    free(mem->by_id);
    free(mem->items);
    free(mem);
}

int VsaMemorySet(VSA_ITEM_MEMORY *mem, SYMBOL_ID id, const VSA_VECTOR *v)
{
    if (mem == NULL || v == NULL || id == SYMBOL_INVALID)
        return 0;

    if (id < mem->id_capacity && mem->by_id[id] != 0)
    {
        VSA_ITEM *item = &mem->items[mem->by_id[id] - 1];
        item->vector = *v;
        item->initialized = 1;
        return 1;
    }

    if (mem->count >= mem->capacity)
    {
        uint32_t new_cap = mem->capacity * 2;
        VSA_ITEM *new_items = (VSA_ITEM *)realloc(mem->items, new_cap * sizeof(VSA_ITEM));
        if (new_items == NULL) return 0;
        memset(new_items + mem->capacity, 0, (new_cap - mem->capacity) * sizeof(VSA_ITEM));
        mem->items = new_items;
        mem->capacity = new_cap;
    }

    if (!VsaEnsureId(mem, id))
        return 0;

    VSA_ITEM *item = &mem->items[mem->count];
    item->id = id;
    item->vector = *v;
    item->initialized = 1;
    mem->by_id[id] = mem->count + 1;
    mem->count++;
    return 1;
}

const VSA_VECTOR *VsaMemoryGet(const VSA_ITEM_MEMORY *mem, SYMBOL_ID id)
{
    if (mem == NULL || id == SYMBOL_INVALID)
        return NULL;

    if (mem->by_id != NULL && id < mem->id_capacity && mem->by_id[id] != 0)
    {
        const VSA_ITEM *item = &mem->items[mem->by_id[id] - 1];
        if (item->initialized)
            return &item->vector;
    }
    return NULL;
}

SYMBOL_ID VsaMemoryCleanup(const VSA_ITEM_MEMORY *mem,
                          const VSA_VECTOR *query,
                          uint32_t *out_dist,
                          float *out_sim)
{
    if (mem == NULL || query == NULL || mem->count == 0)
    {
        if (out_dist) *out_dist = VSA_DIM;
        if (out_sim) *out_sim = -1.0f;
        return SYMBOL_INVALID;
    }

    uint32_t min_dist = VSA_DIM + 1;
    SYMBOL_ID best_id = SYMBOL_INVALID;

    for (uint32_t i = 0; i < mem->count; i++)
    {
        if (!mem->items[i].initialized)
            continue;
        uint32_t d = VsaHammingDistance(query, &mem->items[i].vector);
        if (d < min_dist)
        {
            min_dist = d;
            best_id = mem->items[i].id;
        }
    }

    if (out_dist) *out_dist = min_dist;
    if (out_sim) *out_sim = 1.0f - (2.0f * (float)min_dist) / (float)VSA_DIM;
    return best_id;
}

/* =========================================================================
   Predicate / Role-Filler Encoding
   ========================================================================= */

void VsaRolesInit(VSA_ROLES *roles, uint64_t seed)
{
    if (roles == NULL) return;
    uint64_t s = seed ? seed : 0xCAFEBABE12345678ULL;
    VsaRandom(&roles->role_subj, SplitMix64(&s));
    VsaRandom(&roles->role_pred, SplitMix64(&s));
    VsaRandom(&roles->role_obj,  SplitMix64(&s));
}

void VsaEncodeTriple(const VSA_ROLES *roles,
                     const VSA_VECTOR *s,
                     const VSA_VECTOR *p,
                     const VSA_VECTOR *o,
                     VSA_VECTOR *out_fact)
{
    if (roles == NULL || s == NULL || p == NULL || o == NULL || out_fact == NULL)
        return;

    VSA_VECTOR bound_s, bound_p, bound_o;
    VsaBind(&roles->role_subj, s, &bound_s);
    VsaBind(&roles->role_pred, p, &bound_p);
    VsaBind(&roles->role_obj,  o, &bound_o);

    const VSA_VECTOR *vecs[3];
    vecs[0] = &bound_s;
    vecs[1] = &bound_p;
    vecs[2] = &bound_o;

    /* Majority voting over the 3 bound roles */
    VsaBundle(vecs, 3, out_fact);
}

void VsaQueryRole(const VSA_VECTOR *fact,
                  const VSA_VECTOR *role,
                  VSA_VECTOR *out_query)
{
    /* Unbinding role from fact:
       Q = Role ^ Fact
       Since Fact = (R_s ^ S) + (R_p ^ P) + (R_o ^ O),
       R_o ^ Fact = O + noise */
    VsaUnbind(fact, role, out_query);
}
