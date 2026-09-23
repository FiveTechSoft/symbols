#!/usr/bin/env python3
import sys
from pathlib import Path
ok = True
if not Path('process.c').is_file():
    print('missing file', 'process.c')
    ok = False
if not Path('process.h').is_file():
    print('missing file', 'process.h')
    ok = False
sys.exit(0 if ok else 1)
