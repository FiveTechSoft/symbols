# Shell corpus diagnostic bank

Recovered byte-for-byte from the original 2026-10-24 pre-run bank.
`bank.tsv` SHA-256: `57f3af78e6000792f12f193177b3bffb2e5e5f7f6090bfa586fe446c7f7cdace`.
The 80 questions were written by the corpus author, frozen before the first run,
and are not an independent holdout. They must not be used as proof of functional
correctness, understanding, cross-platform execution, or OpenCode acceptance.

`run.py` preserves the original raw substring scorer but accepts explicit paths:

```sh
python3 tools/shell-bench/run.py --output /tmp/shell-results.jsonl
```

It calls the local Symbols API at port 8100 by default. Start the intended server
and load the intended corpus first. Record commit, corpus hash and endpoint with
each run. The scorer checks tokens, not shell syntax or command effects. An API
run is diagnostic only; final acceptance requires real OpenCode with Symbols and
its tools, checking outputs, file hashes and actual execution where available.
Windows/macOS execution is not established by Linux tests or documentary claims.

The first run reported 37/80 raw token matches, and a later whitespace-normalized
analysis reported 44/80. Normalization can hide broken syntax. Do not compare those
numbers with functional-correctness scores. Original results bytes have not been
recovered, so no reconstructed results file is presented as an original artifact.
