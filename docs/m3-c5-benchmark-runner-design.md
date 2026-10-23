# M3 criterion 5: benchmark runner design (design only)

Base: master `1b222016c179a6e71454c9eeb9edc9bbe4f28420`. This file is a
design; it lands no runner, changes no benchmark and claims no figure.
Phase 3 exit criterion 5 (ROADMAP): performance claims in `README.md` are
produced by versioned benchmark commands, with median and tail latency plus
memory high-water mark.

## Current state

- `bench_1m` (`tests/bench_1m_relations.c`) and `bench_stress`
  (`tests/bench_stress_50k.c`) build but are not ctest tests.
  `tests/bench_tagged_term.c` is not wired into CMake at all.
- `bench_1m` reports single-run aggregates only: mean latency as total/N,
  no median, no tail, no repetitions. Memory is a struct-size estimate, not
  a measured high-water. Its summary prints `RelationCount(relations)` after
  `RelationTableDestroy(relations)`, a use-after-free: the summary counter is
  internally inconsistent (README section 4.5 names this defect). The
  `inserted` counter counts attempts, not unique inserts; `duplicates` is
  dead.
- No benchmark records toolchain, OS, CPU, configuration, corpus version or
  seed. No figure is reproducible as a versioned command.

## Design: contract split

The benchmark executable owns the workload; the runner owns measurement.
A benchmark prints one machine line per repetition on stdout:

```
SAMPLE <phase> <nanoseconds-integer>
```

followed by a final line `BENCH_DONE` and exit 0. Human-readable lines are
allowed and ignored by the parser. `phase` is `[a-z][a-z0-9_]{0,31}` declared
up front. The runner is `tools/bench_runner.py`, stdlib-only Python:

```
python3 tools/bench_runner.py --exe <path> --build-dir <dir> \
    --fixture-id <id> --corpus-version <string-or-sha256> \
    --min-samples 30 --out record.json
```

The runner spawns the executable once per measurement round (default one
round), parses SAMPLE lines strictly, and refuses to write a claim-grade
record when: exit code nonzero, `BENCH_DONE` missing, a SAMPLE line is
malformed or has an undeclared phase, a phase has fewer than `--min-samples`
samples, or a required environment field is unreadable. Refusal means exit
nonzero and no record file. A diagnostic record with `complete:false` is
only written under `--allow-incomplete` and is never claim input.

## Record schema v1 (strict JSON, metrics_snapshot style)

Exact keys, unique keys enforced, bounded printable ASCII text, no bool as
int, NaN/Infinity rejected:

```
{version:1, subject_sha, date_utc, command:[...], executable_sha256,
 fixture:{id, corpus_version}, repetitions, min_samples,
 environment:{os, os_release, arch, cpu, toolchain, build_type, ci},
 phases:{<phase>:{n, unit:"ns", min, median, p95, p99, max, mean, stdev}},
 memory:{peak_rss_bytes, method}, complete:true, limitations:[...]}
```

`command` is the exact argv including every parameter, so the record is its
own versioned command. `executable_sha256` binds the measured bytes.
`subject_sha` is the git HEAD of the source tree. Statistics: median and
p95/p99 percentiles (nearest-rank), mean and stdev over integer ns samples.

## Environment acquisition (stdlib only, fail-closed per field)

- os/arch: `platform.system()`, `platform.release()`, `platform.machine()`.
- cpu: Linux `/proc/cpuinfo` `model name`; macOS `sysctl -n
  machdep.cpu.brand_string`; Windows `winreg` `ProcessorNameString`.
- toolchain/build_type: `CMAKE_C_COMPILER_ID`, `CMAKE_C_COMPILER_VERSION`
  and `CMAKE_BUILD_TYPE` from `<build-dir>/CMakeCache.txt`; explicit
  `--toolchain` / `--build-type` flags override. No cache and no flag means
  refusal.
- ci: `true` only when `GITHUB_ACTIONS=true`, plus `RUNNER_OS` when present.

## Memory high-water

Measured from outside the benchmark, per process. Linux/macOS: `os.wait4`
gives child `ru_maxrss` (KiB on Linux, bytes on macOS; normalized to bytes).
Windows: `GetProcessMemoryInfo` on the still-open child handle after exit,
`PeakWorkingSetSize`. Unknown platform: `method:"unsupported"`, refusal for
claim-grade records. The struct-size estimate pattern in `bench_1m` is not a
memory measurement and does not appear in records.

## Integration with the evidence pipeline

`tools/metrics_snapshot.py` stays the only README metrics writer and stays
curated: a record JSON is evidence input for a human-reviewed
`technical_result` entry, never auto-merged. The record renders its
environment into the snapshot cap (160 ASCII chars, no pipes/backticks) as
`<toolchain> <build_type>, <os> <arch>, <cpu>, n=<repetitions>`; the value
field carries `median <x> ns, p99 <y> ns, peak RSS <z> MiB`. `source_url`
references the commit that lands the record. The runner itself performs no
measurement of anything but its child process and no network, build or
solver actions.

## Limits, stated up front

- One machine, one run: no CPU pinning, isolation or cross-machine
  comparison. Records are development measurements, not latency guarantees.
- The runner does not prove any comparative claim; no baseline math.
- Recording the CI environment on the final SHA needs a workflow change,
  which is not part of this design and remains Antonio's call.
- `bench_1m`'s counter fix and contract adaptation are a later slice, not
  part of the runner slice; this design only defines the contract it must
  emit.

## Slice plan

1. C5-a: this design (docs only).
2. C5-b: `tools/bench_runner.py` + `tests/test_bench_runner.py` with a
   synthetic fixture benchmark (known sample set, exact expected statistics)
   and mutants (nonzero exit, missing `BENCH_DONE`, malformed or undeclared
   SAMPLE lines, too few samples, unreadable environment): the runner must
   refuse each. Memory path tested per platform where CI can measure it,
   SKIP elsewhere.
3. C5-c: correct the `bench_1m` harness (use-after-free summary, dead
   counters), emit the SAMPLE contract, and produce the first real record on
   a recorded environment. No README claim lands in this slice.
4. Later, separately reviewed: a curated README entry from a landed record;
   CI environment recording if Antonio approves a workflow change.
