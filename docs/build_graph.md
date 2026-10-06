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

How the repository declarations were made (m234). I wrote the declarations for the 32 unmapped tests by reading the test scripts, before looking at any trace. Then I ran each test under `strace -f` (open, execve and the stat family) and compared the source-tree files and built executables it touched with the declared globs and targets. The full run produced results for 28 tests (19 passed, 7 skipped, 2 failed); 3 of those 28 declarations were incomplete: `test_bank_harness_setup` reads `tools/wording_rewrites.py`, `test_qemu_collection` reads `tests/test_qemu_closure.py`, `test_typed_c_verify_lock` reads `tests/typed_c_measured_lock.sbl1`. They are added. No undeclared target was seen. I had predicted at least 5 incomplete; the measure was 3 of 28, so that prediction was wrong.

What the comparison does not show.
- It is circular: the 3 additions come from the same trace used to check them. After the fix a re-trace of those 3 tests plus `test_build_graph_mc_sidecar` found nothing undeclared; that shows coverage of what the trace saw, not completeness.
- 9 declarations are not trace-verified: the 7 e2e bash tests were skipped locally (they use a hardcoded `build/symbols-server`, not the ctest build directory), and `test_typed_c_preview` and `test_typed_c_verify` failed locally because `build/c_contract_static` was absent. Those 9 are declared from reading the scripts only. `test_build_graph_mc`, `_mc_scan` and `_mc_decl` were not traced (`_mc_decl` timed out at 100 s in a separate run; the other two have no result line in the full run, cause not checked); they share the `test_build_graph` declaration, which was traced. `_mc_sidecar` was traced separately and found nothing undeclared.
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
