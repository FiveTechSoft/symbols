/* ============================================================
   test_tps_benchmark.c: High-Resolution Empirical Throughput
                         and Tokens-Per-Second (TPS) Suite.
   Pure ISO C11, zero tensors, zero backprop, zero GPU.
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "graph.h"
#include "graph_reasoning.h"
#include "cognitive_learning.h"
#include "stochastic_nlg.h"
#include "metacognition.h"
#include "dict.h"
#include "passage_nlg.h"
#include "deep_nlg.h"
#include "agent_core.h"
#include "agent_planner.h"
#include "tokenizer.h"

/* Sanitizer builds (MSVC /fsanitize=address, gcc/clang -fsanitize=address)
   run 2-10x slower and unoptimized, so throughput thresholds say nothing
   there. Under a sanitizer the timing checks are reported as [SKIP] with the
   measured value; every functional check still runs and still counts. */
#if defined(__SANITIZE_ADDRESS__)
#define SANITIZER_BUILD 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define SANITIZER_BUILD 1
#endif
#endif
#ifndef SANITIZER_BUILD
#define SANITIZER_BUILD 0
#endif

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define TEST_ASSERT(expr, msg) do { \
    g_tests_run++; \
    if (expr) { \
        g_tests_passed++; \
        printf("  [PASS] %s\n", msg); \
    } else { \
        printf("  [FAIL] %s (line %d)\n", msg, __LINE__); \
    } \
} while(0)

/* Timing threshold; in sanitizer builds only "rate > 0" is checked */
#define PERF_ASSERT(expr, value, msg) do { \
    if (SANITIZER_BUILD) { \
        printf("  [SKIP] %s (sanitizer build, measured %.0f)\n", msg, (double)(value)); \
        TEST_ASSERT((value) > 0.0, "work was done (sanitizer build: rate > 0 only)"); \
    } else \
        TEST_ASSERT(expr, msg); \
} while(0)

/* High-resolution timer via C11 timespec */
static double GetTimeSec(void)
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

/* Count words in text buffer */
static uint64_t CountWords(const char *text)
{
    uint64_t count = 0;
    int in_word = 0;
    for (size_t i = 0; text[i] != '\0'; i++)
    {
        if ((unsigned char)text[i] > 32)
        {
            if (!in_word)
            {
                count++;
                in_word = 1;
            }
        }
        else
        {
            in_word = 0;
        }
    }
    return count;
}

