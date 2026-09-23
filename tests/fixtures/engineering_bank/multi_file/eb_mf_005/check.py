#!/usr/bin/env python3
import subprocess, sys, glob
from pathlib import Path
files = sorted(glob.glob("*.c"))
if not files:
    sys.exit(2)
r = subprocess.run(
    ["gcc", "-std=c11", "-Werror=implicit-function-declaration", "-o", "eb_bin"] + files,
    capture_output=True, text=True)
if r.returncode != 0:
    sys.stderr.write(r.stderr)
    sys.exit(1)
bin_path = Path("eb_bin.exe") if Path("eb_bin.exe").is_file() else Path("eb_bin")
if not bin_path.is_file():
    sys.exit(1)
r2 = subprocess.run([str(bin_path.resolve())], capture_output=True, text=True)
if r2.returncode != 0:
    sys.exit(r2.returncode)
ok = True
if not Path('process.c').is_file():
    print('missing file', 'process.c')
    ok = False
if not Path('process.h').is_file():
    print('missing file', 'process.h')
    ok = False
sys.exit(0 if ok else 1)
