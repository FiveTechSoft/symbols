# CHAT: topic retrieval is not full-question support

The literal TEXTQ path selects a corpus sentence. Its optional citation receipt proves the source identity and byte span for that selected emission. It does not prove the corpus true, every requested condition answered, or a derived answer valid. Display is token-spaced normalization, not source-byte reproduction. Internal display cuts still have no verified provenance receipt.

## Separate contracts

- **Topic retrieval:** the selected sentence relates to the question's topic. This is the current scope, not an unconditional guarantee of relevance.
- **Full-question support:** the source answers the requested relation for the requested subject under every qualifying condition. This is not a current API guarantee.

A verified receipt can coexist with a wrong condition. A marked substitute names what was used, but does not prove that substitute equivalent to the requested concept. Handle/Try return whether they handled the line; their boolean result is not `GOAL_ANSWER`. Record route, answer/abstention, provenance, topical relevance and full support independently. A parse failure on a supported question is a coverage miss, not a semantic safety success.

## Measurements at a7b5ca8 (2026-10-13)

No serving change was made in these measurements. Serial Linux runs used the same base commit, fresh source hashes and pre-run fixed gold. Repetitions were identical. A 32-byte output was the exact 31-byte prefix of its 4096-byte decision, with the same status/state. Counts below use questions, not replicated executions as independent samples.

### Known plus unknown keywords

Four existing source excerpts (memory, NASA, physics, mathematics), 24 new keyword queries, three routes, two output capacities and two repetitions: 144 cells, 288 executions. The question gold was frozen before execution. These are unseen queries over existing source fixtures, not an independent unseen corpus.

Resolve and Handle kept the anchor's exact answer in all four excerpts after adding one synthetic absent token. Three absent tokens caused abstention in all four. Adding one natural absent term also retained the anchor-only answer: `hipocampo telepatia`, `seconds neutrinos`, `postulado taquiones`, `pendiente integral`. These keyword bags do not distinguish noise from a required relation, so they do not justify a policy change.

The absent query `neutrinos` alone returned `Segun el texto [neutron]: ...` with an authentic receipt for a sentence about neutron stars, in all three routes and both capacities. That is not literal support for neutrinos. The first run stopped on this exact-topic gold violation; a single authorized continuation kept that gold unchanged and recorded semantic violations as results, while still stopping on technical failures. Try did not handle `seconds` queries because of structural routing; do not call this semantic abstention. No majority rule was added or tuned.

### Full questions and contrastive conditions

The reproducible harness under `tools/chat_coverage` contains four new small Spanish synthetic corpora and 16 questions (eight positive, eight negative), evaluated fresh and in a stateful four-query sequence, through Resolve/Try/Handle, at 32/4096 bytes, twice: 192 cells, 240 processes, 384 query executions. Gold SHA256: `75d771bd383cf4bba050981e0e3d1bb1fa08b68c7e3bb545c40021ce41b630f1`.

Manual scoring reads the full wide answer against the original relation and condition, not just word overlap:

| Arm/route | Positive support (of 8) | Positive misses | Negative safe or explicitly unsupported (of 8) | Wrong-condition negative |
|---|---:|---|---:|---:|
| Fresh Resolve | 4 | 3 abstentions, 1 wrong-condition quote | 7 | 1 |
| Fresh Handle | 4 | 4 abstentions | 7 | 1 |
| Stateful Resolve | 4 | 3 abstentions, 1 wrong-condition quote | 8 | 0 |
| Stateful Handle | 4 | 4 abstentions | 8 | 0 |

Try handled none of the 16 questions in either arm. Do not score those unhandled lines as supported answers or semantic safety wins. Explicit negative source answers and honest abstentions are pooled in the safety column, not claimed as equally useful answers.

Reproducible counterexamples:

- `¿Qué conserva el hipocampo durante el sueño?` Resolve quoted the vigilia sentence, with a true receipt but the wrong condition. Handle rejected it. The fixture states the sleep fact elsewhere.
- `¿Qué conserva la corteza durante la anestesia?` fresh Resolve/Handle quoted the vigilia fact. In the stateful sequence the same question instead quoted the sentence declaring the information not described. Selection history matters; this is not a stable full-support guarantee.
- The positive function/evaluation and operator/inversion questions abstained. No mathematical reasoning capability is shown.

Design caveat preserved: the planned "omitted" conditions are actually mentioned by explicit `el archivo no describe...` sentences. This measures declared lack of support, not pure absence without mention. The fixtures and gold were not changed after the run to hide this gap. They are a closed fictional world, not scientific or medical claims. One machine/configuration, Spanish-only, small corpus calibration, no OOM or broader distribution testing. No independent domain expert validation.

## Reproduce, without certifying semantics

