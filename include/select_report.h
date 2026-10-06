/* select_report.h: a read-only report of the build-graph test selection
   (tools/build_graph.py). It runs that script and nothing else, skips no test
   and changes no state. Every way it can fail to produce a selection is
   reported as "mode: full" with the reason: the full gate is the answer
   whenever the selection is not known.

   Two users share it: `symbols-agent --select-report` and an opt-in hook in
   TaskOpsSolve, gated by SYMBOLS_SELECT_REPORT=<build dir> (unset or "0": no
   code path changes). The hook reports to stderr after a verified edit. */
#ifndef SELECT_REPORT_H
#define SELECT_REPORT_H

#include <stdio.h>

#define SELECT_REPORT_HEADER \
    "[symbols-agent] select-report (read-only: nothing is skipped, the full gate stays the default)"

/* Print the report for `changed` (workspace-relative paths) to `out`.
   Arguments reach a shell, so only a conservative path alphabet is accepted.
   Always returns 0: a report was printed, possibly "mode: full". */
int SelectReportPrint(FILE *out, const char *workspace, const char *build,
                      char **changed, int nchanged, unsigned timeout_ms);

/* Hook for TaskOpsSolve. All are no-ops unless the gate is on. */
int  SelectReportGateOn(void);
void SelectReportNoteReset(void);
void SelectReportNote(const char *rel);
/* Call once after a verified solve: reports the files noted by the last attempt. */
void SelectReportAfterVerified(const char *workspace);

#endif
