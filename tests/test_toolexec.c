/* test_toolexec: minimal tool executor (static tables only).
   Full cycle per NEEDS_TOOL row: ToolRequest -> ToolExecute ->
   ToolResult -> re-resolve same goal -> ANSWER with source marking.
   Tools never answer directly; misses and negatives keep the
   original UNKNOWN/AMBIGUOUS. Real corpus. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "schema.h"
#include "metaschema.h"
#include "learn.h"
#include "chat.h"
#include "tool_contract.h"

static int g_pass = 0, g_fail = 0;

static void check_cycle(CHAT *ch, const char *line, const char *want)
{
    char out[4096], slot[CHAT_TOKEN_MAX], family[CHAT_TOKEN_MAX];
    GoalCause cause = CAUSE_NONE;
    ToolRequest req;
    ToolResult res;
    char final[4096];
    int st = ChatResolveLine(ch, line, out, sizeof(out), slot,
                             sizeof(slot), family, sizeof(family),
                             &cause);
    memset(&req, 0, sizeof(req));
    memset(&res, 0, sizeof(res));
    ToolDecision d =
        ToolClassify(ch, line, (GOAL_STATUS)st, cause, slot, family,
                     &req);
    if (d == DEC_NEEDS_TOOL)
    {
        ToolExecute(&req, &res);
        if (!ToolAnswerGoal(&req, &res, final, sizeof(final)))
            snprintf(final, sizeof(final), "%s", out);
    }
    else
        snprintf(final, sizeof(final), "%s", out);
    /* strip trailing newline for comparison */
    for (size_t i = 0; final[i] != '\0'; i++)
        if (final[i] == '\n' || final[i] == '\r')
            final[i] = '\0';
    if (strcmp(final, want) == 0)
    {
        printf("  PASS %.60s\n", line);
        g_pass++;
    }
    else
    {
        printf("  FAIL %.60s\n    got  %.150s\n    want %.150s\n", line,
               final, want);
        g_fail++;
    }
}

int main(void)
{
    CHAT ch;
    ChatInit(&ch, "data/bible/bible_relations.tsv");

    check_cycle(&ch, "Who was king of Babylonia?",
                "Segun fuente externa, Babylonia es rey de "
                "Nebuchadnezzar.");
    check_cycle(&ch, "Who is the mother of David?",
                "Segun fuente externa, David es mother de Nitzevet.");
    check_cycle(&ch, "Donde nacio Jonas?",
                "Segun fuente externa, Jonas: nacio en Gathepher.");
    check_cycle(&ch, "Cuanto es 23 * 17?",
                "Segun calculo, 23 * 17 = 391.");
    check_cycle(&ch, "De quien fue rey Saul?",
                "Segun fuente externa, Saul es rey de Israel.");
    check_cycle(&ch, "Who is the father of Babylonia?",
                "No tengo constancia del padre de Babylonia en los "
                "textos cargados.");
    check_cycle(&ch, "Who is the father of Zorblax?",
                "No tengo constancia del padre de Zorblax en los "
                "textos cargados.");
    check_cycle(&ch, "Who is the father of James?",
                "Hay 2 constancias del padre de James: ambiguo, "
                "necesito desambiguar.");

    printf("test_toolexec: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
