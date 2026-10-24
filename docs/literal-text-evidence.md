# Literal source text in direct text answers

The lexical matching view inserts spaces between tokens. That view remains
unchanged for ranking and calibrated coverage. It must not be presented as a
literal quote: command options, paths, decimals, quotes and whitespace may carry
meaning. The direct TEXTQ answer now uses a separate contiguous source-span copy
between the selected sentence's first and last token. It retains original bytes,
including internal whitespace, without changing the stored source or ranking.

The source display validates token offsets and bounds, rejects overlap, and marks
buffer truncation. It is display only, not execution or semantic verification.
Emission receipts bind the selected sentence identity to the actual source text;
legacy paths keep their lexical display contract until separately migrated.

## Acceptance scope

Real OpenCode 1.18.32 with Symbols and the shell corpus was checked before/after.
The pipefail question still selects an unrelated sed/awk paragraph: relevance
remains a failure. The returned paragraph now preserves `sed -i`, `tail -f`,
`/etc/passwd` and `awk '{print $1}'`, plus source line breaks. The question and
response were inspected together. This change does not make that answer correct.

Other answer branches and summarization still need an explicit audit and their
own literal-output migration. Never infer general any-corpus correctness from
this direct-text slice. Windows/macOS commands were not executed in this Linux
acceptance environment.
