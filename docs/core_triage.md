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

## Phase E result: hardening

New test `tests/test_core_fuzz.c`: seeded xorshift fuzzing of the three new parsers (20,000 iterations per stage in ctest; `SYMBOLS_FUZZ_ITERS` raises it). Invariants: a destructive command stays destructive under every wrapper and under nested wrappers up to depth 4 (`&&`, `;`, `||`, `$( )`, backticks, subshells, braces, env prefixes, pipes, background); destructive or unparseable always carries a reason; every shell rule's output passes that rule's own intent check; every build plan is well formed. Run under ASan and UBSan with `-fno-sanitize-recover=all` at 200,000 iterations per stage: clean after the fixes below.

Bugs the fuzzer found, all fixed with regression cases:
- `shell_ops`: a trailing lone `$` read past the end of the buffer (heap overflow under ASan; `strchr` matches the terminator).
- `shell_ops`: `${...}` containing quotes was treated as an expansion, so quoting produced text that failed its own check.
- `command_policy`: `$((cmd))` was always treated as arithmetic, so `echo $((git clean -fdx))` and nested forms lost the destructive class. Now the arithmetic body is also classified and counts when it reads as destructive or unparseable; `$( (cmd) )` is handled as a command substitution by paren matching.

Timeouts: every command the new code runs is bounded (cmake configure 60 s, build 120 s, build scripts 30 s, `make -n` 20 s, `sh -n` 10 s); a timeout counts as not verified and the edit is rolled back.

Bank unchanged: 37/56, 0 wrong edits. ctest 105/105.

## Next round: remaining dev failures in shell, debug, refactor

Developed only against dev tasks (odd numbers). New rule `stated_output` in shell_ops ("print exactly TOKEN" with one echo line), and new module `src/c_fix_ops.c` (operator `c_fix`): `loop_bound` (off-by-one in the one simple for-loop condition), `array_fit` (char array smaller than its string initializer, resized to strlen + 1; escapes abstain), `goto_return` (a single forward `if (C) goto L;` whose label only returns becomes a block with that return). c_fix edits are kept only when the program builds and its own exit code is 0 afterwards (and, for goto_return, no goto remains); every rule abstains on two candidates. Fuzzed in test_core_fuzz (ASan/UBSan clean at 200k).

Bank: 37/56 -> 41/56, 0 wrong edits. Dev 25/32 -> 29/32 (sh_001, dbg_001, dbg_007, rf_007). Heldout unchanged at 12/24: none of the new rules fired on sh_002, sh_006, or the heldout debug/refactor tasks, which is the expected result for dev-only development. Still failing on dev: cr_005, dc_005, mf_005.

## Last dev failures: cr_005, dc_005, mf_005

Three more c_fix rules, still developed only against dev tasks:
- `declare_local`: the task says an identifier (with `_` or a digit) is used but never declared. If exactly one such name is used in the source and is never declared, called or defined, and every use sits in one function, `int X = 0;` opens that function.
- `comment_fix`: the task quotes new wording `'Q'` and repeats the stale wording W (3+ words). W must appear in exactly one comment and nowhere in code, and then W becomes Q. The rule abstains with several quotes, several matching comments, or W also appearing in code.
- `split_function`: "split F() out ... into S.c with a prototype in S.h". The one top-level definition of F moves to S.c (a `static` is dropped), S.h gets a guarded prototype, and the source includes S.h. Both files must be new; the linked program must build and exit 0 with both files present, otherwise everything is rolled back, including the created files.

Bank: 41/56 -> 44/56, 0 wrong edits. Dev 32/32, heldout unchanged at 12/24 (sh_002 and sh_006 stay as the generalization measure, by decision). Unit tests and fuzz stage 6 cover the new rules (ASan/UBSan clean at 200k).

## Safety fix: required new files (blind wrong edit)

Mimo's blind re-run on ecd9560 had 1 wrong edit: multi_file, where the agent kept an edit and reported success while a new file the task required was missing. The verify step already required every named file to exist afterwards, but it had two gaps:
- One-letter file names (`a.h`, `x.txt`) were not recognized as file names at all. The two-letter minimum exists to skip "e.g."/"i.e.". A one-letter stem now counts when the extension is a known one (c, h, cc, cpp, hpp, py, sh, md, txt, yml, json, mk).
- A named file that did not exist before could be satisfied by a same-named file in a subdirectory. A new file now has to exist at exactly the named path. Files that already existed keep the subdirectory match.

This is a verification change only: it can turn a kept edit into a rollback, never the other way. The new test in test_task_ops fails on the old code and passes on the new. Bank unchanged at 44/56 with 0 wrong edits; ctest 106/106. This was not tuned on the blind batch. Only Mimo's category/cause line was used.

## Bank harness: per-task setup for git tasks

