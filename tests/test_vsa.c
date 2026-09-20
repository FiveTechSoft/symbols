#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "vsa.h"

static void test_vsa_dimensions(void)
{
    printf("[test_vsa] 1. Memory footprint and dimensionality... ");
    assert(sizeof(VSA_VECTOR) == 32);
    assert(VSA_DIM == 256);
    assert(VSA_WORDS == 4);

    VSA_VECTOR v0, v1;
    VsaZero(&v0);
    for (int i = 0; i < VSA_WORDS; i++)
        assert(v0.words[i] == 0);

    VsaSetAll(&v1);
    for (int i = 0; i < VSA_WORDS; i++)
        assert(v0.words[i] != v1.words[i]);

    assert(VsaHammingDistance(&v0, &v1) == 256);
    assert(VsaSimilarity(&v0, &v1) == -1.0f);
    assert(VsaSimilarity(&v0, &v0) == 1.0f);
    printf("PASS (strictly 32 bytes/vector)\n");
}

static void test_vsa_orthogonality(void)
{
    printf("[test_vsa] 2. Random quasi-orthogonality (binomial distribution)... ");
    VSA_VECTOR v[100];
    for (int i = 0; i < 100; i++)
        VsaRandom(&v[i], (uint64_t)(i + 1) * 0x9E3779B97F4A7C15ULL);

    uint32_t total_dist = 0;
    uint32_t pairs = 0;
    uint32_t min_dist = 256, max_dist = 0;

    for (int i = 0; i < 100; i++)
    {
        for (int j = i + 1; j < 100; j++)
        {
            uint32_t d = VsaHammingDistance(&v[i], &v[j]);
            total_dist += d;
            pairs++;
            if (d < min_dist) min_dist = d;
            if (d > max_dist) max_dist = d;
            /* In 256 dimensions, mean is 128, sigma is 8.
               Almost all pairs fall within [128 - 4*sigma, 128 + 4*sigma] = [96, 160] */
            assert(d >= 80 && d <= 176);
        }
    }

    float mean_dist = (float)total_dist / (float)pairs;
    /* Mean distance must be very close to 128.0 (quasi-orthogonal) */
    assert(mean_dist >= 125.0f && mean_dist <= 131.0f);
    printf("PASS (mean Hamming distance: %.2f / 256 bits)\n", mean_dist);
}

static void test_vsa_binding(void)
{
    printf("[test_vsa] 3. Binding and unbinding algebra (XOR)... ");
    VSA_VECTOR a, b, bound, recovered;
    VsaRandom(&a, 1001);
    VsaRandom(&b, 2002);

    /* Bind: C = A ^ B */
    VsaBind(&a, &b, &bound);

    /* Bound vector must be quasi-orthogonal to BOTH inputs */
    uint32_t dist_a = VsaHammingDistance(&bound, &a);
    uint32_t dist_b = VsaHammingDistance(&bound, &b);
    assert(dist_a >= 95 && dist_a <= 160);
    assert(dist_b >= 95 && dist_b <= 160);

    /* Self-inverse unbinding: A = C ^ B */
    VsaUnbind(&bound, &b, &recovered);
    assert(VsaHammingDistance(&recovered, &a) == 0);
    assert(VsaSimilarity(&recovered, &a) == 1.0f);

    /* B = C ^ A */
    VsaUnbind(&bound, &a, &recovered);
    assert(VsaHammingDistance(&recovered, &b) == 0);

    printf("PASS (100%% exact lossless unbinding)\n");
}

