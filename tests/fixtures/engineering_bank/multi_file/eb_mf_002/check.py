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
import re
ok = True
_t = Path('util.h').read_text(encoding='utf-8', errors='replace') if Path('util.h').is_file() else ''
if not re.search(r'\bint\s+helper\s*\(\s*int(?:\s+\w+)?\s*\)', _t):
    print('missing', '\\bint\\s+helper\\s*\\(\\s*int(?:\\s+\\w+)?\\s*\\)', 'in', 'util.h')
    ok = False
sys.exit(0 if ok else 1)
