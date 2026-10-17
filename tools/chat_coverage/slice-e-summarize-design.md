# Slice E — summarize-path translation: rule (a) in INT_SUMMARIZE

Base: 57b09f138a5221a126e84941daf445c8eb849958 (master, verified 2026-10-17)
Gap: recorded in the slice C addendum ("Quote guard and summarize paths are
unchanged") and the slice D non-goals ("summarize-path translation").

## Root cause (measured)

The INT_SUMMARIZE word assembly (chat.c, "Build query words from tokens (same
as INT_TEXTQ)") applied `DictTranslate` unconditionally — the last text-corpus
consumer without rule (a). With the dictionary loaded, a corpus-known Spanish
word that has a dict entry is translated away before retrieval. Measured with
the real 311-entry dictionary (probe, repo-root cwd): "Resume lo que dice
sobre la nevera" over cocina.txt returns "No encontre informacion sobre ese
tema en los textos." (nevera -> refrigerator leaves zero known words); the
same query without the dictionary returns the nevera summary. The KB /
commonsense / definition DictTranslate sites are a different domain (English
knowledge graphs by design) and are untouched.

## Fix (design, frozen before dispatch)

Apply rule (a) in the summarize word loop, identical to the answer path and
the quote guard: translate only when the original is not corpus-known and the
translation is (TqCorpusKnown, folded vocabulary). Nothing else changes:
retrieval, top-K selection, document-order presentation, emission format,
topic extraction.

## Expected effects (frozen; measured on a local prototype)

Pinned dictionary extended for this slice: sueno=dream, nevera=refrigerator
(SHA 37092496...). Verified against the v4 dict matrix: 0/312 cells changed
with the extended dictionary under both the pre-fix and the post-fix engine.

- cocina-5 (new; "Resume lo que dice sobre la nevera", Spanish corpus):
  dict NOINFO -> SUMMARY sentence 1 (the nevera sentence); no-dict SUMMARY
  sentence 1, unchanged. This is the repaired cell.
- enmemory-3 (new; "Resume lo que dice sobre el sueño", English corpus):
  dict SUMMARY sentence 0, unchanged (intended direction already worked);
  no-dict NOINFO, truthful (no corpus-known query word).
- memory-5 (new; "Resume lo que dice sobre el sueño", Spanish corpus):
  NOINFO in BOTH configurations. Records the OPEN retrieval fold-mismatch
  gap: the query path folds (sueño -> sueno) but TextLexRetrieve matches
  symbols byte-exact (CaseVariants only), so a single-accented-topic
  summarize finds nothing; a retrieval-layer fold is future work, out of
  this slice.
- All v4 cells: byte-identical in both configurations (measured).

Try mode: all 29 queries (26 wh-questions + 3 summarize requests) return
status 0, measured pre and post patch. Summarize answers are quoted without
receipt/citation (the harness validates receipts only when receipt_state=1);
the topic label keeps showing the first content token ("Resumen de 'Lo'"),
a pre-existing cosmetic.

## Non-goals

Retrieval-layer fold (the memory-5 gap), parse fallback ordering (recorded
in the slice D design), topic-extraction polish, KB/commonsense/definition
translation sites, thresholds, calibration, multi-file.

## Verification plan

Gold v5 (29 cases, 348 cells per configuration, SHA-pinned in run.py,
default gold; v3/v4 pins retained). Matrices: `run.py --gold gold-v5.json`
(hermetic) and `--gold gold-v5-dict.json --cwd fixtures-dict`. Local
prototype: 348/348 both configurations, technical integrity green, all 116
resolve/4096 semantic cells match expectations per configuration. Local
ctest: 242/242. CI: full native Release + ASAN matrix via apply-patch.yml
(support first, readback, then the code patch exactly once).
