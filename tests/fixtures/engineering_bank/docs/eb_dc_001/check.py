#!/usr/bin/env python3
import shutil, subprocess, sys, tempfile
from pathlib import Path
if not Path("main.c").is_file() or not Path("README.md").is_file():
    sys.exit(1)
td = Path(tempfile.mkdtemp(prefix="eb_dc001_"))
try:
    obj = td / "main.o"
    r = subprocess.run(
        ["gcc", "-std=c11", "-c", "-o", str(obj), "main.c"],
        capture_output=True, text=True)
    if r.returncode != 0:
        sys.stderr.write(r.stderr)
        sys.exit(1)
    nm = subprocess.run(["nm", str(obj)], capture_output=True, text=True)
    if nm.returncode != 0:
        sys.stderr.write(nm.stderr)
        sys.exit(1)
    syms = set()
    for line in (nm.stdout or "").splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[1] in ("T", "t", "D", "d", "B", "b", "R", "r"):
            syms.add(parts[2])
    if "compute" not in syms:
        print("missing symbol compute", sorted(syms))
        sys.exit(1)
    if "compute_total" in syms:
        print("unexpected symbol compute_total")
        sys.exit(1)
    readme = Path("README.md").read_text(encoding="utf-8", errors="replace")
    if "compute" not in readme:
        print("README missing compute")
        sys.exit(1)
    if "compute_total" in readme:
        print("README still has compute_total")
        sys.exit(1)
    sys.exit(0)
finally:
    shutil.rmtree(td, ignore_errors=True)
