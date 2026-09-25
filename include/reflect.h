/* reflect.h: verbal reflections on refuted attempts (minimal Reflexion loop).
 *
 * A reflection is written only from real toolchain feedback: an operator's
 * edit was applied, the workspace was rebuilt / rerun / rechecked, and the
 * verification refuted it (the edit was rolled back). It records what was
 * tried, what the toolchain reported, and what the next attempt changes
 * (that exact edit is excluded). Guesses, abstentions and write failures are
 * never recorded. Rows are keyed by the task + workspace hash, so they are
 * read back only on a retry of the same task on the same files.
 *
 * Store: TSV, one row per reflection:
 *   key  attempt  op  detail  feedback  source  unix_ts  text
 * source is always "toolchain". Appends and forgets rewrite the file through
 * a temporary and a rename, so a crash never leaves a half row; a failed
 * write is reported, never assumed.
 */
#ifndef REFLECT_H
#define REFLECT_H

#include <stddef.h>

#define REFLECT_MAX_ROWS 256

typedef struct {
    char key[32];
    int  attempt;
    char op[32];
    char detail[256];
    char feedback[160];
    long long ts;
    char text[512];
} REFLECTION;

/* "Tried OP (DETAIL); the toolchain reported: FEEDBACK. Next attempt: exclude this edit." */
void ReflectCompose(REFLECTION *r);
/* rows for key (all rows when key is NULL); returns count */
int  ReflectLoad(const char *path, const char *key, REFLECTION *out, int max);
/* 1 on success, 0 when the store could not be written */
int  ReflectAppend(const char *path, const REFLECTION *r);
/* removes every row of key; returns rows removed, -1 on write failure */
int  ReflectForget(const char *path, const char *key);

#endif
