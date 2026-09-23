#!/usr/bin/env python3
import shutil, subprocess, sys
from pathlib import Path
if not Path("Makefile").is_file():
    sys.exit(1)
make_bin = None
for name in ("make", "mingw32-make", "gmake"):
    p = shutil.which(name)
    if p and "system32" not in p.lower():
        make_bin = p
        break
if not make_bin:
    # Fallback: structural when no make on PATH (e.g. bare Windows without MinGW make).
    t = Path("Makefile").read_text(encoding="utf-8")
    sys.exit(0 if "all: app" in t else 1)
r = subprocess.run([make_bin, "-n", "all"], capture_output=True, text=True)
out = (r.stdout or "") + (r.stderr or "")
sys.exit(0 if r.returncode == 0 and "app" in out and "echo done" not in out else 1)
