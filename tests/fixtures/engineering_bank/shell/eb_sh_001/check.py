#!/usr/bin/env python3
import subprocess, sys
from pathlib import Path
if not Path('run.sh').is_file():
    print("missing", 'run.sh')
    sys.exit(1)
shell = ['C:\\Program Files\\Git\\bin\\bash.exe']
r = subprocess.run(shell + ['run.sh'], capture_output=True, text=True)
out = (r.stdout or "") + (r.stderr or "")
if r.returncode != 0:
    print(out)
    sys.exit(1)
if 'HELLO_SHELL' and 'HELLO_SHELL' not in out:
    print("expected output containing", 'HELLO_SHELL', "got:", out)
    sys.exit(1)
sys.exit(0)
