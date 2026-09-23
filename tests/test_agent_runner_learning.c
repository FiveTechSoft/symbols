/* ============================================================
   test_agent_runner_learning.c: Repeat-task learning probe.

   Runs the same resolvable task twice (byte-identical setup).
   Gate is fail-closed and one-sided:
     - second run must stay solved if first was solved;
     - second run must not use more attempts than the first;
     - first-run time is recorded; second-run delta is reported
       (speedup is informational — no wall-clock flake gate).

   Emits one LEARN line for scripts/bank_report.py.
   ============================================================ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#define getcwd _getcwd
#else
#include <unistd.h>
#endif
#include "agent_runner.h"

#define TARGET "test_learn_dym.c"
/* Applied patch still has the typo so the repair loop must fire. */
#define INITIAL \
    "int Value(int buffer_size) { return 0; }\n"
#define REPLACEMENT \
    "int Value(int buffer_size) { return buff_size; }\n"
#define BUGGY_BODY \
    "int Value(int buffer_size) { return 0; }\n"

static int write_target(void)
{
    FILE *f = fopen(TARGET, "wb");
    if (!f) return 0;
    if (fputs(INITIAL, f) == EOF) { fclose(f); return 0; }
    return fclose(f) == 0;
}

static int run_once(unsigned attempt, SWE_BENCH_RESULT *out, double *ms, int *rc)
{
    AGENT_RUNNER *r;
    SWE_BENCH_TASK t;
    clock_t start;

    if (!write_target()) return 0;
    r = AgentRunnerCreate(".", 3);
    if (!r) return 0;
    memset(&t, 0, sizeof(t));
    snprintf(t.task_id, sizeof(t.task_id), "LEARN-DYM-%u", attempt);
    strncpy(t.issue_description, "repeat-task learning probe",
            sizeof(t.issue_description) - 1);
    strncpy(t.target_file, TARGET, sizeof(t.target_file) - 1);
    t.target_line = 1;
    strncpy(t.buggy_snippet, BUGGY_BODY, sizeof(t.buggy_snippet) - 1);
    strncpy(t.fixed_snippet, REPLACEMENT, sizeof(t.fixed_snippet) - 1);
    snprintf(t.build_command, sizeof(t.build_command),
             "gcc -fsyntax-only %s", TARGET);

    start = clock();
    *rc = AgentRunnerSolveTask(r, &t, out);
    *ms = 1000.0 * (double)(clock() - start) / CLOCKS_PER_SEC;
    AgentRunnerDestroy(r);
    return 1;
}

int main(void)
{
    SWE_BENCH_RESULT r1, r2;
    double ms1 = 0.0, ms2 = 0.0;
    int rc1 = 0, rc2 = 0;
    int ok = 1;
    unsigned a1, a2;
    int improved;

    if (!run_once(1, &r1, &ms1, &rc1) || !run_once(2, &r2, &ms2, &rc2))
    {
        printf("LEARN error=setup pass1_ms=0 pass2_ms=0 attempts1=0 attempts2=0 "
               "improved=0 FAIL\n");
        remove(TARGET);
        return 1;
    }

    a1 = r1.attempts_executed;
    a2 = r2.attempts_executed;

    /* One-sided: second must not regress correctness or attempt count. */
    if (rc1 != 1 || r1.repairs_applied != 1 ||
        strcmp(r1.last_repair_operator, "compiler-did-you-mean") != 0)
        ok = 0;
    if (rc2 != 1 || r2.repairs_applied != 1 ||
        strcmp(r2.last_repair_operator, "compiler-did-you-mean") != 0)
        ok = 0;
    if (a2 > a1)
        ok = 0;
    if (a1 < 2)
        ok = 0;

    improved = (ms2 > 0.0 && ms2 < ms1) ? 1 : 0;

    printf("LEARN pass1_ms=%.3f pass2_ms=%.3f attempts1=%u attempts2=%u "
           "replans1=%u replans2=%u op1=%s op2=%s solved1=%d solved2=%d "
           "improved=%d %s\n",
           ms1, ms2, a1, a2,
           r1.replans_triggered, r2.replans_triggered,
           r1.last_repair_operator, r2.last_repair_operator,
           rc1 == 1, rc2 == 1, improved, ok ? "OK" : "FAIL");

    remove(TARGET);
    return ok ? 0 : 1;
}
