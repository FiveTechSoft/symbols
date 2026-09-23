#!/usr/bin/env python3
import subprocess, sys
from pathlib import Path
shell = ['C:\\Program Files\\Git\\bin\\bash.exe']

r = subprocess.run(shell + ["run.sh"], capture_output=True, text=True)
rc = r.returncode
ok = (rc == 2)
if not ok:
    print("rc", rc, "out", (r.stdout or "") + (r.stderr or ""))
sys.exit(0 if ok else 1)
