#!/usr/bin/env python3
import sys
from pathlib import Path
ok = True
if not Path('main.c').is_file():
    print('missing file', 'main.c')
    ok = False
if not Path('CMakeLists.txt').is_file():
    print('missing file', 'CMakeLists.txt')
    ok = False
sys.exit(0 if ok else 1)
