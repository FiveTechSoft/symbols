#!/usr/bin/env python3
import subprocess, sys
from pathlib import Path
shell = ['C:\\Program Files\\Git\\bin\\bash.exe']
r = subprocess.run(shell + ["run.sh"], capture_output=True, text=True)
if r.returncode != 0:
    print((r.stdout or "") + (r.stderr or ""))
    sys.exit(1)
p = Path("output.txt")
sys.exit(0 if p.is_file() and p.read_text(encoding="utf-8").strip() == "OK" else 1)
