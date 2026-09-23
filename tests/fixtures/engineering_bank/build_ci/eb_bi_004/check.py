#!/usr/bin/env python3
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
ok = True
need = 'project('
if need not in blob:
    print('missing', need)
    ok = False
sys.exit(0 if ok else 1)
