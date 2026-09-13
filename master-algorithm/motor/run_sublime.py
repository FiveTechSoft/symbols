#!/usr/bin/env python3
"""
Run the sublime generative motor on motor/archive-sublime/ with molt loop.

Does NOT touch the live archive (mouth reads motor/archive/).
Does NOT edit talk.py / __main__.py.

  cd /workspace/master-algorithm
  python -m motor.run_sublime
  # or:
  python motor/run_sublime.py --ticks 12 --min-molts 3 --max-molts 8
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(description="Sublime motor + molt loop on scratch archive")
    p.add_argument("--archive", default="motor/archive-sublime")
    p.add_argument("--ticks", type=int, default=12, help="ticks per batch (warmup + each molt)")
    p.add_argument("--min-molts", type=int, default=3)
    p.add_argument("--max-molts", type=int, default=8)
    p.add_argument("--no-reset", action="store_true", help="continue scratch archive")
    args = p.parse_args(argv)

    from motor.molt import run_molt_loop
    from motor.kernel import MotorKernel

    # Sanity: default archive path still works
    try:
        k0 = MotorKernel(reset=False)
        _ = k0.archive.theory_path
    except Exception as e:
        print(f"warning: default kernel probe failed: {e}", file=sys.stderr)

    result = run_molt_loop(
        archive_dir=args.archive,
        batch_ticks=args.ticks,
        min_molts=args.min_molts,
        max_molts=args.max_molts,
        reset=not args.no_reset,
    )

    # Write markdown report
    report = _write_report(result)
    print(json.dumps({
        "molts_kept": result["molts_kept"],
        "molts_reverted": result["molts_reverted"],
        "metrics_final": result["metrics_final"],
        "wrote": [
            "motor/runs/sublime.json",
            "motor/runs/molt.json",
            "10-sublime-kernel.md",
        ],
        "report_chars": len(report),
        "elapsed_s": result["elapsed_s"],
    }, indent=2))
    return 0


def _write_report(result: dict) -> str:
    before = result["metrics_before"]
    warm = result["metrics_after_warmup"]
    final = result["metrics_final"]
    te = result.get("transfer_eval") or {}
    lines = []
    lines.append("# 10 — Sublime kernel: generative language + molt loop")
    lines.append("")
    lines.append("**Scratch archive:** `motor/archive-sublime/` (live `motor/archive/` untouched).")
    lines.append("**Critic:** Prolog `holds_rec` / `holds_period` / finite bilinear checks — no Python-fiat `verified/1`.")
    lines.append("")
    lines.append("## What changed vs the toy catalog")
    lines.append("")
    lines.append("Before: UCB scheduled a **human-named** family menu (`bilinear_fib` → canned Cassini string).")
    lines.append("After: a **HypothesisLanguage** (skin) proposes candidates from **operators**:")
    lines.append("")
    lines.append("- `linrec_scan` — scan coeff space; order grows; novelty bonus escapes `[1,1]`-only exploit")
    lines.append("- `modperiod_schema` — search modulus `m`, discover period (not a fixed Pisano table)")
    lines.append("- `bilinear_schema` — search offset/`r` forms (Cassini-shaped / Catalan-like) + bogus reject")
    lines.append("- `geo_invent` — `mutate_construction`; lemmas archived only if the engine proves them")
    lines.append("- `transfer_horn` — Fib `rec/2` → Lucas/Pell (intelligence metric)")
    lines.append("")
    lines.append("## Before / after metrics")
    lines.append("")
    lines.append("| stage | n_verified | n_types | n_schema_classes | transfer_acc | lucas | pell | reject_rate |")
    lines.append("|-------|------------|---------|------------------|--------------|-------|------|-------------|")
    for label, m in [("empty", before), ("after warmup", warm), ("final (post-molt)", final)]:
        lines.append(
            f"| {label} | {m.get('n_verified')} | {m.get('n_types')} | {m.get('n_schema_classes')} | "
            f"{m.get('transfer_accuracy')} | {m.get('lucas_transfer')} | {m.get('pell_transfer')} | "
            f"{m.get('reject_rate')} |"
        )
    lines.append("")
    lines.append(
        f"Molts: **{result['molts_kept']} kept**, **{result['molts_reverted']} reverted** "
        f"(attempted ≤ {result['max_molts']}, min {result['min_molts']})."
    )
    lines.append("")
    lines.append("### Transfer honesty")
    lines.append("")
    lucas = te.get("lucas", {})
    pell = te.get("pell", {})
    lines.append(
        f"- Fib→Lucas transfer_accuracy = **{lucas.get('transfer_accuracy', final.get('lucas_transfer'))}** "
        f"(must succeed / ~1.0)"
    )
    lines.append(
        f"- Fib→Pell transfer_accuracy = **{pell.get('transfer_accuracy', final.get('pell_transfer'))}** "
        f"(must fail honestly / ~0; Pell law is `[2,1]`)"
    )
    lines.append(f"- summary: `{te.get('summary')}`")
    lines.append("")
    lines.append("## Molt history")
    lines.append("")
    for entry in result.get("molt_log", []):
        if entry.get("action") == "stop":
            lines.append(f"- stop: {entry.get('reason')}")
            continue
        lines.append(
            f"- **molt {entry.get('molt_index')}** gen={entry.get('generation')} "
            f"→ `{entry.get('action')}` | {entry.get('keep_reason')}"
        )
        lines.append(f"  - saturation: {entry.get('saturation_reason')}")
        lines.append(f"  - actions: {entry.get('molt_actions')}")
        mb, ma = entry.get("metrics_before") or {}, entry.get("metrics_after") or {}
        lines.append(
            f"  - Δ verified {mb.get('n_verified')}→{ma.get('n_verified')}, "
            f"types {mb.get('n_types')}→{ma.get('n_types')}, "
            f"schemas unlocked {entry.get('unlocked_before')}→{entry.get('unlocked_after')}"
        )
    lines.append("")
    lines.append("## 5 example clauses (verified or rejected)")
    lines.append("")
    for i, ex in enumerate(result.get("example_clauses") or [], 1):
        lines.append(f"{i}. **{ex.get('status')}**: `{ex.get('clause')}`")
        lines.append(f"   - reason: {ex.get('reason')}")
    lines.append("")
    lines.append("## Remaining honest limits (still human-designed)")
    lines.append("")
    lines.append("- Seed operator set (`SEED_SCHEMAS`) is authored — molt unlocks/grows params, does not invent analysis.")
    lines.append("- Geometry axiom set in `geometry/engine.py` is fixed; invent mutates constructions inside that closure.")
    lines.append("- Companion graph `fib/lucas/pell` is given; transfer tests reuse, not discovery of which sequences exist.")
    lines.append("- UCB + molt heuristics are bandit rules, not a full DreamCoder/NEAT.")
    lines.append("- Scratch archive only; mouth still reads live `motor/archive/`.")
    lines.append("")
    lines.append("## How to run")
    lines.append("")
    lines.append("```bash")
    lines.append("cd /workspace/master-algorithm")
    lines.append("python -m motor.run_sublime --ticks 12 --min-molts 3 --max-molts 8")
    lines.append("# default live archive still works:")
    lines.append("python -m motor tick --steps 5")
    lines.append("# artifacts: motor/runs/sublime.json, motor/runs/molt.json, motor/archive-sublime/")
    lines.append("```")
    lines.append("")
    lines.append(f"Elapsed: {result.get('elapsed_s')}s. {result.get('honesty')}")
    lines.append("")

    text = "\n".join(lines)
    out = ROOT / "10-sublime-kernel.md"
    out.write_text(text, encoding="utf-8")
    return text


if __name__ == "__main__":
    raise SystemExit(main())
