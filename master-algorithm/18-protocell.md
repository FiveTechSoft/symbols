# 18 — Protocell (unit of selection)

**Doctrine:** a real evolutionary leap is a change of *unit of selection*, not another verified clause.
Szathmáry: new unit; conflict must be controlled or the level fails.

**Before** = **425** facts / **6** levers selected separately.
**After** = **1** new *individual* (not +5 clauses).

**Outcome:** the unit **LIVED**.

## The one individual

- name: `unit_protocell_levers`
- levers (frozenset): `bit_circuit, conserv_delta0, form_gate, loop_taxis, rec_companion, rec_to_delta`
- held-out x0: `[4.5, -3.25]` (new starts; combo not seeded there)
- n_proposed_individuals: **1** (not a scan of 20 bundles)

## Joint critic numbers

| metric | value |
|--------|------:|
| joint_ok | **True** |
| mean_err_drop | **3.25** |
| mean_conserv_residual | **0.0** |
| n_transfer_ok (x0) | **2** |
| mode | bit_gated_taxis |

Per held-out start:

| x0 | err_before | err_after | err_drop | conserv_residual | companion |
|----|----------:|---------:|---------:|-----------------:|:---------:|
| 4.5 | 4.5 | 0.5 | 4.0 | 0.0 | True |
| -3.25 | 3.25 | 0.75 | 2.5 | 0.0 | True |

Conservation residual and loop error co-move: taxis drops `|x|` while `|Δx−a|` stays **0** (same additive number / proportional remainder).

## Conflict cases rejected

Joint critic can fail while parts would pass (level conflict).
- solos_all_pass: **True**
- conflict_catch_worked (missing taxis): **True**
- all_negatives_rejected: **True**

- `NEG_unit_missing_taxis` tag=missing_taxis rejected=True conflict=True — bundle missing taxis (notebook of laws, no loop) — joint fail
- `NEG_unit_offpath_bilinear` tag=off_path_bilinear rejected=True conflict=False — off-path individual (bilinear/energy/matmul) — form_gate reject
- `NEG_unit_offpath_energy` tag=off_path_energy rejected=True conflict=False — off-path individual (bilinear/energy/matmul) — form_gate reject
- `NEG_unit_clone_rec_pad` tag=clone_rec_pad rejected=True conflict=False — clone pad (rec_order) — not a new individual (brute)

## Verified / rejected (unit only)

### Verified

- `unit_protocell_levers`

### Rejected

- `NEG_unit_missing_taxis`
- `NEG_unit_offpath_bilinear`
- `NEG_unit_offpath_energy`
- `NEG_unit_clone_rec_pad`

## Merge (idempotent, unit clauses only)

- first merge: `unit_protocell_levers` + 4 `NEG_unit_*` rejects
- re-run merge: added nothing (idempotent)
- no clones merged; live `motor/archive/` not reset

## Files

- `motor/worlds/protocell.py` — Unit frozenset + joint critic
- `motor/run_protocell.py` — one experiment runner
- `motor/archive-protocell/` — scratch
- `motor/runs/protocell.json`
- `motor/understand.py` — `unit_protocell` tag / `n_individuals`

## Leap

The leap is the **unit**, not a fatter `theory.pl`.
n_individuals=1 beats n_facts+=5; levers remain the six evidence forms;
no matrices, no bilinear_gN, no brute coeff scans.
