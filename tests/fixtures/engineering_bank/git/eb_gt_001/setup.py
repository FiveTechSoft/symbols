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
Path("notes.txt").write_text("\n".join(["# release notes", "build 42"]) + "\n", encoding="utf-8")
g("add", ".")
g("commit", "-m", "add release notes")
g("rm", "notes.txt")
g("commit", "-m", "drop notes")
