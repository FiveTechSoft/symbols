/* test_action: agent text protocol (pure, deterministic).
   Byte-exact ACTION/FINAL/ABSTAIN/OBSERVATION lines from the frozen
   spec, round-trips, and malformed-input rejection. No OS. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "agent_action.h"

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

static void check_str(const char *name, const char *got,
                      const char *want)
{
    if (got != NULL && strcmp(got, want) == 0)
    {
        printf("  PASS %s\n", name);
        g_pass++;
    }
    else
    {
        printf("  FAIL %s\n    got  %.160s\n    want %.160s\n", name,
               got ? got : "(null)", want);
        g_fail++;
    }
}

int main(void)
{
    char buf[2048];
    AgentAction act;
    AgentObservation obs;

    /* ---- format pins (spec examples) ---- */
    memset(&act, 0, sizeof(act));
    act.type = ACTION_TOOL_CALL;
    strncpy(act.tool, "gcc", sizeof(act.tool) - 1);
    strncpy(act.args, "main.c -Wall -o main.exe", sizeof(act.args) - 1);
    strncpy(act.prompt, "Compilando archivo principal",
            sizeof(act.prompt) - 1);
    check("format gcc action",
          AgentActionFormat(&act, buf, sizeof(buf)) == 1);
    check_str("gcc action bytes", buf,
              "ACTION: TOOL=gcc ARGS=\"main.c -Wall -o main.exe\" "
              "PROMPT=\"Compilando archivo principal\"");

    memset(&act, 0, sizeof(act));
    act.type = ACTION_TOOL_CALL;
    strncpy(act.tool, "cmd", sizeof(act.tool) - 1);
    strncpy(act.args, "/c dir /b *.c", sizeof(act.args) - 1);
    strncpy(act.prompt, "Listando ficheros de codigo",
            sizeof(act.prompt) - 1);
    check("format cmd action",
          AgentActionFormat(&act, buf, sizeof(buf)) == 1);
    check_str("cmd action bytes", buf,
              "ACTION: TOOL=cmd ARGS=\"/c dir /b *.c\" "
              "PROMPT=\"Listando ficheros de codigo\"");

    memset(&act, 0, sizeof(act));
    act.type = ACTION_FINAL;
    strncpy(act.prompt, "El archivo main.c compilo sin advertencias.",
            sizeof(act.prompt) - 1);
    check("format final",
          AgentActionFormat(&act, buf, sizeof(buf)) == 1);
    check_str("final bytes", buf,
              "FINAL: El archivo main.c compilo sin advertencias.");

    memset(&act, 0, sizeof(act));
    act.type = ACTION_ABSTAIN;
    strncpy(act.prompt,
            "No tengo una herramienta o regla aplicable a la solicitud.",
            sizeof(act.prompt) - 1);
    check("format abstain",
          AgentActionFormat(&act, buf, sizeof(buf)) == 1);
    check_str("abstain bytes", buf,
              "ABSTAIN: No tengo una herramienta o regla aplicable a "
              "la solicitud.");

    memset(&obs, 0, sizeof(obs));
    strncpy(obs.tool, "gcc", sizeof(obs.tool) - 1);
    obs.exit_code = 1;
    strncpy(obs.output,
            "main.c:12:5: error: expected ';' before 'return'",
            sizeof(obs.output) - 1);
    check("format observation",
          AgentObservationFormat(&obs, buf, sizeof(buf)) == 1);
    check_str("observation bytes", buf,
              "OBSERVATION: TOOL=gcc EXIT=1 OUT=\"main.c:12:5: error: "
              "expected ';' before 'return'\"");

    /* ---- round-trips ---- */
    memset(&act, 0, sizeof(act));
    check("parse gcc action",
          AgentActionParse("ACTION: TOOL=powershell ARGS=\"Get-Process "
                           "-Name svchost | Select-Object -First 3\" "
                           "PROMPT=\"Inspeccionando procesos\"",
                           &act) == 1);
    check("parsed type", act.type == ACTION_TOOL_CALL);
    check_str("parsed tool", act.tool, "powershell");
    check_str("parsed args", act.args,
              "Get-Process -Name svchost | Select-Object -First 3");
    check_str("parsed prompt", act.prompt, "Inspeccionando procesos");

    memset(&obs, 0, sizeof(obs));
    check("parse multiline observation",
          AgentObservationParse("OBSERVATION: TOOL=cmd EXIT=0 "
                                "OUT=\"main.c\\ntest.c\\nlearn.c\"",
                                &obs) == 1);
    check_str("obs tool", obs.tool, "cmd");
    check("obs exit", obs.exit_code == 0);
    check_str("obs output", obs.output, "main.c\ntest.c\nlearn.c");

    memset(&obs, 0, sizeof(obs));
    check("parse failing observation",
          AgentObservationParse("OBSERVATION: TOOL=gcc EXIT=1 "
                                "OUT=\"main.c:4:1: error: unknown type "
                                "name 'foo'\"",
                                &obs) == 1);
    check("obs exit 1", obs.exit_code == 1);

    /* ---- rejection ---- */
    memset(&act, 0, sizeof(act));
    check("reject garbage", AgentActionParse("hello world", &act) == 0);
    check("reject truncated action",
          AgentActionParse("ACTION: TOOL=gcc", &act) == 0);
    check("reject bad type",
          AgentActionParse("ACTIONX: TOOL=gcc ARGS=\"x\" PROMPT=\"y\"",
                           &act) == 0);
    memset(&obs, 0, sizeof(obs));
    check("reject bad exit",
          AgentObservationParse("OBSERVATION: TOOL=gcc EXIT=x OUT=\"y\"",
                                &obs) == 0);
    check("reject unterminated",
          AgentObservationParse("OBSERVATION: TOOL=gcc EXIT=0 OUT=\"y",
                                &obs) == 0);
    memset(&act, 0, sizeof(act));
    act.type = ACTION_TOOL_CALL;
    strncpy(act.tool, "gcc", sizeof(act.tool) - 1);
    strncpy(act.args, "a\"b", sizeof(act.args) - 1);
    check("refuse raw quote in args",
          AgentActionFormat(&act, buf, sizeof(buf)) == 0);

    printf("test_action: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
