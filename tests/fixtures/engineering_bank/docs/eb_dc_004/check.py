#!/usr/bin/env python3
import re
import sys
from pathlib import Path
p = Path('README.md')
if not p.is_file():
    print('missing file', 'README.md')
    sys.exit(1)
text = p.read_text(encoding='utf-8', errors='replace')
norm = re.sub(r'\s+', ' ', text)
ok = True
need = 'cmake -S . -B build'
need_n = re.sub(r'\s+', ' ', need)
if need not in text and need_n not in norm:
    print('missing', need, 'in', 'README.md')
    ok = False
bad = 'pip install'
bad_n = re.sub(r'\s+', ' ', bad)
if bad in text or bad_n in norm:
    print('forbidden', bad, 'in', 'README.md')
    ok = False
sys.exit(0 if ok else 1)
