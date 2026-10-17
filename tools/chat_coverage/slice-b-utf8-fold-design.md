# Slice B — TQ coverage gate: UTF-8 accent fold double-standard

Base: b3a468a8785e6de10a60b6af3a1e82c7c06ff8e4 (master, verified 2026-10-17)
Gap analysis: GAPMAP.md (SHA d1d77289…), slice A diagnostic confirmed arithmetically.

## Root cause (slice A, confirmed)

The TQ gate in src/chat.c uses a different string normalization than the rest of chat:

- Query path: Split + FoldChar (~line 280) folds accented UTF-8 to ASCII lowercase
  ('colisión' -> 'colision', 'sueño' -> 'sueno', 'enfría' -> 'enfria').
- Corpus path: TqWords (~55-107) tokenizes sentences with byte-wise C-locale
  isalnum, which rejects every byte >= 0x80. Accented words split:
  'colisión' -> vocab entry 'colisi', 'sueño' -> 'sue', 'enfría' -> 'enfr'.

Consequences, measured with SYMBOLS_TQ_DEBUG at b3a468a:

1. Known-check (exact or stem: >=4-byte prefix match, |len diff| <= 2) admits
   'colision/funcion/evaluacion/inversion' against the truncated vocab entries
   but rejects shorter stems like 'sueno' (vocab 'sue', 3 bytes < 4) ->
   nk undercount, majority rule 2*nk < ncount can fire (NO_VOCAB abstain).
2. TqCoverage is byte strstr on the lowered sentence: query 'colision' is not a
   byte substring of 'colisi\xc3\xb3n', so exact-support sentences score below
   threshold: astronomy-1 exact sentence c = 1.58/3.66 = 0.43 < thr 0.50.
3. Where the discriminating word is excluded from kw (memory-1, nk=3), the
   wrong-condition sentence passes at c = 1.00 and is quoted with an authentic
   receipt -> wrong-condition answer, the worst failure class.
4. thr = 0.50 everywhere in the matrix: TqCalibrate only runs at nsent >= 8 and
   all fixtures have 3 sentences, so the default stands. Calibration keeps
   recomputing at runtime for large corpora; untouched by this fix.

## Fix (design, frozen before patch)

A. Fold inside the gate, reusing the existing chat normalization:
   - TqWords consumes bytes through FoldChar (UTF-8 0xC3 lead map, Latin-1,
     CP850, else '?'), word chars = isalnum on the folded ASCII byte.
     Unmappable high bytes fold to '?' and break the word (not alnum) — same
     treatment Split gives punctuation. The 23-byte truncation cap stays as a
     documented limit (TqIdf low[24]).
   - TqCoverage folds the sentence with FoldChar before matching, and folds the
     query word (idempotent — query words arrive pre-folded). Match rule
     unchanged: strstr, left boundary !isalnum(h[-1]), first hit breaks.
   - TqIdf / known-check / majority rule / TqCalibrate: unchanged code, but they
     now operate on the folded vocab, so accented words match exactly.
B. Selection: among coverage-passing candidates, answer the one with maximum
   coverage; tie falls back to retrieval score order (unchanged).
   Required, not cosmetic: post-fix, memory-4's wrong-condition sentence S1
   passes the gate at c = 0.52 while the 'no describe' sentence S2 covers more
   (0.77); score-first acceptance would quote the wrong-condition sentence.
C. No changes to: Try contract, retrieval, receipts, emission format, IsStopTok,
   thresholds, calibration, follow-up advance (never-repeat) semantics.

## Predicted post-fix behavior (frozen in GOLD.json v2, before execution)

Predictions come from an exact Python replica of the gate (sim.py), validated in
fold-OFF mode against the three instrumented debug values at b3a468a:
astronomy-1 c=0.43 (replica 0.4324), memory-1 c=1.00 nk=3, math-1 c=0.13
(replica 0.1267). No tuning on measured outputs.

Fresh arm (16 regression cells):
- positives 8/8 -> exact support sentence, c = 1.00 (was 4 support, 1 wrong-condition, 3 abstain)
- negatives: -2 cells -> explicit-negative sentence c = 1.00 (safe);
  -4 cells -> 'no describe' sentence c = 0.77 (safe). math-2 improves
  abstain -> explicit-negative quote. No wrong-condition anywhere.
Stateful arm: same except memory-3, astronomy-3, physics-3 (and math-3, already
abstaining) -> ABSTAIN: their correct sentence was already shown by the -2 query
and never-repeat excludes it; remaining candidates score <= 0.29 < 0.50.
Safe-direction regression (support -> abstain, no wrong answer). Alternative of
resetting shown on gate failure was rejected: it would re-quote already-shown
text, breaking the follow-up advance contract.

New accented-condition cells (8, corpora cocina.txt / taller.txt, 3 sentences
each so thr = 0.50 stays deterministic): pin the fixed paths —
- condition words with ó/ú (ebullición, congelación, otoño): known whole, coverage 1.00 on support sentence
- ñ folding in both query and corpus vocab (niño/niña/otoño)
- corpus word with í (enfría) matched by folded query token
- 'dibuja' vs 'dibujos': coverage strstr does NOT prefix-match; separate vocab entries
- 'no describe' sentence winning on coverage over partial-coverage content sentences (cocina-3, taller-3, c = 0.73 vs 0.43)

## Verification plan

1. Support patch (this slice, part 1): DESIGN.md, GOLD.json v2, cocina.txt,
   taller.txt, sim.py under tools/chat_coverage/ (simulator kept as the
   executable derivation of the frozen expectations).
2. Readback at published SHA (raw.githubusercontent byte-compare).
3. Code patch (part 2): src/chat.c gate fold + max-coverage selection;
   run.py gains --gold (default = new GOLD.json v2 SHA); doc addendum to
   docs/chat-query-support.md recording the new matrix.
4. Harness run at the code SHA must reproduce GOLD.json exactly (fresh +
   stateful, all 24 cells); drift control vs a7b5ca8 no longer applies to the
   changed cells — 274-test native CI matrix must remain unchanged (existing CI
   chat tests contain zero accented content, rg-verified).
5. Full native Release+ASAN matrix, six strict gates, no SKIP.

## Explicit non-goals

No 255/ownership/strcasecmp/workflow/allowlist changes; no M0/M1 phase
declarations; no negative-key i18n; no threshold tuning; no changes to
retrieval scoring or the Try wrapper contract.
