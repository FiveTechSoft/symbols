#!/usr/bin/env python3
"""
Closed-loop runner — scratch archive motor/archive-loop/ (live mouth untouched during ticks).

  observe → act → critic(prediction / taxis error)
  Then merge ONLY critic-true + rejected(false policies) into live theory.pl (idempotent).

  cd /workspace/master-algorithm
  python -m motor.run_loop
"""

from __future__ import annotations

import argparse
import json
import re
import sys
import time
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

MOTOR_DIR = Path(__file__).resolve().parent
SCRATCH = MOTOR_DIR / "archive-loop"
LIVE = MOTOR_DIR / "archive"
RUNS = MOTOR_DIR / "runs"


def _prefer_loop(kernel) -> list[str]:
    return [k for k in kernel.arms if k.startswith("loop::")]


def _explicit_pass(kernel) -> dict[str, Any]:
    """Force hypothesize+verify on every unlocked loop family (deterministic coverage)."""
    world = kernel.worlds["loop"]
    results = {"verified": [], "rejected": [], "attempts": []}
    for fid, fam in world.families().items():
        if not fam.unlocked:
            continue
        # use arm copy so param tracks
        key = f"loop::{fid}"
        arm = kernel.arms.get(key, fam)
        conjs = world.hypothesize(arm, kernel.confirmed, step=kernel.total_steps)
        for conj in conjs:
            vf = world.verify(conj)
            vf.step = kernel.total_steps
            outcome = kernel._ingest(vf)
            entry = {
                "name": vf.name,
                "family": vf.family,
                "true": vf.true,
                "outcome": outcome,
                "support": vf.support,
                "counterexample": vf.counterexample,
                "relation_type": vf.relation_type,
                "from_transfer": vf.from_transfer,
            }
            results["attempts"].append(entry)
            if vf.true and outcome in ("new_true", "duplicate_true"):
                results["verified"].append(entry)
            if (not vf.true) and outcome in ("new_reject", "duplicate_reject"):
                results["rejected"].append(entry)
    kernel._persist_arms()
    return results


def _demo_numbers(world) -> dict[str, Any]:
    """Error before vs after bang-bang / linear on a fixed x0 (for 16-loop.md)."""
    from motor.worlds.loop import rollout, policy_bangbang, policy_linear_gain, brightness

    x0 = 5.0
    T = 8
    t_bb = rollout(x0, policy_bangbang, T=T)
    t_lin = rollout(x0, policy_linear_gain(0.5), T=10)
    t_bad = rollout(x0, lambda x, b: 1.0, T=5)
    return {
        "x0": x0,
        "brightness_x0": brightness(x0),
        "bangbang": {
            "err_before": t_bb[0]["err"],
            "err_after": t_bb[-1]["err"],
            "T": T,
        },
        "linear_gain": {
            "err_before": t_lin[0]["err"],
            "err_after": t_lin[-1]["err"],
            "T": 10,
        },
        "always_right": {
            "err_before": t_bad[0]["err"],
            "err_after": t_bad[-1]["err"],
            "T": 5,
        },
    }


def _clause_name(line: str) -> str | None:
    m = re.search(r"verified\(fact\([^,]+,\s*[^,]+,\s*'([^']+)'", line)
    if m:
        return m.group(1)
    m = re.search(r"rejected\('([^']+)'", line)
    if m:
        return m.group(1)
    return None


def merge_to_live(scratch_kernel, live_dir: Path = LIVE) -> dict[str, Any]:
    """Idempotent append of loop verified/rejected into live theory.pl."""
    from motor.kernel import MotorKernel

    live = MotorKernel(reset=False, archive_dir=live_dir)
    live_names = set(live.archive.verified_names())
    live_rejected = set()
    for ln in live.archive._lines:
        if ln.startswith("rejected("):
            n = _clause_name(ln)
            if n:
                live_rejected.add(n)

    added_v, added_r = [], []
    for ln in scratch_kernel.archive._lines:
        if ln.startswith("verified(fact(loop,"):
            name = _clause_name(ln)
            if name and name not in live_names:
                # parse world, family, name, formula
                m = re.match(
                    r"verified\(fact\((\w+),\s*(\w+),\s*'([^']+)',\s*'([^']*)'\)\)\.",
                    ln,
                )
                if m:
                    live.archive.assert_verified(m.group(1), m.group(2), m.group(3), m.group(4))
                    added_v.append(m.group(3))
                    live_names.add(m.group(3))
                else:
                    # fallback: append raw if unique
                    live.archive.append_clause(ln, comment="merged from archive-loop")
                    added_v.append(name or ln[:60])
                    live_names.add(name or ln)
        elif ln.startswith("rejected(") and ("loop_" in ln or "NEG_loop" in ln or "transfer_" in ln and "loop" in ln):
            name = _clause_name(ln)
            if name and name not in live_rejected and name not in live_names:
                m = re.match(r"rejected\('([^']+)',\s*'([^']*)'\)\.", ln)
                if m:
                    live.archive.assert_rejected(m.group(1), m.group(2))
                    added_r.append(m.group(1))
                    live_rejected.add(m.group(1))
                else:
                    live.archive.append_clause(ln, comment="merged reject from archive-loop")
                    added_r.append(name or ln[:60])

    live.archive.save()
    return {"added_verified": added_v, "added_rejected": added_r}


