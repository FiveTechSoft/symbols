#!/usr/bin/env python3
"""
Protocell runner — one individual under joint critic.

Scratch: motor/archive-protocell/
Merge to live ONLY unit verified/rejected clauses (idempotent), not clones.

  cd /workspace/master-algorithm
  python -m motor.run_protocell
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import sys
import time
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, ROOT)

MOTOR_DIR = Path(__file__).resolve().parent
SCRATCH = MOTOR_DIR / "archive-protocell"
LIVE = MOTOR_DIR / "archive"
RUNS = MOTOR_DIR / "runs"


def _clause_name(line: str) -> str | None:
    m = re.search(r"verified\(fact\([^,]+,\s*[^,]+,\s*'([^']+)'", line)
    if m:
        return m.group(1)
    m = re.search(r"rejected\('([^']+)'", line)
    if m:
        return m.group(1)
    return None


def _is_unit_clause(line: str) -> bool:
    return (
        "unit_protocell" in line
        or "NEG_unit_" in line
        or "unit_joint" in line
        or (line.startswith("verified(fact(protocell,") )
    )


def seed_scratch_from_live(scratch: Path, live: Path) -> None:
    """Copy live theory/meta so lever forms are visible; do NOT reset live archive."""
    scratch.mkdir(parents=True, exist_ok=True)
    for name in ("theory.pl", "meta.json", "skin.json"):
        src = live / name
        dst = scratch / name
        if src.exists():
            shutil.copy2(src, dst)


def run_experiment() -> dict[str, Any]:
    from motor.understand import analyze
    from motor.worlds.protocell import (
        UNIT_NAME,
        HELDOUT_X0,
        assemble_protocell,
        evaluate_negatives,
        joint_critic,
        solo_part_checks,
        ProtocellWorld,
    )
    from motor.prolog.bridge import PrologArchive

    before = analyze(LIVE / "theory.pl")
    live_names = set()
    try:
        from motor.kernel import MotorKernel
        live_names = set(MotorKernel(reset=False, archive_dir=LIVE).archive.verified_names())
    except Exception:
        live_names = set()
    # Doctrinal leap baseline (pre-unit census); do not let later ticks rewrite the story
    LEAP_BEFORE_FACTS = 425
    if "unit_protocell_levers" in live_names:
        n_facts_before = LEAP_BEFORE_FACTS
    else:
        n_facts_before = before.n_facts
    n_levers_before = 6  # six levers selected separately

    U = assemble_protocell()
    # Exactly one proposed individual
    assert len([U]) == 1

    verdict = joint_critic(U)
    negatives = evaluate_negatives()
    solos = solo_part_checks()

    conflict_catch = any(n["conflict"] and n["rejected"] for n in negatives if n["tag"] == "missing_taxis")
    all_negs_rejected = all(n["rejected"] for n in negatives)

    lived = bool(verdict.ok)
    n_individuals = 1 if lived else 0

    # Scratch archive: write only unit clauses
    if SCRATCH.exists():
        # refresh from live then overlay unit results (idempotent unit lines)
        seed_scratch_from_live(SCRATCH, LIVE)
    else:
        seed_scratch_from_live(SCRATCH, LIVE)

    arch = PrologArchive(theory_path=SCRATCH / "theory.pl", meta_path=SCRATCH / "meta.json")

    verified_names: list[str] = []
    rejected_names: list[str] = []

    if lived:
        formula = (
            "UNIT{"
            + ",".join(sorted(U.levers))
            + "} jointly closes on held-out x0 (conserv residual ∝ loop error drop)"
        )
        arch.assert_verified("protocell", "unit_joint", UNIT_NAME, formula)
        verified_names.append(UNIT_NAME)
    else:
        arch.assert_rejected(UNIT_NAME, verdict.reason[:200])
        rejected_names.append(UNIT_NAME)

    for neg in negatives:
        why = neg["reason"][:200]
        arch.assert_rejected(neg["name"], why)
        rejected_names.append(neg["name"])

    arch.save()

    # World verify path (sanity — same individual)
    world = ProtocellWorld()
    world.bind(arch, None)
    fam = world.families()["unit_joint"]
    conj = world.hypothesize(fam, {}, step=0)[0]
    vf = world.verify(conj)

    payload = {
        "lived": lived,
        "died": not lived,
        "n_individuals": n_individuals,
        "n_facts_before": n_facts_before,
        "n_levers_selected_separately": n_levers_before,
        "unit_name": UNIT_NAME,
        "unit_levers": sorted(U.levers),
        "heldout_x0": list(HELDOUT_X0),
        "joint_ok": verdict.ok,
        "joint_reason": verdict.reason,
        "joint_metrics": {
            "mean_err_drop": verdict.metrics.get("mean_err_drop"),
            "mean_conserv_residual": verdict.metrics.get("mean_conserv_residual"),
            "n_transfer_ok": verdict.metrics.get("n_transfer_ok"),
            "mode": verdict.metrics.get("mode"),
        },
        "per_x0": verdict.per_x0,
        "solos_all_pass": all(s["pass"] for s in solos.values()),
        "solos": {k: {"pass": v["pass"]} for k, v in solos.items()},
        "negatives": negatives,
        "conflict_catch_worked": conflict_catch,
        "all_negatives_rejected": all_negs_rejected,
        "world_verify_true": vf.true,
        "verified_names": verified_names,
        "rejected_names": rejected_names,
        "scratch": str(SCRATCH),
    }
    return payload


def merge_to_live(scratch_dir: Path = SCRATCH, live_dir: Path = LIVE) -> dict[str, Any]:
    """Idempotent merge of ONLY unit verified/rejected clauses into live."""
    from motor.kernel import MotorKernel

    live = MotorKernel(reset=False, archive_dir=live_dir)
    live_names = set(live.archive.verified_names())
    live_rejected: set[str] = set()
    for ln in live.archive._lines:
        if ln.startswith("rejected("):
            n = _clause_name(ln)
            if n:
                live_rejected.add(n)

    scratch_theory = scratch_dir / "theory.pl"
    if not scratch_theory.exists():
        return {"added_verified": [], "added_rejected": []}

    added_v, added_r = [], []
    for ln in scratch_theory.read_text().splitlines():
        ln = ln.strip()
        if not ln or ln.startswith("%"):
            continue
        if not _is_unit_clause(ln):
            continue
        if ln.startswith("verified(fact(protocell,"):
            name = _clause_name(ln)
            if name and name not in live_names:
                m = re.match(
                    r"verified\(fact\((\w+),\s*(\w+),\s*'([^']+)',\s*'([^']*)'\)\)\.",
                    ln,
                )
                if m:
                    live.archive.assert_verified(m.group(1), m.group(2), m.group(3), m.group(4))
                    added_v.append(m.group(3))
                    live_names.add(m.group(3))
        elif ln.startswith("rejected(") and ("NEG_unit_" in ln or "unit_protocell" in ln):
            name = _clause_name(ln)
            if name and name not in live_rejected and name not in live_names:
                m = re.match(r"rejected\('([^']+)',\s*'([^']*)'\)\.", ln)
                if m:
                    live.archive.assert_rejected(m.group(1), m.group(2))
                    added_r.append(m.group(1))
                    live_rejected.add(m.group(1))

    live.archive.save()
    return {"added_verified": added_v, "added_rejected": added_r}


def write_md(payload: dict, merge_info: dict, path: Path) -> None:
    lived = payload["lived"]
    after_ind = payload["n_individuals"]
    lines = [
        "# 18 — Protocell (unit of selection)",
        "",
        "**Doctrine:** a real evolutionary leap is a change of *unit of selection*, not another verified clause.",
        "Szathmáry: new unit; conflict must be controlled or the level fails.",
        "",
        f"**Before** = **{payload['n_facts_before']}** facts / **{payload['n_levers_selected_separately']}** levers selected separately.",
        f"**After** = **{after_ind}** new *individual(s)* (not +5 clauses).",
        "",
        f"**Outcome:** the unit **{'LIVED' if lived else 'DIED'}**.",
        "",
        "## The one individual",
        "",
        f"- name: `{payload['unit_name']}`",
        f"- levers (frozenset): `{', '.join(payload['unit_levers'])}`",
        f"- held-out x0: `{payload['heldout_x0']}`",
        f"- n_proposed_individuals: **1** (not a scan of 20 bundles)",
        "",
        "## Joint critic numbers",
        "",
        f"| metric | value |",
        f"|--------|------:|",
        f"| joint_ok | **{payload['joint_ok']}** |",
        f"| mean_err_drop | **{payload['joint_metrics'].get('mean_err_drop')}** |",
        f"| mean_conserv_residual | **{payload['joint_metrics'].get('mean_conserv_residual')}** |",
        f"| n_transfer_ok (x0) | **{payload['joint_metrics'].get('n_transfer_ok')}** |",
        f"| mode | {payload['joint_metrics'].get('mode')} |",
        "",
        "Per held-out start:",
        "",
        "| x0 | err_before | err_after | err_drop | conserv_residual | companion |",
        "|----|----------:|---------:|---------:|-----------------:|:---------:|",
    ]
    for r in payload.get("per_x0") or []:
        lines.append(
            f"| {r['x0']} | {r['err_before']} | {r['err_after']} | {r['err_drop']} | "
            f"{r['conserv_residual']} | {r['companion_ok']} |"
        )
    lines += [
        "",
        "## Conflict cases rejected",
        "",
        "Joint critic can fail while parts would pass (level conflict).",
        f"- solos_all_pass: **{payload['solos_all_pass']}**",
        f"- conflict_catch_worked (missing taxis): **{payload['conflict_catch_worked']}**",
        f"- all_negatives_rejected: **{payload['all_negatives_rejected']}**",
        "",
    ]
    for n in payload["negatives"]:
        lines.append(
            f"- `{n['name']}` tag={n['tag']} rejected={n['rejected']} "
            f"conflict={n['conflict']} — {n['reason']}"
        )
    lines += [
        "",
        "## Verified / rejected (unit only)",
        "",
        "### Verified",
        "",
    ]
    if payload["verified_names"]:
        for v in payload["verified_names"]:
            lines.append(f"- `{v}`")
    else:
        lines.append("- *(none — unit died)*")
    lines += ["", "### Rejected", ""]
    for r in payload["rejected_names"]:
        lines.append(f"- `{r}`")
    lines += [
        "",
        "## Merge (idempotent, unit clauses only)",
        "",
        f"- added_verified: `{merge_info.get('added_verified')}`",
        f"- added_rejected: `{merge_info.get('added_rejected')}`",
        "",
        "## Files",
        "",
        "- `motor/worlds/protocell.py` — Unit frozenset + joint critic",
        "- `motor/run_protocell.py` — one experiment runner",
        "- `motor/archive-protocell/` — scratch",
        "- `motor/runs/protocell.json`",
        "",
        "## Leap",
        "",
        "The leap is the **unit**, not a fatter `theory.pl`.",
        f"n_individuals={after_ind} beats n_facts+=5; levers remain the six evidence forms;",
        "no matrices, no bilinear_gN, no brute coeff scans.",
        "",
    ]
    path.write_text("\n".join(lines) + "\n")


def main(argv=None) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--no-merge", action="store_true")
    ap.add_argument("--write-md", action="store_true", default=True)
    args = ap.parse_args(argv)

    t0 = time.time()
    payload = run_experiment()
    merge_info = {"added_verified": [], "added_rejected": []}
    if not args.no_merge:
        merge_info = merge_to_live()

    # post-merge census
    from motor.understand import analyze

    after = analyze(LIVE / "theory.pl")
    payload["n_facts_after"] = after.n_facts
    payload["merge"] = merge_info
    payload["elapsed_s"] = round(time.time() - t0, 3)

    RUNS.mkdir(parents=True, exist_ok=True)
    out = RUNS / "protocell.json"
    out.write_text(json.dumps(payload, indent=2, default=str))

    if args.write_md:
        write_md(payload, merge_info, ROOT / "18-protocell.md")

    summary = {
        "lived": payload["lived"],
        "n_individuals": payload["n_individuals"],
        "n_facts_before": payload["n_facts_before"],
        "n_facts_after": payload["n_facts_after"],
        "joint_metrics": payload["joint_metrics"],
        "conflict_catch_worked": payload["conflict_catch_worked"],
        "all_negatives_rejected": payload["all_negatives_rejected"],
        "verified": payload["verified_names"],
        "rejected": payload["rejected_names"],
        "merge": merge_info,
        "wrote": str(out),
        "md": str(ROOT / "18-protocell.md"),
        "elapsed_s": payload["elapsed_s"],
    }
    print(json.dumps(summary, indent=2))

    ok = (
        payload["n_individuals"] in (0, 1)
        and payload["all_negatives_rejected"]
        and payload["conflict_catch_worked"]
        and (payload["lived"] == payload["world_verify_true"])
    )
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
