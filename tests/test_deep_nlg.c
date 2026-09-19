/* ============================================================
   test_deep_nlg: Test suite for Deep Symbolic NLG (Camino 1).
   Verifies:
     - 1-hop, 2-hop, 3-hop genealogical chain aggregation
     - Compound multi-fact aggregation with subject elision
     - Fail-closed articulated abstention (epistemic honesty)
     - Multi-lingual rendering (ES, EN, FR)
     - Zero alucinations, deterministic idempotence
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "deep_nlg.h"
#include "i18n.h"

static int g_pass = 0;
static int g_fail = 0;

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
    char out[DEEP_NLG_BUF_MAX];
    printf("=== RUNNING DEEP SYMBOLIC NLG TESTS (CAMINO 1) ===\n\n");

    /* 1. Spanish 2-Hop Chain: Boaz -> Obed -> Jesse */
    {
        LangSet(LANG_ES);
        const char hops[2][DEEP_NLG_TOKEN_MAX] = { "obed", "jesse" };
        DeepNLG_GenerateChainText("boaz", "engendro a", hops, 2, "abuelo", "jesse", out, sizeof(out));
        check_contains("ES 2-hop chain includes start", out, "Boaz");
        check_contains("ES 2-hop chain includes relative connective", out, "quien a su vez fue padre de");
        check_contains("ES 2-hop chain includes deduction conclusion", out, "Por tanto, Boaz es el abuelo de Jesse");
    }

    /* 2. English 3-Hop Deep Chain: Boaz -> Obed -> Jesse -> David */
    {
        LangSet(LANG_EN);
        const char hops[3][DEEP_NLG_TOKEN_MAX] = { "obed", "jesse", "david" };
        DeepNLG_GenerateChainText("boaz", "begat", hops, 3, "great-grandfather", "david", out, sizeof(out));
        check_contains("EN 3-hop chain includes start", out, "Boaz");
        check_contains("EN 3-hop chain intermediate connective", out, "who in turn was the father of");
        check_contains("EN 3-hop chain last connective", out, "and the latter was the father of");
        check_contains("EN 3-hop chain conclusion", out, "Therefore, Boaz is the great-grandfather of David");
    }

    /* 3. Compound Entity Fact Aggregation (ES) */
    {
        LangSet(LANG_ES);
        const char rels[3][DEEP_NLG_TOKEN_MAX] = { "hijo", "rey", "padre" };
        const char objs[3][DEEP_NLG_TOKEN_MAX] = { "jesse", "israel", "salomon" };
        DeepNLG_GenerateMultiFactText("david", rels, objs, 3, out, sizeof(out));
        check_contains("ES compound fact includes intro", out, "Segun consta en los registros verificados");
        check_contains("ES compound fact includes first relation", out, "David es hijo de Jesse");
        check_contains("ES compound fact includes coordinator", out, "y rey de Israel");
        check_contains("ES compound fact includes final connector", out, "y ademas padre de Salomon");
    }

    /* 4. Fail-Closed Honest Abstention (Entity unknown) */
    {
        LangSet(LANG_ES);
        DeepNLG_GenerateAbstainText("lucifer", NULL, NULL, out, sizeof(out));
        check_contains("ES honest abstention on unknown entity", out, "No tengo constancia de Lucifer");
    }

    /* 5. Fail-Closed Honest Abstention (Relation unknown with context hint) */
    {
        LangSet(LANG_ES);
        DeepNLG_GenerateAbstainText("david", "madre", "1 Samuel 16:1 menciona a su padre Jesse", out, sizeof(out));
        check_contains("ES honest abstention on missing relation", out, "no hay constancia verificada de madre");
        check_contains("ES abstention includes context hint", out, "La referencia mas cercana senala");
        check_contains("ES abstention includes hint content", out, "1 Samuel 16:1 menciona a su padre Jesse");
    }

    /* 6. French Language Support */
    {
        LangSet(LANG_FR);
        const char hops[2][DEEP_NLG_TOKEN_MAX] = { "obed", "jesse" };
        DeepNLG_GenerateChainText("boaz", "engendra", hops, 2, "grand-pere", "jesse", out, sizeof(out));
        check_contains("FR 2-hop chain connective", out, "qui a son tour fut le pere de");
        check_contains("FR 2-hop conclusion", out, "Par consequent, Boaz est le grand-pere de Jesse");
    }

    /* 7. Idempotence verification (pass 1 == pass 2) */
    {
        LangSet(LANG_ES);
        char pass1[DEEP_NLG_BUF_MAX], pass2[DEEP_NLG_BUF_MAX];
        const char hops[2][DEEP_NLG_TOKEN_MAX] = { "obed", "jesse" };
        DeepNLG_GenerateChainText("boaz", "engendro a", hops, 2, "abuelo", "jesse", pass1, sizeof(pass1));
        DeepNLG_GenerateChainText("boaz", "engendro a", hops, 2, "abuelo", "jesse", pass2, sizeof(pass2));
        if (strcmp(pass1, pass2) == 0)
        {
            printf("  [PASS] Idempotence: pass 1 and pass 2 identical\n");
            g_pass++;
        }
        else
        {
            printf("  [FAIL] Idempotence failed\n");
            g_fail++;
        }
    }

    printf("\n=== RESULTS ===\n");
    printf("Passed: %d\nFailed: %d\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
