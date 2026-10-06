# Build graph and conservative test selection (M3 slice 1, m231)

Status: prototype, read-only, not connected to the agent. Landed in `8db56f9` (run 37415774004, all jobs read: apply 199, linux 199, msvc 223, asan-msvc 223, `ast-inspect-windows-ninja` 10 tests OK). This is the first M3 deliverable ("CMake File API and CTest metadata ingestion", "conservative affected-test selection with a full-suite fallback"). It is not an M3 exit claim.

## What it does

`tools/build_graph.py` has three commands.
- `configure SRC BUILD [-G GEN]` writes the File API query `codemodel-v2` into the build tree and configures.
- `graph BUILD [--config C]` prints targets (name, type, listed sources, generated outputs, direct dependencies, artifacts), tests (name, command, mapped target, labels) and an `unknown` list.
- `select BUILD --changed F...` prints `{mode, tests, reasons}`.

Selection: a changed file is looked up in the sources the codemodel lists per target; the targets that list it, and every target that depends on them, are affected; the tests whose command is an affected target's artifact are selected.

## When it selects the full gate

Anything it cannot prove: no File API reply, a generator other than Unix Makefiles or Ninja, a codemodel major version other than 2, several configurations with none chosen, a test whose command is not the artifact of exactly one target (a script, or an executable that is not built yet), unreadable ctest JSON, a changed `CMakeLists.txt`, `*.cmake` or `CMakeCache.txt`, and a changed file that no target lists (headers, generator inputs, data files). Mode is then `full` and all tests are returned.

Two facts read on cmake 3.22.1 that shaped it: `ctest --show-only=json-v1` omits the `command` of a test whose executable does not exist yet, so the tree must be built first; and the codemodel lists an object library as a dependency of every target that uses its objects, and lists generated outputs as `isGenerated` sources.

## Evidence

`tests/test_build_graph.py` on three labeled fixtures (`tests/fixtures/graph_basic`, `graph_mixed`, `graph_script`):
- The graph equals a hand-written expected JSON for `basic` (3 targets, 2 tests, labels) and `mixed` (static, object and generated-source libraries, a shared dependency, 3 tests). The `generated` field is not in the expected files.
- Recall oracle: touch one source, rebuild, and take the tests whose executable was relinked. Over 9 file rows the selection contained every one (recall 9/9); 10 tests selected, 10 needed (precision 1.00 on these fixtures).
- Full-gate rules: changed `CMakeLists.txt`, a header, a generator input and an unbuilt tree each select the full gate; `graph_script` has a script test that is reported unmapped and selects the full gate.
- `test_build_graph_mc` removes the dependent closure inside the test (the tool has no hook) and exits 0 only if exactly `recall_basic` and `recall_mixed` fail.

Predictions that were wrong: precision below 100% (measured 1.00) and a first-version mistake on object libraries or generated sources (none). Both come from the fixtures being small: they hold no structure that over-selects, so 1.00 says nothing about precision on a real repository. An early version of the oracle was wrong (future mtimes cascaded rebuilds) and was fixed before these numbers.

## Limits

- Recall 100% is measured on two small fixtures, not on a real repository.
- Header dependencies are not in the codemodel, so every header change selects the full gate (safe, low precision). A test that reads data files or runs a script is not modelled; both select the full gate.
- Only single-configuration Makefiles and Ninja. The multi-configuration check runs only where `ninja` exists; it did not run on the machine where the numbers were measured (no ninja), and I did not check the CI runner, so it is unmeasured.
- Windows is not measured: both tests print SKIP and are reported as skipped on `msvc` and `asan-msvc`. The counts of 223 include them as skipped.
- The "ok" and the mutant-kill lines are known from the exit code, not from stdout, which CTest hides.
- No `src/` change, no agent integration. Nothing here changes what the agent does.

## Update: include scan (m232)

Landed in `cdc13be` (run 37419789922, all jobs read: apply 200, linux 200, msvc 224, asan-msvc 224, `ast-inspect-windows-ninja` 10 tests OK). Linux ran the three `build_graph` entries (log lines read: all Passed); the Windows jobs skip them.

Why. The first run of the tool on this repository's own build (178 targets, 199 tests) found a miss that the fixtures could not show. `tests/test_relation_subject_index.c` does `#include "../src/relation.c"`. The codemodel lists `relation.c` only under the `symbolic` library and lists no dependency for that test, so touching `relation.c` relinked the test (oracle) and the selection did not contain it. The first sample was 6 files: 5 matched, 1 missed. With the rule as landed in m231 the recall claim was false for any listed source that is also included textually.

