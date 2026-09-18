/* test_self: scope data (data/agentic/self.tsv).
   Loader fail-closed, trigger matching case/accent-insensitive via
   canonical tokens, scope reply through ChatResolveLine, cold
   non-triggers still abstain, normal queries intact. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "schema.h"
#include "metaschema.h"
#include "learn.h"
#include "bible_chat.h"
#include "tool_contract.h"

static int g_pass = 0, g_fail = 0;

static void check(const char *name, int cond)
{
    if (cond)
    {
        printf("  PASS %s\n", name);
        g_pass++;
    }
    else
    {
        printf("  FAIL %s\n", name);
        g_fail++;
    }
}

static const char *g_tmp = "test_self_tmp.tsv";

static const char *SCOPE =
    "Soy Symbols, un motor de consultas sobre genealogía bíblica: "
    "respondo sobre padre, hijo, reyes, esposa y hermano según los "
    "registros cargados. De lo demás no tengo constancia.\n";

static void WriteTmp(void)
{
    FILE *f = fopen(g_tmp, "w");
    if (f == NULL)
    {
        printf("FAIL cannot write temp TSV\n");
        exit(1);
    }
    fputs("# temp self (exercises every parser branch)\n", f);
    fputs("scope\tes\tSU AMBITO\n", f);
    fputs("trigger\thola friend\tscope\n", f);
    fputs("trigger\tbad\n", f);
    fputs("bogus\ta\tb\n", f);
    fputs("trigger\t\t scope\n", f);
    fclose(f);
}

static int Resolve(CHAT *ch, const char *line, char *out, size_t size)
{
    char slot[CHAT_TOKEN_MAX], fam[64];
    GoalCause cause = CAUSE_NONE;
    memset(out, 0, size);
    return ChatResolveLine(ch, line, out, size, slot, sizeof(slot),
                           fam, sizeof(fam), &cause);
}

int main(void)
{
    CHAT ch;
    char out[2048];

    /* ---- 1. missing file -> no scope, legacy abstain ---- */
    SelfInitFrom("test_self_no_such_file.tsv");
    check("missing scope empty", SelfScopeText()[0] == '\0');
    check("missing triggers zero", SelfTriggerCount() == 0);

    /* ---- 2. temp file: scope + 1 trigger, bad rows die ---- */
    WriteTmp();
    SelfInitFrom(g_tmp);
    check("file scope text", strcmp(SelfScopeText(), "SU AMBITO") == 0);
    check("file trigger count", SelfTriggerCount() == 1);
    check("file trigger content",
          SelfTriggerAt(0) != NULL &&
              strcmp(SelfTriggerAt(0), "hola friend") == 0);
    check("trigger oob guard", SelfTriggerAt(99) == NULL);
    remove(g_tmp);

    /* ---- 3. reply pins through the real engine ---- */
    ChatInit(&ch, "data/bible/bible_relations.tsv");
    SelfInitFrom("data/agentic/self.tsv");
    check("quien eres", Resolve(&ch, "quien eres?", out,
                                sizeof(out)) == GOAL_ANSWER &&
                           strcmp(out, SCOPE) == 0);
    check("hola", Resolve(&ch, "hola", out, sizeof(out)) == GOAL_ANSWER &&
                      strcmp(out, SCOPE) == 0);
    check("caps+accent fold",
          Resolve(&ch, "QUIÉN ERES?", out, sizeof(out)) ==
                  GOAL_ANSWER &&
              strcmp(out, SCOPE) == 0);
    check("cold non-trigger abstains",
          Resolve(&ch, "el primero", out, sizeof(out)) == -1);
    check("nonsense abstains",
          Resolve(&ch, "xyzzy nonsense", out, sizeof(out)) == -1);
    check("normal query intact",
          Resolve(&ch, "Who is the father of David?", out,
                  sizeof(out)) == GOAL_ANSWER &&
              strstr(out, "Jesse") != NULL);

    /* ---- 4. shipped file + idempotence ---- */
    SelfInitFrom("data/agentic/self.tsv");
    SelfInitFrom("data/agentic/self.tsv");
    check("shipped triggers", SelfTriggerCount() == 6);
    check("shipped scope live", SelfScopeText()[0] != '\0');
    check("shipped reply",
          Resolve(&ch, "hello", out, sizeof(out)) == GOAL_ANSWER);

    printf("test_self: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
