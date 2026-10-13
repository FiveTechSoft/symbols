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
python3 tools/chat_coverage/run.py --probe /tmp/chat-coverage-probe --cwd "$PWD" --output /tmp/chat-coverage-new.json
```

Use a new output filename. The adapter disables episodic auto-save; source fixtures are read-only. The runner fixes query/source hashes, checks repetitions, authentic receipt spans and prefix/state invariants. It records raw outcomes, never calls semantic misses green. It is an external measurement harness, not a CTest addition and not part of the Windows CI execution. `--replay existing.json` validates a saved raw matrix without rerunning the engine; replay is not a fresh integration measurement.

Reproducing the original SHA and reproducing these exact empirical outputs are different from building a later engine. Keep the tested engine SHA with new evidence. Do not update the original gold because a new implementation produces different answers. Semantic scoring must inspect the wide original output and the whole source relation/condition. A truncated display alone cannot establish full support.

## Next gate

Keep the public contract at topic retrieval. A future full-support path needs an explicit representation of requested relations/conditions and a separately checked support or derivation contract, tested on new unseen questions. Receipts and token overlap alone are insufficient. Neither these counts nor the existing source-calibrated guard declares a roadmap phase exit.
