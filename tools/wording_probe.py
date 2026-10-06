#!/usr/bin/env python3
"""Wording-robustness probe: run the bank on the real task text and under each declared
rewrite (tools/wording_rewrites.py), and print retention of the baseline passes.

  python3 tools/wording_probe.py --agent "build/symbols-agent -w {workdir} {task}" [--out FILE]

Development evidence only: the bank texts are known, the rewrites are mechanical, and
retention measures wording brittleness, not blind recall. Prints, per rewrite: passes,
retained (of the baseline passes), gained, wrong edits, and lost passes by category.
"""
import argparse
import json
import subprocess
import sys
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent


def run(agent, extra, timeout):
    cmd = [sys.executable, str(HERE / "bank_harness.py"), "--agent", agent, "--timeout", str(timeout)] + extra
    p = subprocess.run(cmd, capture_output=True, text=True)
    line = p.stdout.strip().splitlines()[-1] if p.stdout.strip() else ""
    return json.loads(line)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--agent", required=True)
    ap.add_argument("--timeout", type=int, default=60)
    ap.add_argument("--out")
    ap.add_argument("--rewrites", default="R1,R2,R3,R4,R5")
    a = ap.parse_args()
    base = run(a.agent, [], a.timeout)
    bp = {t["id"] for t in base["tasks"] if t["passed"]}
    cat = {t["id"]: t["category"] for t in base["tasks"]}
    out = {"baseline": {"passed": len(bp), "total": base["tasks_total"], "wrong_edits": base["wrong_edits"]}, "rewrites": {}}
    print("baseline: %d/%d wrong_edits=%d" % (len(bp), base["tasks_total"], base["wrong_edits"]))
    for name in a.rewrites.split(","):
        r = run(a.agent, ["--rewrite", name, "--reasons"], a.timeout)
        rp = {t["id"] for t in r["tasks"] if t["passed"]}
        lost = Counter(cat[i] for i in bp - rp)
        row = {"passed": len(rp), "retained": len(bp & rp), "gained": len(rp - bp),
               "wrong_edits": r["wrong_edits"], "lost_by_category": dict(sorted(lost.items())),
               "reasons_total": r.get("reasons_total", {})}
        out["rewrites"][name] = row
        print("%s: passed=%d retained=%d/%d gained=%d wrong_edits=%d lost=%s" % (
            name, row["passed"], row["retained"], len(bp), row["gained"], row["wrong_edits"], dict(lost)))
        sys.stdout.flush()
    if a.out:
        Path(a.out).write_text(json.dumps(out, ensure_ascii=False) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
