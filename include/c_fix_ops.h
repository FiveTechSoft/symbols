/* c_fix_ops.h: small deterministic C fault repairs chosen from the task.
 *   loop_bound  - the task reports an off-by-one loop bound; the one for-loop
 *                 whose condition holds a single < or <= gets it flipped
 *   array_fit   - the task reports a too-small buffer; the one char array
 *                 initialized from a longer string literal is resized to
 *                 strlen + 1
 *   goto_return - the task asks to remove goto; "if (C) goto L;" whose label
 *                 L is followed only by "return X;" at the end of the function
 *                 becomes "if (C) { return X; }" and the label is removed
 * Verification (caller): the program builds and its own exit code goes from
 * failing to 0 (loop_bound, array_fit) or stays the same (goto_return).
 * Abstain: NULL when zero or several candidates exist.
 */
#ifndef C_FIX_OPS_H
#define C_FIX_OPS_H

#include <stddef.h>

char *CFixApply(const char *src, const char *task, char *rule, size_t rule_size, char *detail, size_t detail_size);

#endif
