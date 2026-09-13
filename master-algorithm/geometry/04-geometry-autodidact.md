# Geometry autodidact — inventing figures vs staring at one

**Hypothesis (Anto / Master Algorithm):** Fibonacci plateaus because the object is a fixed sequence. Geometry should **not** plateau if the system can invent constructions, prove lemmas, and use those lemmas to invent harder problems (*library growth → new reach*). The mathematician loop is: conjecture → prove/refute → archive lemma → generate a slightly harder frontier problem.

**This is a toy Euclidean autodidact, not AlphaGeometry.** It tests library-growth vs more-of-the-same-figure with a fixed, documented axiom set and a symbolic critic.

## Loop recipe (T = 30, seed = 42)

1. **GENERATE** a construction program (points + midpoints / equilateral / parallelogram ops). Curriculum: midline → medial → isosceles → equilateral → parallelogram → isos+midline → double midline → Varignon; later mutate by adding midpoints.
2. **CONJECTURE** equalities / parallels / half-segments not already archived.
3. **CRITIC:** numeric coordinate check (ε) as sanity filter; a fact enters the library **only** if the symbolic forward-chainer proves it. Numerically false conjectures are rejected.
4. **ARCHIVE** proven lemmas with a name, type, and **schema** (`midline_parallel`, `midline_half`, `isos_base`, `para_opp`, or `raw`).
5. **FRONTIER:** score by “uses a recent lemma family” + “not already proven” + structure (midpoints > random noise).
6. **LIBRARY GROWTH:** once a schema is archived, later proofs apply it **instead of re-hitting the raw axiom**, and record a citation — that citation count is the key measurement.

### Controls

| Mode | Behavior |
|------|----------|
| **Full** | Grow lemma library; later proofs may cite schemas |
| **Baseline A** | Same generator / curriculum; **discard library** (never cite) |
| **Baseline B** | One fixed free triangle forever (Fibonacci-style fixed object) |

## Axiom set (honest, small)

| ID | Statement |
|----|-----------|
| A1 | Midpoint halves: Midpoint(M,A,B) ⇒ AM = MB |
| A2 | Midpoint theorem: Midpoint(M,A,B), Midpoint(N,A,C) ⇒ MN ∥ BC and 2·MN = BC |
| A3 | Isosceles base angles: AB = AC ⇒ ∠ABC = ∠ACB |
| A4 | Isosceles converse: ∠ABC = ∠ACB ⇒ AB = AC |
| A5 | Vertical angles equal when two segments share a midpoint |
| A6–A8 | SSS / SAS / ASA congruence ⇒ corresponding parts equal |
| A9 | Parallelogram opposite sides equal and parallel |
| A10 | Equilateral ⇒ all sides and angles equal |
| A11 | Symmetry + transitivity of EqSeg / EqAng / Parallel |
| A12 | Reflexivity of EqSeg and EqAng |

Domain deliberately shrunk to **midpoints + midline + isosceles/equilateral + parallelogram/Varignon** so lemma reuse is measurable.

## Results (real run, ~23 s CPU)

| Metric | Full (library) | Baseline A (no cite) | Baseline B (fixed △) |
|--------|----------------|----------------------|----------------------|
| Final library size (citable schemas/instances) | **27** | 0 | 0 |
| Unique proven discoveries | 29 | **42** | **0** |
| Distinct lemma *types* | **12** | **18** | **0** |
| Proofs citing a non-axiom lemma | **17** | **0** | **0** |
| % conjectures rejected by critic | 98.1% | 97.8% | 100% |
| Plateau in last 5 steps (discovered count) | late flat at 29 | late flat at 42 | **yes, flat at 0** |

### Library size / discoveries vs step

| Step | Full discovered | Full #types | A discovered | B discovered |
|------|-----------------|-------------|--------------|--------------|
| 0 | 2 | 2 | 2 | 0 |
| 5 | 9 | 7 | 18 | 0 |
| 10 | 13 | 8 | 25 | 0 |
| 15 | 15 | 9 | 34 | 0 |
| 20 | 22 | 9 | 38 | 0 |
| 25 | 29 | 12 | 40 | 0 |
| 29 | 29 | 12 | 42 | 0 |

### Lemma reuse (examples from Full)

| Step | Goal | Cites |
|------|------|-------|
| 5 | AB = CD (parallelogram) | `L5_para_opp` |
| 9 | AC ∥ PQ (Varignon) | `L1_midline_parallel` |
| 9 | PQ ∥ RS (Varignon) | prior Varignon / midline schema |
| demo | BC ∥ MN on isos+midline | `seed_midline_parallel` |
| demo | ∠ABC = ∠ACB on isos+midline | `seed_isos_base` |

Deterministic reuse demo (same engine): after archiving midline + isos schemas, the next figures prove the same facts **with citations**; Baseline A proves them via axioms with **cites = []**.

### Type inventory (Full)

`midline:Parallel/HalfSeg`, `isos:EqAng/EqSeg`, `para:Parallel/EqSeg/EqAng`, `varignon:Parallel/HalfSeg/EqAng`, `equilateral:EqAng/EqSeg` (12 distinct types).

## Interpretation

- **Baseline B plateaus immediately at 0.** A lone free triangle with no midpoints/isos tags yields no archiveable theorems in this language; random side/angle equalities are rejected (100%). Same shape as Fibonacci: fixed object → no new constructions → no growth.
- **Full grows a library and reuses it.** 17 proofs cite archived schemas (midline → Varignon parallels/halves; para opposite sides; isos base angles). That is the “developed math to learn more” signal.
- **Baseline A discovers more raw instance-types (18) but never accumulates.** Same curriculum, zero citations. Breadth without a citable library does not compound into new *methods*.
- **Checker works.** ~98% of sampled conjectures are rejected numerically/symbolically; only symbolically proved facts enter the library.
- **Late plateau of the toy is expected.** After ~step 22 the small domain’s frontier saturates (types stuck at 12). That is finite language exhaustion, not a counterexample to library growth — growth happened *because* constructions expanded (midline → Varignon), then stopped when the toy ran out of schemas.

## Verdict

**Evidence supports the theory in this toy:** inventing constructions and archiving lemmas produces library growth and measurable lemma reuse (17 citing proofs; Varignon reached via midline schemas), while staring at one fixed triangle plateaus at zero — the Fibonacci failure mode. Without a critic almost every conjecture would be noise; without a library Baseline A rediscovers but does not compound; without a frontier curriculum the system would repeat easy midline facts forever. This does **not** claim a general mathematician or AlphaGeometry; it shows that in a closed Euclidean micro-world, **knowledge compounds when proofs can cite earlier lemmas on harder figures**, and does not when the figure is frozen.
