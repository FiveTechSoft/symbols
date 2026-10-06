/* ============================================================
   agent_cli_main.c: symbols-agent CLI, a deterministic code verification
   and repair agent. It is not a general autonomous programmer.
   Pure ISO C11, zero tensors, zero backprop, fail-closed verification.

   Usage:
     symbols-agent [options] [task_description]

   Options:
     -w, --workspace <dir>    Set target repository workspace (default: .)
     -b, --blast-radius <sym> Compute and display blast radius for symbol
     -d, --diagnose <file>    Diagnose compiler/linter error output from file
     -i, --index              Index workspace and display structural stats
     -r, --replans <n>        Max replan attempts on failure (default: 3)
     --ask-missing-goal     Ask only for a typed stdout-goal-missing task
     --continue-stdout-goal --workspace-key HEX --stdout-answer TEXT
                            Continue a bound typed request noninteractively
     --select-report <build> --changed <file>...
                            Print which tests the build graph selects for the
                            changed files (read-only report, runs no test)
     -h, --help               Show this help message
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "compat.h"
#include "agent_runner.h"
#include "code_graph.h"
#include "agent_diagnose.h"
#include "task_ops.h"

static void PrintHelp(const char *prog)
{
    printf("=========================================================\n");
    printf("  SYMBOLS-AGENT: deterministic code verification and\n");
    printf("  repair agent. Pure ISO C11, fail-closed verification.\n");
    printf("  Not a general autonomous programmer: work outside its\n");
    printf("  verified operators abstains. See README.md, section 7.\n");
    printf("=========================================================\n\n");
    printf("Usage:\n");
    printf("  %s [options] [task_description]\n\n", prog);
    printf("Commands & Modes:\n");
    printf("  -w, --workspace <dir>    Target codebase directory (default: .)\n");
    printf("  -b, --blast-radius <sym> Compute impact and callers for a symbol\n");
    printf("  -d, --diagnose <file>    Abductive diagnosis of compiler/linter errors\n");
    printf("  -i, --index              Scan and index workspace, display stats\n");
    printf("  -r, --replans <num>      Max healing replans on failure (default: 3)\n");
    printf("  --ask-missing-goal      Ask only for a typed stdout-goal-missing task\n");
    printf("  --continue-stdout-goal --workspace-key HEX --stdout-answer TEXT\n");
    printf("  --select-report <build> --changed <file>...\n");
    printf("                           Report the build-graph test selection (read-only;\n");
    printf("                           nothing is skipped, the full gate stays the default)\n");
    printf("  -h, --help               Display this help guide\n\n");
    printf("Examples:\n");
    printf("  %s -b AgentProcessObservation\n", prog);
    printf("  %s -d build_errors.log\n", prog);
    printf("  %s -w /path/to/project -i\n", prog);
    printf("  %s \"Fix memory leak in code_graph.c\"\n\n", prog);
}

static char *ReadFile(const char *path, size_t *out_size)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 10 * 1024 * 1024) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    buf[rd] = '\0';
    fclose(f);
    if (out_size) *out_size = rd;
    return buf;
}

/* --select-report: a read-only report of the build-graph test selection
   (tools/build_graph.py). It runs nothing but that script, skips no test and
   changes no state. Every way it can fail to produce a selection is reported
   as "mode: full" with the reason: the full gate is the answer whenever the
   selection is not known. Arguments reach a shell, so only a conservative
   path alphabet is accepted. */
static int PathArgSafe(const char *a)
{
    if (!a[0] || a[0] == '-') return 0;
    for (const char *c = a; *c; c++)
    {
        if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') ||
              *c == '_' || *c == '.' || *c == '/' || *c == '+' || *c == '-'))
            return 0;
    }
    return 1;
}

static int SelectReportFull(const char *reason)
{
    printf("[symbols-agent] select-report (read-only: nothing is skipped, the full gate stays the default)\n");
    printf("mode: full\nreason: %s\n", reason);
    return 0;
}

