/* test_toolplan: Agent Core ToolContract (diagnostic, no execution).
   The planner never resolves: it routes on failure cause + tool
   contract, never on UNKNOWN text. Pins: 5 NEEDS_TOOL (with tool +
   subject), 4 GENUINE UNKNOWN, 1 AMBIGUOUS passthrough, 1 ANSWER.
   Real corpus (needs its relations and vocabulary). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "schema.h"
#include "metaschema.h"
#include "learn.h"
#include "bible_chat.h"
#include "tool_contract.h"

static int g_pass = 0, g_fail = 0;

static const char *DecName(ToolDecision d)
{
    switch (d)
    {
    case DEC_IS_ANSWER:
        return "ANSWER";
    case DEC_AMBIGUOUS:
        return "AMBIGUOUS";
    case DEC_NEEDS_TOOL:
        return "NEEDS_TOOL";
    default:
        return "UNKNOWN";
    }
}

static const char *ToolName(ToolId t)
{
    switch (t)
    {
    case TOOL_LOOKUP_PERSON:
        return "lookup_person";
    case TOOL_LOOKUP_RELATION:
        return "lookup_relation";
    case TOOL_CALCULATOR:
        return "calculator";
    default:
        return "none";
    }
}

static void check_row(CHAT *ch, const char *line, ToolDecision want_dec,
                      const char *want_tool, const char *want_subj)
{
    char out[4096], slot[CHAT_TOKEN_MAX], family[CHAT_TOKEN_MAX];
    GoalCause cause = CAUSE_NONE;
    ToolRequest req;
    int st = ChatResolveLine(ch, line, out, sizeof(out), slot,
                             sizeof(slot), family, sizeof(family),
                             &cause);
    memset(&req, 0, sizeof(req));
    ToolDecision got = ToolClassify(ch, line, (GOAL_STATUS)st, cause,
                                    slot, family, &req);
    if (got == want_dec && strcmp(ToolName(req.tool), want_tool) == 0 &&
        strcmp(req.subject, want_subj) == 0)
    {
        printf("  PASS %.60s -> %s/%s/%s\n", line, DecName(got),
               ToolName(req.tool), req.subject);
        g_pass++;
    }
    else
    {
        printf("  FAIL %.60s\n    got  %s/%s/%s status=%d cause=%d slot=%.20s fam=%.20s\n    want %s/%s/%s\n",
               line, DecName(got), ToolName(req.tool), req.subject, st,
               (int)cause, slot, family, DecName(want_dec), want_tool,
               want_subj);
        g_fail++;
    }
}

int main(void)
{
    CHAT ch;
    ChatInit(&ch, "data/bible/bible_relations.tsv");

    check_row(&ch, "Who was king of Babylonia?", DEC_NEEDS_TOOL,
              "lookup_relation", "babylonia");
    check_row(&ch, "Who is the mother of David?", DEC_NEEDS_TOOL,
              "lookup_relation", "david");
    check_row(&ch, "Donde nacio Jonas?", DEC_NEEDS_TOOL,
              "lookup_person", "jonas");
    check_row(&ch, "Cuanto es 23 por 17?", DEC_NEEDS_TOOL, "calculator",
              "23 por 17");
    check_row(&ch, "Who is the father of Babylonia?", DEC_UNKNOWN,
              "none", "");
    check_row(&ch, "Who is the father of Zorblax?", DEC_UNKNOWN, "none",
              "");
    check_row(&ch, "Who is the grandfather of Babylonia?", DEC_UNKNOWN,
              "none", "");
    check_row(&ch, "Who is the father of James?", DEC_AMBIGUOUS, "none",
              "");
    check_row(&ch, "De quien fue rey Saul?", DEC_NEEDS_TOOL,
              "lookup_relation", "saul");
    check_row(&ch, "Quien es el hermano de Zorblax?", DEC_UNKNOWN,
              "none", "");
    check_row(&ch, "Who is the father of David?", DEC_IS_ANSWER, "none",
              "");

    printf("test_toolplan: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