def write_md(payload: dict, path: Path) -> None:
    demo = payload["demo"]
    lines = [
        "# 16 — Closed loop (observe → act → critic)",
        "",
        "**Before:** 352 Horn clauses = notebook (flies pass laughing).",
        "**After:** one dimension, critic-gated prediction/control — a loop that closes.",
        "",
        "## Numbers",
        "",
        f"| policy | err before (x0={demo['x0']}) | err after |",
        "|--------|------------------------------|-----------|",
        f"| bang-bang taxis T={demo['bangbang']['T']} | **{demo['bangbang']['err_before']}** | **{demo['bangbang']['err_after']}** |",
        f"| linear gain k=0.5 T={demo['linear_gain']['T']} | **{demo['linear_gain']['err_before']}** | **{demo['linear_gain']['err_after']}** |",
        f"| always-right (rejected) T={demo['always_right']['T']} | **{demo['always_right']['err_before']}** | **{demo['always_right']['err_after']}** |",
        "",
        f"- prediction law `Δ(position)=action`: pred_err_max ≈ **{payload.get('pred_err_max', 0)}**",
        f"- n_verified (loop): **{payload['n_verified']}**",
        f"- n_rejected (loop): **{payload['n_rejected']}**",
        f"- Δ(position)=action transferred from conserv Δ=0: **{payload['delta_eq_action_transferred']}**",
        f"- rec[1,1]→position: **{payload['rec11_result']}** (honest miss)",
        "",
        "## Verified",
        "",
    ]
    for v in payload["verified_names"]:
        lines.append(f"- `{v}`")
    lines += ["", "## Rejected", ""]
    for r in payload["rejected_names"]:
        lines.append(f"- `{r}`")
    lines += [
        "",
        "## Files",
        "",
        "- `motor/worlds/loop.py`",
        "- `motor/archive-loop/` (scratch ticks)",
        "- seeds: `loop_taxis_scan`, `loop_pred_scan`, `loop_transfer_form`, `loop_dead_wrong_policy`",
        "",
        "Doctrine: if the loop does not close, it is not a brain.",
        "",
    ]
    path.write_text("\n".join(lines) + "\n")


def run(reset_scratch: bool = True, merge: bool = True, ticks: int = 6) -> dict:
    from motor.kernel import MotorKernel
    from motor.worlds.loop import LoopWorld

    t0 = time.time()
    SCRATCH.mkdir(parents=True, exist_ok=True)
    kernel = MotorKernel(reset=reset_scratch, archive_dir=SCRATCH)

    # Seed a fib rec into scratch so transfer can *see* [1,1] prior (honest try)
    if not any(s == "fib" for s, _ in kernel.archive.list_recs()):
        kernel.archive.assert_rec("fib", [1, 1])

    prefer = _prefer_loop(kernel)
    warm = kernel.tick(steps=ticks, prefer_arms=prefer)
    explicit = _explicit_pass(kernel)

    world: LoopWorld = kernel.worlds["loop"]
    demo = _demo_numbers(world)

    verified_names = sorted(
        {
            a["name"]
            for a in explicit["attempts"]
            if a["true"] and a["outcome"] in ("new_true", "duplicate_true")
        }
    )
    rejected_names = sorted(
        {
            a["name"]
            for a in explicit["attempts"]
            if (not a["true"]) and a["outcome"] in ("new_reject", "duplicate_reject")
        }
    )

    delta_xfer = any(
        a["name"] == "transfer_conserv_delta0_to_loop_dx_eq_a" and a["true"]
        for a in explicit["attempts"]
    )
    rec11 = next(
        (a for a in explicit["attempts"] if a["name"] == "transfer_rec11_to_loop_position"),
        None,
    )
    rec11_result = "miss" if rec11 and not rec11["true"] else ("hit" if rec11 and rec11["true"] else "skip")

    pred_err = 0.0
    if isinstance(world.last_metrics.get("pred_err_max"), (int, float)):
        pred_err = world.last_metrics["pred_err_max"]

    merge_info = {"added_verified": [], "added_rejected": []}
    if merge:
        merge_info = merge_to_live(kernel, LIVE)

    payload = {
        "archive_dir": str(SCRATCH),
        "elapsed_s": round(time.time() - t0, 3),
        "ticks": ticks,
        "warm_total_steps": warm.get("total_steps"),
        "demo": demo,
        "pred_err_max": pred_err,
        "n_verified": len(verified_names),
        "n_rejected": len(rejected_names),
        "verified_names": verified_names,
        "rejected_names": rejected_names,
        "delta_eq_action_transferred": delta_xfer,
        "rec11_result": rec11_result,
        "explicit_attempts": explicit["attempts"],
        "merge": merge_info,
        "observe": world.observe(),
    }
    return payload


def main(argv=None) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--ticks", type=int, default=6)
    ap.add_argument("--no-merge", action="store_true")
    ap.add_argument("--no-reset", action="store_true")
    ap.add_argument("--write-md", action="store_true", default=True)
    ap.add_argument("--snapshot", action="store_true", help="python -m motor snapshot dump after merge")
    args = ap.parse_args(argv)

    payload = run(
        reset_scratch=not args.no_reset,
        merge=not args.no_merge,
        ticks=args.ticks,
    )
    out = RUNS / "loop-closed.json"
    RUNS.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(payload, indent=2))

    if args.write_md:
        write_md(payload, ROOT / "16-loop.md")

    summary = {
        "wrote": str(out),
        "n_verified": payload["n_verified"],
        "n_rejected": payload["n_rejected"],
        "verified": payload["verified_names"],
        "rejected": payload["rejected_names"],
        "delta_eq_action_transferred": payload["delta_eq_action_transferred"],
        "rec11_result": payload["rec11_result"],
        "demo": payload["demo"],
        "merge_added_v": payload["merge"]["added_verified"],
        "merge_added_r": payload["merge"]["added_rejected"],
    }
    print(json.dumps(summary, indent=2))

    if args.snapshot:
        from motor.snapshot import dump

        print(json.dumps(dump(), indent=2))

    ok = payload["n_verified"] >= 3 and payload["n_rejected"] >= 3 and payload["delta_eq_action_transferred"]
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
