/* c_fix_ops.h: small deterministic C fault repairs chosen from the task.
 *   loop_bound  - the task reports an off-by-one loop bound; the one for-loop
 *                 whose condition holds a single < or <= gets it flipped
 *   array_fit   - the task reports a too-small buffer; the one char array
 *                 initialized from a longer string literal is resized to
 *                 strlen + 1
 *   goto_return - the task asks to remove goto; "if (C) goto L;" whose label
 *                 L is followed only by "return X;" at the end of the function
 *                 becomes "if (C) { return X; }" and the label is removed
 *   declare_local - the task says one identifier (with _ or a digit) is used
 *                 but never declared; "int X = 0;" opens the one function
 *                 that uses it
 *   comment_fix - the task quotes new wording 'Q' and repeats stale wording
 *                 (3+ words) found in exactly one comment and not in code;
 *                 that wording becomes Q
 * CFixSplit: the task asks to split F() out into S.c with a prototype in
 * S.h; the one top-level definition of F moves to S.c (static dropped), S.h
 * gets a guarded prototype, and the source includes S.h.
 * Verification (caller): the program builds and its own exit code goes from
 * failing to 0 (loop_bound, array_fit) or stays the same (goto_return).
 * Abstain: NULL when zero or several candidates exist.
 */
#ifndef C_FIX_OPS_H
#define C_FIX_OPS_H

#include <stddef.h>

char *CFixApply(const char *src, const char *task, char *rule, size_t rule_size, char *detail, size_t detail_size);

typedef struct {
    char *new_src, *c_text, *h_text;
    char c_rel[80], h_rel[80];
    char detail[160];
} CFIX_SPLIT;

int CFixSplit(const char *src, const char *src_rel, const char *task, CFIX_SPLIT *out);
void CFixSplitFree(CFIX_SPLIT *s);

/* Evidence candidates: every single edit of these kinds, for a caller that
 * keeps only the one that makes a failing program exit 0 (task wording is
 * not consulted):
 *   tier 1 boundary   - < <-> <=, > <-> >= in an if/while/for condition or return
 *   tier 2 array_fit  - char a[N] = "literal" that does not fit -> strlen + 1
 *   tier 2 init_local - "TYPE x;" whose first use updates it (x += ..., x++) -> "= 0" (or "= 1" for *=, /=)
 *   tier 3 direction  - < <-> >, <= <-> >= in the same places
 *   tier 3 equality   - == <-> != in a condition or return
 */
typedef struct {
    char *text;
    int tier;
    char rule[24];
    char detail[96];
} CFIX_CAND;

int CFixCandidates(const char *src, CFIX_CAND *out, int max);
void CFixCandidatesFree(CFIX_CAND *c, int n);

/* Compiler evidence: gcc reported NAME as undeclared (first use in a
 * function) with no "did you mean" note; declare_local's rule applies to
 * that one name ("int NAME = 0;" opens the one function using it). */
char *CFixDeclareUndeclared(const char *src, const char *name, char *detail, size_t detail_size);

#endif
