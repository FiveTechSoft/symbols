# Hidden QA split — measure only

`tests/qa_hidden.tsv` is the held-out question set for scoring the engine.

## Rules

1. **Never tune on this file.** Do not add, remove, or rewrite rows to improve a score. Development uses `tests/qa_eval*.tsv` only.
2. **English only.** Questions and expected answer substrings are English.
3. **Expectations are substrings**, case-insensitive after accent folding (same judge as `test_eval_qa`).
4. **Runner:** `test_qa_hidden` prints `HIDDEN_QA pass= total= rate=` and exits:
   - `0` if the TSV is well-formed and the model loaded (accuracy is reported, not gated here);
   - `77` if `wiki_model.bin` is absent;
   - `2` if the TSV cannot be opened or has zero data rows.
5. Instinct’s metrics harness may gate on the printed rate; Mimo does not choose that threshold.

## Provenance

Rows are independent of `tests/qa_eval*.tsv` (no duplicate question strings). Expected answers are fixed world/corpus facts written once when the file was created.
