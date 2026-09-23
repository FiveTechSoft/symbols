#!/usr/bin/env python3
import subprocess, sys
from pathlib import Path
shell = ['C:\\Program Files\\Git\\bin\\bash.exe']

r = subprocess.run(shell + ["run.sh", "fail"], capture_output=True, text=True)
rc = r.returncode
ok = (rc == 3)
if not ok:
    print("rc", rc, "out", (r.stdout or "") + (r.stderr or ""))
sys.exit(0 if ok else 1)
