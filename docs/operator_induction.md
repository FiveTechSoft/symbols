# Operator induction: learning repair operators from verified repairs

Status: design accepted (2026-09-23), build started. It is measured against Mimo's new blind bank batch.

## Why

Today every task-ops operator is hand-written. That worked on the tasks it was written from and failed to transfer: over five commits, dev went from 9/32 to 19/32 while the then-blind held-out stayed at 5/24. The only operators that fired on held-out were the ones driven by observed state (compiler fix-its, implicit declarations). The ones cued by task wording never fired there.

The goal is to replace "a person writes an operator per phrasing" with "the engine proposes candidate edits, verifies them, and keeps as a reusable operator only what verified across several different cases".

## Core idea

1. **Propose.** From a small, fixed set of edit primitives, generate candidate edits for a failing task.
2. **Verify.** Run each candidate in an isolated copy of the workspace against evidence the engine can observe on its own: build status, exit code, stdout, compiler diagnostics, and an optional task-derived check.
3. **Record.** Store every verified repair as a trace: the before-state features, the primitive, where it applied, and the evidence that it worked.
4. **Induce.** Periodically generalize over the traces. A pattern becomes an operator only when it verified on at least K distinct workspaces and failed on none of the cases where its precondition held.
5. **Reuse.** Induced operators run before blind search. They are cheaper and ranked by their recorded success.

## Edit primitives (fixed, small, hand-written)

These are the only hand-written parts. They are syntax-level and know nothing about any task.

- token swap: operator (`<`/`<=`, `+`/`-`, `&&`/`||`), literal (+/-1, 0/1), identifier (to another identifier in scope)
- insert/delete a line: an `#include`, a prototype, a `return`, an initializer
- rename an identifier everywhere
- move a declaration: into a header, out of a file
- text-file edits for non-C files: replace a line, append a line, balance a delimiter

`relop_search` (operator 11) is already a hand-written instance of "token swap + verify". Induction turns such searches into learned, conditioned operators instead of adding one search per category.

## Preconditions come from observed state, never from task wording

A trace's features are only things the engine observes:

- the compiler diagnostic class and its position (error code, warning flag, note text)
- the build/run result: compiles, exit code, timeout, crash
- a stdout diff against a stated or recorded expectation
- the structure at the edit site: node type, the operator, whether it's in a loop condition

Task text can only contribute **anchors**: identifiers, file names, and literals that already exist in the workspace or that the task quotes. It can never contribute trigger words. This is what failed today, and the design removes it on purpose.

## Induction rule

An operator is a tuple (feature pattern -> primitive at a located site). It is promoted when:

- it verified on at least K = 3 distinct workspaces (distinct by content hash), and
- every time its feature pattern matched, applying it verified (0 false fires), or it abstained correctly, and
- it holds under a leave-one-out check: induced from K-1 traces, it still solves the held-out trace.

It is demoted when a later application fails verification. Wrong edits are what the bank counts most heavily, so one failure demotes it until it is re-verified.

## Search budget

Blind search is bounded. The default is 64 builds per task, which is what relop_search uses today. Candidates are ordered from the smallest change and by the success record of the primitive in similar feature contexts. Candidates can run in parallel in isolated copies. Only one edit is kept, and two equally ranked verified edits = abstain.

## Storage

- `traces/`: one line per verified or failed attempt (features, primitive, site, evidence, workspace hash). Append-only.
- `operators.tsv`: the induced operators with their pattern, primitive, support count, fire count and failure count.
- Both go through `declared_rules.tsv` reporting, so learned rules are listed apart from hand-written ones.

## Anti-cheating guards (kept from today)

- Test files are never edited to make a test pass, and equality is never flipped in an assertion.
- The golden `after/` and `check.py` are never read by the proposer or the inducer.
- The bank's blind batch is never used for induction or tuning. It is only run with `bank_harness.py --counts-only`.

## How we measure it

1. Baseline: current hand-written operators on Mimo's blind batch (counts only).
2. Induction trained only on the current 56 tasks (all development now) and on synthetic self-generated breakages of the repo's own C files. For example: flip a relational operator in a function with a test, and check that the test catches it.
3. Report blind pass rate, wrong edits, and how many passes came from induced operators versus hand-written ones.

Success criterion: the blind pass rate goes up with 0 wrong edits, and the induced operators account for the gain.

## Decisions (Antonio left them to Instinct's judgment, 2026-09-23; the recommendations were adopted)

1. **Synthetic training data:** yes. Mutation testing on the repo's own C functions that have tests: break one thing, and check that the test catches it and that the engine can repair it.
2. **K = 3** distinct workspaces to promote an operator.
3. **Wording-cued operators** (doc_sync, remove_dead_function, the change-to cue) are removed once induced operators match them on dev. The difference is reported.
4. **Budget:** 64 builds per task, with parallel isolated copies allowed.

## Build plan (one gated commit per phase)

1. **Traces.** TaskOpsSolve appends one line per attempt (observed features, operator, site, verified) to a trace file when `SYMBOLS_TRACE` is set. Nothing changes in behavior.
2. **Mutation corpus.** A script creates broken copies of small repo functions that have tests (single primitive mutations), and records whether the test catches each one. These are training workspaces only; the bank's blind batch is never used.
3. **Primitive search.** Generalize relop_search into a bounded primitive search: token swaps, +/-1 on literals, line insert/delete. Verification is by observed state. Every attempt is traced.
4. **Inducer.** Group verified traces by (feature pattern, primitive). Promote at K=3 with 0 false fires and leave-one-out. Write `operators.tsv`. Load the induced operators before blind search.
5. **Measure and retire.** Compare on dev and on Mimo's blind batch (counts only). Remove the wording-cued operators once the induced ones match them on dev, and report the difference.

## Phase 2b result (2026-09-23): strong-oracle sources, table still off

`tests/fixtures/mutation_sources/` adds 9 workspaces cut verbatim from `src/` (StemWord, NumericParseMeasure, the calculator, LcpLen, CensusHash, count_token_in, tok_distance, HexVal, LevBounded), each with a `main()` that checks several inputs, so their repairs carry `/o2`. Building them exposed an engine bug: with Allman braces (`{` on the line after the head) the primitive search lost track of both the anchored function's body and main's body, and an `#include` naming the anchor could carry "inside the anchored function" into main. Fixed in `relop_search`; three new test_task_ops checks cover it. The bank corpus is unchanged by the fix (31 repaired / 27 exact / 4 wrong).

Corpus with the new sources (49 sources, 227 caught mutants): 69 repaired, 62 exact, 7 wrong (the new sources add 38/35/3). Leave-one-out: 65 repaired, 58 exact, 7 wrong. Fewer exact, the same wrong, so the table stays off. The 3 new wrong repairs are relational swaps in stem_word and tok_distance whose checks, though multi-input, do not separate `>=` from `>` at that site; their only demoting evidence comes from the same source, which leave-one-out removes. Induction needs relational failures that recur across different sources before a demotion can generalize.
