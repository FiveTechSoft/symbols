/* test_queryplan: Fase A Paso 1 (representation only). ChatBuildPlan
   must segment the 14 coordination-battery inputs into isolated
   QueryGoals with the deduced relation bound per goal (elliptical
   spans inherit goal 1). Asserts goal counts + relation families +
   inherit flags: representation META_RECALL 100%. No evaluator/NLG
   involved; ChatHandle behavior is untouched by the addition. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "schema.h"
#include "metaschema.h"
#include "learn.h"
#include "chat.h"

static int g_pass = 0, g_fail = 0;

static const char *g_scratch = "test_queryplan_scratch.tsv";

static void WriteScratch(void)
{
    FILE *f = fopen(g_scratch, "w");
    if (f == NULL)
    {
        printf("FAIL cannot write scratch corpus\n");
        exit(1);
    }
    fputs("david\tHIJO_DE\tjesse\n", f);
    fputs("jesse\tPADRE_DE\tdavid\n", f);
    fputs("david\tREY_DE\tisrael\n", f);
    fputs("abigail\tESPOSA_DE\tnabal\n", f);
    fclose(f);
}

static void check_plan(CHAT *ch, const char *line, uint32_t want_n,
                       const char *want_fam[], const int want_inherit[])
{
    QueryPlan plan;
    char toks[16][CHAT_TOKEN_MAX];
    uint32_t ntok = 0;
    uint32_t n = ChatBuildPlan(ch, line, &plan, toks, &ntok, NULL);
    if (n != want_n || plan.count != want_n)
    {
        printf("  FAIL count %.50s (got %u want %u)\n", line, n, want_n);
        g_fail++;
        return;
    }
    for (uint32_t g = 0; g < want_n; g++)
    {
        const QueryGoal *goal = &plan.goals[g];
        const char *fam =
            goal->kwx >= 0 ? ch->kws[goal->kwx].family : "(none)";
        if (strcmp(fam, want_fam[g]) != 0 ||
            goal->inherit != want_inherit[g] ||
            goal->connector != (g == 0 ? 0 : 1) ||
            goal->end <= goal->start)
        {
            printf("  FAIL goal%u %.40s (fam %s want %s, inh %d want %d)\n",
                   g, line, fam, want_fam[g], goal->inherit,
                   want_inherit[g]);
            g_fail++;
            return;
        }
    }
    printf("  PASS %.60s\n", line);
    g_pass++;
}

int main(void)
{
    WriteScratch();
    CHAT ch;
    ChatInit(&ch, g_scratch);
    remove(g_scratch);

    /* KwdRecord generates one keyword for each unique relation in
       the isolated scratch corpus (4 relations: HIJO_DE, PADRE_DE,
       REY_DE, ESPOSA_DE). Persistent episodic memory may contribute
       extra stems locally, so require at least the 4 scratch stems
       instead of an exact count. */
    if (ch.num_kws < 4)
    {
        printf("FAIL deduced kws (got %u want >= 4)\n", ch.num_kws);
        ChatDestroy(&ch);
        return 1;
    }
    printf("  PASS deduced %u relation stems (>= 4 scratch)\n", ch.num_kws);
    g_pass++;

    /* arg coordination */
    {
        const char *f[] = {"father", "father"};
        const int inh[] = {0, 1};
        check_plan(&ch, "quien es el padre de david y de salomon?",
                   2, f, inh);
    }
    /* goal coordination (own kw in span 2) */
    {
        const char *f[] = {"father", "reigns"};
        const int inh[] = {0, 0};
        check_plan(&ch, "quien fue el padre de david y de quien fue rey?",
                   2, f, inh);
    }
    /* EN arg coordination */
    {
        const char *f[] = {"father", "father"};
        const int inh[] = {0, 1};
        check_plan(&ch, "Who was the father of David and Solomon?", 2, f,
                   inh);
    }
    /* EN genitive + own-kw span 2 with anaphora gap */
    {
        const char *f[] = {"father", "reigns"};
        const int inh[] = {0, 0};
        check_plan(&ch,
                   "Who was David's father and who was he king of?", 2, f,
                   inh);
    }
    /* bare fragments: no interrogative force anywhere, so the
       left span cannot trial-parse: single goal, legacy ABSTAIN */
    {
        const char *f[] = {"father"};
        const int inh[] = {0};
        check_plan(&ch, "el padre de david y de salomon", 1, f, inh);
    }
    {
        const char *f[] = {"taxonomy"};
        const int inh[] = {0};
        check_plan(&ch, "el hijo de david y de betsy", 1, f, inh);
    }
    /* wife arg coordination (generic frame relation) */
    {
        const char *f[] = {"wife", "wife"};
        const int inh[] = {0, 1};
        check_plan(&ch, "la esposa de nabal y de esau", 2, f, inh);
    }
    /* two full goals */
    {
        const char *f[] = {"father", "father"};
        const int inh[] = {0, 0};
        check_plan(&ch,
                   "quien es el padre de david y quien es el padre de "
                   "salomon?", 2, f, inh);
    }
    /* comma + coordinator: separator, not G5 apposition */
    {
        const char *f[] = {"father", "reigns"};
        const int inh[] = {0, 0};
        check_plan(&ch,
                   "quien fue el padre de david, y de quien fue rey?", 2,
                   f, inh);
    }
    /* present-tense EN arg coordination */
    {
        const char *f[] = {"father", "father"};
        const int inh[] = {0, 1};
        check_plan(&ch, "Who is the father of David and Solomon?", 2, f,
                   inh);
    }
    /* mixed goals: father + child-frame */
    {
        const char *f[] = {"father", "taxonomy"};
        const int inh[] = {0, 0};
        check_plan(&ch,
                   "quien es el padre de david y de quien es hijo "
                   "salomon?", 2, f, inh);
    }
    /* known + unknown-vocab arg */
    {
        const char *f[] = {"father", "father"};
        const int inh[] = {0, 1};
        check_plan(&ch, "quien es el padre de david y de babylonia?", 2,
                   f, inh);
    }
    /* known + ambiguous arg */
    {
        const char *f[] = {"father", "father"};
        const int inh[] = {0, 1};
        check_plan(&ch, "quien es el padre de david y de james?", 2, f,
                   inh);
    }
    /* mixed args + goals: 3 goals */
    {
        const char *f[] = {"father", "father", "reigns"};
        const int inh[] = {0, 1, 0};
        check_plan(&ch,
                   "quien es el padre de david y de salomon y de quien "
                   "fue rey?", 3, f, inh);
    }

    printf("test_queryplan: %d passed, %d failed\n", g_pass, g_fail);
    ChatDestroy(&ch);
    return g_fail == 0 ? 0 : 1;
}