Git tasks need a repository with history, which a plain before/ tree cannot hold. tools/bank_harness.py now runs an optional `setup.py` in the workdir before the snapshot and the agent, with a fixed git identity and fixed dates. In self-test mode it also runs an optional `golden.py` for golden states that are git operations. The snapshot ignores .git/ internals and records HEAD, refs and status as one `@git` entry. A failing setup is a harness error, not a pass or a wrong edit. Covered by tests/test_bank_harness_setup.py (ctest). The fixtures themselves come from Mimo.

## Evidence-driven C repair (`evidence_fix`)

A change of approach, approved by Antonio: the edit is chosen by what the program itself does, not by matching the task's wording. New operator `evidence_fix` in `src/task_ops.c`, candidates from `CFixCandidates` in `src/c_fix_ops.c`. It runs only when the C program builds.

- Run fails (non-zero exit, not a timeout): every single candidate edit is written, built and run. Tiers are tried in order: tier 1 boundary (`<` <-> `<=`, `>` <-> `>=` in a condition or return), tier 2 `array_fit` (char array too small for its literal -> strlen + 1) and `init_local` (`TYPE x;` whose first use updates it -> `= 0`, or `= 1` for `*=`/`/=`), tier 3 direction (`<` <-> `>`) and equality (`==` <-> `!=`). The first tier with exactly one candidate that makes the program exit 0 wins; several winners in that tier means abstain. Test files are never edited.
- Run already passes: only tier-2 candidates on a line where `gcc -Wall -Wextra -O1` warns ("too long", "uninitialized"), and the warning must disappear while the exit stays 0.
- Comments and string literals are never candidates.
- main() is the oracle: when the program defines other functions, main is never edited; when main is the only function, its return lines are never edited. `evidence_fix` always runs after every wording-anchored operator, and never after a relop abstention on a demoted induced pattern.

