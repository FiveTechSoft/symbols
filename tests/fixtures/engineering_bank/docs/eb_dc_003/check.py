#!/usr/bin/env python3
import re
import sys
from pathlib import Path
blob = ''
skip_names = {'check.py'}
skip = {'.pyc', '.o', '.obj', '.exe', '.bin', '.png', '.jpg'}
for p in Path('.').rglob('*'):
    if not p.is_file() or p.suffix in skip or p.name in skip_names:
        continue
    if p.stat().st_size > 65536:
        continue
    try:
        blob += p.read_text(encoding='utf-8', errors='replace') + '\n'
    except OSError:
        pass
norm = re.sub(r'\s+', ' ', blob)
ok = True
need = 'helper(int n)'
need_n = re.sub(r'\s+', ' ', need)
if need not in blob and need_n not in norm:
    print('missing', need)
    ok = False
bad = 'helper()'
bad_n = re.sub(r'\s+', ' ', bad)
if bad in blob or bad_n in norm:
    print('forbidden', bad)
    ok = False
sys.exit(0 if ok else 1)
