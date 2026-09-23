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
_t = Path('README.md').read_text(encoding='utf-8', errors='replace') if Path('README.md').is_file() else ''
if '--debug' not in _t:
    print('missing', '--debug', 'in', 'README.md')
    ok = False
_t = Path('README.md').read_text(encoding='utf-8', errors='replace') if Path('README.md').is_file() else ''
if '--verbose' in _t:
    print('forbidden', '--verbose', 'in', 'README.md')
    ok = False
sys.exit(0 if ok else 1)
