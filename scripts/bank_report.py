#!/usr/bin/env python3
"""Per-commit task-bank report for the agent runner.

Runs the fixed banks already registered in CTest:
  - test_agent_runner_external  (development + evaluation suites)
  - test_agent_runner_heldout   (held-out suite)
  - test_agent_runner_learning  (repeat-task probe)

Parses METRICS / LEARN / case OK|FAIL lines and appends one JSON object per run.

Schema v1 (one JSONL row):
  schema          int   = 1
  commit          str   git short hash (or "unknown")
  branch          str   git branch (or "unknown")
  ts              str   UTC ISO-8601
  ctest_exit      int   0 = all bank tests passed
  pass            bool  ctest_exit == 0
  wrong_edits     int   false positives (edit when expected to abstain)
  learning        obj|null  repeat-task probe (pass1/pass2 times, improved)
  global          obj   aggregate over all suites
  suites          obj   name -> metrics

Per-suite metrics:
  cases, resolved, resolution_rate, false_positives,
  correct_abstentions, baseline_resolved, attempts, replans,
  cpu_ms, pass_cases, fail_cases, clean_rate

clean_rate = pass_cases / cases  (met expectation with no wrong edit)
resolution_rate = resolved / cases  (rc==1; includes wrong resolves)

Usage (from repo root, after cmake --build build-gcc):
  python scripts/bank_report.py
  python scripts/bank_report.py --out bank_report.jsonl
  python scripts/bank_report.py --dry-run
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
TESTS = (
    "test_agent_runner_external",
    "test_agent_runner_heldout",
    "test_agent_runner_learning",
)

METRICS_RE = re.compile(
    r"^METRICS\s+(?P<body>.+)$"
)
LEARN_RE = re.compile(r"^LEARN\s+(?P<body>.+)\s+(?P<verdict>OK|FAIL)\s*$")
KV_RE = re.compile(r"(\w+)=([^\s]+)")
CASE_RE = re.compile(
    r"^(?:\[\w+\]\s+)?(?P<dir>\S+)\s+.*\b(?P<verdict>OK|FAIL)\s*$"
)


def run_ctest() -> tuple[int, str]:
    cmd = [
        "ctest",
        "--test-dir",
        "build-gcc",
        "-R",
        "|".join(TESTS),
        "-V",
    ]
    proc = subprocess.run(
        cmd,
        cwd=REPO,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    return proc.returncode, (proc.stdout or "") + (proc.stderr or "")


def git(*args: str) -> str:
    try:
        out = subprocess.run(
            ["git", *args],
            cwd=REPO,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=10,
        )
        if out.returncode == 0:
            return out.stdout.strip()
    except (OSError, subprocess.TimeoutExpired):
        pass
    return "unknown"


def parse_metrics_line(body: str) -> dict:
    row: dict = {}
    for key, raw in KV_RE.findall(body):
        if key in ("suite",):
            row[key] = raw
            continue
        try:
            if "." in raw:
                row[key] = float(raw)
            else:
                row[key] = int(raw)
        except ValueError:
            row[key] = raw
    return row


def empty_suite() -> dict:
    return {
        "cases": 0,
        "resolved": 0,
        "resolution_rate": 0.0,
        "false_positives": 0,
        "correct_abstentions": 0,
        "baseline_resolved": 0,
        "attempts": 0,
        "replans": 0,
        "cpu_ms": 0.0,
        "pass_cases": 0,
        "fail_cases": 0,
        "clean_rate": 0.0,
    }


def finalize(suite: dict) -> None:
    cases = int(suite.get("cases") or 0)
    passed = int(suite.get("pass_cases") or 0)
    suite["pass_cases"] = passed
    suite["fail_cases"] = max(cases - passed, 0)
    suite["clean_rate"] = round(passed / cases, 4) if cases else 0.0
    if "resolution_rate" not in suite or cases:
        suite["resolution_rate"] = round(
            int(suite.get("resolved") or 0) / cases, 4
        ) if cases else 0.0


def suite_for_case_line(line: str, current: str | None) -> str | None:
    if "[development]" in line:
        return "development"
    if "[evaluation]" in line:
        return "evaluation"
    if "solved=" in line and "expected=" in line:
        return current or "heldout"
    return None


def parse_learn_line(body: str, verdict: str) -> dict:
    row: dict = {"verdict": verdict, "pass": verdict == "OK"}
    for key, raw in KV_RE.findall(body):
        if key in ("op1", "op2"):
            row[key] = raw
            continue
        try:
            if "." in raw:
                row[key] = float(raw)
            else:
                row[key] = int(raw)
        except ValueError:
            row[key] = raw
    p1 = float(row.get("pass1_ms") or 0.0)
    p2 = float(row.get("pass2_ms") or 0.0)
    row["speedup_ms"] = round(p1 - p2, 3) if p1 and p2 else None
    return row


def parse_output(text: str) -> tuple[dict[str, dict], dict | None]:
    suites: dict[str, dict] = {}
    learning: dict | None = None
    current: str | None = None
    for raw in text.splitlines():
        # ctest -V prefixes each stdout line with "N: "
        line = re.sub(r"^\s*\d+:\s+", "", raw).strip()
        if "Start" in line and "test_agent_runner_heldout" in line:
            current = "heldout"
        if "Start" in line and "test_agent_runner_external" in line:
            current = None
        if "Start" in line and "test_agent_runner_learning" in line:
            current = None

        m_learn = LEARN_RE.match(line)
        if m_learn:
            learning = parse_learn_line(
                m_learn.group("body"), m_learn.group("verdict")
            )
            continue

        m = METRICS_RE.match(line)
        if m:
            parsed = parse_metrics_line(m.group("body"))
            suite_name = parsed.pop("suite", None) or current or "heldout"
            suite = suites.setdefault(suite_name, empty_suite())
            for key, val in parsed.items():
                if key in ("resolution_rate", "cpu_ms"):
                    suite[key] = float(val)
                else:
                    suite[key] = val
            continue

        m_case = CASE_RE.match(line)
        if not m_case:
            continue
        name = suite_for_case_line(line, current)
        if not name:
            continue
        suite = suites.setdefault(name, empty_suite())
        suite["cases"] = int(suite.get("cases") or 0) + 1
        if m_case.group("verdict") == "OK":
            suite["pass_cases"] = int(suite.get("pass_cases") or 0) + 1

    for name in list(suites):
        finalize(suites[name])
    return suites, learning


def merge_global(suites: dict[str, dict]) -> dict:
    g = empty_suite()
    g.pop("resolution_rate")
    g.pop("clean_rate")
    g.pop("pass_cases")
    g.pop("fail_cases")
    resolved = 0
    cases = 0
    passed = 0
    for s in suites.values():
        cases += int(s.get("cases") or 0)
        resolved += int(s.get("resolved") or 0)
        passed += int(s.get("pass_cases") or 0)
        for k in (
            "false_positives",
            "correct_abstentions",
            "baseline_resolved",
            "attempts",
            "replans",
        ):
            g[k] = int(g.get(k) or 0) + int(s.get(k) or 0)
        g["cpu_ms"] = float(g.get("cpu_ms") or 0.0) + float(s.get("cpu_ms") or 0.0)
    g["cases"] = cases
    g["resolved"] = resolved
    g["pass_cases"] = passed
    g["fail_cases"] = max(cases - passed, 0)
    g["resolution_rate"] = round(resolved / cases, 4) if cases else 0.0
    g["clean_rate"] = round(passed / cases, 4) if cases else 0.0
    g["cpu_ms"] = round(float(g["cpu_ms"]), 3)
    return g


def build_row(ctest_exit: int, output: str) -> dict:
    suites, learning = parse_output(output)
    # Fallback: if case lines were not parsed, derive pass from exit + metrics
    if suites and all(int(s.get("pass_cases") or 0) == 0 for s in suites.values()):
        for name, s in suites.items():
            cases = int(s.get("cases") or 0)
            fp = int(s.get("false_positives") or 0)
            if ctest_exit == 0:
                s["pass_cases"] = cases
            else:
                s["pass_cases"] = max(cases - fp, 0)
            finalize(s)
    g = merge_global(suites)
    if learning is not None and not learning.get("pass", False):
        ctest_exit = ctest_exit or 1
    return {
        "schema": 1,
        "commit": git("rev-parse", "--short", "HEAD"),
        "branch": git("rev-parse", "--abbrev-ref", "HEAD"),
        "ts": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "ctest_exit": int(ctest_exit),
        "pass": ctest_exit == 0,
        "wrong_edits": int(g.get("false_positives") or 0),
        "learning": learning,
        "global": g,
        "suites": suites,
    }


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--out",
        default="bank_report.jsonl",
        help="JSONL path (default: bank_report.jsonl in repo root)",
    )
    ap.add_argument("--dry-run", action="store_true", help="print only, do not append")
    args = ap.parse_args()

    exit_code, output = run_ctest()
    row = build_row(exit_code, output)
    line = json.dumps(row, ensure_ascii=False, sort_keys=True)
    print(line)
    if not args.dry_run:
        out_path = Path(args.out)
        if not out_path.is_absolute():
            out_path = REPO / out_path
        with out_path.open("a", encoding="utf-8") as fh:
            fh.write(line + "\n")
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
