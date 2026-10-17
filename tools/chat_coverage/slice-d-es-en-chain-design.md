# Slice D — ES-query/EN-corpus chain: t_sub file filter + quote-guard tokenizer

Base: 3a9dd6f908e13f7bf3bf3c08fb736c3d44199212 (master, verified 2026-10-17)
Gap: recorded at the end of the slice C addendum in docs/chat-query-support.md
("Known remaining gap"). Diagnosis instrumented with SYMBOLS_TQ_DEBUG and the
acceptance probe at aea62ab9/3a9dd6f9.

## Root causes (measured)

The dictionary direction the translation layer exists for (Spanish query over
an English corpus, enmemory-1) abstained in both configurations. Three links
in the chain, two fixed here:

1. Parse geometric fallback ordering (NOT fixed in this slice): the fallback
   loop checks each token for a dictionary hit first, then for a fuzzy
   Levenshtein match, and returns at the first token that substitutes. A fuzzy
   hit on an early token (`consolida` ~ `consolidate`, distance 2) preempts
   the exact dictionary hit later in the query (`sueno` -> `dream`). Fixing
   the order (exact-dict pass over all tokens before any fuzzy pass) changes
   parse semantics for every multi-token query with mixed candidates; no gold
   case distinguishes the two orders (enmemory-1 answers sentence 0 either
   way, because the assembly layer still applies rule (a) to `sueno`), so this
   is recorded as a future candidate, not bundled.
2. t_sub file filter (FIXED): the fallback marks `t_sub` with the substituted
   WORD; the wrap-around and coverage-select loops then skip every file whose
   path does not equal `t_sub` (`strcmp(ch->tfiles[f], p->t_sub) != 0`). A
   substituted word never equals a file path, so any query resolved through
   the substitution path abstained unconditionally. The filter's intent is a
   named-text restriction ("according to text X"); it now applies only when
   `t_sub` actually names a loaded file, precomputed once per call.
3. Quote-guard tokenizer (FIXED): `ChatQuoteVerified` tokenized the raw line
   with C-locale isalnum, splitting UTF-8 words (`sueño` -> `sue`, `o`, both
   dropped as < 3 bytes), and ran `DictTranslate` on the raw pieces, so the
   dictionary could never fire. The guard now folds each letter through
   `FoldChar` (the same normalization as the answer path and the slice B
   gate) and applies rule (a) for the translation choice, mirroring the
   assembly: keep a corpus-known original, translate only when the original
   is unknown and the translation is known. The guard still only verifies the
   quote; it never selects the answer.

## Expected effects (frozen before the dispatch, measured on a local prototype)

- enmemory-1, dict configuration (pinned dictionary sueno=dream): fresh and
  stateful arms ANSWER sentence 0 ("In the test file, the hippocampus
  consolidates memories during the dream.") with the substitution marked
  ("Segun el texto [consolidate]: ..."). Gate coverage S0 = 1.00, S1 = 0.50,
  thr = 0.50 (3-sentence fixture, calibration off).
- enmemory-1, hermetic no-dict: still ABSTAIN, mechanism changes from the
  t_sub file filter to the quote guard (no known query words; the guard
  rejects the loosely selected quote). New label ABSTAIN-guard.
- enmemory-2 (new case, unsupported premise: the corpus never describes
  anything consolidating the cortex):
  - dict fresh: ANSWER sentence 0. Recorded wrong-condition semantic miss:
    only the translated condition word `dream` reaches the gate; `corteza`
    is invisible, so the hippocampus-dream sentence passes coverage. This is
    the EN-direction mirror of the single-condition-word limit already
    recorded for memory-1 pre-slice-C.
  - dict stateful (after enmemory-1 showed S0): ABSTAIN-guard; sentence 1
    passes the gate at c = 0.50 but does not cover `dream`, and the guard
    rejects the quote. The guard catching what the gate admitted is the
    intended layering.
  - no-dict, both arms: ABSTAIN-guard.
- The 24 Spanish-corpus cases: unchanged in both configurations. Measured
  prototype delta: 0/300 cells no-dict, exactly the 8 enmemory-1 cells
  (fresh/stateful x resolve/handle x 32/4096) in the dict matrix; try mode
  untouched (all 26 queries are wh-questions, structural reject).

## Non-goals

Fallback ordering (link 1), summarize-path translation, multi-file named-text
routing beyond preserving the filter's intent, thresholds, calibration,
retrieval, receipts, emission format: unchanged.

## Verification plan

Gold v4 (`gold-v4.json`, `gold-v4-dict.json`; 26 cases, 312 cells per
configuration) frozen with SHA pins in `run.py`; default gold moves to v4,
v3 pins retained for replay. Matrices: `run.py --gold gold-v4.json` (hermetic)
and `--gold gold-v4-dict.json --cwd fixtures-dict`. Local prototype (this
tree): 312/312 both configurations, technical integrity green, all 104
resolve/4096 semantic cells match expectations per configuration. CI: full
native Release + ASAN matrix via apply-patch.yml (support patch first, readback,
then the code patch exactly once).
