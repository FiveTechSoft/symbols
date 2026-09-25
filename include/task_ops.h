/*
 * task_ops.h - Generic edit operators driven by a task description.
 *
 * The agent runner verifies and repairs a patch it is given; this module
 * proposes one. Loop: perceive (index workspace text files, lex the task into
 * code-shaped tokens, quoted literals and compiler flags) -> reason (choose an
 * operator whose preconditions hold in this workspace) -> act (apply to an
 * in-memory copy, write atomically) -> verify (operator intent holds and no
 * regression in the agent's own build/run probe; the task's hidden checker is
 * never read) -> roll back unless verified.
 *
 * No task ids, file names or answers live in this module.
 */
#ifndef TASK_OPS_H
#define TASK_OPS_H

#include <stddef.h>

#define TASK_OPS_MAX_FILES 64
#define TASK_OPS_MAX_PATH  512
#define TASK_OPS_MAX_FILE  (256 * 1024)

typedef struct
{
    char   rel[TASK_OPS_MAX_PATH];  /* path relative to the workspace root */
    char  *data;                    /* NUL-terminated contents */
    size_t len;
} TASK_OPS_FILE;

typedef struct
{
    char          root[TASK_OPS_MAX_PATH];
    TASK_OPS_FILE files[TASK_OPS_MAX_FILES];
    int           count;
} TASK_OPS_WORKSPACE;

typedef struct
{
    char op[32];          /* operator name, "" when none applied */
    char detail[256];     /* e.g. "OLD_LIMIT -> NEW_LIMIT in 2 files" */
    char reason[256];     /* why nothing was kept, when applicable */
    int  candidates;      /* operator instances whose preconditions held */
    int  applied;         /* files written */
    int  verified;        /* 1 = kept after verification */
    int  compile_before;  /* -1 = no C sources / no compiler, 0 fail, 1 ok */
    int  compile_after;
    int  run_before;      /* -1 = not run, otherwise exit code */
    int  run_after;
    int  memory_skipped;  /* operators skipped: they rolled back before on this workspace+task */
    int  memory_reordered;/* 1 = operator order came from remembered outcomes */
    int  attempts;        /* attempts made (Reflexion loop, <= 3) */
    int  reflections_written;  /* refuted edits reflected on in this solve */
    int  reflections_recalled; /* reflections read back for this task */
    char reflection[512]; /* the last reflection written, "" when none */
    char clarification_key[32]; /* task + post-solve workspace binding; never a permission */
} TASK_OPS_REPORT;

/* Load text files (no NUL bytes, <= TASK_OPS_MAX_FILE) under root. */
int  TaskOpsLoadWorkspace(const char *root, TASK_OPS_WORKSPACE *ws);
void TaskOpsFreeWorkspace(TASK_OPS_WORKSPACE *ws);

/* Whole-token occurrence count of an identifier across the workspace. */
int  TaskOpsCountToken(const TASK_OPS_WORKSPACE *ws, const char *ident);

/* Rename reasoning only (no I/O): finds the unique pair (A, B) of adjacent
   code-shaped task tokens with A present in the workspace and B absent.
   Returns number of distinct qualifying pairs; fills a/b when exactly one. */
int  TaskOpsFindRename(const TASK_OPS_WORKSPACE *ws, const char *task,
                       char *a, size_t a_size, char *b, size_t b_size);

/* Read-only question classifier after an ordinary abstention. Returns 1 and a
   bounded question only for the exact typed task "stdout-goal-missing", after
   a bound no-edit solve with a safe, input-free C workspace. It never
   reads stdin, executes the program, writes files, or changes the task. */
int TaskOpsClarification(const char *workspace, const char *task,
                         const TASK_OPS_REPORT *rep, char *question, size_t size);

/* Full loop on a workspace directory. Returns 1 when an edit was kept. */
int  TaskOpsSolve(const char *workspace, const char *task, TASK_OPS_REPORT *rep);

#endif /* TASK_OPS_H */
