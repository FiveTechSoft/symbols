#!/usr/bin/env python3
import subprocess, sys
from pathlib import Path
want = "\n".join(["# release notes", "build 42"]) + "\n"
p = Path("notes.txt")
if not p.is_file():
    print("missing notes.txt")
    sys.exit(1)
if p.read_text(encoding="utf-8") != want:
    print("notes.txt content mismatch")
    sys.exit(1)
r = subprocess.run(["git", "ls-files", "--error-unmatch", "notes.txt"],
                   capture_output=True)
if r.returncode != 0:
    print("notes.txt not tracked by git")
    sys.exit(1)
sys.exit(0)
