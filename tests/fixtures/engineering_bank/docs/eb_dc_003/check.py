#!/usr/bin/env python3
import re, subprocess, sys
from pathlib import Path
if not Path("util.c").is_file() or not Path("README.md").is_file():
    sys.exit(1)
src = Path("util.c").read_text(encoding="utf-8", errors="replace")
m = re.search(r"\bhelper\s*\(([^)]*)\)", src)
if not m:
    print("no helper definition in util.c")
    sys.exit(1)
params = re.sub(r"\s+", " ", m.group(1)).strip()
if not params:
    print("helper has no parameters in source")
    sys.exit(1)
r = subprocess.run(
    ["gcc", "-std=c11", "-fsyntax-only", "util.c"],
    capture_output=True, text=True)
if r.returncode != 0:
    sys.stderr.write(r.stderr)
    sys.exit(1)
readme = Path("README.md").read_text(encoding="utf-8", errors="replace")
readme_n = re.sub(r"\s+", " ", readme)
sig = "helper(" + params + ")"
if sig not in readme and sig not in readme_n:
    print("README missing signature", sig)
    sys.exit(1)
if re.search(r"helper\s*\(\s*\)", readme):
    print("README still has empty helper()")
    sys.exit(1)
sys.exit(0)