Measurement. Public bank with the real task text: 44/56, 0 wrong edits (unchanged). Wording probe (`tools/bank_harness.py --task-text "The program fails; fix the bug so it exits 0."`, every task gets the same neutral text): 9/56 -> 13/56; debug 0/7 -> 4/7 (dbg_001, 003, 006, 007). The one wrong edit in the probe (eb_mf_003, `declare_implicit` puts the prototype in main.c) exists before this change and is not from `evidence_fix`. dbg_004 needs two edits (comparison and main's return), so it is out of reach of a single-edit search; dbg_002 (null guard) and dbg_005 (no warning at -O1) stay open. Progress is measured on Mimo's blind batch, counts only.

Tests: `test_task_ops` adds a neutral-wording fix (main untouched) and a two-winner abstention; `test_c_fix_ops` covers every candidate kind, the max limit, comments/strings, and a deterministic 3000-iteration mutation fuzz (clean under ASan/UBSan).

## More evidence sources: cmake output and `sh -n`

Same approach as `evidence_fix`: the tool's own output picks the edit, the task wording is not read, and any ambiguity means abstain.

- cmake (`build_repair`, evidence mode, `BuildOpsPlanEvidence` in `src/build_ops.c`): when no wording rule applies and the workspace has a top-level CMakeLists.txt, cmake configures it in a scratch dir. Its output selects the existing rule: "No project() command is present" -> `cmake_project` (target name, or `app`); "No SOURCES given to target" -> `cmake_add_source` (only when the workspace has exactly one C file); "Cannot find source file" naming the one missing source -> `cmake_missing_source` (only when no C file defines main). The edit is verified exactly like the wording rules: a real configure and build.
- `sh -n` (`shell_harden`, syntax mode, `ShellSyntaxCandidates` in `src/shell_ops.c`): for a script the shell parser rejects, candidates are tier 1 `missing_then` / `missing_do`, `close_quote` (odd double quotes on a plain line), `close_block` (the one open if/for/while/case closed where indentation returns to the opener's level, only when the body is indented), and tier 2 `stray_closer` (a lone fi/done/esac removed). The one lowest-tier candidate after which `sh -n` passes is kept; several means abstain. A flat, unindented body leaves the closer's place open, so it abstains.

Measurement: public bank with the real task text still 44/56, 0 wrong edits. Neutral-wording probe 13/56 -> 16/56 (build_ci 0/7 -> 3/7: bi_001, bi_004, bi_006). No public shell task is a syntax error, so `sh -n` does not move the public bank; unit tests, a 3000-case fuzz, and end-to-end tests in `test_task_ops` cover it. Real progress is measured on Mimo's blind batch, counts only.

## Compiler-error evidence: one undeclared name

When the program does not compile and gcc reports exactly one identifier as "undeclared (first use in this function)", in one file, with no "did you mean" hint anywhere, `evidence_fix` applies the existing `declare_local` rule to that name (`int NAME = 0;` opens the one function that uses it; the name must hold `_` or a digit, must never be called, declared or #defined). The edit is kept only if the program then builds and exits 0. A typo gcc can name a fix for, two undeclared names, or uses in two functions all abstain. gcc quotes with U+2018/U+2019 under UTF-8 locales (Python's subprocess coerces LC_CTYPE to C.UTF-8), so both quote styles are read.

Measurement: public bank still 44/56, 0 wrong edits. Neutral-wording probe 16/56 -> 17/56 (compiler_repair 6/7 -> 7/7, cr_005).

## Safety: declare_implicit never hides a missing header

The neutral-wording probe had one wrong edit, eb_mf_003: `add()` is defined in util.c, main.c has no include, and `declare_implicit` put a local prototype in main.c. It compiled, but the declaration belongs in a header. Now, when the definition lives in another source file and no header can take the prototype (none included, several candidates, or not editable), `declare_implicit` abstains instead of writing a local prototype. A function defined later in the same file still gets a local prototype, and the header paths (the one header the caller includes, the one the defining file includes, or a header the task names) are unchanged.

Measurement: public bank 44/56, 0 wrong edits (unchanged). Neutral-wording probe 17/56 with wrong edits 1 -> 0.

## Safety: evidence_fix only when the bare run is the whole criterion

Mimo's counts-only blind re-runs showed evidence_fix adding a debug wrong edit (754e071: wrong_edits 1 -> 2; b29e959: 1, debug). Without looking at the blind task, the operator was made conservative in general:

- Bug found: main() protection missed one-line mains (`int main(void) { return f(3) == 6 ? 0 : 1; }`). The head test rejected any line holding `;`, so the check in such a main could be flipped to make the program exit 0. A definition is now recognized when its `{` comes before the first `;` (same fix in the one-main count).
- The bare run must be the only criterion in sight, otherwise abstain: no argv/argc, stdin reads (scanf, getchar, stdin, getline, read(0)) or getenv; no test sources, tests/ dir, Makefile, CMakeLists.txt, shell or Python scripts; exactly one main.
- The kept edit must be the only candidate in any tier that makes the program exit 0 (no longer the lowest tier with one winner), and the program's stdout must be unchanged by it.
- A passing run is never edited: the gcc-warning path is removed (a warning alone is not evidence of the task).

Measurement: public bank 44/59 (the same 44; the 3 new git tasks are untouched), 0 wrong edits. Neutral-wording probe 17/56 -> 16/56, 0 wrong edits (dbg_007 needed the removed warning path). New tests in test_task_ops: one-line main never edited, argv program abstains, passing program with only a warning untouched.

## Git operators (repository state, verified with git)

`src/git_ops.c` runs before every file operator when the workspace is the top
of a git work tree. Git reports the state; the task text only has to agree.

| operator | git evidence | task must | action | verified by |
|---|---|---|---|---|
| resolve_merge | `MERGE_HEAD` exists, conflicted paths from `git diff --diff-filter=U` | say to keep both sides | each hunk becomes ours then theirs, `git add`, `git commit --no-edit` | `MERGE_HEAD` gone, HEAD has two parents, clean status, no markers in HEAD's blobs |
| restore_deleted | HEAD deleted the file (`git diff --diff-filter=D HEAD~1 HEAD`) and it is absent | name the file and ask to restore it | `git checkout HEAD~1 -- FILE` | bytes equal `git show HEAD~1:FILE`, `git ls-files --error-unmatch` |
| revert_head | clean tree, single-parent HEAD, a changed C file fails `gcc -fsyntax-only` at HEAD while every changed C file compiled at HEAD~1 | ask to undo or revert a commit | `git revert --no-edit HEAD` (history kept, never `reset`) | HEAD tree equals old HEAD~1 tree, sources compile |

A failed verification restores the prior state (conflict re-created with
`git checkout -m`, restored file unstaged and removed, revert undone). A merge
without "keep both" wording, a deleted file the task does not name, or a
revert without build evidence is an abstention: nothing is edited and the
file operators do not run on that repository state either.

## Run-based evidence_fix is opt-in

Mimo's blind re-measure on 817a947 still had one debug wrong edit: the run-based evidence search kept a boundary rewrite that made the bare program exit 0, and the hidden check rejected it. An exit code alone cannot tell the intended fix from another edit that also passes, so the run-based search is now off by default and runs only with `SYMBOLS_EVIDENCE_RUN=1`. The compiler-evidence path (`undeclared_local`, chosen by gcc's own "undeclared" error) is unchanged. The search comes back on by default only with an oracle stronger than the bare run.

Measurement: public bank 47/59, 0 wrong edits (no public pass used the run search). Neutral-wording probe 16/59 -> 13/59, 0 wrong edits (eb_dbg_001, 003, 006 needed it). New test: failing program that one boundary edit would fix is left untouched by default; the existing evidence tests run with the switch on.