What it does now. `select` scans the source tree (`.c .h .cc .cpp .cxx .hpp .hh .inc .inl`, skipping dot directories and directories that hold a `CMakeCache.txt`), matches each `#include` by basename only, and treats every file that includes a changed file as changed, transitively. A header is no longer a blanket full gate: it selects through its includers. Still the full gate: a changed file in no target with no includer, a changed CMake file, any quoted include whose basename matches no scanned file (for example a generated header), and any unmapped test.

Evidence.
- Fixture `graph_incl` reproduces the miss (a test target that includes a library `.c` and links nothing). The mutant with the scan off (`--mutant scan_off`) exits 0 only if exactly `header_scan` and `recall_incl` fail; measured, killed by exactly those two. The older mutant (`--mutant no_closure`) now fails exactly `header_scan recall_basic recall_incl recall_mixed`; I had predicted three, the fourth is measured.
- Repository oracle, local Linux, 15 files chosen with `random.seed(232)` (`src/relation.c`, 9 random sources, 5 random `include/*.h`), each touched and rebuilt, comparing the tests whose executable relinked with the selection (the 29 unmapped tests ignored for this hypothetical): 0 misses, 1502 selected, 1502 needed, 0 extra. The `relation.c` case is fixed (134 of 134). The scan found 0 unresolved quoted includes after `.inc` was added to the scanned extensions (one existed before, `fs_create_win.inc`). 12 basenames collide in the repository.
- Predictions that were wrong: that precision would drop through basename collisions (extra was 0 on the 15 files; I did not check whether any of them touches a colliding basename); that a new miss class would appear (none did; the classes I listed were not tested, they are not shown absent); the exact mutant set for `no_closure` (see above).

Limits.
- Macro-built includes (`#include MACRO`) are not matched by the scan and are not covered by the unresolved-include rule: a hole.
- Includes resolved through `-I` with the same basename in two directories are over-approximated, not tested. Generated headers rely on the unresolved-include rule, not tested on a real case.
- The oracle sees only relinked test executables, not tests that read data files.
- On this repository 29 of 199 tests are not mapped to a target (20 python, 7 bash, 2 `cmake -P`), so every selection here is still the full gate; a subset is selected only on fixtures. 362 of the 603 tracked `.c` and `.h` files are in no target.
- Windows is unmeasured. Nothing is connected to the agent; no `src/` change.

## Update: declared test inputs (m233, m234, m235)

Landed in `9c884ec` (m233, run 37423145841, all jobs read: apply 202, linux 202, msvc 226, asan-msvc 226, `ast-inspect-windows-ninja` 10 tests OK; the linux log shows `test_build_graph`, `_mc`, `_mc_scan`, `_mc_sidecar` and `_mc_decl` Passed) and `7913d20` (m235, run 37427902342, all jobs read: apply 202, linux 202 with the same five `build_graph` entries Passed, msvc 226, asan-msvc 226, `ast-inspect-windows-ninja` 10 tests OK).

