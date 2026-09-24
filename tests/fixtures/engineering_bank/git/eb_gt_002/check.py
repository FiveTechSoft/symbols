#!/usr/bin/env python3
import subprocess, sys
from pathlib import Path
p = Path("conflict.txt")
if not p.is_file():
    print("missing conflict.txt")
    sys.exit(1)
text = p.read_text(encoding="utf-8")
lines = [ln.strip() for ln in text.splitlines() if ln.strip()]
if lines != ["alpha", "beta"]:
    print("bad resolution:", lines)
    sys.exit(1)
for m in ("<<<<<<<", ">>>>>>>", "======="):
    if m in text:
        print("conflict marker left:", m)
        sys.exit(1)
r = subprocess.run(["git", "rev-parse", "-q", "--verify", "MERGE_HEAD"],
                   capture_output=True)
if r.returncode == 0:
    print("merge not completed")
    sys.exit(1)
r = subprocess.run(["git", "status", "--porcelain"], capture_output=True,
                   text=True, encoding="utf-8", errors="replace")
dirty = [l for l in r.stdout.splitlines()
         if l.strip() and not l.endswith("check.py")]
if dirty:
    print("dirty worktree:", dirty)
    sys.exit(1)
sys.exit(0)
