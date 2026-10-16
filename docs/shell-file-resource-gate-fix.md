# Correct PSS type interpretation and the resource evidence gate
Base c7d078b9e8677357526fc7d6c4205ad83ef88b85. Implementation follows this independently reviewed correction design.

## Preserve the falsification
CI455 MSVC/ASAN both reproduced pattern and passed helper controls but executed
ZERO candidate queries. The implementation compared PSS File type against8bytes;
PSS observed10bytes including terminal NUL. This was our interpretation bug,
not a failed resource query or proof of observer effect. No path/owner result.
Explicit invocation exited1. CTest259 incorrectly Passed because our semicolon
marker became a LIST of regex alternatives (including generic complete=1), and
PASS_REGULAR_EXPRESSION ignores process exitcode. Both original255 failures and
this new raw failure are preserved; never retroactively relabel455 successful.

## Narrow correction, no resource/production contract change
1. Introduce a small private portable comparison helper for counted UTF-16 ASCII
   type names: File accepts exactly4codeunits or5with ONE final NUL. Compare all
   four chars exactly. Reject interiorNUL, multipleNUL, oddbytes, overlong, prefix,
   suffix, lowercase and missing type flag. No trimming other content, no strlen
   on external buffers, no clipping. Keep original PSS bytes in output/tables.
   A portable C test exercises both valid encodings and each negative. Existing
   snapshot fixture/gold and original255 remain byte-identical.
2. Remove PASS_REGULAR_EXPRESSION ONLY from the resource test. Register it through
   a Python runner, using already-discovered Python3; require Python at configure
   on Windows (fatal if absent, do not silently omit gate). Runner launches exact
   native executable, captures merged stdout/stderr as UTF-8 strict with bounded
   subprocess timeout180s, echoes it, and returns0 ONLY if native exit0 AND complete
   evidence validator accepts. Timeout/nonzero/decode/error each nonzero. CTest
   outer200s, allowing runner cleanup. Explicit MSVC/ASAN step uses SAME runner;
   all prior diagnostics and always-run CTest behavior remain unchanged.
3. Validator requires exactly one controls complete1; all three PSS arms complete1
   with frozen deltas; one successful identification of candidate; exactly THREE
   RESOURCE_QUERY context=candidate records, ordered modes0,1,2; attempted/returned1,
   no harness/duplicate/timeout/pending errors, close1; finalpath(mode1) complete1
   with a nonempty full \\Device\\ prefix path actually returned. Type category/
   native partial result is printed but does not identify resource on its own.
   Require six successful resource_before/resource_after captures (one pair per
   mode) and their entrycount equality; require RESOURCE_RESULT full_path_observed,
   target_stable1/candidate_not_closed1/ownerunproven; exact final marker ONCE and
   last nonblank line. Reject complete0 PSS arm/identification, PSS_ERROR/OBSERVER,
   invalid UTF or inconsistent/missing/duplicate/reordered evidence.
   A category or partial path alone remains blocked and nonzero. Ordinary query
   API failure on mode0/2 remains evidence, allowed only if mode1 supplies full NT
   path and no incomplete execution/cleanup exists, as original approved design.
4. Add Python gate tests on Linux/Windows: synthetic complete stream exit0 positive;
   exit1 with complete-looking output negative; controls-only complete1 negative;
   actual CI455 failing tail negative; full marker with missing/duplicate/reordered
   query negative; partialname/no finalpath negative; pending/timeout/cleanup0/
   stable0/missingcapture/encoding-error/missing marker negatives. Exercise runner
   through fake producer subprocesses, not validator alone. Use a temporary real
   CTest mini-project (no PASS regex) running runner against zero/nonzero producer
   to prove nonzero cannot become Passed. Synthetic positive is gate mechanics
   only, never Windows/resource efficacy evidence.

## Unchanged and closeout
Gold original resource assumptions all retained; new revision only documents
counted-type semantics and the new AND gate/negative bank. Existing gold file is
not replaced; add separate revision gold/doc. No target fixture/ABI/src change,
no candidate close, no broader access/query, no ETW/debugger. Query transport,
helper3s + bounded kill/reap, target20s/60s unchanged. CTest outer200 replaces180
only to allow runner-owned timeout/cleanup (new resource test only).

Local Linux build/regression, portable type helper/gate negatives, pristine split
apply/readback; parent hash review before publish A(code/tests/CMake/doc) then
B(ci.yml), readback every path and final autoPAT CI only. Native raw0+complete
validated transcript needed for route. If anything fails, report exact blocker
and preserve red. Resource path remains separate from source owner/unproven.

## Source contracts checked
https://learn.microsoft.com/en-us/windows/win32/api/processsnapshot/ns-processsnapshot-pss_handle_entry
https://cmake.org/cmake/help/latest/prop_test/PASS_REGULAR_EXPRESSION.html
https://cmake.org/cmake/help/latest/prop_test/FAIL_REGULAR_EXPRESSION.html
PSS byte-length strings may lack NUL; observed length10 contains terminalNUL.
CMake explicitly documents list semantics and ignored exitcode for PASS regex.

## Integration evidence and scope
The new portable Python test declares its script/gate/fixture inputs in the
existing build_graph_inputs sidecar. Missing declaration initially caused three
local regression failures (unknown test mapping); adding that declaration fixed
them in an explicit confirmation. No build-graph algorithm was changed. Local
results are latest-per-test across batches plus confirmation, not one pristine
single run. New native evidence still required. The mini CTest uses Release
configuration explicitly on multi-config platforms. The frozen CI455 red fixture
is a timestamp-stripped public raw excerpt, not a synthetic resource result:
https://github.com/FiveTechSoft/symbols/actions/runs/38838246124/job/116772094409
