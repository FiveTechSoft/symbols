/* =========================================================================
   test_ccg_realizer.c: Verification suite for Dynamic Surface Realization & CCG
   Pillar 2: Native C11 Generative Grammar & Combinatory Categorial Realizer
   - Tests CCG category algebra and combinators (>, <, >B, <B, &)
   - Tests declarative morphosyntax, determiners, verb inflections across EN/ES/FR
   - Tests linear-time CCG chart reduction proving derivations derive root category S
   - Tests dynamic subgraph surface realization (5 distinct topologies)
   - Benchmarks high-throughput generation (< 500 us target milestone)
   ========================================================================= */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "ccg_realizer.h"
#include "i18n.h"

static int g_pass = 0;
static int g_fail = 0;

static void check_str(const char *test_name, const char *actual, const char *expected)
{
    if (actual != NULL && strcmp(actual, expected) == 0)
    {
        printf("  [PASS] %s\n", test_name);
        g_pass++;
    }
    else
    {
        printf("  [FAIL] %s\n    Expected: \"%s\"\n    Got:      \"%s\"\n",
               test_name, expected ? expected : "(null)", actual ? actual : "(null)");
        g_fail++;
    }
}

static void check_int(const char *test_name, int actual, int expected)
{
    if (actual == expected)
    {
        printf("  [PASS] %s\n", test_name);
        g_pass++;
    }
    else
    {
        printf("  [FAIL] %s\n    Expected: %d\n    Got:      %d\n",
               test_name, expected, actual);
        g_fail++;
    }
}

static void check_contains(const char *test_name, const char *haystack, const char *needle)
{
    if (haystack != NULL && strstr(haystack, needle) != NULL)
    {
        printf("  [PASS] %s\n", test_name);
        g_pass++;
    }
    else
    {
        printf("  [FAIL] %s\n    Looking for: \"%s\"\n    In text:     \"%s\"\n",
               test_name, needle, haystack ? haystack : "(null)");
        g_fail++;
    }
}

