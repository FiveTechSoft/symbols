# Wording rewrite harness: strict execution and structured noop baseline

2026-10-16. Base0b20877062579624d1766b880b9d221fae482cb5. Proposal before patch. Scope only test_wording_rewrite_harness, new gate+gold/selftest, no agent/harness semantics or task fixtures changed. Latent Windows diagnostic gates are later slices.

## Finding and source-derived baseline

CMake invokes Python bank_harness.py --agent noop --rewrite R3 --counts-only, then PASS_REGULAR_EXPRESSION only `"tasks_passed": 0`. CTest PASS ignores ordinary nonzero process exits if a substring matches. Thus partial JSON or another mode/agent/zero tasks/setup failures can satisfy the old regex; output may contain the prefix before an eventual error. Native product failure has not been claimed; synthetic reproduction required before change.

Live source readback0b208: tools/bank_harness.py prints one JSON stdout object, one BANK_HARNESS summary stderr and returns nonzero on setup_failures. --counts-only omits tasks, not metadata. --agent noop executes no edits; all unchanged-before checks fail in this fixture bank. read_index uses TSV metadata and split_of parity. Current index59tasks,9categories (eight7plusgit3),dev34/heldout25. Measured Linux actual command exit0: passed0, wrong_edits0, untouched59,agent_errors0,setup_failures0. Time dynamic finite>=0. Only public index metadata used; no check answers/after/heldouttasktext read or tuned.

## Contract

Run exactly the current harness command under sys.executable, cwd repo, shell=False, timeout100s (outerCTest120). Exit0 AND structured baseline. Keep stdout/stderr separate. Strict one JSON object, duplicate keys refused, NaN/Infinity refused, no extra text. Require exact top-level key set of the existing counts-onlyschema, schema1 (int notbool), modeagent, agentnoop, taskstotal equal validated index count>0, taskspassed0, passrate0, wrongedits0, untouchedtotal, agenterrors0,setupfailures0; no per-task rows. Category totals/splits derived from index, not observed output. Exact category map key sets and 0/n rates, splitdev/heldout0/n. wall_ms_total finite nonnegative number (notbool). Do not freeze time/platformvalues. stderr exactly one known BANK_HARNESS summary line, fields/value order from current source and metadata, optional trailingnewline; reject extra diagnostics as not established baseline, no permissive substringmatch.

Wrapper derives expected summary from metadata using a small independent parser consistent with documented index: valid3+fields, skipheader/comments, reject duplicateID/missingcat/missingfixturepath/unknown split? Current split accepts nonnumeric suffix as heldout; preserve that semantics. Do not inspect before/after/check.py contents. Read index source-of-truth current file each run; if malformed fail0coverage rather than silentlyaccept. Relative fixture path containment within bank should be validated only if used; metadata can be read without openingtaskdirectories. No new dataset completion claims.

Executable default launches harness. Self-test calls same validator with frozen synthetic complete stdout+stderr and returncodes; subprocess negatives exercise exact runner using temporary fake commands, including matchingmarkerexit1,stderr-only,timeouts and truncation. Never pass nonzero as expected mutant success. No gold tuning on actual future output; source contracts prevail, changed schema needs reviewed new gold.

## CMake/integration

Replace only test_wording_rewrite_harness command with wrapper; remove PASS property there and keep TIMEOUT120. Add selftest test_wording_rewrite_gate pointing same Python --self-test. Newtest dependency mapping must be added to existing build_graph sidecar exactpath entry, not bypass tests. No SKIP if harness absent; requirePython integration already exists. Add supportdesign+gold first then codeexpectedHEAD/readback. ExactpatchSHA parentreview before dispatch; MSVCRelease+ASANall270+one new=271 if no otherchanges,6gates/selftest/actualcheckoutverified. Expected totals must be derived from actual CTest count, never assert guessed count as observed.

## Gold and gates before code

GOLD.json complete source-derived expectation+negative catalog. Synthetic isolatedCTest oldPASS demonstrates {"tasks_passed":0}+exit1 accepted and partialstdout accepted, runner intended rejects. Actual baseline raw retained. Gate selftests: correct, missing JSON fields, wrongagent/mode/schema/bools,wrongtotal0/58, wrongcategory/splits,setupfail,agenterror,wrongedit,untouchedpartial,passednonzero,duplicatedkey,NaN/Infinity/timebool/negative, tasksleak,extrastdout,malformed/truncated,stderrsummarymismatch/extradiagnostic,nonzeroexit,timeout. Marker+exit1/controlonly must fail. Privatemutants exitignored/JSONsubstring/metadataignored/stderrignored must be killed,controlgreen.

## Limits

Gate proves the noop/R3counts baseline ran and its complete declared summary matches; it does not prove R3 was internally applied to each text (noop does not consume it), agent repair quality, all checks sound, heldout generalization or lack of unrelated product bugs. Add a separate test if rewrite plumbing itself needs stronger evidence, not silently claim it here. No assertions about handleownership/255 or production shellquote behavior.

## Sources

https://raw.githubusercontent.com/FiveTechSoft/symbols/0b20877062579624d1766b880b9d221fae482cb5/CMakeLists.txt
https://raw.githubusercontent.com/FiveTechSoft/symbols/0b20877062579624d1766b880b9d221fae482cb5/tools/bank_harness.py
https://cmake.org/cmake/help/latest/prop_test/PASS_REGULAR_EXPRESSION.html