static int SelectReport(const char *workspace, const char *build, char **changed, int nchanged)
{
    char script[MAX_PATCH_PATH + 32];
    char cmd[8192];
    FILE *f;
    SHELL_EXEC_RESULT *res;
    size_t n;
    const char *p;

    if (nchanged <= 0) return SelectReportFull("no changed files were given");
    if (!PathArgSafe(build)) return SelectReportFull("the build directory path has a character outside the supported set");
    snprintf(script, sizeof(script), "%s/tools/build_graph.py", workspace);
    f = fopen(script, "rb");
    if (!f) return SelectReportFull("tools/build_graph.py was not found in the workspace");
    fclose(f);
    n = (size_t)snprintf(cmd, sizeof(cmd), "python3 tools/build_graph.py select %s --changed", build);
    for (int i = 0; i < nchanged; i++)
    {
        if (!PathArgSafe(changed[i])) return SelectReportFull("a changed file path has a character outside the supported set");
        if (n + strlen(changed[i]) + 2 >= sizeof(cmd)) return SelectReportFull("the changed file list is too long");
        n += (size_t)snprintf(cmd + n, sizeof(cmd) - n, " %s", changed[i]);
    }
    res = (SHELL_EXEC_RESULT *)malloc(sizeof(*res));
    if (!res) return SelectReportFull("out of memory");
    AgentShellResultInit(res);
    AgentShellExecGuarded(cmd, workspace, 60000, res);
    if (res->execution_failed || res->exit_code != 0)
    {
        free(res);
        return SelectReportFull("python3 tools/build_graph.py did not run to a successful exit");
    }
    p = strstr(res->stdout_buf, "\"mode\": \"");
    if (!p || res->stdout_buf[0] != '{' || res->stdout_truncated)
    {
        free(res);
        return SelectReportFull("the selection output was not recognised");
    }
    printf("[symbols-agent] select-report (read-only: nothing is skipped, the full gate stays the default)\n");
    printf("%s", res->stdout_buf);
    if (res->stdout_len && res->stdout_buf[res->stdout_len - 1] != '\n') printf("\n");
    free(res);
    return 0;
}