int main(void)
{
    printf("=== RUNNING PILLAR 2: DYNAMIC CCG SURFACE REALIZATION TESTS ===\n\n");

    /* =====================================================================
       Test 1: CCG Category Construction & String Notation
       ===================================================================== */
    printf("--- Test 1: CCG Category Calculus & Construction ---\n");
    {
        CCG_CAT_POOL pool;
        CcgPoolInit(&pool);

        const CCG_CAT *s = CcgCatS(&pool);
        const CCG_CAT *np = CcgCatNP(&pool);
        const CCG_CAT *n = CcgCatN(&pool);
        const CCG_CAT *tv = CcgCatTransitiveVerb(&pool);
        const CCG_CAT *det = CcgCatDeterminer(&pool);
        const CCG_CAT *rel = CcgCatRelativePronoun(&pool);

        char buf[128];
        CcgCatToString(s, buf, sizeof(buf));
        check_str("CCG Category S", buf, "S");

        CcgCatToString(np, buf, sizeof(buf));
        check_str("CCG Category NP", buf, "NP");

        CcgCatToString(det, buf, sizeof(buf));
        check_str("CCG Determiner NP/N", buf, "NP/N");

        CcgCatToString(tv, buf, sizeof(buf));
        check_str("CCG Transitive Verb (S\\NP)/NP", buf, "(S\\NP)/NP");

        CcgCatToString(rel, buf, sizeof(buf));
        check_str("CCG Relative Pronoun (NP\\NP)/(S\\NP)", buf, "(NP\\NP)/(S\\NP)");
    }

    /* =====================================================================
       Test 2: CCG Combinatory Operators (>, <, >B, <B, &)
       ===================================================================== */
    printf("\n--- Test 2: CCG Combinatory Rules ---\n");
    {
        CCG_CAT_POOL pool;
        CcgPoolInit(&pool);

        const CCG_CAT *s = CcgCatS(&pool);
        const CCG_CAT *np = CcgCatNP(&pool);
        const CCG_CAT *n = CcgCatN(&pool);
        const CCG_CAT *det = CcgCatDeterminer(&pool);       /* NP/N */
        const CCG_CAT *tv = CcgCatTransitiveVerb(&pool);    /* (S\NP)/NP */
        const CCG_CAT *iv = CcgCatIntransitiveVerb(&pool);  /* S\NP */

        /* Forward Application: (NP/N) + N => NP */
        const CCG_CAT *np_derived = CcgApplyForward(&pool, det, n);
        check_int("Forward Application (NP/N) + N => NP", CcgCatEquals(np_derived, np), 1);

        /* Forward Application: ((S\NP)/NP) + NP => S\NP */
        const CCG_CAT *vp_derived = CcgApplyForward(&pool, tv, np);
        check_int("Forward Application ((S\\NP)/NP) + NP => S\\NP", CcgCatEquals(vp_derived, iv), 1);

        /* Backward Application: NP + (S\NP) => S */
        const CCG_CAT *s_derived = CcgApplyBackward(&pool, np, iv);
        check_int("Backward Application NP + (S\\NP) => S", CcgCatEquals(s_derived, s), 1);

        /* Coordination: (S\NP) + and((S\NP)\(S\NP))/(S\NP) + (S\NP) => S\NP */
        const CCG_CAT *conj = CcgCatConjunction(&pool, iv);
        const CCG_CAT *coord_vp = CcgCoordinate(&pool, iv, conj, iv);
        check_int("Coordination VP + and + VP => VP", CcgCatEquals(coord_vp, iv), 1);
    }

    /* =====================================================================
       Test 3: Morphosyntactic Agreement & Inflection Tables (HARDCODING=0)
       ===================================================================== */
    printf("\n--- Test 3: Morphosyntactic Agreement & Inflections ---\n");
    {
        MORPHO_AGREEMENT agr;
        CcgMorphDefault(&agr);

        char buf[64];

        /* Determiners */
        agr.noun_kind = NOUN_KIND_COMMON;
        agr.number = MORPH_NUM_SG;
        agr.gender = MORPH_GENDER_MASC;
        CcgGetDeterminer(LANG_EN, &agr, 1, buf, sizeof(buf));
        check_str("EN Definite Singular Determiner", buf, "the");

        CcgGetDeterminer(LANG_ES, &agr, 1, buf, sizeof(buf));
        check_str("ES Definite Singular Masculine Determiner", buf, "el");

        agr.gender = MORPH_GENDER_FEM;
        CcgGetDeterminer(LANG_ES, &agr, 1, buf, sizeof(buf));
        check_str("ES Definite Singular Feminine Determiner", buf, "la");

        agr.number = MORPH_NUM_PL;
        CcgGetDeterminer(LANG_ES, &agr, 1, buf, sizeof(buf));
        check_str("ES Definite Plural Feminine Determiner", buf, "las");

        /* Proper Noun Zero Determiner */
        agr.noun_kind = NOUN_KIND_PROPER;
        int has_det = CcgGetDeterminer(LANG_EN, &agr, 1, buf, sizeof(buf));
        check_int("EN Proper Noun Omit Determiner", has_det, 0);

        /* Verb Inflections */
        agr.noun_kind = NOUN_KIND_COMMON;
        agr.number = MORPH_NUM_SG;
        agr.tense = MORPH_TENSE_PAST;

        CcgInflectVerb(LANG_EN, "flee", &agr, buf, sizeof(buf));
        check_str("EN Past Irregular 'flee' -> 'fled'", buf, "fled");

        CcgInflectVerb(LANG_ES, "flee", &agr, buf, sizeof(buf));
        check_str("ES Past Irregular 'flee' -> 'huyo'", buf, "huyo");

        agr.tense = MORPH_TENSE_PRES;
        CcgInflectVerb(LANG_EN, "pray", &agr, buf, sizeof(buf));
        check_str("EN Present 3SG 'pray' -> 'prays'", buf, "prays");

        agr.number = MORPH_NUM_PL;
        CcgInflectVerb(LANG_EN, "pray", &agr, buf, sizeof(buf));
        check_str("EN Present 3PL 'pray' -> 'pray'", buf, "pray");

        /* Copula */
        agr.number = MORPH_NUM_SG;
        agr.tense = MORPH_TENSE_PRES;
        CcgInflectVerb(LANG_EN, "be", &agr, buf, sizeof(buf));
        check_str("EN Copula Present 3SG 'is'", buf, "is");

        CcgInflectVerb(LANG_ES, "be", &agr, buf, sizeof(buf));
        check_str("ES Copula Present 3SG 'es'", buf, "es");

        CcgInflectVerb(LANG_FR, "be", &agr, buf, sizeof(buf));
        check_str("FR Copula Present 3SG 'est'", buf, "est");
    }

    /* =====================================================================
       Test 4: Syntactic CCG Chart Verification (Linear-time reduction to S)
       ===================================================================== */
    printf("\n--- Test 4: CCG Chart Reduction to Root S ---\n");
    {
        CCG_CAT_POOL pool;
        CcgPoolInit(&pool);

        /* Sentence: "The prophet prayed"
           "The": NP/N
           "prophet": N
           "prayed": S\NP */
        CCG_WORD words1[3] = {
            { .surface = "The",     .category = CcgCatDeterminer(&pool) },
            { .surface = "prophet", .category = CcgCatN(&pool) },
            { .surface = "prayed",  .category = CcgCatIntransitiveVerb(&pool) }
        };

        char trace[128];
        int valid1 = CcgVerifyReduction(&pool, words1, 3, trace, sizeof(trace));
        check_int("CCG Reduction [NP/N, N, S\\NP] => S", valid1, 1);

        /* Sentence: "Jonah fled to Tarshish"
           "Jonah": NP
           "fled": (S\NP)/PP
           "to Tarshish": PP */
        const CCG_CAT *prep_v = CcgCatRight(&pool, CcgCatIntransitiveVerb(&pool), CcgCatPP(&pool));
        CCG_WORD words2[3] = {
            { .surface = "Jonah", .category = CcgCatNP(&pool) },
            { .surface = "fled",  .category = prep_v },
            { .surface = "to Tarshish", .category = CcgCatPP(&pool) }
        };

        int valid2 = CcgVerifyReduction(&pool, words2, 3, trace, sizeof(trace));
        check_int("CCG Reduction [NP, (S\\NP)/PP, PP] => S", valid2, 1);
    }

    /* =====================================================================
       Test 5: Subgraph Topology 1: Simple Transitive Triple
       ===================================================================== */
    printf("\n--- Test 5: Dynamic Realization - Simple Triple ---\n");
    {
        CCG_SUBGRAPH g;
        CcgSubgraphInit(&g, LANG_EN, CCG_TOPO_SIMPLE_TRIPLE);

        MORPHO_AGREEMENT s_agr, o_agr;
        CcgMorphDefault(&s_agr);
        CcgMorphDefault(&o_agr);
        s_agr.noun_kind = NOUN_KIND_PROPER;
        o_agr.noun_kind = NOUN_KIND_PROPER;

        CcgSubgraphAddTriple(&g, "jonah", "flee", "tarshish", "to",
                             &s_agr, &o_agr, MORPH_TENSE_PAST, MORPH_POL_AFFIRMATIVE);

        char out[CCG_BUF_MAX];
        CcgRealizeSubgraph(&g, out, sizeof(out));
        check_str("EN Simple Triple Realization", out, "Jonah fled to Tarshish.");

        /* Spanish realization */
        CCG_SUBGRAPH g_es;
        CcgSubgraphInit(&g_es, LANG_ES, CCG_TOPO_SIMPLE_TRIPLE);
        CcgSubgraphAddTriple(&g_es, "jonas", "flee", "tarsis", "to",
                             &s_agr, &o_agr, MORPH_TENSE_PAST, MORPH_POL_AFFIRMATIVE);
        CcgRealizeSubgraph(&g_es, out, sizeof(out));
        check_str("ES Simple Triple Realization", out, "Jonas huyo a Tarsis.");

        /* English with Common Noun Subject: "The prophet fled to Tarshish." */
        g.lang = LANG_EN;
        s_agr.noun_kind = NOUN_KIND_COMMON;
        CcgSubgraphInit(&g, LANG_EN, CCG_TOPO_SIMPLE_TRIPLE);
        CcgSubgraphAddTriple(&g, "prophet", "flee", "tarshish", "to",
                             &s_agr, &o_agr, MORPH_TENSE_PAST, MORPH_POL_AFFIRMATIVE);
        CcgRealizeSubgraph(&g, out, sizeof(out));
        check_str("EN Common Noun Subject with Determiner", out, "The prophet fled to Tarshish.");

        /* Negative polarity: auxiliary did not + base lemma */
        CcgSubgraphInit(&g, LANG_EN, CCG_TOPO_SIMPLE_TRIPLE);
        s_agr.noun_kind = NOUN_KIND_PROPER;
        CcgSubgraphAddTriple(&g, "jonah", "flee", "tarshish", "to",
                             &s_agr, &o_agr, MORPH_TENSE_PAST, MORPH_POL_NEGATIVE);
        CcgRealizeSubgraph(&g, out, sizeof(out));
        check_str("EN Negative Polarity Realization", out, "Jonah did not flee to Tarshish.");
    }

    /* =====================================================================
       Test 6: Subgraph Topology 2: Copular & Adjectival Attribution
       ===================================================================== */
    printf("\n--- Test 6: Dynamic Realization - Copular Attribution ---\n");
    {
        CCG_SUBGRAPH g;
        CcgSubgraphInit(&g, LANG_EN, CCG_TOPO_COPULA_ATTR);

        MORPHO_AGREEMENT s_agr, o_agr;
        CcgMorphDefault(&s_agr);
        CcgMorphDefault(&o_agr);
        s_agr.noun_kind = NOUN_KIND_PROPER;
        s_agr.tense = MORPH_TENSE_PRES;

        CcgSubgraphAddTriple(&g, "god", "be", "merciful", NULL,
                             &s_agr, &o_agr, MORPH_TENSE_PRES, MORPH_POL_AFFIRMATIVE);

        char out[CCG_BUF_MAX];
        CcgRealizeSubgraph(&g, out, sizeof(out));
        check_str("EN Copular Attribution", out, "God is merciful.");

        /* Spanish */
        g.lang = LANG_ES;
        CcgRealizeSubgraph(&g, out, sizeof(out));
        check_str("ES Copular Attribution", out, "God es merciful.");
    }

    /* =====================================================================
       Test 7: Subgraph Topology 3: 2-Hop Chain with Relative Subordination
       ===================================================================== */
    printf("\n--- Test 7: Dynamic Realization - 2-Hop Relative Subordination ---\n");
    {
        CCG_SUBGRAPH g;
        CcgSubgraphInit(&g, LANG_EN, CCG_TOPO_CHAIN_2HOP);

        MORPHO_AGREEMENT s_agr, o_agr;
        CcgMorphDefault(&s_agr);
        CcgMorphDefault(&o_agr);
        s_agr.noun_kind = NOUN_KIND_PROPER;
        o_agr.noun_kind = NOUN_KIND_PROPER;
        o_agr.animacy   = ANIMACY_ANIMATE;

        /* Abraham begat Isaac */
        CcgSubgraphAddTriple(&g, "abraham", "begat", "isaac", NULL,
                             &s_agr, &o_agr, MORPH_TENSE_PAST, MORPH_POL_AFFIRMATIVE);

        /* Isaac begat Jacob */
        CcgSubgraphAddTriple(&g, "isaac", "begat", "jacob", NULL,
                             &s_agr, &o_agr, MORPH_TENSE_PAST, MORPH_POL_AFFIRMATIVE);

        char out[CCG_BUF_MAX];
        CcgRealizeSubgraph(&g, out, sizeof(out));
        check_str("EN 2-Hop Relative Clause Subordination", out, "Abraham begat Isaac, who begat Jacob.");

        /* Spanish */
        g.lang = LANG_ES;
        CcgRealizeSubgraph(&g, out, sizeof(out));
        check_str("ES 2-Hop Relative Clause Subordination", out, "Abraham engendro a Isaac, quien engendro a Jacob.");
    }

    /* =====================================================================
       Test 8: Subgraph Topology 4: Coordinated Predication (Shared Subject)
       ===================================================================== */
    printf("\n--- Test 8: Dynamic Realization - Coordinated Predication ---\n");
    {
        CCG_SUBGRAPH g;
        CcgSubgraphInit(&g, LANG_EN, CCG_TOPO_COORDINATED_PRED);

        MORPHO_AGREEMENT s_agr, o1_agr, o2_agr;
        CcgMorphDefault(&s_agr);
        CcgMorphDefault(&o1_agr);
        CcgMorphDefault(&o2_agr);
        s_agr.noun_kind = NOUN_KIND_PROPER;
        o1_agr.noun_kind = NOUN_KIND_COMMON;
        o2_agr.noun_kind = NOUN_KIND_PROPER;

        /* Jonah prayed to the Lord */
        CcgSubgraphAddTriple(&g, "jonah", "pray", "lord", "to",
                             &s_agr, &o1_agr, MORPH_TENSE_PAST, MORPH_POL_AFFIRMATIVE);

        /* and fled to Tarshish */
        CcgSubgraphAddTriple(&g, "jonah", "flee", "tarshish", "to",
                             &s_agr, &o2_agr, MORPH_TENSE_PAST, MORPH_POL_AFFIRMATIVE);

        char out[CCG_BUF_MAX];
        CcgRealizeSubgraph(&g, out, sizeof(out));
        check_str("EN Coordinated Predication", out, "Jonah prayed to the Lord and fled to Tarshish.");
    }

    /* =====================================================================
       Test 9: Subgraph Topology 5: Causal Entailment Subordination
       ===================================================================== */
    printf("\n--- Test 9: Dynamic Realization - Causal Subordination ---\n");
    {
        CCG_SUBGRAPH g;
        CcgSubgraphInit(&g, LANG_EN, CCG_TOPO_CAUSAL_ENTAILMENT);

        MORPHO_AGREEMENT s1_agr, s2_agr;
        CcgMorphDefault(&s1_agr);
        CcgMorphDefault(&s2_agr);
        s1_agr.noun_kind = NOUN_KIND_COMMON;
        s2_agr.noun_kind = NOUN_KIND_COMMON;

        /* Because the tempest was violent */
        CcgSubgraphAddTriple(&g, "tempest", "be", "violent", NULL,
                             &s1_agr, &s1_agr, MORPH_TENSE_PAST, MORPH_POL_AFFIRMATIVE);

        /* the ship was endangered */
        CcgSubgraphAddTriple(&g, "ship", "be", "endangered", NULL,
                             &s2_agr, &s2_agr, MORPH_TENSE_PAST, MORPH_POL_AFFIRMATIVE);

        char out[CCG_BUF_MAX];
        CcgRealizeSubgraph(&g, out, sizeof(out));
        check_str("EN Causal Subordination", out, "Because the tempest was violent, the ship was endangered.");
    }

    /* =====================================================================
       Test 10: Performance & Latency Benchmark (< 500 us milestone)
       ===================================================================== */
    printf("\n--- Test 10: Performance & Latency Benchmark ---\n");
    {
        CCG_SUBGRAPH g;
        CcgSubgraphInit(&g, LANG_EN, CCG_TOPO_CHAIN_2HOP);

        MORPHO_AGREEMENT s_agr, o_agr;
        CcgMorphDefault(&s_agr);
        CcgMorphDefault(&o_agr);
        s_agr.noun_kind = NOUN_KIND_PROPER;
        o_agr.noun_kind = NOUN_KIND_PROPER;
        o_agr.animacy   = ANIMACY_ANIMATE;

        CcgSubgraphAddTriple(&g, "abraham", "begat", "isaac", NULL,
                             &s_agr, &o_agr, MORPH_TENSE_PAST, MORPH_POL_AFFIRMATIVE);
        CcgSubgraphAddTriple(&g, "isaac", "begat", "jacob", NULL,
                             &s_agr, &o_agr, MORPH_TENSE_PAST, MORPH_POL_AFFIRMATIVE);

        char out[CCG_BUF_MAX];
        const uint32_t N_ITER = 500000;
        clock_t t0 = clock();
        for (uint32_t i = 0; i < N_ITER; i++)
        {
            CcgRealizeSubgraph(&g, out, sizeof(out));
        }
        clock_t t1 = clock();
        double total_sec = (double)(t1 - t0) / CLOCKS_PER_SEC;
        double us_per_sentence = (total_sec / N_ITER) * 1e6;
        double s_per_sec = N_ITER / total_sec;

        printf("  [BENCH] %u sentences generated in %.3f s\n", N_ITER, total_sec);
        printf("  [BENCH] Latency: %.3f us/sentence (Target: < 500.000 us)\n", us_per_sentence);
        printf("  [BENCH] Throughput: %.0f sentences/sec\n", s_per_sec);

        check_int("Latency meets Roadmap Pillar 2 milestone (< 500 us)", (us_per_sentence < 500.0), 1);
    }

    printf("\n=======================================================\n");
    printf("PILLAR 2 CCG REALIZER SUMMARY: %d PASSED, %d FAILED\n", g_pass, g_fail);
    printf("=======================================================\n");

    return (g_fail == 0) ? 0 : 1;
}
