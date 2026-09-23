/* c_edit_ops.h: deterministic, generic edit operators over observed C code.
 *
 * Perceive: parse the observed .h/.c sources into structs (members and
 * types), collections (arrays of a struct type and their counters),
 * initializer groups ({...} for a struct, from compound literals or array
 * initializers) and main().
 * Operate: add a field to a struct and propagate per-item values to every
 * initializer (matched by the item's identifying string field); add an
 * aggregation function over a collection; print it from main; manage
 * includes and prototypes.
 * Abstain: when the observed code does not support an unambiguous change,
 * return 0 with a reason instead of guessing.
 */
#ifndef C_EDIT_OPS_H
#define C_EDIT_OPS_H

#include <stddef.h>

#define CEO_MAX_FILES   10
#define CEO_MAX_HUNKS   16
#define CEO_HUNK_OLD    1536
#define CEO_HUNK_NEW    2048

typedef struct
{
    int  file;                        /* index into the input files */
    char old_text[CEO_HUNK_OLD];      /* unique in the original file */
    char new_text[CEO_HUNK_NEW];
} CeoHunk;

typedef struct
{
    CeoHunk hunks[CEO_MAX_HUNKS];
    int     nhunks;
    char    struct_name[64];
    char    field[32];
    char    total_func[64];
    double  expected_total;           /* sum over observed items */
    int     order_assumed;            /* values mapped by request order (names not found) */
    int     nitems;
    char    summary[768];
    char    reason[512];              /* set when planning abstains (Spanish, matches the server wrapper) */
} CeoPlan;

/* Request asks to add a per-item quantity and an aggregate total. */
int CeoIsAddFieldAndTotalRequest(const char *issue);

/* Plan the change. paths/srcs are the observed files (any names/roles).
   Returns number of hunks (>0) or 0 with plan->reason set. */
int CeoPlanAddFieldAndTotal(const char *issue, const char *const *paths,
                            const char *const *srcs, int nfiles, CeoPlan *plan);

/* Apply the plan's hunks to srcs (for tests and self-checks). out[i] must
   hold at least out_size bytes. Returns 1 when every hunk applied. */
int CeoApplyPlan(const CeoPlan *plan, const char *const *srcs, int nfiles,
                 char out[][8192], size_t out_size);

/* Build-and-run command from observed files (Makefile, CMake, or gcc). */
int CeoDeriveRunCommand(const char *const *paths, const char *const *srcs,
                        int nfiles, const char *listing, char *out, size_t size);

#endif
