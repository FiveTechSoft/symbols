"""CLI: python -m motor tick|status|theory|live|vive|talk|molt|snapshot"""

from __future__ import annotations

import argparse
import json
import os
import sys
from pathlib import Path

MOTOR_DIR = Path(__file__).resolve().parent
ROOT = MOTOR_DIR.parent
BOOKBRAIN = ROOT / "bookbrain"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="motor", description="Motor autodidáctico multi-mundo")
    sub = parser.add_subparsers(dest="cmd", required=True)

    p_tick = sub.add_parser("tick", help="Run K scheduler steps (persistent)")
    p_tick.add_argument("--steps", type=int, default=40)
    p_tick.add_argument("--reset", action="store_true", help="Wipe archive and start fresh")

    sub.add_parser("status", help="Show archive / arms / theory preview")

    p_theory = sub.add_parser("theory", help="Print full Prolog theory.pl")
    p_theory.add_argument("--path", action="store_true", help="Only print path")

    sub.add_parser("live", help="Interactive Master Algorithm via BookBrain (swipl live.pl)")

    p_vive = sub.add_parser("vive", help="Self-improve exam loop (tick + sync + re-exam)")
    p_vive.add_argument("--rounds", type=int, default=2)
    p_vive.add_argument("--tick-steps", type=int, default=5, dest="tick_steps")

    p_molt = sub.add_parser("molt", help="Self-modify talk.py skins until exam worthy")
    p_molt.add_argument("--max", type=int, default=8, help="Max molt rounds")

    p_talk = sub.add_parser("talk", help="Natural-language mouth over theory.pl")
    p_talk.add_argument("-q", dest="question", help="One-shot question")
    p_talk.add_argument("--growth", action="store_true", help="Ask creciste")

    p_snap = sub.add_parser("snapshot", help="Dump/verify recoverable binary of learned state")
    p_snap.add_argument("action", nargs="?", default="dump", choices=["dump", "verify", "restore"])
    p_snap.add_argument("--dest", help="restore destination motor dir")

    args = parser.parse_args(argv)

    if args.cmd == "molt":
        from motor.molt_talk import run_molt, _worthy
        payload = run_molt(max_molts=args.max)
        return 0 if _worthy(payload["final_score"]) else 1

    if args.cmd == "talk":
        from motor.talk import main as talk_main

        talk_argv: list[str] = []
        if args.growth:
            talk_argv.append("--growth")
        if args.question:
            talk_argv.extend(["-q", args.question])
        return talk_main(talk_argv)

    if args.cmd == "live":
        live_pl = BOOKBRAIN / "live.pl"
        if not live_pl.exists():
            print(f"missing {live_pl}", file=sys.stderr)
            return 1
        cmd = ["swipl", "-q", "-s", str(live_pl), "-g", "live"]
        os.chdir(BOOKBRAIN)
        os.execvp(cmd[0], cmd)

    if args.cmd == "vive":
        from motor.vive import run_vive

        run_vive(rounds=args.rounds, tick_steps=args.tick_steps)
        return 0

    from motor.kernel import MotorKernel

    if args.cmd == "tick":
        k = MotorKernel(reset=args.reset)
        latest = k.tick(steps=args.steps)
        print(json.dumps({
            "total_steps": latest["total_steps"],
            "ticks_this_run": latest["ticks_this_run"],
            "n_verified_facts": latest["n_verified_facts"],
            "n_distinct_types": latest["n_distinct_types"],
            "transfer_accuracy": latest.get("transfer_accuracy"),
            "lemma_reuse_rate": latest.get("lemma_reuse_rate"),
            "reject_rate": latest.get("reject_rate"),
            "transfer_eval_summary": latest.get("transfer_eval", {}).get("summary"),
            "baselines_A_type_growth": latest.get("baselines", {})
                .get("A_more_data_frozen_language", {})
                .get("type_growth_20_to_40"),
            "elapsed_s": latest["elapsed_s"],
            "backend": latest["backend"],
            "wrote": "motor/runs/latest.json",
            "theory": latest["theory_path"],
        }, indent=2))
        return 0

    if args.cmd == "status":
        k = MotorKernel(reset=False)
        st = k.status()
        print(json.dumps(st, indent=2))
        return 0

    if args.cmd == "theory":
        k = MotorKernel(reset=False)
        if args.path:
            print(k.archive.theory_path)
        else:
            print(k.archive.theory_text())
        return 0

    if args.cmd == "snapshot":
        from motor.snapshot import dump, restore, verify_roundtrip
        if args.action == "dump":
            print(json.dumps(dump(), indent=2))
            return 0
        if args.action == "verify":
            r = verify_roundtrip()
            print(json.dumps(r, indent=2))
            return 0 if r["ok"] else 1
        if args.action == "restore":
            if not args.dest:
                print("--dest required for restore", file=sys.stderr)
                return 2
            print(json.dumps(restore(Path(args.dest)), indent=2))
            return 0

    return 1


if __name__ == "__main__":
    sys.exit(main())
