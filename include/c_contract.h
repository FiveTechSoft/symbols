/* c_contract.h: repair a C program that builds but does not do what the
 * task states, by searching single edits and running each one.
 *
 * Perceive (task text, no ids): in clauses that state the wanted behaviour
 * (they hold "should", "must", "expected", "needs to", "so that", ...; a
 * ", but ..." tail, clauses about the current behaviour and parenthesised
 * text are skipped) the wanted stdout: "print(s)/output/stdout" followed by
 * a `quoted` or "quoted" value, a number, a single last word, or a later
 * "is N" / "be N". A stated exit status must be 0 (the default).
 * Operate: single-edit candidates inside function bodies (main only when it
 * is the one function; printf formats anywhere):
 *   tier 1 boundary    (< <-> <=, > <-> >=)
 *   tier 2 index_shift (every [E] of one function -> [E - 1]; [E + 1] is tier 4),
 *          init_mul    (x = 0 where x *= follows -> x = 1),
 *          int_div     (return A / B in a double/float function -> (double)A / B),
 *          stale_swap  (t = X; X = Y; Y = X; -> Y = t;)
 *   tier 3 direction   (< <-> >, <= <-> >=), equality (== <-> !=),
 *          format      (%d / %ld / %lld width in a format string)
 *   tier 4 plus_minus  (binary + <-> -), int_literal (N -> N +/- 1)
 * A candidate that writes the wanted output into the code as a new literal
 * is dropped.
 * Verify (caller): each candidate is built and run in a throwaway copy; only
 * the one candidate of the lowest tier that prints exactly the wanted output
 * and exits 0 is kept (two in that tier: abstain).
 */
#ifndef C_CONTRACT_H
#define C_CONTRACT_H

typedef struct {
    int  has_out;
    char out[128];
    /* when has_out is 0, why no wanted stdout was read, closed form:
       nogoal (no clause states a goal), now (goal words only in clauses about
       the current state), exit (a goal of non-zero exit), file (goal clauses
       are about a file), noval (a goal clause, no value found), multi (two
       different wanted outputs) */
    char why[8];
} C_CONTRACT;

typedef struct {
    char *text;
    int  tier;
    char rule[24];
    char detail[120];
} C_CAND;

/* 1 when one wanted stdout was read (and no other exit than 0 is wanted) */
int  CContractParse(const char *task, C_CONTRACT *c);
/* allow_main: main's body may be edited (it is the only function) */
int  CContractCandidates(const char *src, const C_CONTRACT *c, int allow_main, C_CAND *out, int max);
void CContractFree(C_CAND *c, int n);

#endif
