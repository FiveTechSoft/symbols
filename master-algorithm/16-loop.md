# 16 — Closed loop (observe → act → critic)

**Before:** 352 Horn clauses = notebook (flies pass laughing).
**After:** one dimension, critic-gated prediction/control — a loop that closes.

## Numbers

| policy | err before (x0=5.0) | err after |
|--------|------------------------------|-----------|
| bang-bang taxis T=8 | **5.0** | **0.0** |
| linear gain k=0.5 T=10 | **5.0** | **0.015625** |
| always-right (rejected) T=5 | **5.0** | **10.0** |

- prediction law `Δ(position)=action`: pred_err_max ≈ **0.0**
- n_verified (loop): **4**
- n_rejected (loop): **8**
- Δ(position)=action transferred from conserv Δ=0: **True**
- rec[1,1]→position: **miss** (honest miss)

## Verified

- `loop_pred_dx_eq_action_s7069`
- `loop_taxis_bangbang_s7069`
- `loop_taxis_linear_gain_s7069`
- `transfer_conserv_delta0_to_loop_dx_eq_a`

## Rejected

- `NEG_loop_policy_increases_error`
- `loop_pred_double_action_s7069`
- `loop_pred_ignore_action_s7069`
- `loop_pred_overfit_one_x0_s7069`
- `loop_taxis_always_left_s7069`
- `loop_taxis_always_right_s7069`
- `transfer_conserv_to_loop_position_frozen`
- `transfer_rec11_to_loop_position`

## Files

- `motor/worlds/loop.py`
- `motor/archive-loop/` (scratch ticks)
- seeds: `loop_taxis_scan`, `loop_pred_scan`, `loop_transfer_form`, `loop_dead_wrong_policy`

Doctrine: if the loop does not close, it is not a brain.

