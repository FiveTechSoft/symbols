#!/usr/bin/env python3
"""Engineering-bank harness: run an agent on every bank task and check it.

For each row of tests/fixtures/engineering_bank/index.tsv:
  1. copy the task's before/ into a fresh temp workdir;
  2. run the agent under test in that workdir with the task.md text;
  3. copy check.py in and run it (CWD = workdir); exit 0 = pass.
The golden after/ state is read only in --self-test mode (then it replaces
step 2, to prove every check can pass).

Output: one JSON object on stdout (and --out FILE), with
  tasks_total, tasks_passed, pass_rate, pass_rate_by_category,
  wrong_edits (the agent changed files and the check still fails),
  untouched (the agent changed nothing), agent_errors, wall_ms_total,
  tasks: [{id, category, passed, check_rc, agent_rc, changed_files,
           wrong_edit, wall_ms, agent_ms}]
No task ids are hardcoded; everything comes from index.tsv.

Agent command: --agent is a shell-free template split on spaces; the
placeholders {workdir}, {task} (task text) and {task_file} are
substituted per task. Default: build/symbols-agent -w {workdir} {task}
--agent noop runs nothing (baseline: every check must fail).

Usage (repo root, after building):
  python3 tools/bank_harness.py                       # symbols-agent
  python3 tools/bank_harness.py --agent noop          # baseline
  python3 tools/bank_harness.py --self-test           # golden states
  python3 tools/bank_harness.py --min-pass-rate 0.1   # exit 1 below it
"""
from __future__ import annotations

import argparse
import filecmp
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BANK = ROOT / "tests" / "fixtures" / "engineering_bank"
EXE = ".exe" if os.name == "nt" else ""
DEFAULT_AGENT = f"build/symbols-agent{EXE} -w {{workdir}} {{task}}"


def read_index(bank: Path) -> list[tuple[str, str, Path]]:
    rows = []
    for line in (bank / "index.tsv").read_text(encoding="utf-8").splitlines():
        parts = line.rstrip("\n").split("\t")
        if len(parts) < 3 or parts[0] in ("", "id") or parts[0].startswith("#"):
            continue
        rows.append((parts[0], parts[1], bank / parts[2]))
    return rows


def split_of(task_id: str) -> str:
    """Fixed 50/50 split by the parity of the id's trailing number:
    odd = dev (used while developing the agent), even = heldout."""
    digits = task_id.rsplit("_", 1)[-1]
    return "dev" if digits.isdigit() and int(digits) % 2 == 1 else "heldout"


def snapshot(d: Path) -> dict[str, str]:
    out = {}
    for p in sorted(d.rglob("*")):
        if p.is_file():
            out[p.relative_to(d).as_posix()] = hashlib.sha256(p.read_bytes()).hexdigest()
    return out


def changed(before: dict[str, str], after: dict[str, str]) -> list[str]:
    keys = set(before) | set(after)
    return sorted(k for k in keys if before.get(k) != after.get(k))


def run_agent(template: str, workdir: Path, task_text: str, task_file: Path,
              timeout: int) -> tuple[int, float, str]:
    if template == "noop":
        return 0, 0.0, ""
    argv = []
    for tok in template.split(" "):
        if tok == "":
            continue
        argv.append(tok.replace("{workdir}", str(workdir))
                       .replace("{task_file}", str(task_file))
                       .replace("{task}", task_text))
    if argv and not os.path.isabs(argv[0]) and (ROOT / argv[0]).exists():
        argv[0] = str(ROOT / argv[0])
    t0 = time.perf_counter()
    try:
        r = subprocess.run(argv, cwd=workdir, capture_output=True, text=True,
                           encoding="utf-8", errors="replace", timeout=timeout)
        rc, log = r.returncode, (r.stdout or "") + (r.stderr or "")
    except subprocess.TimeoutExpired:
        rc, log = 124, "agent timeout"
    except OSError as e:
        rc, log = 127, f"agent not runnable: {e}"
    return rc, (time.perf_counter() - t0) * 1000.0, log


