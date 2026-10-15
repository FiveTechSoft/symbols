# Private Windows shell boundary and handle ledger diagnostic

Two native CI runs on `28edb94da11d36bbf78ef5d331cb5d95b5c7e02d` measured
exactly one extra handle on the first of27 shell launch-failure calls and no
growth over26 later calls. MSVC counted81 then82 throughout; ASAN107 then108.
The diagnostic passed, but the original strict fixture remains red (85 to86,
111 to112). That evidence does not establish the handle's owner or harmlessness.

The next diagnostic, `test_shell_handle_owner_win`, compares fresh normal and
private-instrumented runner processes. Both keep the same invalid-image OS
probe193/216, positive witness control/mask5 and27 exact shell failure results.
The original strict test, original gold and prior diagnostic are unchanged.
No production source or ABI changes are included.

`tests/shell_handle_boundary_win.c` compiles a private translation unit by
including the unchanged `src/agent_shell.c`. Only private exported symbol names
and the launch-failure path's Win32 API names are redirected. Each wrapper calls
the real OS function, preserving incoming and outgoing LastError around handle
counts and fixed bounded trace writes. No output, allocation or object-name
query occurs inside the measured call. Before/after counts and real API return
values are buffered. Each call's records are printed afterward. The private
unit is linked only into this diagnostic executable, never into `symbolic`.

The wrapped boundaries are file attributes, performance frequency/counter,
pipe creation, handle flags, Job Object creation/configuration, borrowed stdin,
process creation and handle close. An explicit ledger requires two pipe pairs
and one Job Object per rejected launch, each closed successfully once; no
borrowed stdin or foreign handle may be closed. The sum of per-boundary deltas
must equal the outer call's count delta. Trace overflow or count failure is a
failure. The frozen protocol is `tests/shell_handle_owner_win_gold.json`.

Normal and instrumented count traces are classified independently. A changed
trace under instrumentation is an observer-effect warning, not an excuse to
substitute the instrumented trace for the control. A localized positive delta
names a boundary, not an object's type, owner or safety. Even complete closure
of all five explicit handles cannot certify a different handle. Further owner
identification may still be required before any contract or production change.

Native CI prints both arms and the API ledger before CTest. Publication splits
test/CMake/docs/gold from ci.yml; the former uses ordinary guarded patching,
the workflow uses the workflow-only bootstrap. Each is read back exactly.
The automatic PAT-push CI may supply final-SHA raw evidence; no duplicate CI is
needed solely for visibility. This is diagnosis with an unresolved original red,
not five green jobs, a phase exit or an efficacy claim. The private child bound
is30seconds, and unexpected traces or failed invariants remain nonzero.