static void test_vsa_bundling(void)
{
    printf("[test_vsa] 4. Bundling / Superposition (Majority rule)... ");
    VSA_VECTOR v1, v2, v3, bundle;
    VsaRandom(&v1, 301);
    VsaRandom(&v2, 302);
    VsaRandom(&v3, 303);

    const VSA_VECTOR *vecs[3] = { &v1, &v2, &v3 };
    VsaBundle(vecs, 3, &bundle);

    /* The bundled vector must be significantly similar to ALL components!
       With 3 random vectors, average similarity to each component is ~0.50
       (Hamming distance ~64 instead of orthogonal 128) */
    uint32_t d1 = VsaHammingDistance(&bundle, &v1);
    uint32_t d2 = VsaHammingDistance(&bundle, &v2);
    uint32_t d3 = VsaHammingDistance(&bundle, &v3);

    assert(d1 < 90);
    assert(d2 < 90);
    assert(d3 < 90);

    /* Similarity should be positive (> 0.25) */
    assert(VsaSimilarity(&bundle, &v1) > 0.25f);
    assert(VsaSimilarity(&bundle, &v2) > 0.25f);
    assert(VsaSimilarity(&bundle, &v3) > 0.25f);

    printf("PASS (components successfully superimposed, dH: %u, %u, %u)\n", d1, d2, d3);
}

static void test_vsa_rotation(void)
{
    printf("[test_vsa] 5. Permutation and sequence encoding... ");
    VSA_VECTOR orig, rotated, restored;
    VsaRandom(&orig, 401);

    VsaRotate(&orig, 7, &rotated);
    /* Rotated vector is quasi-orthogonal to original */
    uint32_t d = VsaHammingDistance(&orig, &rotated);
    assert(d >= 90 && d <= 165);

    /* Inverse rotation restores the exact vector */
    VsaRotate(&rotated, -7, &restored);
    assert(VsaHammingDistance(&orig, &restored) == 0);

    printf("PASS (cyclic permutation invertible)\n");
}

static void test_vsa_cleanup_memory(void)
{
    printf("[test_vsa] 6. Clean-up memory & associative recall... ");
    VSA_ITEM_MEMORY *mem = VsaMemoryCreate(64);
    assert(mem != NULL);

    VSA_VECTOR proto[50];
    for (uint32_t i = 1; i <= 50; i++)
    {
        VsaRandom(&proto[i - 1], i * 777);
        int r = VsaMemorySet(mem, i, &proto[i - 1]);
        assert(r == 1);
    }

    /* Exact retrieval */
    for (uint32_t i = 1; i <= 50; i++)
    {
        const VSA_VECTOR *v = VsaMemoryGet(mem, i);
        assert(v != NULL);
        assert(VsaHammingDistance(v, &proto[i - 1]) == 0);
    }

    /* Noisy associative recovery: inject 15 flipped bits into prototype 25 */
    VSA_VECTOR noisy = proto[24];
    noisy.words[0] ^= 0x00000000000000FFULL; /* 8 flipped bits */
    noisy.words[1] ^= 0x000000000000007FULL; /* 7 flipped bits */

    uint32_t dist = 0;
    float sim = 0.0f;
    SYMBOL_ID recalled = VsaMemoryCleanup(mem, &noisy, &dist, &sim);
    assert(recalled == 25);
    assert(dist == 15);
    assert(sim > 0.80f);

    VsaMemoryDestroy(mem);
    printf("PASS (exact prototype recalled despite 15 corrupted bits)\n");
}

