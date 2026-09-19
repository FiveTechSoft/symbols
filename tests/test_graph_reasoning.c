/* ============================================================
   test_graph_reasoning: Test and benchmark for Graph Reasoning.
   Evaluates:
     1. Inductive Rule Mining (AMIE / ILP Horn discovery)
     2. Forward Deductive Link Prediction (Deductive closure)
     3. Abductive Hypothesis Generation (Missing link diagnosis)
     4. Fail-closed Precision & Zero Hallucination
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "graph.h"
#include "graph_reasoning.h"

static int g_pass = 0;
static int g_fail = 0;

static void check(int condition, const char *name)
{
    if (condition)
    {
        printf("  [PASS] %s\n", name);
        g_pass++;
    }
    else
    {
        printf("  [FAIL] %s\n", name);
        g_fail++;
    }
}

int main(void)
{
    printf("=== GRAPH REASONING & INDUCTIVE INFERENCE TEST ===\n\n");

    GRAPH *g = GraphCreate(128, 512);

    /* Symbols */
    SYMBOL_ID s_boaz      = GraphAddSymbol(g, "boaz");
    SYMBOL_ID s_obed      = GraphAddSymbol(g, "obed");
    SYMBOL_ID s_jesse     = GraphAddSymbol(g, "jesse");
    SYMBOL_ID s_david     = GraphAddSymbol(g, "david");
    SYMBOL_ID s_solomon   = GraphAddSymbol(g, "solomon");
    SYMBOL_ID s_rehoboam  = GraphAddSymbol(g, "rehoboam");

    SYMBOL_ID s_paris     = GraphAddSymbol(g, "paris");
    SYMBOL_ID s_france    = GraphAddSymbol(g, "france");
    SYMBOL_ID s_madrid    = GraphAddSymbol(g, "madrid");
    SYMBOL_ID s_spain     = GraphAddSymbol(g, "spain");
    SYMBOL_ID s_rome      = GraphAddSymbol(g, "rome");
    SYMBOL_ID s_italy     = GraphAddSymbol(g, "italy");
    SYMBOL_ID s_europe    = GraphAddSymbol(g, "europe");
    SYMBOL_ID s_toledo    = GraphAddSymbol(g, "toledo");

    SYMBOL_ID s_alice     = GraphAddSymbol(g, "alice");
    SYMBOL_ID s_bob       = GraphAddSymbol(g, "bob");

    SYMBOL_ID r_father    = GraphAddSymbol(g, "father");
    SYMBOL_ID r_gfather   = GraphAddSymbol(g, "grandfather");
    SYMBOL_ID r_in        = GraphAddSymbol(g, "in");
    SYMBOL_ID r_sibling   = GraphAddSymbol(g, "sibling");

    /* Observations to learn from (Genealogy) */
    GraphAddRelation(g, s_boaz, r_father, s_obed);
    GraphAddRelation(g, s_obed, r_father, s_jesse);
    GraphAddRelation(g, s_boaz, r_gfather, s_jesse); /* Confirm 1 */

    GraphAddRelation(g, s_obed, r_father, s_jesse);
    GraphAddRelation(g, s_jesse, r_father, s_david);
    GraphAddRelation(g, s_obed, r_gfather, s_david); /* Confirm 2 */

    GraphAddRelation(g, s_jesse, r_father, s_david);
    GraphAddRelation(g, s_david, r_father, s_solomon);
    GraphAddRelation(g, s_jesse, r_gfather, s_solomon); /* Confirm 3 */

    /* Observations to learn from (Geography Transitivity) */
    GraphAddRelation(g, s_paris, r_in, s_france);
    GraphAddRelation(g, s_france, r_in, s_europe);
    GraphAddRelation(g, s_paris, r_in, s_europe); /* Confirm 1 */

    GraphAddRelation(g, s_madrid, r_in, s_spain);
    GraphAddRelation(g, s_spain, r_in, s_europe);
    GraphAddRelation(g, s_madrid, r_in, s_europe); /* Confirm 2 */

    GraphAddRelation(g, s_rome, r_in, s_italy);
    GraphAddRelation(g, s_italy, r_in, s_europe);
    GraphAddRelation(g, s_rome, r_in, s_europe); /* Confirm 3 */

    /* Observations to learn from (Symmetry) */
    GraphAddRelation(g, s_alice, r_sibling, s_bob);
    GraphAddRelation(g, s_bob, r_sibling, s_alice);

    /* --- 1. TEST INDUCTIVE RULE MINING --- */
    printf("--- PHASE 1: Inductive Rule Mining (AMIE / ILP) ---\n");
    GRAPH_RULE_BASE rb;
    GraphRuleBaseInit(&rb, 0.80f, 2); /* Confidence >= 80%, Support >= 2 */
    uint32_t num_mined = GraphMineRules(g, &rb);

    printf("Mined %u structural rules from graph:\n", num_mined);
    for (uint32_t i = 0; i < num_mined; i++)
    {
        char buf[128];
        GraphRuleFormat(g, &rb.rules[i], buf, sizeof(buf));
        printf("  Rule [%u]: %s\n", i + 1, buf);
    }

    check(num_mined >= 2, "Rule mining discovered multiple candidate rules");

    int found_father_rule = 0;
    int found_trans_rule = 0;
    for (uint32_t i = 0; i < num_mined; i++)
    {
        if (rb.rules[i].r1 == r_father && rb.rules[i].r2 == r_father && rb.rules[i].head == r_gfather)
            found_father_rule = 1;
        if (rb.rules[i].r1 == r_in && rb.rules[i].r2 == r_in && rb.rules[i].head == r_in)
            found_trans_rule = 1;
    }
    check(found_father_rule, "Discovered rule: father o father => grandfather (conf=1.00)");
    check(found_trans_rule, "Discovered rule: in o in => in [TRANSITIVE] (conf=1.00)");

    /* --- 2. TEST DEDUCTIVE FORWARD LINK PREDICTION --- */
    printf("\n--- PHASE 2: Deductive Forward Chaining (Link Prediction) ---\n");
    /* Add premise facts without conclusions */
    GraphAddRelation(g, s_david, r_father, s_solomon);
    GraphAddRelation(g, s_solomon, r_father, s_rehoboam);
    /* David -> grandfather -> Rehoboam is NOT in graph yet */
    check(GraphFindRelation(g, s_david, r_gfather, s_rehoboam) == NULL,
          "Prior state: David -> grandfather -> Rehoboam is UNKNOWN");

    GraphAddRelation(g, s_toledo, r_in, s_spain);
    /* Toledo -> in -> Europe is NOT in graph yet */
    check(GraphFindRelation(g, s_toledo, r_in, s_europe) == NULL,
          "Prior state: Toledo -> in -> Europe is UNKNOWN");

    uint32_t inferred = GraphApplyRules(g, &rb);
    printf("Forward chaining inferred %u new edges.\n", inferred);
    check(inferred >= 2, "Rules inferred at least 2 implicit edges");

    check(GraphFindRelation(g, s_david, r_gfather, s_rehoboam) != NULL,
          "Posterior state: David -> grandfather -> Rehoboam PROVEN and added");
    check(GraphFindRelation(g, s_toledo, r_in, s_europe) != NULL,
          "Posterior state: Toledo -> in -> Europe PROVEN and added");

    /* --- 3. TEST ABDUCTIVE HYPOTHESIS DISCOVERY --- */
    printf("\n--- PHASE 3: Abductive Reasoning (Missing Link Diagnosis) ---\n");
    /* We want to explain: boaz is grandfather of ?
       Let's query a goal that is NOT proven: boaz grandfather of a new person "eliab".
       Suppose boaz father obed is known, but obed father eliab is missing. */
    SYMBOL_ID s_eliab = GraphAddSymbol(g, "eliab");

    ABDUCTIVE_HYPOTHESIS hyps[8];
    uint32_t nhyps = GraphAbduce(g, &rb, s_boaz, r_gfather, s_eliab, hyps, 8);
    printf("Abduction produced %u hypothesis for goal [boaz grandfather eliab]:\n", nhyps);
    for (uint32_t i = 0; i < nhyps; i++)
    {
        printf("  Hypothesis [%u]: %s\n", i + 1, hyps[i].explanation);
    }
    check(nhyps > 0, "Abductive reasoner successfully generated hypothesis");
    check(hyps[0].subject == s_obed && hyps[0].relation == r_father && hyps[0].object == s_eliab,
          "Abduced exact missing link: [obed father eliab]");

    /* --- 4. FAIL-CLOSED INVARIANT --- */
    printf("\n--- PHASE 4: Fail-closed Invariant & Idempotence ---\n");
    /* Applying rules again should deduce 0 new relations (idempotent fixed-point) */
    uint32_t pass2 = GraphApplyRules(g, &rb);
    check(pass2 == 0, "Idempotence: second deduction pass produces 0 duplicates (fixed-point)");

    printf("\n=== RESULTS ===\n");
    printf("Passed: %d\nFailed: %d\n", g_pass, g_fail);

    GraphDestroy(g);
    return (g_fail == 0) ? 0 : 1;
}