int main(int argc, char **argv)
{
    char workspace[MAX_PATCH_PATH] = ".";
    char blast_symbol[MAX_CODE_NAME] = {0};
    char diagnose_file[MAX_PATCH_PATH] = {0};
    char task_desc[4096] = {0}; /* full task text; the runner copy is bounded */
    bool do_index_only = false;
    bool ask_missing_goal = false;
    bool continue_stdout_goal = false;
    char workspace_key[32] = {0};
    char stdout_answer[128] = {0};
    bool has_key = false, has_answer = false, bad_input = false, extraneous = false;
    bool has_workspace = false, has_task = false;
    uint32_t max_replans = 3;
    bool select_report = false, has_changed = false;
    char select_build[MAX_PATCH_PATH] = {0};
    char *changed_files[256];
    int nchanged = 0;

    if (argc < 2)
    {
        PrintHelp(argv[0]);
        return 0;
    }

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            PrintHelp(argv[0]);
            return 0;
        }
        else if ((strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--workspace") == 0) && i + 1 < argc)
        {
            const char *v = argv[++i];
            if (has_workspace || !v[0] || strlen(v) >= sizeof(workspace)) bad_input = true;
            else snprintf(workspace, sizeof(workspace), "%s", v);
            has_workspace = true;
        }
        else if ((strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--blast-radius") == 0) && i + 1 < argc)
        {
            extraneous = true;
            strncpy(blast_symbol, argv[++i], sizeof(blast_symbol) - 1);
        }
        else if ((strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--diagnose") == 0) && i + 1 < argc)
        {
            extraneous = true;
            strncpy(diagnose_file, argv[++i], sizeof(diagnose_file) - 1);
        }
        else if ((strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--replans") == 0) && i + 1 < argc)
        {
            extraneous = true;
            max_replans = (uint32_t)atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--select-report") == 0 && i + 1 < argc && !select_report)
        {
            const char *v = argv[++i];
            select_report = true;
            if (strlen(v) >= sizeof(select_build)) bad_input = true;
            else snprintf(select_build, sizeof(select_build), "%s", v);
        }
        else if (strcmp(argv[i], "--changed") == 0 && !has_changed)
        {
            has_changed = true;
            while (i + 1 < argc && argv[i + 1][0] != '-')
            {
                if (nchanged >= 256) bad_input = true;
                else changed_files[nchanged++] = argv[i + 1];
                i++;
            }
        }
        else if (strcmp(argv[i], "--select-report") == 0 || strcmp(argv[i], "--changed") == 0)
        {
            bad_input = true;
        }
        else if (strcmp(argv[i], "--ask-missing-goal") == 0)
        {
            if (ask_missing_goal) bad_input = true;
            ask_missing_goal = true;
        }
        else if (strcmp(argv[i], "--continue-stdout-goal") == 0)
        {
            if (continue_stdout_goal) bad_input = true;
            continue_stdout_goal = true;
        }
        else if (strcmp(argv[i], "--workspace-key") == 0 && i + 1 < argc && !has_key)
        {
            const char *v = argv[++i];
            has_key = true;
            if (strlen(v) >= sizeof(workspace_key)) bad_input = true;
            else snprintf(workspace_key, sizeof(workspace_key), "%s", v);
        }
        else if (strcmp(argv[i], "--stdout-answer") == 0 && i + 1 < argc && !has_answer)
        {
            const char *v = argv[++i];
            has_answer = true;
            if (strlen(v) >= sizeof(stdout_answer)) bad_input = true;
            else snprintf(stdout_answer, sizeof(stdout_answer), "%s", v);
        }
        else if (strcmp(argv[i], "--workspace-key") == 0 || strcmp(argv[i], "--stdout-answer") == 0)
        {
            bad_input = true;
        }
        else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--index") == 0)
        {
            extraneous = true;
            do_index_only = true;
        }
        else if (argv[i][0] == '-')
        {
            bad_input = true;
        }
        else if (argv[i][0] != '-')
        {
            if (has_task) extraneous = true;
            has_task = true;
            if (task_desc[0] == '\0')
                strncpy(task_desc, argv[i], sizeof(task_desc) - 1);
            else
            {
                size_t len = strlen(task_desc);
                snprintf(task_desc + len, sizeof(task_desc) - len, " %s", argv[i]);
            }
        }
    }

    if (continue_stdout_goal)
    {
        TASK_OPS_REPORT continuation;
        if (bad_input || extraneous || !has_key || !has_answer || ask_missing_goal ||
            strcmp(task_desc, "stdout-goal-missing"))
        {
            fprintf(stderr, "[symbols-agent] Continuation refused: exact typed request, key and single-line answer required.\n");
            return 1;
        }
        if (!TaskOpsContinueStdout(workspace, task_desc, workspace_key, stdout_answer, &continuation))
        {
            fprintf(stderr, "[symbols-agent] %s\n", continuation.reason);
            return 1;
        }
        printf("[symbols-agent] Verified: your stated goal is reachable by exactly one safe edit.\n");
        printf("[symbols-agent] Goal: user-asserted via typed CLI; edit: executed (normalized stdout, exit 0, no regression). No durable episode.\n");
        return 0;
    }
    if (select_report || has_changed)
    {
        if (!select_report || !has_changed || bad_input || extraneous || has_task || ask_missing_goal ||
            has_key || has_answer)
        {
            fprintf(stderr, "[symbols-agent] --select-report needs --changed and cannot be combined with other modes.\n");
            return 1;
        }
        return SelectReport(workspace, select_build, changed_files, nchanged);
    }
    if (bad_input || has_key || has_answer) {
        fprintf(stderr, "[symbols-agent] Continuation flags require --continue-stdout-goal.\n");
        return 1;
    }

    printf("[symbols-agent] Target Workspace: '%s'\n", workspace);
    clock_t t0 = clock();

    AGENT_RUNNER *runner = AgentRunnerCreate(workspace, max_replans);
    if (!runner)
    {
        fprintf(stderr, "Error: Failed to initialize autonomous agent runner.\n");
        return 1;
    }

    /* Index workspace */
    uint32_t files_indexed = AgentRunnerIndexWorkspace(runner);
    double index_ms = (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC;

    printf("[symbols-agent] Indexed %u files (%u functions, %u classes) in %.2f ms\n",
           files_indexed,
           runner->code_graph ? runner->code_graph->total_functions : 0,
           runner->code_graph ? runner->code_graph->total_classes : 0,
           index_ms);

    /* Mode 1: Index stats only */
    if (do_index_only)
    {
        printf("\n=== Workspace Structural Knowledge Summary ===\n");
        printf("- Root: %s\n", workspace);
        printf("- Files: %u\n", files_indexed);
        printf("- Functions: %u\n", runner->code_graph ? runner->code_graph->total_functions : 0);
        printf("- Classes: %u\n", runner->code_graph ? runner->code_graph->total_classes : 0);
        AgentRunnerDestroy(runner);
        return 0;
    }

    /* Mode 2: Blast Radius Calculation */
    if (blast_symbol[0] != '\0')
    {
        printf("\n[symbols-agent] Computing impact analysis for '%s'...\n\n", blast_symbol);
        CODE_BLAST_RADIUS radius;
        if (CodeGraphComputeBlastRadius(runner->code_graph, blast_symbol, 4, &radius))
        {
            char report[MAX_REPORT_SIZE];
            CodeGraphFormatBlastRadius(runner->code_graph, &radius, report, sizeof(report));
            printf("%s\n", report);
        }
        else
        {
            printf("Error: Symbol '%s' not found in workspace code graph.\n", blast_symbol);
        }
        AgentRunnerDestroy(runner);
        return 0;
    }

    /* Mode 3: Compiler/Linter Diagnostic Abduction */
    if (diagnose_file[0] != '\0')
    {
        printf("\n[symbols-agent] Diagnosing compiler output from '%s'...\n", diagnose_file);
        size_t sz = 0;
        char *log_content = ReadFile(diagnose_file, &sz);
        if (!log_content)
        {
            fprintf(stderr, "Error: Failed to read log file '%s'.\n", diagnose_file);
            AgentRunnerDestroy(runner);
            return 1;
        }

        DIAGNOSTIC_REPORT diag;
        DiagnosticParseOutput(log_content, &diag);
        char remedy[512] = {0};
        DiagnosticAbduceRemedy(&diag, runner->code_graph, remedy, sizeof(remedy));
        free(log_content);

        char diag_md[MAX_REPORT_SIZE];
        DiagnosticFormatReport(&diag, diag_md, sizeof(diag_md));
        printf("\n%s\n", diag_md);
        if (remedy[0] != '\0')
        {
            printf("### Abductive Remediation Strategy\n%s\n\n", remedy);
        }

        AgentRunnerDestroy(runner);
        return 0;
    }

    /* Mode 4: Autonomous Task Resolution */
    if (task_desc[0] != '\0')
    {
        printf("\n[symbols-agent] Formulating plan for task: \"%s\"\n\n", task_desc);

        SWE_BENCH_TASK task;
        memset(&task, 0, sizeof(task));
        snprintf(task.task_id, sizeof(task.task_id), "TASK-%ld", (long)time(NULL));
        strncpy(task.issue_description, task_desc, sizeof(task.issue_description) - 1);

        /* Heuristic check: does issue name a known function? */
        char buf[MAX_TASK_DESC];
        strncpy(buf, task_desc, sizeof(buf) - 1);
        char *tok = strtok(buf, " \t\r\n,.;:\"'()");
        while (tok)
        {
            const char *file = CodeGraphGetFunctionFile(runner->code_graph, tok);
            if (file)
            {
                strncpy(task.target_symbol, tok, sizeof(task.target_symbol) - 1);
                strncpy(task.target_file, file, sizeof(task.target_file) - 1);
                break;
            }
            tok = strtok(NULL, " \t\r\n,.;:\"'()");
        }

        SWE_BENCH_RESULT result;
        memset(&result, 0, sizeof(result));

        clock_t tr_start = clock();
        int solved = AgentRunnerSolveTask(runner, &task, &result);
        /* The runner verifies a proposed patch; with none given (CLI task
           mode), let the generic task operators propose, verify or roll back. */
        TASK_OPS_REPORT ops;
        memset(&ops, 0, sizeof(ops));
        if (!solved)
        {
            solved = TaskOpsSolve(workspace, task_desc, &ops);
            if (ops.reflections_recalled > 0)
                printf("[symbols-agent] Reflexion: %d reflection(s) on this task read back; those edits are excluded\n",
                       ops.reflections_recalled);
            if (ops.reflections_written > 0)
                printf("[symbols-agent] Reflection (attempt %d of %d): %s\n", ops.reflections_written, ops.attempts,
                       ops.reflection);
            if (ops.op[0])
                printf("[symbols-agent] Operator %s: %s (%s)\n", ops.op, ops.detail,
                       ops.verified ? "verified, kept" : "rolled back");
            if (!ops.verified && ops.reason[0])
                printf("[symbols-agent] No edit kept: %s\n", ops.reason);
            if (ask_missing_goal && !ops.verified)
            {
                char question[256];
                if (TaskOpsClarification(workspace, task_desc, &ops, question, sizeof(question)))
                    printf("[symbols-agent] Clarification: %s\n[symbols-agent] Workspace key: %s\n", question, ops.clarification_key);
            }
            if (ops.op[0])
                printf("[symbols-agent] Probe compile %d->%d, run %d->%d\n", ops.compile_before,
                       ops.compile_after, ops.run_before, ops.run_after);
        }
        double solve_ms = (double)(clock() - tr_start) * 1000.0 / CLOCKS_PER_SEC;

        printf("---------------------------------------------------------\n");
        printf("Task ID:        %s\n", result.task_id);
        printf("Status:         %s\n", solved ? "RESOLVED (100% Pass@1)" : "FAILED / UNVERIFIED");
        printf("Resolution Time: %.2f ms\n", solve_ms);
        printf("Tool Calls:     %u\n", result.total_tool_calls);
        printf("Replans:        %u\n", result.replans_triggered);
        printf("Risk Level:     %s\n", result.risk_level[0] ? result.risk_level : "LOW");
        printf("---------------------------------------------------------\n\n");

        if (result.senior_engineer_report[0] != '\0')
        {
            printf("%s\n", result.senior_engineer_report);
        }

        AgentRunnerDestroy(runner);
        return solved ? 0 : 1;
    }

    PrintHelp(argv[0]);
    AgentRunnerDestroy(runner);
    return 0;
}
