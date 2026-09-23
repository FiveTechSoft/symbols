# Mutation sources (operator induction, phase 2b)

Training workspaces for `tools/mutation_corpus.py --repo-sources`. Each
directory holds one function copied verbatim from `src/` (the header comment
names the file) and a `main()` that checks it on several inputs and exits 0
only when every check passes. Where the repo has a unit test for the function
(`test_stem`, `test_numeric`), the cases come from it; otherwise the expected
values are the original function's outputs.

They exist so the inducer sees repairs verified by strong oracles (2 or more
distinct inputs, pattern suffix `/o2`); the bank's golden trees all check one
input. They are training data only; the blind batch is never used here.
