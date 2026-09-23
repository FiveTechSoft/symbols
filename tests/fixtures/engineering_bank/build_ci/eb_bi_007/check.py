#!/usr/bin/env python3
import re
import sys
from pathlib import Path
p = Path('ci.yml')
if not p.is_file():
    print('missing file', 'ci.yml')
    sys.exit(1)
text = p.read_text(encoding='utf-8', errors='replace')
norm = re.sub(r'\s+', ' ', text)
ok = True
if not re.search('run:\\s*.*ctest', text):
    print('missing pattern', 'run:\\s*.*ctest', 'in', 'ci.yml')
    ok = False
sys.exit(0 if ok else 1)
