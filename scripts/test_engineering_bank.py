#!/usr/bin/env python3
"""Bank self-test: every task's check.py must fail on before/ and pass on after/.

Exit 0 only if all tasks satisfy the contract. Prints a one-line summary.
Usage: python scripts/test_engineering_bank.py
"""
from __future__ import annotations

import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BANK = ROOT / "tests" / "fixtures" / "engineering_bank"


def _sh_args(script: str) -> list[str]:
    import shutil
    for cand in ("bash", "sh", r"C:\Program Files\Git\bin\bash.exe",
                 r"C:\Program Files\Git\usr\bin\sh.exe"):
        if cand.endswith(".exe"):
            path = cand if Path(cand).is_file() else None
        else:
            path = shutil.which(cand)
        if path:
            return [path, script]
    return ["sh", script]


def run_check(task_dir: Path, state: str) -> int:
    src = task_dir / state
    if not src.is_dir():
        return 2
    with tempfile.TemporaryDirectory(prefix="ebank_") as td:
        wd = Path(td) / "work"
        shutil.copytree(src, wd)
        check = task_dir / "check.py"
        if not check.is_file():
            return 2
        shutil.copy2(check, wd / "check.py")
        r = subprocess.run(
            [sys.executable, "check.py"],
            cwd=wd,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=60,
        )
        return r.returncode


def main() -> int:
    index = BANK / "index.tsv"
    if not index.is_file():
        print("FAIL missing index.tsv")
        return 1
    rows = []
    for line in index.read_text(encoding="utf-8").splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        parts = line.split("\t")
        if len(parts) != 3:
            print("FAIL bad index line:", line)
            return 1
        if parts[0] == "id" and parts[1] == "category":
            continue  # header
        rows.append(parts)

    total = len(rows)
    if total < 50:
        print(f"FAIL need >=50 tasks, got {total}")
        return 1

    categories = {r[1] for r in rows}
    if len(categories) < 8:
        print(f"FAIL need >=8 categories, got {len(categories)}")
        return 1

    fail_before = fail_after = 0
    bad = []
    for tid, cat, rel in rows:
        tdir = BANK / rel
        if not (tdir / "task.md").is_file() or not (tdir / "check.py").is_file():
            bad.append(f"{tid}: missing task.md/check.py")
            continue
        rb = run_check(tdir, "before")
        ra = run_check(tdir, "after")
        if rb == 0:
            fail_before += 1
            bad.append(f"{tid}: check PASSED on before/ (must fail)")
        if ra != 0:
            fail_after += 1
            bad.append(f"{tid}: check FAILED on after/ (rc={ra})")

    print(
        f"BANK_SELF_TEST total={total} categories={len(categories)} "
        f"before_leaks={fail_before} after_regressions={fail_after} "
        f"bad={len(bad)}"
    )
    for b in bad:
        print("  ", b)
    return 0 if not bad else 1


if __name__ == "__main__":
    sys.exit(main())