Build the library normally, then compile the read-only adapter (POSIX example from repo root):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target symbolic --parallel 1
cc -O2 -Iinclude tools/chat_coverage/probe.c build/libsymbolic.a -lm -lpthread -o /tmp/chat-coverage-probe
python3 tools/chat_coverage/run.py --probe /tmp/chat-coverage-probe --output /tmp/chat-coverage-new.json
```

Use a new output filename. The adapter disables episodic auto-save; source fixtures are read-only. The runner fixes query/source hashes, checks repetitions, authentic receipt spans and prefix/state invariants. It records raw outcomes, never calls semantic misses green. It is an external measurement harness, not a CTest addition and not part of the Windows CI execution. `--replay existing.json` validates a saved raw matrix without rerunning the engine; replay is not a fresh integration measurement.

Reproducing the original SHA and reproducing these exact empirical outputs are different from building a later engine. Keep the tested engine SHA with new evidence. Do not update the original gold because a new implementation produces different answers. Semantic scoring must inspect the wide original output and the whole source relation/condition. A truncated display alone cannot establish full support.

## Next gate

Keep the public contract at topic retrieval. A future full-support path needs an explicit representation of requested relations/conditions and a separately checked support or derivation contract, tested on new unseen questions. Receipts and token overlap alone are insufficient. Neither these counts nor the existing source-calibrated guard declares a roadmap phase exit.

## Addendum 2026-10-17: UTF-8 accent fold in the coverage gate (slice B)

The matrix above was measured at a7b5ca8 and reproduced at b3a468a. Its wrong-condition and under-coverage cells shared one root cause: the TQ gate tokenized the corpus with byte-wise C-locale `isalnum` while the query path folds accents through `FoldChar`. Accented UTF-8 corpus words split (`colisión` -> `colisi`, `sueño` -> `sue`), so folded query words missed the vocabulary check (`sueno` rejected, `nk` undercount) or scored exact-support sentences below threshold (`colision` is not a byte substring of `colisi\xc3\xb3n`: c = 0.43 < 0.50 for the exact astronomy sentence). Where the discriminating word dropped out of the known set, a wrong-condition sentence passed at c = 1.00 and was quoted with an authentic receipt.

Fix (this slice): the gate consumes corpus bytes through the same `FoldChar` used by the query path (UTF-8 0xC3 map, Latin-1, CP850; unmappable bytes fold to `?` and break the word like punctuation), in `TqWords` and `TqCoverage`. Selection among coverage-passing candidates now answers the sentence covering the most of the question instead of the first score-ranked pass; a looser sentence (c = 0.52) can no longer win over the exact one (c = 1.00). Shown sentences stay excluded from selection (advance, never repeat); the previous verify pass could re-quote an already shown sentence, and now cannot. Try contract, retrieval, receipts, emission format, thresholds and calibration are unchanged; `TqCalibrate` still only runs at nsent >= 8, so these 3-sentence fixtures keep thr = 0.50. The 23-byte word cap is a documented limit.

Design, frozen expectations and the executable derivation (Python replica of the gate, validated in pre-fix mode against the instrumented values c = 0.43 / 1.00 / 0.13 at b3a468a) live in `tools/chat_coverage/slice-b-utf8-fold-design.md`, `gold-v2.json` and `sim.py`. Expectations were frozen before the patched engine was run; the measured matrix below reproduces them exactly (288/288 cells: 24 cases x {fresh, stateful} x {resolve, handle} x {32, 4096}; try arm records the structural reject). Gold lineage: `4833519d` (design review) -> `32b73f77` (harness keys restored) -> `b78ef950` (try-mode contract note). The case array and all predictions are byte-identical across the three versions (canonical cases SHA-256 `b5a058729a3110cc...` in every version); only harness metadata and notes changed.

Post-fix measured matrix (24 questions: the original 16 as regression plus 8 new accented-condition cases over `cocina.txt` / `taller.txt`):

| Arm | Full support | Safe negatives (explicit-no / not-described quote) | Abstentions | Wrong-condition |
| --- | --- | --- | --- | --- |
| Fresh Resolve/Handle | 12 | 12 | 0 | 0 |
| Stateful Resolve/Handle | 6 | 12 | 6 | 0 |

Declared stateful regression (safe direction): memory-3, astronomy-3, physics-3 (and the new cocina-4, taller-4 follow-ups) now abstain. Their correct sentence was already shown by the preceding negative query; never-repeat excludes it and the remaining candidates score <= 0.29 < 0.50. Resetting the shown list on gate failure was rejected because it would re-quote already shown text. math-3 already abstained before this fix. No wrong-condition answer remains in the matrix.

Try handled none of the 24 questions in either arm, before and after: every query is a structured wh-question and `ChatTryTextLineCore` rejects those by design (status 0, no quote). Verified identical with the pre-patch probe at b3a468a. Do not score those unhandled lines as supported answers or semantic safety wins.

Measurement configuration pin: `run.py` now runs the probe from an empty temporary working directory by default. This matters because `ChatInit` loads `data/english-spanish.txt` relative to the cwd when present, and the dictionary maps query words before the gate (`sueno` -> `dream`): with the repo root as cwd the discriminating word leaves the known set (nk = 3), the exact and wrong-condition sentences tie at c = 1.00, and retrieval order quotes the wrong-condition one even after this fix. Every measurement behind this document (gap map, frozen gold, the matrix above) used a dictionary-free cwd; the harness now enforces it. The dictionary divergence itself (query translation misaligned with the corpus vocabulary) is pre-existing, unchanged by this slice, and recorded here as future work, not certified either way. Pass `--cwd` only to reproduce a specific historical configuration.

The same caveats as the base measurement apply: closed fictional world, one machine/configuration, Spanish-only, small corpora, no semantic certification. `run.py` now takes `--gold` (default `gold-v2.json`, SHA-pinned) and derives the expected cell count from the gold; `--replay` validates against the same gold.

## Correction (2026-10-17): two stateful gold v2.1 cells were wrong

The stateful expectations frozen for cocina-4 and taller-4 in gold v2.1 (`b78ef950`) were wrong: the simulator accumulated the shown set across the whole corpus group, but the engine resets it at each answer that changes topic, so never-repeat excludes only the previous topic's shown sentence(s). Measured on the live engine (aea62ab9, receipt-validated): both follow-ups ANSWER sentence 1 with correct support, they do not abstain. The real stateful abstentions are 4, not 6: memory-3, astronomy-3, physics-3, math-3 (their correct sentence was shown by the immediately preceding negative query). The table row above ("6 abstentions", and the sentence naming cocina-4 and taller-4) is superseded by this correction. The miss was masked in the original verification by a comparator that detected abstentions via shown-list length deltas; the corrected method reads status/outcome (receipt_state) per cell. All other 286 v2.1 cells re-verified as measured. Gold v3 (`gold-v3.json`, SHA-pinned in run.py) carries the corrected expectations; no silent edit was made to v2.1.

## Slice C: query-translation (dictionary) layer

Mechanism: `ChatInit` unconditionally loads `data/english-spanish.txt` relative to the cwd (chat.c:7797, 311 entries ALIAS=CANONICAL, intended Spanish query over English corpus). `DictTranslate` is a case-insensitive exact alias lookup (no accent folding) applied to query words before the coverage gate in the fresh INT_TEXTQ assembly and in the quote guard. Coverage is computed over known words only, so a translated word the corpus does not know silently leaves the known set instead of penalizing coverage.

Measured divergence on the 24-case matrix (probe at aea62ab9, dict-loaded cwd vs hermetic cwd): only the memory corpus diverges. With the dictionary loaded, memory-1 (`... durante el sueño?`) flips fresh and stateful to the wrong-condition sentence S1: `sueno` -> `dream` removes the discriminating word, S0/S1 tie at c = 1.00, retrieval order picks S1 (instrumented nk = 3). memory-2 stateful cascades to abstain (its correct sentence S1 is already shown by memory-1's wrong answer). Pre-fix, dict vs no-dict is nearly invisible (one cell changes which safe sentence is quoted).

Fix (this slice): a query word is translated only when the corpus vocabulary does not know the original form and does know the translated form (`TqCorpusKnown`, the same folded vocabulary the coverage gate uses, union over loaded texts, rebuilt when the text set changes). This keeps the intended direction (Spanish query over an English corpus receives the translation) and cures the false negative (a Spanish corpus keeps its Spanish words). Quote guard and summarize paths are unchanged (documented: guard translation only relaxes its coverage check; it never selects the answer). Try contract, retrieval, receipts, thresholds and calibration are unchanged.

Measured with the fix, both configurations reproduce the frozen gold exactly: 300/300 cells no-dict (gold-v3.json) and 300/300 cells with a pinned minimal dictionary (`fixtures-dict/`, sueno=dream): memory-1 answers S0 in both arms, memory-2 stateful answers S1, and every other cell is identical across configurations. `run.py` gains the second pinned gold (`gold-v3-dict.json`) selected with `--gold` plus `--cwd fixtures-dict`; the dict fixture bytes are SHA-pinned from the gold.

Known remaining gap (recorded, not fixed here): the intended direction itself is blocked upstream of the gate. For `enmemory-1` (Spanish query over an English mirror corpus) the parse layer's geometric fallback fires first: the first token with a fuzzy Levenshtein match (`consolida` ~ `consolidate`, distance 2) wins over the exact dictionary hit later in the query (`sueno` -> `dream`), marks `t_sub` with the substituted word, and the coverage-select file filter compares that word against file paths (`strcmp(ch->tfiles[f], p->t_sub)`), skipping every file and forcing an abstain in both configurations. The quote guard additionally tokenizes the raw line with C-locale `isalnum`, splitting UTF-8 words (`sueño` -> `sue`, `o`). Delivering the ES-query/EN-corpus direction end-to-end (fallback ordering, t_sub semantics, guard tokenizer) is proposed as slice D; gold v3 records today's truthful abstain for enmemory-1 in both configurations.

The same caveats as the base measurement apply: closed fictional world, one machine/configuration, small corpora, no semantic certification.
