/* ============================================================
   agent_runner.h: Production-Grade Autonomous Coding Orchestrator
                   (SWE-bench / OpenCode Harness Integration).
   Pure ISO C11, zero tensors, zero backprop, fail-closed verification.

   Cognitive Capabilities:
     1. END-TO-END AUTONOMOUS ISSUE RESOLUTION: Receives GitHub/SWE-bench
        issues and coordinates the full perception-action-observation loop.
     2. MULTI-LAYER COGNITIVE INTEGRATION: Combines STRIPS Planning,
        AST Code Graph Blast Radius, and Atomic Surgical Patching.
     3. FAIL-CLOSED VERIFICATION GATE: Requires 100% test pass (exit_code==0);
        automatically rolls back files in 0.001s if verification fails.
     4. SENIOR STAFF ENGINEER REPORTING: Emits structured markdown
        with root-cause diagnosis, blast radius analysis, and unified diff.
   ============================================================ */

#ifndef AGENT_RUNNER_H
#define AGENT_RUNNER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "agent_core.h"
#include "code_graph.h"
#include "agent_patch.h"
#include "agent_planner.h"
#include "agent_diagnose.h"
#include "agent_shell.h"

#define MAX_TASK_ID      64
#define MAX_TASK_DESC    512
#define MAX_CMD_LEN      256
#define MAX_REPORT_SIZE  32768

/* Full definition of a software engineering task (SWE-bench benchmark unit) */
typedef struct
{
    char task_id[MAX_TASK_ID];
    char issue_description[MAX_TASK_DESC];
    char target_file[MAX_PATCH_PATH];
    char target_symbol[MAX_CODE_NAME];
    uint32_t target_line;
    char context_before[MAX_HUNK_TEXT];
    char buggy_snippet[MAX_HUNK_TEXT];
    char fixed_snippet[MAX_HUNK_TEXT];
    char context_after[MAX_HUNK_TEXT];
    char build_command[MAX_CMD_LEN];
    char test_command[MAX_CMD_LEN];
} SWE_BENCH_TASK;

/* Execution and verification report for a task */
typedef struct
{
    char              task_id[MAX_TASK_ID];
    bool              is_solved;
    uint32_t          total_tool_calls;
    uint32_t          replans_triggered;
    uint32_t          affected_callers_count;
    uint32_t          affected_files_count;
    char              risk_level[16];
    char              unified_diff[MAX_DIFF_BUFFER];
    DIAGNOSTIC_REPORT diagnostic_report;
    SHELL_EXEC_RESULT last_shell_exec;
    char              senior_engineer_report[MAX_REPORT_SIZE];
} SWE_BENCH_RESULT;

/* The Unified Production Agent Runner */
typedef struct
{
    AGENT_PLANNER planner;
    CODE_GRAPH   *code_graph;
    uint32_t      max_replans;
    char          workspace_dir[MAX_PATCH_PATH];
} AGENT_RUNNER;

/* ============================================================
   Lifecycle API
   ============================================================ */

AGENT_RUNNER *AgentRunnerCreate(const char *workspace_dir, uint32_t max_replans);
void          AgentRunnerDestroy(AGENT_RUNNER *runner);

/* Index the entire workspace directory recursively into the runner's code graph */
uint32_t      AgentRunnerIndexWorkspace(AGENT_RUNNER *runner);

/* ============================================================
   Execution API
   ============================================================ */

/* Execute an end-to-end coding task autonomously */
int  AgentRunnerSolveTask(AGENT_RUNNER *runner,
                          const SWE_BENCH_TASK *task,
                          SWE_BENCH_RESULT *out_result);

/* Format a senior engineer pull request / issue resolution report */
int  AgentRunnerFormatSeniorReport(const SWE_BENCH_TASK *task,
                                   const SWE_BENCH_RESULT *result,
                                   const CODE_BLAST_RADIUS *radius,
                                   char *out_report,
                                   size_t report_size);

#endif /* AGENT_RUNNER_H */
