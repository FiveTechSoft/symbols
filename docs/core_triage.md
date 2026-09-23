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

## Phase C result: build and CI files

New module `src/build_ops.c` (`include/build_ops.h`), wired into task_ops as operator `build_repair`. General rules, each only when the task states it and the precondition holds, abstaining on any ambiguity (two candidate sources, two steps lists, a target already with prerequisites, a script that already compiles): `cmake_add_source`, `cmake_project`, `cmake_missing_source`, `make_dep`, `build_compile_step`, `ci_ctest`. Verification really builds: cmake configure (+ build) out of tree with no "No project()" warning; `make -n all` must show the source; a build script runs in a temp copy and must pass on the real source and fail on a broken one; `ci_ctest` is checked statically.

Bank: 31/56 -> 38/56, 0 wrong edits, 0 agent errors (dev 22/32 -> 26/32, heldout 9/24 -> 12/24). build_ci 0/7 -> 7/7. Caveat: all seven build_ci task texts, heldout included, were read before the rules were written, so 7/7 is development evidence, not held-out evidence; the blind batch (Mimo) is the independent test.

Note: Mimo's d61b64b turned eb_dbg_001 into a behavior check; the agent now abstains on it (no wrong edit), so the combined bank on top of that commit is 37/56.

## Phase D result: command safety policy (shell and git)

New module `src/command_policy.c` (`include/command_policy.h`): every command line is split into simple commands (on `;`, `&`, `|`, `&&`, `||`, newlines, subshells, and inside `$( )` and backticks), tokenized with POSIX quoting and escapes, and classified; the worst class wins. Classes: read, write, destructive, unparseable. Destructive covers git history and ref loss (force push in every form, `+`/`:` refspecs, `--delete`, `--mirror`, `reset --hard`, `clean -f`, `checkout --`/`.`, `restore` of the worktree, `branch -D`, `tag -d`, `stash drop/clear`, `commit --amend`, `rebase`, `filter-branch`, `reflog expire`, `gc --prune`, `update-ref -d`, `config --global`) and shell damage (`rm -r`, `rm` outside the workspace or with a glob, `sudo`, piping into a shell, `dd`, `mkfs`, `kill`, `chmod -R`, `find -delete/-exec`, writes or redirects outside the workspace). Unbalanced quotes and `eval` are unparseable. Unknown programs are write, never read.

Enforcement: `AgentShellExecGuarded` refuses destructive and unparseable commands without running them (exit 126, reason in stderr); the only grant is `SYMBOLS_ALLOW_DESTRUCTIVE=1`. The agent runner now runs every task-supplied build and test command through it. Internal fixed commands (compiler probes, cmake, `sh -n`) are unchanged.

Evidence: tests/test_command_policy.c, 82 labeled commands (read 14, write 16, destructive 48, unparseable 4) including obfuscations (`r\m -rf`, `'rm' -rf`, `git push -"f"`, `true || rm -rf /`, `$(git reset --hard)`), all classified correctly, plus a guarded-execution test that proves a refused command leaves no side effect. Bank git tasks are still to come: the harness needs repository fixtures, which is a scripts/ change.