Why. A test that runs a script or reads data files has no executable the codemodel can map, so the tool sent every change to the full gate. 29 of 199 tests were in that class at m232; the repository has 32 unmapped tests of 202 on the m235 tree, and the sidecar declares exactly those 32 (checked: the graph's `unknown` list is empty).

What it does. A JSON sidecar, `tools/build_graph_inputs.json`, declares inputs per unmapped test: `{"version": 1, "tests": {NAME: {"files": [globs], "targets": [names]}}}`. It applies only to tests the codemodel cannot map. A declared test is selected when a changed file (or a file that includes it, through the include scan) matches one of its globs, or when a declared target is in the affected set. Fail-closed: an unreadable sidecar, a wrong version, a malformed entry, an unknown test or target, an empty entry, or an unmapped test with no declaration each select the full gate. Until m236 a changed sidecar matched no glob and selected the full gate; since m236 three tests declare it as an input, so a change to it selects those 3 tests (see the m236 section).

Evidence (local, fixtures).
- Fixture `graph_script` oracle: 5 of 5 rows (touch a declared input, rebuild or rerun, compare). `fail_closed`: 6 of 6 cases.
- Mutants exit 0 only on the exact failing set: `sidecar_off` fails `script_subset`; `decl_targets_off` fails `recall_script`; `scan_off` fails `header_scan recall_incl`; `no_closure` fails `header_scan recall_basic recall_incl recall_mixed`. Re-run after m235 with the same sets.

How the repository declarations were made (m234). I wrote the declarations for the 32 unmapped tests by reading the test scripts, before looking at any trace. Then I ran each test under `strace -f` (open, execve and the stat family) and compared the source-tree files and built executables it touched with the declared globs and targets. The full run produced results for 28 tests (19 passed, 7 skipped, 2 failed); 3 of those 28 declarations were incomplete (a lower bound, not a measurement: the trace tool dropped relative paths, see the correction at the end): `test_bank_harness_setup` reads `tools/wording_rewrites.py`, `test_qemu_collection` reads `tests/test_qemu_closure.py`, `test_typed_c_verify_lock` reads `tests/typed_c_measured_lock.sbl1`. They are added. No undeclared target was seen. I had predicted at least 5 incomplete; the measure was 3 of 28, so that prediction was wrong.

What the comparison does not show.
- It is circular: the 3 additions come from the same trace used to check them. After the fix a re-trace of those 3 tests plus `test_build_graph_mc_sidecar` found nothing undeclared (this was measured with the tool that dropped relative paths, so it is void; see the correction at the end); that shows coverage of what the trace saw, not completeness.
- (Superseded by the correction at the end: those 9 were later traced.) 9 declarations were not trace-verified at m234: the 7 e2e bash tests were skipped locally (they use a hardcoded `build/symbols-server`, not the ctest build directory), and `test_typed_c_preview` and `test_typed_c_verify` failed locally because `build/c_contract_static` was absent. Those 9 are declared from reading the scripts only. `test_build_graph_mc`, `_mc_scan` and `_mc_decl` were not traced (`_mc_decl` timed out at 100 s in a separate run; the other two have no result line in the full run, cause not checked); they share the `test_build_graph` declaration, which was traced. `_mc_sidecar` was traced separately and found nothing undeclared.
- An open-only strace misses a Python source that is loaded from a cached `.pyc` (only a stat of the source happens), which is why the stat family is traced.
- Precision of the declared subsets is not measured against an oracle; only coverage of traced reads is.

Measured on this repository with the sidecar (local): `tests/test_i18n.c` selects 1 of 202 tests, `tools/wording_rewrites.py` 4, `tools/build_graph.py` 6, `src/relation.c` 145, `src/vsa.c` 144; `CMakeLists.txt` and `docs/build_graph.md` select the full 202, and so did `tools/build_graph_inputs.json` before m236. This is the first time a subset appears on the repository itself; it is a local measurement, not a CI run.

A failed dispatch (m234). The first version placed the sidecar at the repository root. `apply-patch.yml` refused it ("Disallowed path: build_graph_inputs.json", run 37426761333): its path allowlist does not include a new root file. Nothing landed. That is the guardrail working as designed, and the mistake was mine: I did not check the allowlist before building the diff. m235 moved the default location to `tools/` and the `graph_script` fixture sidecar to `tests/fixtures/graph_script/tools/`; a sidecar at the root is no longer read (a garbage file placed there produced no problem).

Limits.
- The sidecar is a hand-maintained declaration. A new input to a script test that nobody adds to it is not detected; the strace comparison is a one-time check, not a gate.
- Windows is unmeasured for these tests (they skip there). Nothing is connected to the agent; no `src/` change.
- The "ok" and mutant-kill lines are known from the exit code, not from stdout, which CTest hides.

## Update: repository guard (m236)

Landed in `cf62d9f` (run 37434542274, base 3d8cbb0, readback equal to the GO sha). The linux log shows `test_build_graph_repo`, `test_build_graph_repo_mc_drop` and `test_build_graph_repo_mc_glob` Passed, followed by the five earlier `build_graph` entries. All jobs read: apply 205, linux 205, msvc 229, asan-msvc 229, `ast-inspect-windows-ninja` 10 tests OK.

What it does. `tests/test_build_graph_repo.py BUILD` runs in the built tree and reads the real build graph of this repository with the real sidecar. It checks three things: `loads_clean` (the graph's `unknown` list is empty), `all_declared` (every test the codemodel cannot map has a declaration) and `globs_live` (every declared file glob matches at least one file of the source tree). Two mutants work on a copy of the sidecar and exit 0 only on the exact failing set: `drop_decl` fails `all_declared` and `loads_clean`; `stale_glob` fails `globs_live`. Windows prints SKIP.

Scope. An undeclared or renamed test only costs precision (the full gate), so this guard protects precision and catches dead globs. It cannot detect an incomplete glob list; that stays the hand-maintained part, and the strace comparison of m234 is a one-time check.

Two things found while building it.
- A File API query written by `CMakeLists.txt` during configure is read only by the next cmake run (cmake 3.22.1: no reply after the first configure, a reply after a second `cmake .`). My prediction that one configure would be enough was wrong. The test therefore writes the query and re-runs cmake on the build directory when the reply is missing; that rewrites the generated build files with the same content and it is a side effect on the build tree. On the CI run the reply was created this way (the three tests passed on a fresh configure and build; I read the Passed lines, not the test's stdout).
- The first version of that re-run failed 2 of 3 new tests under `ctest -j8`: two tests re-ran cmake at once and one read a reply file that was being replaced. The three tests now share `RESOURCE_LOCK build_graph_repo_reply`. Locally 3 runs with the reply deleted before each passed 6 of 6; CI runs ctest without `-j`, so CI does not exercise the lock.

Consequence. The three tests declare `tools/build_graph_inputs.json` as an input, so a change to the sidecar selects 3 of 205 tests and a change to `tools/build_graph.py` selects 9 (local measure). `CMakeLists.txt` still selects all 205.

Limits. Windows is unmeasured (SKIP). The linux job configures with the default generator (no `-G` in `ci.yml`); Ninja and multi-configuration generators are not covered by this test. Nothing is connected to the agent; no `src/` change.

## Correction: the m234 trace numbers (D11)

This section corrects the m234 numbers above, and the D9 and D10 text that repeated them. The error was mine.

The trace tool I used in m234 dropped every relative path. A test that opens `README.md` or `CMakeLists.txt` with a relative name was not seen. So "3 of 28 declarations incomplete" and "a re-trace found nothing undeclared" were lower bounds, not measurements. I found this when I ran the fixed tool (relative paths resolved against the source root) on all 32 tests.

Corrected history of the 32 repository declarations. 9 of 32 were incomplete when first written.
- 3 were fixed in m234: `wording_rewrites.py` (`test_bank_harness_setup`), `tests/test_qemu_closure.py` (`test_qemu_collection`) and `tests/typed_c_measured_lock.sbl1` (`test_typed_c_verify_lock`).
- 2 more read `CMakeLists.txt` and `README.md`: `test_engineering_bank` and `test_wording_rewrite_harness`. Fixed in m237 (declared as files).
- 3 more read the whole tree: `test_episodic_memory_e2e`, `test_git_inquiry_e2e` and `test_session_isolation_e2e` open about 1050 files, 604 of them undeclared. A file list cannot describe that honestly. Fixed in m238 with `always` (below).
- 1 is not a declaration gap: `test_opencode_subagent_e2e` showed only `observations.jsonl`. It is ignored by git (`.gitignore:67`) and written into the working directory, so it is an output, not an input. Nothing was added.
The 9 tests that were "not trace-verified" in m234 were traced after I built `build/symbols-server` and `build/c_contract_static`: they ran and passed. The targets seen were `symbols-server` for all 7 e2e tests and `c_contract_static` for `test_typed_c_preview` and `test_typed_c_verify`; no undeclared target.

Soundness correction for D9. Before m238, selecting for `tests/test_i18n.c` returned 1 test and missed the 3 e2e tests that read the whole tree. So the D9 subset numbers (for example 1 for `tests/test_i18n.c`) were measurements of what the selection returned, not evidence that the selection was sound. After m238, `tests/test_i18n.c` selects 4 and `tools/wording_rewrites.py` selects 7 (local measure on a full build); after m237, `README.md` selects a subset of 5 (the 2 bank tests and the 3 `always` tests) instead of the full gate, and `CMakeLists.txt` and `docs/build_graph.md` still select the full 206.

The `always` field (m238). A sidecar entry may carry `"always": true`. Such a test is selected in every subset selection and claims no inputs, so an unknown file still selects the full gate (it does not become "covered" by an always test). Fail-closed rules: `always` must be a boolean, a malformed value selects the full gate, and an entry with no files, no targets and no `always: true` declares nothing and is a problem. The test `test_build_graph_mc_always` and the `always_off` mutant pin this. m238 landed as `ce510c1` (run 37439642422, base e7c1a34, readback equal to the GO sha). All jobs read: apply 206, linux 206 (`test_build_graph_mc_always` Passed), msvc 230, asan-msvc 230, `ast-inspect-windows-ninja` 10 tests OK. m237 landed as `89a8abe` (run 37441980606, base ce510c1, readback equal to the GO sha).

Caveats on the trace.
- Relative paths are resolved against the source root. This over-approximates if a test changes directory elsewhere.
- The tool does not tell a stat from an open. An open-only strace of `test_episodic_memory_e2e` showed 543 `O_RDONLY` opens under `src`, `include` and `docs`, so the whole-tree read is real.
- Three tests showed "failed" in my local trace only because the build was partial: `test_episode_replay_contract` needs `symbols-agent`; `test_build_graph_repo` and `test_build_graph_repo_mc_glob` see unknown entries on a tree that is not fully built. They are not defects, and `test_build_graph_repo` passes on the full build.
- The re-check is circular: the declarations were derived from the same tool that now finds nothing undeclared. It shows coverage of what the trace saw. A read that no run triggers stays undeclared.
- The declarations are still hand-maintained and the one-time trace is not a gate. Non-claims unchanged: Windows is unmeasured for these tests (they skip there), and the local numbers above are not CI runs.

## Update: a read-only selection report in the agent (S2a, m239)

Landed in `4df0579` (run 37451442788, base 32d80a8, readback equal to the GO sha). All jobs read: apply 207, linux 207 (`test_select_report` Passed, 1.35 s, next to `test_build_graph_repo` Passed), msvc 231, asan-msvc 231, `ast-inspect-windows-ninja` 10 tests OK. On the two Windows jobs the new test prints SKIP; I did not measure the new C code on Windows and make no Windows claim.

What it does. `symbols-agent [-w WORKSPACE] --select-report BUILD --changed FILE...` prints one header line and then the exact output of `python3 tools/build_graph.py select BUILD --changed FILE...`, run in the workspace through the guarded shell (60 s timeout). It runs before the runner is created: it indexes nothing, patches nothing, runs no test and writes nothing. Nothing is skipped and the full gate stays the default; the report is information only. Every non-flag argument after `--changed` is a changed file.

Fail-closed. When the selection cannot be produced the report says `mode: full` and gives the reason, with exit 0: no changed file, a build or file path outside `[A-Za-z0-9_./+-]` (arguments reach a shell, so anything else is refused), `tools/build_graph.py` missing from the workspace, the script not running to a successful exit (a missing `python3` lands here), output that does not start with `{` and contain `"mode": "`, or truncated output. A build directory with no File API reply is handled by the script itself, which reports `full` with its own reason; the report relays it. A syntax error (`--select-report` without `--changed`, or combined with `-i`, a task or the continuation flags) exits 1 with one new stderr line. The help text gains three lines; the other existing modes keep their exit codes and stderr (measured on the binary before the change, and asserted by the test).

Test. `tests/test_select_report.py` (one ctest entry, `RESOURCE_LOCK build_graph_repo_reply`, SKIP on Windows):
- 5 changed-file sets (subset and full) whose report body must equal `build_graph.select` computed in the test process;
- 17 fail-closed input cases: no script, no `python3` (PATH pointing at an empty directory), no File API reply, no changed file, 6 paths outside the alphabet, a shell-injection attempt with a marker file that must not appear, 4 fake scripts (not JSON, `{ }`, a mode token that is not JSON, exit 3) and 2 bad build directories (`bad dir;x`, `--help`);
- the exit code and stderr of the existing modes.
It is declared in the sidecar with the three files it reads, the `symbols-agent` target and `*.c`, `*.h`, `*.inc`: my first trace of it showed 609 undeclared source files, because the include scan reads every C and header file; after the declaration a re-trace shows none. That check is circular, as before. So a change to any C or header file selects this test (it selects 4 of 207 for `tests/test_select_report.py`, 146 for `src/relation.c`, 5 for `README.md`, local measures).

Mutants, local only. I did not add a test hook to `src/`: a read-only report mode does not justify test-only code there. Instead I ran 10 source mutants of `agent_cli_main.c` against the test by hand: the alphabet allowing `;`, the script check removed, a nonzero exit relayed, an empty list not refused, the mode check removed, one changed file dropped, the JSON-start check removed, the header changed, the header dropped, a leading `-` allowed. Three survived the first versions of the test and were found only by running them: the mode check (the fake scripts did not exist yet), the JSON-start check, and the leading-`-` check. I added cases for them and all ten are now killed. One header mutant crashed the test with a traceback instead of failing a check; I made the parse tolerant. These runs are not part of CI and are not reproducible from the repository; the 17 input cases are what CI keeps.

Local runs. A full local `ctest` of 207 tests gave 21 failures in a first `-j6` run: 19 fs fuzz, journal, race and git-gate tests pass when rerun without `-j` (they race under `-j`; CI runs without it), and `test_typed_c_verify` and `test_typed_c_preview` need `build/c_contract_static` in the source tree's `build/` directory and pass once it is there. This is a local measurement; the CI numbers are above. My trace tool had the source root hardcoded; I made it read `SRC` from the environment and saw that a trace on a full build lists the same 155 test executables as undeclared targets for two different tests (`test_select_report` and `test_build_graph_repo`); I read that as the `ctest` wrapper touching them, not as test inputs, but I did not isolate the cause.

Limits. The report is as complete as the sidecar and the codemodel: a changed file nobody declared is not selected. Windows is unmeasured. Nothing consumes the report yet (S2b would call it from the runner, after this is read in use); no test is skipped anywhere.

## Update: an opt-in selection report in the TaskOps solver (S2b)

Landed in `e316608` (run 37460501856, base fc2f84f, readback `git diff --binary` equal to the GO sha). All jobs read: apply 208, linux 208, msvc 232, asan-msvc 232, `ast-inspect-windows-ninja` 10 tests OK. I read the pass totals on the Windows jobs and not a skip line; the new test returns SKIP there by design, so the new C code is unmeasured on Windows.

Where the hook is. The CLI task path is `TaskOpsSolve` in `src/task_ops.c`, not the runner loop in `src/agent_runner.c`. I measured this with a temporary print (reverted): a CLI rename task resolved in `TaskOpsSolve` and never reached `AgentRunnerSolveTask`, which fails first for a task without a hunk. So the hook is in `TaskOpsSolve`; a hook in the runner loop would only be reached by the harness.

What it does. With `SYMBOLS_SELECT_REPORT=<build dir>` set (unset or `0` means nothing runs and no path is recorded), a verified solve prints one report to stderr: a header line, then the S2a report for the files the solve wrote. Stdout and the edited files are unchanged. The report is information only: the build and test commands are untouched and nothing is skipped. `src/select_report.c` holds the logic that S2a had in `agent_cli_main.c`; `--select-report` now calls it (60 s timeout) and its output is unchanged (`test_select_report` passes unmodified). The hook uses a 30 s timeout, at most once per solve.

Which files. TaskOps does not report its changed files (`TASK_OPS_REPORT` has a count only) and `TaskOpsLoadWorkspace` is capped at 64 text files, depth 4. So the hook records the relative paths at the three places the generic act step writes (`next[i]`, `created`, `created2`) and clears the record before every attempt. The two workspace loads I had planned to measure are therefore not needed, and I have no number for them. The cost is one subprocess: `python3 tools/build_graph.py select` took 0.13-0.16 s and `symbols-agent --select-report` 0.13-0.22 s on this repository (5 runs each, local, not CI).

Fail-closed. A missing script, a non-zero exit, unrecognised output, a path outside `[A-Za-z0-9_./+-]`, more than 64 files, or no recorded files each print one `mode: full` line with a reason. The solve result and the edit do not change.

Test. `tests/test_select_hook.py` (one ctest entry, SKIP on Windows) runs the real `symbols-agent` on a temporary workspace with a fake `tools/build_graph.py` that prints its arguments. It checks: gate unset and gate `0` print nothing on stderr; gate on, a verified rename of `a.c` and `b.c` gives exactly one report naming those two files and the build directory; stdout (time and task-id lines removed) and the edited files are identical with the gate on and off; four failure cases (missing script, exit 3, unsafe build directory, garbage output) give one `mode: full` line and identical stdout, exit code and files; a task that changes nothing prints no report. It is declared in the sidecar with the `symbols-agent` target and `*.c`, `*.h`, `*.inc`; I did not run the strace trace on it.

Mutants, local only (not CI). Six source mutants, each against the test: no note at the file write site, gate `0` treated as on, report to stdout, silent failure (no full line) are killed. Two survived:
- `if (v && rep->verified)` against `if (v)`: equivalent. `v` is the verified flag, so I used `if (v)`.
- Removing `SelectReportNoteReset()` survives. It needs a first attempt that writes and is refuted, then a second that verifies. I tried several tasks and could not get the operator to run on them, so the refuted-attempt path and the reset are untested. The reset is one line; its effect (a refuted attempt's files leaking into the next report) is not shown absent by any test.

Limits.
- Outside this repository there is no `tools/build_graph.py`, so real use there reports `mode: full` with "not found". Real-usage data will mostly say that.
- Only the generic act step records files. Other operators write files elsewhere in `task_ops.c` (I read those sites; I did not run them) and give `mode: full`, "not recorded", even when verified.
- The test covers the rename operator only. The "not recorded" path is checked by a mutant (removing the note call), not by a test of another operator.
- My R5 prediction (that this path is reached only by non-rename operators, and checked by mutant only) was framed too narrowly; the test shows only the mutant check.
- The report is as complete as the sidecar and the codemodel (see S2a). Windows is unmeasured. Nothing skips a test.
- Correction to the S2a section: its last sentence says S2b would call the report from the runner. The hook is in `TaskOpsSolve` instead, for the reason above. The runner loop is not hooked.

## Update: CTest result classes, and the first Clang and sanitizer runs (S3, S4, Y1, Y2)

This reads M3 exit criteria 3 and 4 against what is landed. Criterion 5 (benchmarks) is untouched.

### S3: `tools/ctest_report.py` (criterion 4)

Landed in `5922344` (run 37468916459, base 552590e, readback equal to the GO sha). All jobs read: apply 209, linux 209, msvc 233, asan-msvc 233, `ast-inspect-windows-ninja` 10 tests OK. On the two Windows jobs the new test prints SKIP, so it is unmeasured there.

Why not JUnit. On CTest 3.22.1 (a hand fixture, then the repository fixture) `ctest --output-junit` writes a timeout, an abort and a segfault all as `<failure message="">`, so JUnit cannot tell them apart. The Test.xml that `ctest -T Test` writes can: it holds the reason in the "Exit Code" and "Completion Status" measurements.

What it reads. `ctest_report.py TEST_XML [--registered BUILD]` prints JSON with one record per test (name, class, detail, exit code, exit value, seconds) and a count per class: passed, failed, timed_out, crashed, skipped, not_run, unknown. The reason strings it classifies are those read from CTest 3.22.1:
- failed: "Failed". timed_out: "Timeout".
- crashed: SEGFAULT, "Subprocess aborted", NUMERICAL, ILLEGAL, "Subprocess killed", "Subprocess terminated".
- skipped: "SKIP_RETURN_CODE=77", "SKIP_REGULAR_EXPRESSION_MATCHED". not_run: "Unable to find executable", "Disabled".
- passed needs Status passed, Completion Status "Completed" and no Exit Code or "Completed" (a WILL_FAIL test is passed, with Exit Value 1).
Any other string, a missing field or an unknown Status gives `unknown`, never `passed`. A malformed, missing or empty file exits 2 with no report. With `--registered`, a test that the build registers and Test.xml lacks (after `ctest -R`) is `not_run`, "absent from Test.xml".

Test. `tests/test_ctest_report.py` (one ctest entry, SKIP on Windows) builds `tests/fixtures/ctest_report` (17 tests, one reason each), runs `ctest -T Test`, and checks: each test lands in its designed class (passed: pass, will_fail; failed: fail, fail_regex, regex_fail, exit77_noskip; timed_out: timeout; crashed: segv, abort, fpe, ill, kill9, term; skipped: skip, skip_regex; not_run: notrun, notrun_disabled); no phantom tests from the `<TestList>` entries; the counts add up; five edited copies of the real Test.xml (unknown Exit Code, unknown Status, a bad Completion Status on a passed, a notrun and a failed test) give `unknown`; three bad inputs exit 2 with no output; the `-R` absent case. Ten mutants of the tool (text replacements in a temp copy, inside the same test) must each fail exactly their expected set of checks. Those mutants are part of the test and run in CI. The test takes 6.6 s under ctest locally.

Wrong predictions, named. I predicted the first run of the mutants would show a wrong expected set, and it did: for `segv_unlisted` and `skip_regex_unlisted` I expected one failed check, but `counts_add_up` also fails (it requires 0 unknown). That was my expectation, not a tool defect; the expected sets were corrected and all 10 mutants die on their exact sets. My first parser also counted the 17 bare `<Test>name</Test>` entries of `<TestList>` as unknown tests; skipping childless entries and a phantom-tests check fixed that. I guessed the test would take 3-5 s; it takes 6.6 s.

Limits. Linux only. Windows reason strings are unmeasured and would land in `unknown`. Only CTest 3.22.1 was read locally; the CTest versions on the CI runners were not read (the compilers run below printed cmake 3.31.6). "Subprocess killed" and "terminated" (SIGKILL, SIGTERM) are classed crashed, but an external kill (an out-of-memory kill, a CI time limit) looks the same. Reasons not in the fixture land in `unknown`. Nothing in CI runs the tool on the real suite's results: this is a classifier and a fixture. So criterion 4 is shown on Linux on a labeled fixture, not "CI test reports distinguish the six states".

### Y1, S4, Y2: a manual Clang and sanitizer workflow (criterion 3)

Criterion 3 asks for GCC, Clang and MSVC builds in CI and sanitizer jobs with zero findings. Before this, `ci.yml` had gcc and MSVC builds and an MSVC ASan job; its only clang was the pinned libclang for the AST corpus. There was no Clang compile of the C core and no Linux sanitizer job.

Y1, `a3c0a8e` (apply-github-patch run 37471326454): one new file, `.github/workflows/compilers.yml`, `workflow_dispatch` only, with a Clang job and a gcc ASan and UBSan job. It is manual so that a red result would not touch the CI gate. It went through the bootstrap `apply-github-patch.yml`; the readback `git diff --binary` equal to the GO sha; the push needed the `WORKFLOW_PAT` secret and it went through.

First dispatch (run 37473220913, head a3c0a8e): both jobs red.
- Clang 18.1.3: the build stopped at `tests/test_task_ops_refactor_guard.c:83`, an implicit declaration of `TaskOpsTestRefactorGuards`, an error in clang 18 and a warning in gcc. Cause: `include/task_ops.h` declares the test hooks only under `TASK_OPS_TEST_FAULTS`, and `CMakeLists.txt` built `test_task_ops_refactor_guard_mc` with only `REFACTOR_GUARD_MUTANT=1`. A latent defect in a test target, not in `src/`. ctest never ran.
- gcc ASan and UBSan: 207 of 209 passed, 2 failed, 7 skipped. This was my job configuration: I built into `build-san`, while the e2e scripts look for `build/symbols-server` (they exit 77 when it is missing) and the typed_c tests need `build/c_contract_static` (ValueError generator_unavailable). The e2e tests that run the server under ASan did not run at all. No sanitizer line was in the log for the tests that ran.
- Wrong predictions, named: I wrote the job without checking the build directory the tests expect, and I assumed gcc and clang would not differ.

S4, `9a0acab` (run 37476377141, readback equal to the GO sha): one line in `CMakeLists.txt`, `TASK_OPS_TEST_FAULTS` added to the compile definitions of `test_task_ops_refactor_guard_mc`. All jobs read: apply 209, linux 209, msvc 233, asan-msvc 233, ninja 10 OK. A gcc build of the unmodified tree with `-Werror=implicit-function-declaration` and `make -k` (412 objects) found only that one site.

Y2, `3ccbe78` (apply-github-patch run 37478476302, readback equal to the GO sha): the sanitizer job builds into `build/`.

Second dispatch (run 37479988705, head 3ccbe78, status Success), logs read:
- Clang 18.1.3, cmake 3.31.6, Release: `100% tests passed, 0 tests failed out of 209`, test time 186.99 s, 0 skipped.
- gcc 13.3.0, cmake 3.31.6, `-fsanitize=address,undefined -fno-sanitize-recover=undefined`, ASAN `detect_leaks=1`, UBSAN `halt_on_error=1`: `100% tests passed, 0 tests failed out of 209`, test time 564.82 s, 0 skipped. I found no AddressSanitizer, LeakSanitizer, `runtime error` or `SUMMARY:` line in the log. A clean run prints none, so "zero findings" rests on the pass total (a finding would abort a test) plus that empty search, not on sanitizer output.

What this shows, and what it does not. Criterion 3 is shown on one manual run at `3ccbe78`: Clang Release 209/209 and GCC with ASan and UBSan 209/209, zero skips. It is not in the gate: `ci.yml` still has no Clang job and no Linux sanitizer job, so nothing re-checks it on later pushes. That is one run on one runner image, with no Clang sanitizer and no Windows clang; Clang ran without `-Werror`, and I did not read its build warnings. Folding the jobs into `ci.yml` (or a nightly `schedule:` trigger) is left to Antonio. The runs of `ci.yml` that the Y1, S4 and Y2 pushes may have started were not all read by me beyond S4's.
