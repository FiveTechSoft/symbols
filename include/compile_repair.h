/* compile_repair.h: single-edit repairs for C that does not compile, chosen
 * from the compiler's first error (task wording is not read).
 *
 * Perceive: the first "error:" line of gcc (or the linker's first
 * "undefined reference to `X'") names a class and a subject.
 * Operate: every single edit of these kinds becomes a candidate:
 *   tier 1 ident_near   - X undeclared / no member X / undefined reference
 *                         to X: X becomes the one workspace identifier within
 *                         edit distance 2 (all its occurrences in the file,
 *                         or in every file for a link error)
 *   tier 1 include_def  - X undeclared, struct X incomplete or type X unknown,
 *                         and a workspace header the file does not include
 *                         defines X: the file includes that header
 *   tier 1 header_near  - an #include names a missing file: it names the one
 *                         workspace file within edit distance 2 instead
 *   tier 1 proto_sync   - conflicting types / wrong argument count for F, and
 *                         F has one definition and a prototype that differs:
 *                         the prototype becomes the definition's header
 *   tier 1 semicolon    - expected ';': it is added at the end of the
 *                         previous code line
 *   tier 2 loop_decl    - X undeclared and assigned in "for (X =": that
 *                         becomes "for (int X ="
 *   tier 2 close_brace  - end of input inside a block: the innermost open
 *                         block is closed before the first later line
 *                         indented no deeper than its opener
 * Verify (caller): the workspace compiles after the edit (and a program
 * that runs exits 0); only the one compiling candidate of the lowest tier
 * is kept, several = abstain.
 */
#ifndef COMPILE_REPAIR_H
#define COMPILE_REPAIR_H

#include "task_ops.h"

typedef struct {
    int  file;          /* index into the workspace files; -1 = every file (link rename) */
    char *text;         /* new text of that file (file >= 0) */
    char from[64], to[64];  /* rename pair when file == -1 */
    int  tier;
    char rule[24];
    char detail[160];
} CR_CAND;

/* first compiler error of diag (gcc stderr) -> candidates; returns count */
int  CompileRepairCandidates(const TASK_OPS_WORKSPACE *ws, const char *diag, CR_CAND *out, int max);
void CompileRepairFree(CR_CAND *c, int n);
int  CompileRepairEditDistance(const char *a, const char *b, int cap);

#endif
