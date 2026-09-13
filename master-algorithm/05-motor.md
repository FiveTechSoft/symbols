# 05 — Motor autodidáctico multi-mundo (híbrido Python + Prolog)

**User (Anto):** *un pequeño motor que aprende y aprende por sí solo… absorbiendo inteligencia de todo.*

**Builds on:** `fibonacci/miner.py`, `autodidact/loop.py` / `03-autodidact.md`, `geometry/engine.py`.

## Architecture

```
                 ┌─────────────────────────────────────┐
   tick/status   │  Python kernel (UCB over world×family)│
                 │  score, expand/unlock, baselines      │
                 └──────────────┬────────────────────────┘
                                │ hypothesize / verify
          ┌─────────────────────┼─────────────────────┐
          ▼                     ▼                     ▼
     sequences              logic                 geometry
   Fib/Lucas/Pell        bit tables           ProofEngine
          │                     │                     │
          └─────────┬───────────┴──────────┬──────────┘
                    ▼                      ▼
           PrologCritic (swipl)     theory.pl grows
           holds_rec / holds_bit_fn / lemma/3 / rejected/2
```

- **Archive** = Horn theory on disk (`motor/archive/theory.pl`) + JSON meta for arms/curves.
- **Critic** = resolution query; reject = finite fail + optional counterexample.
- **Transfer** = `rec(fib,[1,1])` reused as prior on Lucas/Pell via `companion/2`.
- **Honesty:** human-designed family catalog; no LLM learner; SWI-Prolog 9.2.9 used as backend.

## Real run (CPU, `--steps 40 --reset`)

Elapsed ≈ **4.0 s**. Backend: `swipl`.

### Intelligence curves (samples)

| tick | n_verified_facts | n_distinct_types | transfer_accuracy | lemma_reuse_rate | reject_rate |
|------|------------------|------------------|-------------------|------------------|-------------|
| 1    | 2                | 2                | 0.000             | 0.000            | 0.000       |
| 5    | 4                | 5                | 0.000             | 1.000            | 0.714       |
| 10   | 7                | 7                | 0.000             | 1.000            | 0.650       |
| 20   | 22               | 9                | 0.000             | 0.800            | 0.488       |
| 30   | 40               | 15               | 0.500             | 0.833            | 0.444       |
| 40   | 40               | 15               | 0.500             | 0.833            | 0.444       |

`transfer_accuracy = 0.5` averages Lucas hit (1.0) and Pell miss (0.0) under the Fib prior — intentional.

### Transfer vs baseline

| Target | Transfer (Fib prior) | Baseline B wrong `[2,0]` | Beats baseline? |
|--------|----------------------|-------------------------|-----------------|
| Lucas  | **1.0**              | 0.0                     | **yes** (absorbed) |
| Pell   | **0.0**              | ≈0.04                   | **no** (expected; different law `[2,1]`) |

Baseline **A** (more data, frozen language): N=20→40 adds **0** new relation types.

### Persistence

Second process `tick --steps 5` continued from disk: **total_steps 40→45**, verified **40→41**, `theory.pl` kept growing. `python -m motor status` shows the Horn theory preview.

## Verdict

The engine got **measurably smarter across ticks**: verified facts and distinct types rose (2→40 facts, 2→15 types by tick 30–40) while reject_rate fell from curiosity waste toward a steadier critic load; geometry lemma reuse stayed high (~0.83). It **did absorb intelligence** in the narrow sense required — a Fib `rec/2` clause transferred to Lucas and beat the no-archive baseline — and **honestly failed** to transfer that same clause to Pell, which is the correct scientific outcome, not a hidden success. Scheduler+critic+growing Horn archive beat “just read more Fibonacci terms with a frozen language.” It remains a toy with a human catalog, not open-ended mathematical discovery.
