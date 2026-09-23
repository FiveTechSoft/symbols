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

#define ROOT "tests/fixtures/agent_runner/evaluation"

typedef struct { unsigned resolved, false_positive, attempts, replans; double ms; } METRICS;

static int copy_file(const char *from, const char *to)
{
    FILE *in = fopen(from, "rb"), *out;
    char buf[4096]; size_t n;
    if (!in) return 0;
    out = fopen(to, "wb");
    if (!out) { fclose(in); return 0; }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
        if (fwrite(buf, 1, n, out) != n) { fclose(in); fclose(out); return 0; }
    fclose(in); return fclose(out) == 0;
}

static int same_file(const char *a, const char *b)
{
    FILE *fa = fopen(a, "rb"), *fb = fopen(b, "rb"); int ca, cb;
    if (!fa || !fb) { if (fa) fclose(fa); if (fb) fclose(fb); return 0; }
    do { ca = fgetc(fa); cb = fgetc(fb); } while (ca == cb && ca != EOF);
    fclose(fa); fclose(fb); return ca == cb;
}

static int run_case(const char *dir, const char *replacement, int expected,
                    const char *expected_header, METRICS *m)
{
    char initial[1024], target[1024], cmd[1280], cwd[512];
    if (!getcwd(cwd, sizeof(cwd))) return 0;
    snprintf(initial, sizeof(initial), "%s/%s/%s/main.initial.c", cwd, ROOT, dir);
    snprintf(target, sizeof(target), "%s/%s/%s/main.c", cwd, ROOT, dir);
    if (!copy_file(initial, target)) return 0;
    char workspace[1024]; snprintf(workspace, sizeof(workspace), "%s/%s/%s", cwd, ROOT, dir);
    AGENT_RUNNER *r = AgentRunnerCreate(workspace, 1);
    SWE_BENCH_TASK t; SWE_BENCH_RESULT result;
    memset(&t, 0, sizeof(t));
    snprintf(t.task_id, sizeof(t.task_id), "HELDOUT-%s", dir);
    strncpy(t.issue_description, "held-out missing-header evaluation", sizeof(t.issue_description)-1);
    strncpy(t.target_file, target, sizeof(t.target_file)-1);
    t.target_line = 1;
    FILE *f = fopen(initial, "rb");
    if (!f || !fgets(t.buggy_snippet, sizeof(t.buggy_snippet), f)) { if (f) fclose(f); AgentRunnerDestroy(r); remove(target); return 0; }
    fclose(f);
    strncpy(t.fixed_snippet, replacement, sizeof(t.fixed_snippet)-1);
    snprintf(cmd, sizeof(cmd), "gcc -std=c11 -Werror=implicit-function-declaration -fsyntax-only %s", target);
    strncpy(t.build_command, cmd, sizeof(t.build_command)-1);
    clock_t start = clock();
    int rc = AgentRunnerSolveTask(r, &t, &result);
    m->ms += 1000.0 * (double)(clock() - start) / CLOCKS_PER_SEC;
    m->resolved += (rc == 1); m->attempts += result.attempts_executed; m->replans += result.replans_triggered;
    if (!expected && (rc || result.repairs_applied)) m->false_positive++;
    int ok = ((rc == 1) == expected) && result.attempts_executed == 2;
    if (expected) ok = ok && result.repairs_applied == 1 &&
        strcmp(result.last_repair_operator, "missing-header") == 0 &&
        strstr(result.unified_diff, expected_header) != NULL;
    else ok = ok && result.repairs_applied == 0 && same_file(initial, target);
    printf("%-18s solved=%d expected=%d attempts=%u replans=%u repair=%s %s\n",
           dir, rc, expected, result.attempts_executed, result.replans_triggered,
           result.last_repair_operator[0] ? result.last_repair_operator : "none",
           ok ? "OK" : "FAIL");
    AgentRunnerDestroy(r); remove(target); return ok;
}

int main(void)
{
    METRICS m = {0}; int ok = 1;
    ok &= run_case("case_function", "int heldout_function(void) { return EvalTransform(3); }\n", 1, "api.h", &m);
    ok &= run_case("case_type", "int heldout_type(void) { EvalRecord x = {4}; return x.value; }\n", 1, "model.h", &m);
    ok &= run_case("case_none", "int heldout_none(void) { return EvalAbsent(3); }\n", 0, "", &m);
    ok &= run_case("case_ambiguous", "int heldout_ambiguous(void) { return EvalCollision(3); }\n", 0, "", &m);
    printf("METRICS cases=4 resolved=%u resolution_rate=%.2f false_positives=%u attempts=%u replans=%u cpu_ms=%.3f\n",
           m.resolved, m.resolved/4.0, m.false_positive, m.attempts, m.replans, m.ms);
    return ok ? 0 : 1;
}
