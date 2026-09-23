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
import re
ok = True
_t = Path('util.h').read_text(encoding='utf-8', errors='replace') if Path('util.h').is_file() else ''
if '#ifndef UTIL_H' not in _t:
    print('missing', '#ifndef UTIL_H', 'in', 'util.h')
    ok = False
if '#define UTIL_H' not in _t:
    print('missing', '#define UTIL_H', 'in', 'util.h')
    ok = False
_t = Path('util.h').read_text(encoding='utf-8', errors='replace') if Path('util.h').is_file() else ''
if 'MAIN_H' in _t:
    print('forbidden', 'MAIN_H', 'in', 'util.h')
    ok = False
sys.exit(0 if ok else 1)
