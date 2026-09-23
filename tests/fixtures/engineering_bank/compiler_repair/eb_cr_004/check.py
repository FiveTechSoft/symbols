#!/usr/bin/env python3
import subprocess, sys, glob
from pathlib import Path
files = sorted(glob.glob("*.c"))
if not files:
    sys.exit(2)
cmd = ["gcc", "-std=c11", "-Werror=implicit-function-declaration", "-fsyntax-only"] + files
r = subprocess.run(cmd, capture_output=True, text=True)
if r.returncode != 0:
    sys.stderr.write(r.stderr)
sys.exit(r.returncode)
