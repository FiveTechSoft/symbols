#!/usr/bin/env python3
import subprocess, sys
from pathlib import Path
# compile util+main and run
src = Path("util.c").read_text(encoding="utf-8") if Path("util.c").is_file() else ""
if "i <= n" not in src:
    print("loop not inclusive")
    sys.exit(1)
files = [f for f in ("main.c", "util.c") if Path(f).is_file()]
r = subprocess.run(["gcc", "-std=c11", "-Werror=implicit-function-declaration", "-o", "tbin"] + files,
                   capture_output=True, text=True)
if r.returncode != 0:
    sys.stderr.write(r.stderr); sys.exit(1)
bin_path = Path("tbin.exe") if Path("tbin.exe").is_file() else Path("tbin")
r = subprocess.run([str(bin_path.resolve())])
sys.exit(r.returncode)
