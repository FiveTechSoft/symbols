# ctest_report.py against the real suite, 2026-10-24 (validation record)

Subject: master `6368b59446cab4b9e11274a4aca5c036fdd36456` (the tree the
suite ran on; a later docs-only README commit `acbb104` does not change any
test). Method: Release build with GCC (Ubuntu 11.4.0) and Unix Makefiles,
CTest 3.22.1 (the version the classifier's reason strings were read from),
`ctest -T Test` run serially in a Linux sandbox, then
`tools/ctest_report.py Testing/20261024-0139/Test.xml --registered .`
Output preserved verbatim in `report.json`.

Result: 250 registered tests, all classified, zero `unknown`:

| class | count | tests |
|---|---|---|
| passed | 241 | (see report.json) |
| failed | 2 | `test_typed_c_verify`, `test_typed_c_preview` |
| skipped | 7 | the seven `*_e2e` tests (`SKIP_RETURN_CODE=77`) |

Cross-check against CTest's own summary of the same run: "2 tests failed out
of 250" (`test_typed_c_verify`, `test_typed_c_preview`) and "did not run"
listing exactly the same seven e2e tests. Counts and names match the
classifier one to one.

The two failures are a sandbox environment cause, not a product defect: both
trace to `ValueError: generator_unavailable` from `installed_generator()` in
`tools/typed_c_contract/preview.py` (the typed-C generator is not installed in
this sandbox; both tests are green in CI). The classifier labels them
`failed`, which is the correct outcome class; it classifies outcome, not
cause.

Limits, stated up front:

- One Linux sandbox run, not CI; nothing here runs in the gate.
- This run exercised only three of the six outcome classes on the real suite
  (passed, failed, skipped). `timed_out`, `crashed` and `not_run` did not
  occur, so their real-suite behavior remains covered by fixtures and
  mutants only.
- Windows reason strings remain unmeasured, as documented in the tool.
- This record changes no M3 reading; the criterion-4 gap "not run on the
  real suite in CI" stands. It is evidence, not a declaration.
