# M2 exit reading (Phase 2: Safe Git)

Reading of the Phase 2 exit criteria in `ROADMAP.md` against the tests on `master` at `2e063dd`
(code last changed at `d05031f`, CI run 37188087523: linux 171/171, msvc and asan-msvc 181/181, all four jobs green).
Nothing here is a new measurement except where a run is named. This is a reading of what the tests
cover; M2 is not declared. The declaration is Antonio's.

## Which git tests run where

| Test | Linux | Windows (one runner, `windows-latest`) |
|---|---|---|
| `test_agent_git` | runs | runs (no `#ifndef _WIN32` around the cells; Windows branches only for fixture cleanup). Ctest shows Passed; the assertion count was not read from the log. |
| `test_agent_git_remote` | runs | runs since m112 (`1662322`, run 37190124619): remote-advance preflight (58 cells in the source, the count was not read from the Windows log), bare remote and two clones; ctest Passed on msvc (10.64 s) and asan-msvc (12.67 s), 181/181 on both |
| `test_agent_git_contract` | runs | runs since m114 (`d6658aa`, run 37191494917): commit contract and already-applied cells (44 in the source, count not read from the Windows log); ctest Passed on msvc (4.46 s), asan-msvc (3.84 s), 181/181 on both |
| `test_agent_git_submodule` | runs | runs since m116 (`446958d`, run 37192775579): submodule states seen by the preflight (33 cells in the source, count not read from the Windows log); ctest Passed on msvc (4.57 s) and asan-msvc (4.72 s), 181/181 on both |
| `test_git_gate` | runs | SKIP stub (POSIX only) |
| `test_git_gate_e2e` | runs | runs since m118 (`4c6a36c`). The first Windows run (37193477478) was red, 18 of 31 cells: my port had missed one `2>/dev/null` in the control reset, which cmd.exe cannot open (found with the m119 diagnostic, run 37194050162). Fixed in m120 (`dab0ab5`, run 37194684610): ctest Passed on msvc (5.81 s) and asan-msvc (6.43 s), 181/181 on both; the 31/31 count was not read from the Windows log |
| `test_git_destructive_audit` | runs | SKIP stub (directory walk) |
| `test_git_ops` | runs | git-backed part is `#ifndef _WIN32` |
| `test_git_gate_e2e_mc` (m109 mutant) | runs, kill = exit 0 | not built (`if(NOT WIN32)`) |

A Passed on Windows for a SKIP stub is not evidence of the behaviour.

## Criteria

1. **Dirty-tree, stale-HEAD, detached-HEAD, conflict, remote-advance and submodule fixtures fail without losing user changes.**
   Linux: shown for the listed cells (`test_agent_git`: dirty, stale, detached, conflict; `test_agent_git_remote` and the m95b gate e2e: remote advance and the control; `test_agent_git_submodule`: submodule).
   Windows: shown for the listed cells on one runner. The `test_agent_git` cells and, since m112, the `test_agent_git_remote` cells (remote advance, diverged, no upstream, detached, unreachable remote, dirty tree) run. Since m116 the `test_agent_git_submodule` cells run too (modified and untracked content inside the submodule, a moved or older recorded commit, an uninitialised submodule), and since m120 the gate e2e remote-advance sequence runs through the gate (preflight `--remote-sync`, verify-staged, verify-head, the refused non-fast-forward push, the control). All Passed on one runner.
2. **A produced commit's tree exactly matches the reviewed mutation manifest.**
   Linux: shown for the listed cells (`test_agent_git_contract`, `test_git_gate` verify-staged and verify-head).
   Windows: shown for the `test_agent_git_contract` cells since m114 (staged and HEAD commit against the manifest, a path with a space, merge commit, already-applied detection, on one runner, with `core.autocrlf` set to false in the fixtures, which was not measured as needed); `test_git_gate` verify-staged and verify-head are NOT SHOWN (SKIP stub).
3. **Retry after an interrupted operation is idempotent or reports the already-completed result.**
   Linux: PARTIAL. `AgentGitPatchState` reports already applied for an applied-uncommitted and an applied-committed patch (`test_agent_git_contract`, `test_git_gate`). Since m109 (`d05031f`) the gate e2e has an interrupted-run cell: apply, stage, verify and commit done, no push. Measured on Linux CI: `patch-state` reports already applied with exit 3 (R1), `verify-head` passes (R2), a second `git apply` is refused and changes nothing (R3), the push then finishes once with exactly one more commit on the remote (R4), a second push is a no-op (R5), and `patch-state` after the push still reports already applied (R6). One mutant, `AGENT_GIT_PATCH_STATE_MUTANT` (the reverse check is dropped), is killed by R1 and R6 (`test_git_gate_e2e_mc` Passed on Linux CI, where exit 0 means killed; the 29 of 31 cell count is from the local run, the CI log hides it). R2 to R5 do not depend on that mutant, so the mutant does not test them.
   Windows: the same R1 to R6 cells run through the gate since m120 and the test Passed on msvc and asan-msvc (cell count not read from the log). The mutant target is not built on Windows, so the Windows R cells are not mutant-checked.
   Not shown: a process killed during the sequence (the interruption is simulated by not running the push), a different crash point, retry through the workflow YAML.
4. **No test path invokes destructive reset, clean, checkout or force push implicitly.**
   Linux: shown for the source inventory in `test_git_destructive_audit`. It does not cover the workflow YAML. The gate e2e control uses an explicit `git reset --hard` on its own scratch clone, labelled as a fixture step. Windows: NOT SHOWN (SKIP stub).
5. **The apply-patch workflow uses the Git contracts instead of open-coded assumptions.**
   NOT MET. `.github/workflows/apply-patch.yml` calls `symbols_git_gate` for part of the sequence but still open-codes the expected-head `rev-parse`, `git ls-remote`, `git apply`, `git add` and `git push`. Changing those steps is Antonio's parked decision.

## Limits

- One runner and one account for the Windows cells; Linux cells on `ubuntu` only.
- Local bare remotes only; no hosted-remote behaviour (auth, hooks, branch protection) is covered.
- No power-loss or process-kill claim for the git sequence.
- Cooperating writers only.

## Reading

Phase 2 exit is NOT met. Criteria 1, 2 and 4 are shown on Linux for the listed cells; on Windows criterion 1 is shown for the cells of `test_agent_git`, `_remote`, `_submodule` and the gate e2e (one runner), criterion 2 for the `test_agent_git_contract` cells only (`test_git_gate` verify cells are a SKIP stub), and criterion 4 is not shown. Criterion 3 is partial, with the m109 cell as its first end-to-end retry measurement, now also run on Windows (m120). Criterion 5 is not met and parked.
