#ifndef VSA_H
#define VSA_H

#include <stdint.h>
#include <stddef.h>
#include "symbol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
   Vector Symbolic Architecture (VSA) / Hyperdimensional Computing (HDC)
   Pillar 1: 256-bit Binary Spatter Codes (Kanerva's BSC)
   - strictly 32 bytes per vector (2 vectors fit in a 64-byte L1 cache line)
   - Binding (XOR) in 1 CPU cycle (< 0.5 ns)
   - Similarity (Hamming via POPCNT) in 2 CPU cycles (< 0.8 ns)
   - 100% deterministic, zero neural weights, zero floating-point matmul
   ========================================================================= */

#define VSA_DIM    256
#define VSA_WORDS  (VSA_DIM / 64) /* 4 x 64-bit words = 32 bytes */

typedef struct
{
    uint64_t words[VSA_WORDS];
} VSA_VECTOR;

/* Fast cross-platform 64-bit population count (bits set to 1) */
static inline uint32_t VsaPopcount64(uint64_t x)
{
#if defined(__GNUC__) || defined(__clang__)
    return (uint32_t)__builtin_popcountll(x);
#elif defined(_MSC_VER)
    return (uint32_t)__popcnt64(x);
#else
    /* Portable 64-bit SWAR Hamming weight */
    x = x - ((x >> 1) & 0x5555555555555555ULL);
    x = (x & 0x3333333333333333ULL) + ((x >> 2) & 0x3333333333333333ULL);
    x = (x + (x >> 4)) & 0x0F0F0F0F0F0F0F0FULL;
    return (uint32_t)((x * 0x0101010101010101ULL) >> 56);
#endif
}

/* Vector Initialization & Reset */
void VsaZero(VSA_VECTOR *v);
void VsaSetAll(VSA_VECTOR *v);
void VsaRandom(VSA_VECTOR *v, uint64_t seed);

/* Core VSA Operations: Binding, Bundling, Permutation */
/* Binding (XOR): self-inverse (a ^ b ^ b = a), preserves distance */
void VsaBind(const VSA_VECTOR *a, const VSA_VECTOR *b, VSA_VECTOR *out);
void VsaUnbind(const VSA_VECTOR *bound, const VSA_VECTOR *key, VSA_VECTOR *out);

/* Bundling / Superposition (Majority voting): preserves similarity to components */
void VsaBundle(const VSA_VECTOR **vectors, uint32_t count, VSA_VECTOR *out);
void VsaBundle2(const VSA_VECTOR *a, const VSA_VECTOR *b, uint64_t tie_breaker, VSA_VECTOR *out);

/* Cyclic Permutation / Rotation: encodes sequential order (rho) */
void VsaRotate(const VSA_VECTOR *in, int shift, VSA_VECTOR *out);

/* Similarity Metrics */
/* Hamming distance: number of differing bits [0..256] */
uint32_t VsaHammingDistance(const VSA_VECTOR *a, const VSA_VECTOR *b);

/* Normalized Cosine/Bipolar similarity [-1.0, 1.0] (1.0 = identical, 0.0 = orthogonal) */
float VsaSimilarity(const VSA_VECTOR *a, const VSA_VECTOR *b);

/* Item Memory / Clean-up Memory (Associative Recall) */
typedef struct
{
    SYMBOL_ID   id;
    VSA_VECTOR  vector;
    int         initialized;
} VSA_ITEM;

typedef struct
{
    VSA_ITEM   *items;
    uint32_t    count;
    uint32_t    capacity;
    uint32_t   *by_id;       /* dense O(1) index by SYMBOL_ID */
    uint32_t    id_capacity;
} VSA_ITEM_MEMORY;

VSA_ITEM_MEMORY *VsaMemoryCreate(uint32_t capacity);
void VsaMemoryDestroy(VSA_ITEM_MEMORY *mem);

int VsaMemorySet(VSA_ITEM_MEMORY *mem, SYMBOL_ID id, const VSA_VECTOR *v);
const VSA_VECTOR *VsaMemoryGet(const VSA_ITEM_MEMORY *mem, SYMBOL_ID id);

/* Clean-up Memory Query: finds the nearest canonical item to query */
SYMBOL_ID VsaMemoryCleanup(const VSA_ITEM_MEMORY *mem,
                          const VSA_VECTOR *query,
                          uint32_t *out_dist,
                          float *out_sim);

/* Predicate / Role-Filler Encoding System:
   Encodes <Subject, Predicate, Object> into a composite 256-bit vector:
   Fact = (R_subj ^ S) + (R_pred ^ P) + (R_obj ^ O)
   Allows microsecond unbinding: Query_Object = R_obj ^ Fact -> Cleanup(Query) => O */
typedef struct
{
    VSA_VECTOR role_subj;
    VSA_VECTOR role_pred;
    VSA_VECTOR role_obj;
} VSA_ROLES;

void VsaRolesInit(VSA_ROLES *roles, uint64_t seed);

void VsaEncodeTriple(const VSA_ROLES *roles,
                     const VSA_VECTOR *s,
                     const VSA_VECTOR *p,
                     const VSA_VECTOR *o,
                     VSA_VECTOR *out_fact);

void VsaQueryRole(const VSA_VECTOR *fact,
                  const VSA_VECTOR *role,
                  VSA_VECTOR *out_query);

#ifdef __cplusplus
}
#endif

#endif /* VSA_H */