def run_task(tid: str, cat: str, tdir: Path, agent: str, self_test: bool,
             timeout: int) -> dict:
    t0 = time.perf_counter()
    task_md = tdir / "task.md"
    task_text = task_md.read_text(encoding="utf-8").strip() if task_md.is_file() else ""
    with tempfile.TemporaryDirectory(prefix="bank_") as td:
        wd = Path(td) / "work"
        shutil.copytree(tdir / "before", wd)
        snap0 = snapshot(wd)
        if self_test:
            shutil.rmtree(wd)
            shutil.copytree(tdir / "after", wd)
            agent_rc, agent_ms, log = 0, 0.0, ""
        else:
            agent_rc, agent_ms, log = run_agent(agent, wd, task_text, task_md, timeout)
        diff = changed(snap0, snapshot(wd))
        shutil.copy2(tdir / "check.py", wd / "check.py")
        try:
            r = subprocess.run([sys.executable, "check.py"], cwd=wd, capture_output=True,
                               text=True, encoding="utf-8", errors="replace", timeout=timeout)
            check_rc = r.returncode
        except subprocess.TimeoutExpired:
            check_rc = 124
    passed = check_rc == 0
    return {
        "id": tid,
        "category": cat,
        "passed": passed,
        "check_rc": check_rc,
        "agent_rc": agent_rc,
        "changed_files": diff,
        "wrong_edit": bool(diff) and not passed,
        "wall_ms": round((time.perf_counter() - t0) * 1000.0, 1),
        "agent_ms": round(agent_ms, 1),
        "agent_log_tail": log[-300:] if (not passed and log) else "",
    }


def summarize(tasks: list[dict], agent: str, self_test: bool) -> dict:
    by_cat: dict[str, list[int]] = {}
    for t in tasks:
        c = by_cat.setdefault(t["category"], [0, 0])
        c[0] += 1
        c[1] += 1 if t["passed"] else 0
    total = len(tasks)
    passed = sum(1 for t in tasks if t["passed"])
    return {
        "schema": 1,
        "mode": "self-test" if self_test else "agent",
        "agent": "golden after/" if self_test else agent,
        "tasks_total": total,
        "tasks_passed": passed,
        "pass_rate": round(passed / total, 4) if total else 0.0,
        "pass_rate_by_category": {k: round(v[1] / v[0], 4) for k, v in sorted(by_cat.items())},
        "passed_by_category": {k: f"{v[1]}/{v[0]}" for k, v in sorted(by_cat.items())},
        "wrong_edits": sum(1 for t in tasks if t["wrong_edit"]),
        "untouched": sum(1 for t in tasks if not t["changed_files"]),
        "agent_errors": sum(1 for t in tasks if t["agent_rc"] in (124, 127)),
        "wall_ms_total": round(sum(t["wall_ms"] for t in tasks), 1),
        "passed_by_split": {sp: f"{sum(1 for t in tasks if t['passed'] and split_of(t['id']) == sp)}"
                                f"/{sum(1 for t in tasks if split_of(t['id']) == sp)}"
                            for sp in ("dev", "heldout")},
        "tasks": tasks,
    }


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--agent", default=DEFAULT_AGENT)
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--bank", default=str(BANK))
    ap.add_argument("--timeout", type=int, default=60)
    ap.add_argument("--out")
    ap.add_argument("--min-pass-rate", type=float)
    ap.add_argument("--max-wrong-edits", type=int)
    ap.add_argument("--split", choices=("all", "dev", "heldout"), default="all")
    a = ap.parse_args()
    rows = [r for r in read_index(Path(a.bank)) if a.split == "all" or split_of(r[0]) == a.split]
    if not rows:
        print("no tasks in index.tsv", file=sys.stderr)
        return 2
    tasks = [run_task(tid, cat, tdir, a.agent, a.self_test, a.timeout) for tid, cat, tdir in rows]
    res = summarize(tasks, a.agent, a.self_test)
    text = json.dumps(res, ensure_ascii=False)
    if a.out:
        Path(a.out).write_text(text + "\n", encoding="utf-8")
    print(text)
    s = {k: res[k] for k in ("mode", "tasks_total", "tasks_passed", "pass_rate", "wrong_edits", "untouched", "agent_errors")}
    s.update({f"pass_{k}": v for k, v in res["passed_by_split"].items()})
    print("BANK_HARNESS " + " ".join(f"{k}={v}" for k, v in s.items()), file=sys.stderr)
    rc = 0
    if a.min_pass_rate is not None and res["pass_rate"] < a.min_pass_rate:
        rc = 1
    if a.max_wrong_edits is not None and res["wrong_edits"] > a.max_wrong_edits:
        rc = 1
    return rc


if __name__ == "__main__":
    sys.exit(main())
