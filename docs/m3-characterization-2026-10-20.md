# M3 characterization, 2026-10-20 (not a declaration)

Subject: master `f9b85de141ec89ded2fb028456da043019b92ec3`. This reads the five
Phase 3 exit criteria against landed evidence. It declares nothing: the M3 exit
declaration is Antonio's. Evidence detail lives in
[docs/build_graph.md](build_graph.md); this file is the per-criterion reading.

## Criterion 1: 100% recall on a labeled fixture; precision reported, not optimized

**Partial.** Shown on Linux: recall 9/9 on the labeled fixtures
(`graph_basic`, `graph_mixed`, `graph_incl`, plus the later hardening fixtures),
with mutants killed by their exact predicted sets. On this repository, the
bounded relink oracle (S5, 16-file stratified local sample, Linux, GCC,
Makefiles) measured 0 missed mapped tests and aggregate precision 548/1027 =
0.5336. Not shown: recall on Windows (every build-graph test prints SKIP there);
recall as a census on this or any real repository (S5 is a bounded sample, and
its needed-tests oracle sees relinked executables only, not data-file reads);
precision is fixture-1.00 and sample-0.53, reported, not tuned.

## Criterion 2: unknown generators, dynamic dependencies or missing metadata select the full gate

**Shown on Linux fixtures, unmeasured on Windows.** The full gate is selected
for: no File API reply, a generator other than Unix Makefiles or Ninja,
codemodel major version other than 2, several configurations with none chosen,
an unmapped test without a sidecar declaration, unreadable CTest JSON, a changed
CMake file, a changed file no target or includer claims, dynamic (macro)
includes (S6), spliced or comment-split include spellings and other malformed
directives (S7), an unreadable or malformed sidecar, and an unknown file
(`always` sidecar entries do not absorb unknowns). Each rule has a fixture case
and the mutants are killed by exact sets. The multi-configuration check ran
only where ninja exists and is otherwise unmeasured.

## Criterion 3: clean builds with GCC, Clang and MSVC in CI; sanitizer jobs report zero findings on the gated suite

**Partial.** In the push gate (`ci.yml`): GCC (Linux) and MSVC (msvc and
asan-msvc, the gated sanitizer job) are green on the final SHA. Clang and the
Linux ASan/UBSan job live in `compilers.yml`, which is manual plus a nightly
cron, not the gate; folding them in is Antonio's call. Evidence there:
scheduled runs green 2026-10-07 to 2026-10-16, red 2026-10-17 to 2026-10-19
(two stacked Clang-only defects: a missing `FsRemoveFile` include from
`88a540d`, fixed in `c81de2f`; and a `-Werror,-Wstring-concatenation` failure
on the intentional split French literal in `src/i18n.c`, masked by the first
defect, fixed in `51ad357`), green again on 2026-10-20: the scheduled run
[39394093529](https://github.com/FiveTechSoft/symbols/actions/runs/39394093529) (head `51ad357`,
both jobs success) and the manual verification run
[39391381278](https://github.com/FiveTechSoft/symbols/actions/runs/39391381278).
Not shown: a stable stretch of green nightlies after the fixes (one green run
is not a stretch), Clang in the push gate, a Clang sanitizer job, Windows Clang.

## Criterion 4: test reports distinguish not-run, skipped, passed, failed, timed out, crashed

**Partial.** `tools/ctest_report.py` classifies the six states plus `unknown`
from `ctest -T Test` XML (JUnit cannot distinguish them on CTest 3.22.1), shown
on Linux labeled fixtures with mutants (S3) and hardened against malformed XML,
duplicate keys and missing critical measurements (S8 to S12). Nothing in CI
runs it on the real suite's results; Windows reason strings are unmeasured and
would land in `unknown`; an external kill (OOM, CI time limit) is
indistinguishable from a crash.

## Criterion 5: performance claims produced by versioned benchmark commands

**Not shown.** No benchmark runner is landed; criterion 5 is untouched. The
README metrics block carries no performance claims (regenerated 2026-10-20 for
subject `f9b85de`; it states CI and developer-contract evidence only).

## Summary

| Criterion | Status | Main gap |
|---|---|---|
| 1 fixture recall / reported precision | Partial | Windows unmeasured; no real-repo census |
| 2 full-gate fallback | Shown on Linux fixtures | Windows unmeasured |
| 3 GCC/Clang/MSVC + sanitizers in CI | Partial | Clang and Linux sanitizers not in the gate (Antonio's call); post-fix nightly stretch pending |
| 4 six-state test reports | Partial | fixture-level only; not run on the real suite in CI; Windows unmeasured |
| 5 versioned benchmarks | Not shown | no benchmark runner exists |

M3 stays **partial, exit not declared**. Nothing here changes what the agent
does; no `src/` change is part of this reading.
