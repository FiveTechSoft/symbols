/* ============================================================
   agent_patch.h: Surgical Editing, Unified Diff & Atomic Rollback
                  Engine for Autonomous Agentic Coding.
   Pure ISO C11, zero tensors, zero backprop, fail-closed verification.

   Cognitive Capabilities:
     1. SURGICAL HUNK FORMULATION: Formulates targeted code modifications
        with contextual anchors (context_before, target, replacement,
        context_after) to eliminate ambiguity.
     2. PRE-FLIGHT VERIFICATION: Dry-runs proposed patches in-memory
        before touching disk, enforcing strict uniqueness (ambiguity == 1)
        and computing dynamic line offset drift.
     3. ATOMIC APPLICATION & ROLLBACK: Retains in-memory snapshots of
        modified files; allows instantaneous 0.001s byte-exact restoration
        if the build or test suite verification fails.
     4. UNIFIED DIFF GENERATION: Formats standard 'diff -u' outputs
        for harness logging, human inspection, and git integration.
   ============================================================ */

#ifndef AGENT_PATCH_H
#define AGENT_PATCH_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MAX_PATCH_PATH       260
#define MAX_HUNK_TEXT        4096
#define MAX_PATCH_HUNKS      16
#define MAX_DIFF_BUFFER      16384
#define MAX_PATCH_DIAG       512

/* Status of a patch verification pre-flight check */
typedef enum
{
    PATCH_CHECK_OK = 0,             /* Target found exactly once; safe to apply */
    PATCH_CHECK_NOT_FOUND,          /* Target content not found in file */
    PATCH_CHECK_AMBIGUOUS,          /* Multiple occurrences found; requires more context */
    PATCH_CHECK_IO_ERROR,           /* Cannot read target file */
    PATCH_CHECK_OFFSET_DRIFT        /* Found with line drift, but uniquely anchorable */
} PATCH_CHECK_STATUS;

/* A single surgical modification hunk */
typedef struct
{
    uint32_t expected_line;         /* Hinted line number (or 0 if unknown) */
    char     context_before[MAX_HUNK_TEXT]; /* Leading anchor lines */
    char     target_content[MAX_HUNK_TEXT]; /* Exact lines to replace */
    char     replacement[MAX_HUNK_TEXT];    /* Replacement lines */
    char     context_after[MAX_HUNK_TEXT];  /* Trailing anchor lines */
} PATCH_HUNK;

/* Verification report from dry-running a hunk against a file */
typedef struct
{
    PATCH_CHECK_STATUS status;
    bool               is_applicable;
    uint32_t           matched_line;       /* 1-indexed actual start line in file */
    uint32_t           occurrences_found;  /* Total occurrences in file (must be 1) */
    int32_t            line_drift;         /* Difference between expected and actual line */
    char               diagnostic[MAX_PATCH_DIAG];
} PATCH_VERIFY_REPORT;

/* A multi-hunk patch plan targeting a specific file */
typedef struct
{
    char        target_file[MAX_PATCH_PATH];
    PATCH_HUNK  hunks[MAX_PATCH_HUNKS];
    uint32_t    hunk_count;
    bool        is_applied;
    char       *backup_content;     /* Snapshot of original file content for rollback */
    size_t      backup_size;
} PATCH_PLAN;

/* ============================================================
   Lifecycle API
   ============================================================ */

/* Initialize a new patch plan targeting target_file */
int  PatchPlanInit(PATCH_PLAN *plan, const char *target_file);

/* Release internal backup buffers and clean up plan */
void PatchPlanFree(PATCH_PLAN *plan);

/* ============================================================
   Hunk Formulation API
   ============================================================ */

/* Add a surgical modification hunk to the plan */
int  PatchPlanAddHunk(PATCH_PLAN *plan,
                      uint32_t expected_line,
                      const char *context_before,
                      const char *target_content,
                      const char *replacement,
                      const char *context_after);

/* ============================================================
   Pre-Flight Verification API (Fail-Closed Dry-Run)
   ============================================================ */

/* Dry-run verification of the patch against file on disk */
int  PatchVerifyPlan(const PATCH_PLAN *plan, PATCH_VERIFY_REPORT *report);

/* Dry-run verification against in-memory source string */
int  PatchVerifyAgainstBuffer(const PATCH_PLAN *plan,
                              const char *source_buffer,
                              PATCH_VERIFY_REPORT *report);

/* ============================================================
   Atomic Execution & Rollback API
   ============================================================ */

/* Apply the patch plan to disk, taking an in-memory backup first */
int  PatchApplyAtomic(PATCH_PLAN *plan);

/* Restore the file to its exact pre-patch state from the backup snapshot */
int  PatchRollback(PATCH_PLAN *plan);

/* ============================================================
   Unified Diff Formatting API
   ============================================================ */

/* Format the patch plan as a standard unified diff (diff -u) */
int  PatchFormatUnifiedDiff(const PATCH_PLAN *plan,
                            const PATCH_VERIFY_REPORT *report,
                            char *out_diff,
                            size_t max_size);

#endif /* AGENT_PATCH_H */
