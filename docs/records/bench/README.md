# Benchmark records

Benchmark records are produced by `tools/bench_runner.py` over the versioned
benchmark programs (`tools/bench_1m.c`, ...). Each record is dated under
`bench/YYYY-MM-DD/` and is self-describing: `environment` (OS, arch, build
type, toolchain), `phases` (per-phase latency samples with median/p99/max),
and `memory` (peak RSS with its measurement method).

A record with `"complete": true` covers every phase requested in the command
used to produce it and may be cited as a measurement of that exact
environment. A record with `"complete": false` is diagnostic only.

These are development measurements, not latency guarantees, and carry no
comparative or baseline claim.