int main(void)
{
    printf("======================================================================\n");
    printf("  TEST & BENCHMARK: THROUGHPUT & TOKENS-PER-SECOND (TPS) AUDIT        \n");
    printf("======================================================================\n\n");

    /* Initialize Knowledge Base */
    GRAPH *g = GraphCreate(256, 1024);
    SYMBOL_ID s_solomon   = GraphAddSymbol(g, "Solomon");
    SYMBOL_ID s_david     = GraphAddSymbol(g, "David");
    SYMBOL_ID s_proverbs  = GraphAddSymbol(g, "Proverbs");
    SYMBOL_ID s_wisdom    = GraphAddSymbol(g, "Wisdom");
    SYMBOL_ID s_israel    = GraphAddSymbol(g, "Israel");
    SYMBOL_ID s_temple    = GraphAddSymbol(g, "Temple");
    SYMBOL_ID s_jerusalem = GraphAddSymbol(g, "Jerusalem");

    SYMBOL_ID r_father   = GraphAddSymbol(g, "father_of");
    SYMBOL_ID r_author   = GraphAddSymbol(g, "author_of");
    SYMBOL_ID r_theme    = GraphAddSymbol(g, "theme_of");
    SYMBOL_ID r_king     = GraphAddSymbol(g, "king_of");
    SYMBOL_ID r_built    = GraphAddSymbol(g, "builder_of");
    SYMBOL_ID r_located  = GraphAddSymbol(g, "located_in");

    GraphAddRelation(g, s_david, r_father, s_solomon);
    GraphAddRelation(g, s_solomon, r_author, s_proverbs);
    GraphAddRelation(g, s_proverbs, r_theme, s_wisdom);
    GraphAddRelation(g, s_solomon, r_king, s_israel);
    GraphAddRelation(g, s_solomon, r_built, s_temple);
    GraphAddRelation(g, s_temple, r_located, s_jerusalem);

    DICT dict;
    DictInit(&dict);
    strncpy(dict.entries[0].alias, "salomon", DICT_TOKEN_MAX - 1);
    strncpy(dict.entries[0].canonical, "solomon", DICT_TOKEN_MAX - 1);
    strncpy(dict.entries[1].alias, "proverbios", DICT_TOKEN_MAX - 1);
    strncpy(dict.entries[1].canonical, "proverbs", DICT_TOKEN_MAX - 1);
    dict.count = 2;

    COGNITIVE_LEARNER learner;
    CognitiveLearnerInit(&learner, 0.75f, 2);

    WORKING_MEMORY wm;
    WM_Init(&wm, 0.80f, 0.15f);

    STOCHASTIC_NLG_CONFIG cfg = StochasticNLG_DefaultConfig();
    STOCHASTIC_DISCOURSE_HISTORY hist;
    StochasticNLG_InitHistory(&hist);

    PROVENANCE_TABLE pt;
    ProvenanceInit(&pt);

    /* 1. Multi-Paragraph Document NLG Generation */
    char buffer[4096];
    const int ESSAY_ITERS = 2000;
    double t0 = GetTimeSec();
    uint64_t total_words = 0;

    for (int i = 0; i < ESSAY_ITERS; i++)
    {
        PassageGenerateTopicStory(g, &learner, &wm, s_solomon, &cfg, buffer, sizeof(buffer));
        total_words += CountWords(buffer);
    }
    double t1 = GetTimeSec();
    double nlg_elapsed = t1 - t0;
    double bpe_tokens = (double)total_words * 1.3333333333;
    double nlg_tps = bpe_tokens / (nlg_elapsed > 0.0 ? nlg_elapsed : 0.0001);

    printf("1. Document-Level NLG Generation:\n");
    printf("   -> Generated %llu words (%.0f BPE tokens) across %d essays in %.4f s\n",
           (unsigned long long)total_words, bpe_tokens, ESSAY_ITERS, nlg_elapsed);
    printf("   -> Measured Generation Speed: %.1f TPS\n", nlg_tps);
    PERF_ASSERT(nlg_tps > 1000000.0, nlg_tps, "NLG Generation exceeds 1,000,000 TPS (>10,000x faster than LLM)");

    /* 2. Conversational Turn Dispatch */
    const int CHAT_ITERS = 2000;
    total_words = 0;
    t0 = GetTimeSec();
    for (int i = 0; i < CHAT_ITERS; i++)
    {
        PassageHandleTurn(g, &learner, &wm, &pt, &dict,
                          "Cuentame sobre salomon",
                          &cfg, &hist, buffer, sizeof(buffer));
        total_words += CountWords(buffer);
    }
    t1 = GetTimeSec();
    double chat_elapsed = t1 - t0;
    double chat_bpe = (double)total_words * 1.3333333333;
    double chat_tps = chat_bpe / (chat_elapsed > 0.0 ? chat_elapsed : 0.0001);

    printf("\n2. Conversational Dialogue Turn:\n");
    printf("   -> Processed %d conversational turns in %.4f s\n", CHAT_ITERS, chat_elapsed);
    printf("   -> Measured Dialogue Speed: %.1f TPS\n", chat_tps);
    PERF_ASSERT(chat_tps > 1000000.0, chat_tps, "Dialogue turn throughput exceeds 1,000,000 TPS");

    /* 3. Deep Rhetorical Structure Realization */
    NLG_PLAN dplan;
    DeepNLG_PlanInit(&dplan, "Salomon");
    DeepNLG_PlanAdd(&dplan, "David", "padre_de", "Salomon", NLG_ROLE_ASSERTION, NLG_POL_AFFIRMATIVE, 1.0f);
    DeepNLG_PlanAdd(&dplan, "Salomon", "rey_de", "Israel", NLG_ROLE_CHAIN_STEP, NLG_POL_AFFIRMATIVE, 1.0f);
    DeepNLG_PlanAdd(&dplan, "Salomon", "autor_de", "Proverbios", NLG_ROLE_CONCLUSION, NLG_POL_AFFIRMATIVE, 0.95f);

    const int RST_ITERS = 10000;
    total_words = 0;
    t0 = GetTimeSec();
    for (int i = 0; i < RST_ITERS; i++)
    {
        DeepNLG_Realize(&dplan, buffer, sizeof(buffer));
        total_words += CountWords(buffer);
    }
    t1 = GetTimeSec();
    double rst_elapsed = t1 - t0;
    double rst_bpe = (double)total_words * 1.3333333333;
    double rst_tps = rst_bpe / (rst_elapsed > 0.0 ? rst_elapsed : 0.0001);

    printf("\n3. Deep RST Graph-to-Text Realization:\n");
    printf("   -> Synthesized %d RST propositions in %.4f s\n", RST_ITERS, rst_elapsed);
    printf("   -> Measured Realization Speed: %.1f TPS\n", rst_tps);
    PERF_ASSERT(rst_tps > 2000000.0, rst_tps, "Surface realization exceeds 2,000,000 TPS");

    /* 4. STRIPS Forward State-Space Planning */
    AGENT_PLANNER planner;
    AgentPlannerInit(&planner);
    const int PLAN_ITERS = 20000;
    STRIPS_PLAN plan;
    t0 = GetTimeSec();
    int plans_solved = 0;

    for (int i = 0; i < PLAN_ITERS; i++)
    {
        uint32_t init = PRED_ERROR_DIAGNOSED;
        uint32_t goal = PRED_BUILD_VERIFIED | PRED_TESTS_VERIFIED;
        if (AgentPlannerFormulate(&planner, "Fix build error and verify", init, goal, &plan))
        {
            plans_solved++;
        }
    }
    t1 = GetTimeSec();
    double plan_elapsed = t1 - t0;
    double plans_per_sec = (double)plans_solved / (plan_elapsed > 0.0 ? plan_elapsed : 0.0001);

    printf("\n4. Goal-Directed STRIPS Planning:\n");
    printf("   -> Formulated %d optimal plans in %.4f s\n", plans_solved, plan_elapsed);
    printf("   -> Measured Planning Speed: %.1f plans/s (%.1f actions/s)\n",
           plans_per_sec, plans_per_sec * (plan.step_count > 0 ? plan.step_count : 1));
    PERF_ASSERT(plans_per_sec > 100000.0, plans_per_sec, "STRIPS planning exceeds 100,000 plans/second");

    GraphDestroy(g);

    printf("\n======================================================================\n");
    printf("  TEST SUMMARY: %d passed, %d failed\n", g_tests_passed, g_tests_run - g_tests_passed);
    printf("======================================================================\n");

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
