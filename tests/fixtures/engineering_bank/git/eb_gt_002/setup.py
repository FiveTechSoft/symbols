#!/usr/bin/env python3
import subprocess, sys
def g(*a, ok=(0,)):
    r = subprocess.run(["git", *a], capture_output=True, text=True,
                       encoding="utf-8", errors="replace")
    if r.returncode not in ok:
        sys.stderr.write(r.stderr or r.stdout or "")
        sys.exit(r.returncode)
    return r
from pathlib import Path
g("init", "-b", "main")
Path("conflict.txt").write_text("base\n", encoding="utf-8")
g("add", ".")
g("commit", "-m", "base")
g("checkout", "-b", "feature-a")
Path("conflict.txt").write_text("alpha\n", encoding="utf-8")
g("add", ".")
g("commit", "-m", "alpha line")
g("checkout", "main")
g("checkout", "-b", "feature-b")
Path("conflict.txt").write_text("beta\n", encoding="utf-8")
g("add", ".")
g("commit", "-m", "beta line")
g("checkout", "main")
g("merge", "feature-a", "-m", "merge feature-a")
g("merge", "feature-b", ok=(1,))
