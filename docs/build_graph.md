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
