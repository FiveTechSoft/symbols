# Core triage: shell, git, and C (phase A)

Baseline: master 818935c, `python3 tools/bank_harness.py`, 27/56 passed, 0 wrong edits, 29 untouched, 0 agent errors (dev 19/32, heldout 8/24; odd task number = dev, even = heldout).

Every one of the 29 failures ends the same way: `No edit kept: no operator preconditions hold`. The agent abstains; it never writes a wrong edit. The cause differs by area:

| Area | Pass | Failing tasks | Cause |
|---|---|---|---|
| shell | 0/7 | sh_001..007 | Perception gap: `.sh` files are not indexed (0 files), and there are no shell operators (shebang, `set -e`, quoting, exit-code guard, file guard, redirection, output literal). |
| build_ci | 0/7 | bi_001..007 | Perception gap: CMakeLists.txt, Makefile, build.sh and ci.yml are not indexed; only main.c is seen. No build-file operators and no verifier that runs cmake/make. |
| docs | 3/7 | dc_002, 004, 005, 006 | Mostly non-C files not indexed (0 files on 3 of 4). |
| debug | 4/7 | dbg_002, 004, 007 | C seen, no operator matches the fault. |
| refactor | 4/7 | rf_004, 006, 007 | C seen, no operator matches. |
| test_authoring | 5/7 | ta_002, 006 | C seen, no operator matches. |
| multi_file | 5/7 | mf_005, 006 | C seen, no operator matches. |
| compiler_repair | 6/7 | cr_005 | C seen, no operator matches. |

Conclusion: shell and build_ci fail on perception before any operator runs. Phase B adds shell perception, shell operators, and a verifier (`sh -n` plus a probe of the stated intent); phase C does the same for CMake, Make, and CI build scripts, verified by really running the build. Development uses only dev tasks; heldout tasks are reported, never tuned on.

## Phase B result: shell hardening

New module `src/shell_ops.c` (`include/shell_ops.h`), wired into task_ops as operator `shell_harden`. General rules, each chosen only when the task states it and its precondition holds in the script: `shebang`, `fail_closed` (set -e), `quote_vars` (double-quote bare parameter expansions outside comments and quotes), `file_guard` (guard a run of a named file with `if [ -f F ]`, exit code taken from the task, abstaining on none or conflicting codes). Verification: the rule's static intent holds on the written file and `sh -n` accepts it; no shell available means not verified, so the edit is rolled back. Scripts are never executed by the agent.

Bank: 27/56 -> 31/56, 0 wrong edits, 0 agent errors (dev 19/32 -> 22/32, heldout 8/24 -> 9/24). Shell 0/7 -> 4/7: sh_003, sh_005, sh_007 (dev), sh_004 (heldout). Still failing: sh_001 (stated output literal), sh_002 (argument-dependent exit code), sh_006 (redirection to a file); the agent abstains on all three. Caveat: heldout task texts were read during triage, so the bank heldout split is weaker evidence than Mimo's blind batch.