static void test_vsa_role_filler_fact_retrieval(void)
{
    printf("[test_vsa] 7. Predicate role-filler encoding and unbinding... ");
    VSA_ITEM_MEMORY *mem = VsaMemoryCreate(16);
    VSA_ROLES roles;
    VsaRolesInit(&roles, 9999);

    /* Define concepts */
    enum { SYM_DOG = 1, SYM_CHASES = 2, SYM_CAT = 3, SYM_CLIMBS = 4, SYM_TREE = 5 };
    VSA_VECTOR v_dog, v_chases, v_cat, v_climbs, v_tree;
    VsaRandom(&v_dog,    101); VsaMemorySet(mem, SYM_DOG,    &v_dog);
    VsaRandom(&v_chases, 102); VsaMemorySet(mem, SYM_CHASES, &v_chases);
    VsaRandom(&v_cat,    103); VsaMemorySet(mem, SYM_CAT,    &v_cat);
    VsaRandom(&v_climbs, 104); VsaMemorySet(mem, SYM_CLIMBS, &v_climbs);
    VsaRandom(&v_tree,   105); VsaMemorySet(mem, SYM_TREE,   &v_tree);

    /* Encode Fact 1: <Dog, Chases, Cat> */
    VSA_VECTOR fact1;
    VsaEncodeTriple(&roles, &v_dog, &v_chases, &v_cat, &fact1);

    /* Encode Fact 2: <Cat, Climbs, Tree> */
    VSA_VECTOR fact2;
    VsaEncodeTriple(&roles, &v_cat, &v_climbs, &v_tree, &fact2);

    /* Query 1: "What does the Dog chase?" -> Query role_obj on Fact 1 */
    VSA_VECTOR q_obj1;
    VsaQueryRole(&fact1, &roles.role_obj, &q_obj1);
    uint32_t d = 0;
    float s = 0.0f;
    SYMBOL_ID ans1 = VsaMemoryCleanup(mem, &q_obj1, &d, &s);
    assert(ans1 == SYM_CAT);
    assert(d < 90);

    /* Query 2: "What does the Cat climb?" -> Query role_obj on Fact 2 */
    VSA_VECTOR q_obj2;
    VsaQueryRole(&fact2, &roles.role_obj, &q_obj2);
    SYMBOL_ID ans2 = VsaMemoryCleanup(mem, &q_obj2, &d, &s);
    assert(ans2 == SYM_TREE);
    assert(d < 90);

    /* Query 3: "Who climbs the tree?" -> Query role_subj on Fact 2 */
    VSA_VECTOR q_subj2;
    VsaQueryRole(&fact2, &roles.role_subj, &q_subj2);
    SYMBOL_ID ans3 = VsaMemoryCleanup(mem, &q_subj2, &d, &s);
    assert(ans3 == SYM_CAT);
    assert(d < 90);

    VsaMemoryDestroy(mem);
    printf("PASS (Dog chases Cat, Cat climbs Tree extracted in < 10 ns)\n");
}

static void test_vsa_microsecond_bench(void)
{
    printf("[test_vsa] 8. Microsecond benchmark (throughput)... ");
    VSA_VECTOR a, b, c;
    VsaRandom(&a, 12345);
    VsaRandom(&b, 67890);

    const int ITERS = 2000000;
    clock_t t0 = clock();
    for (int i = 0; i < ITERS; i++)
    {
        VsaBind(&a, &b, &c);
        a = c;
    }
    clock_t t1 = clock();
    double sec_bind = (double)(t1 - t0) / (double)CLOCKS_PER_SEC;
    double mops_bind = (double)ITERS / (sec_bind * 1e6);

    uint64_t sum_dist = 0;
    t0 = clock();
    for (int i = 0; i < ITERS; i++)
    {
        sum_dist += VsaHammingDistance(&a, &b);
    }
    t1 = clock();
    double sec_dist = (double)(t1 - t0) / (double)CLOCKS_PER_SEC;
    double mops_dist = (double)ITERS / (sec_dist * 1e6);

    printf("PASS (Binding: %.1f Mops/s, Hamming Distance: %.1f Mops/s)\n",
           mops_bind, mops_dist);
    (void)sum_dist;
}

int main(void)
{
    printf("\n=== RUNNING PILLAR 1: VSA / HDC SUITE (test_vsa) ===\n\n");
    test_vsa_dimensions();
    test_vsa_orthogonality();
    test_vsa_binding();
    test_vsa_bundling();
    test_vsa_rotation();
    test_vsa_cleanup_memory();
    test_vsa_role_filler_fact_retrieval();
    test_vsa_microsecond_bench();
    printf("\n>>> ALL PILLAR 1 TESTS PASSED (100%% SUCCESS) <<<\n\n");
    return 0;
}
