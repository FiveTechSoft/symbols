#!/usr/bin/env python3
import subprocess, sys
def g(*a, ok=(0,)):
    r = subprocess.run(["git", *a], capture_output=True, text=True,
                       encoding="utf-8", errors="replace")
    if r.returncode not in ok:
        sys.stderr.write(r.stderr or r.stdout or "")
        sys.exit(r.returncode)
    return r
g("checkout", "HEAD~1", "--", "notes.txt")
