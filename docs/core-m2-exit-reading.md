# M2 exit reading (Phase 2: Safe Git)

Reading of the Phase 2 exit criteria in `ROADMAP.md` against the tests on `master` at `2e063dd`
(code last changed at `d05031f`, CI run 37188087523: linux 171/171, msvc and asan-msvc 181/181, all four jobs green).
Nothing here is a new measurement except where a run is named. This is a reading of what the tests
cover; M2 is not declared. The declaration is Antonio's.

## Which git tests run where

| Test | Linux | Windows (one runner, `windows-latest`) |
|---|---|---|
| `test_agent_git` | runs | runs (no `#ifndef _WIN32` around the cells; Windows branches only for fixture cleanup). Ctest shows Passed; the assertion count was not read from the log. |
| `test_agent_git_remote`, `test_agent_git_contract`, `test_agent_git_submodule` | run | `#ifndef _WIN32`: the Windows build is a SKIP stub that returns 0 |
| `test_git_gate`, `test_git_gate_e2e` | run | SKIP stub (POSIX only) |
| `test_git_destructive_audit` | runs | SKIP stub (directory walk) |
| `test_git_ops` | runs | git-backed part is `#ifndef _WIN32` |
| `test_git_gate_e2e_mc` (m109 mutant) | runs, kill = exit 0 | not built (`if(NOT WIN32)`) |

A Passed on Windows for a SKIP stub is not evidence of the behaviour.

## Criteria

1. **Dirty-tree, stale-HEAD, detached-HEAD, conflict, remote-advance and submodule fixtures fail without losing user changes.**
   Linux: shown for the listed cells (`test_agent_git`: dirty, stale, detached, conflict; `test_agent_git_remote` and the m95b gate e2e: remote advance and the control; `test_agent_git_submodule`: submodule).
   Windows: PARTIAL. Only the `test_agent_git` cells run. Remote advance, submodule and the gate e2e are NOT SHOWN.
2. **A produced commit's tree exactly matches the reviewed mutation manifest.**
   Linux: shown for the listed cells (`test_agent_git_contract`, `test_git_gate` verify-staged and verify-head).
   Windows: NOT SHOWN (those tests are SKIP stubs).
3. **Retry after an interrupted operation is idempotent or reports the already-completed result.**
   Linux: PARTIAL. `AgentGitPatchState` reports already applied for an applied-uncommitted and an applied-committed patch (`test_agent_git_contract`, `test_git_gate`). Since m109 (`d05031f`) the gate e2e has an interrupted-run cell: apply, stage, verify and commit done, no push. Measured on Linux CI: `patch-state` reports already applied with exit 3 (R1), `verify-head` passes (R2), a second `git apply` is refused and changes nothing (R3), the push then finishes once with exactly one more commit on the remote (R4), a second push is a no-op (R5), and `patch-state` after the push still reports already applied (R6). One mutant, `AGENT_GIT_PATCH_STATE_MUTANT` (the reverse check is dropped), is killed by R1 and R6 (`test_git_gate_e2e_mc` Passed on Linux CI, where exit 0 means killed; the 29 of 31 cell count is from the local run, the CI log hides it). R2 to R5 do not depend on that mutant, so the mutant does not test them.
   Not shown: a process killed during the sequence (the interruption is simulated by not running the push), a different crash point, retry through the workflow YAML, anything on Windows.
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

Phase 2 exit is NOT met. Criteria 1, 2 and 4 are shown on Linux for the listed cells and not (or only partly) on Windows. Criterion 3 is partial, with the m109 cell as its first end-to-end retry measurement. Criterion 5 is not met and parked.
