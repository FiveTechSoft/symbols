# Separate Windows spawn handle diagnostic

The strict original `test_shell_spawn_win` and its gold are unchanged.
Run [588](https://github.com/FiveTechSoft/symbols/actions/runs/38804730818)
on `f8482e99999cedaa03e7914fbc37d0141ee21870` failed that original test in
MSVC and ASAN. Both observed the positive control with sensitivity mask 5,
OS launch rejection with error216, and three shell-API calls with exact expected
failure fields, empty captures/totals and no witness (mask0). The handle counts
increased85 to86 and111 to112. Those observations do not identify the handle's
owner or prove that it is safe.

`test_shell_spawn_handles_win` is separate diagnostic work. Its private runner
records baseline and counts immediately after each of27 identical shell-API
calls, before printing each call's diagnostic, in blocks3+12+12. The same
positive control, OS probe193/216, exact failure-result oracle, private directory
checks, cleanup and30-second child bound apply. It prints all28 handle counts,
each block's final delta and a classification. Its independent frozen protocol
is `tests/shell_spawn_handles_win_gold.json`.

The allowed traces are either all counts equal baseline (`flat`), or exactly
one additional handle on the first call with all later counts equal that first
count (`one_time_increase`). Any other count trace or failed invariant is a
nonzero diagnostic result. Neither accepted classification certifies which
subsystem owns a handle or clears the original test. There is no rebaseline,
no deletion of an unknown handle and no widening after the measurement.

CI invokes the diagnostic explicitly on MSVC Release and MSVC ASAN before
ordinary CTest, so successful trace output is visible in raw logs. The original
fixture stays strict and may keep both native jobs red. This slice's result is
the measured trace and the unresolved original-red state, not a promise of five
green jobs, a phase exit or a solver efficacy claim. No production or ABI change
is included. Handle growth after the first call requires review rather than a
silent retry.

The apply workflow's reusable CI definition comes from its dispatched ref, even
when its checkout validates the newly published SHA. This publication therefore
uses two separately recorded runs: the patch application with its inherited CI
definition, then an explicit CI workflow dispatch from updated master with the
same exact target SHA to expose the new diagnostic output. The second run is
planned diagnostic execution, not an environment rerun of the original red.
