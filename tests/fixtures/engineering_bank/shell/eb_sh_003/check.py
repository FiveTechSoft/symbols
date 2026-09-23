#!/usr/bin/env python3
import subprocess, sys
from pathlib import Path
import shutil
def _sh_bin():
    for name in ("bash", "sh"):
        path = shutil.which(name)
        if path and "system32" not in path.lower():
            return [path]
    for cand in (
        r"C:\Program Files\Git\bin\bash.exe",
        r"C:\Program Files\Git\usr\bin\sh.exe",
        r"C:\Program Files (x86)\Git\bin\bash.exe",
    ):
        if Path(cand).is_file():
            return [cand]
    path = shutil.which("sh")
    if path:
        return [path]
    return ["sh"]
shell = _sh_bin()

r = subprocess.run(shell + ["run.sh", "a  b"], capture_output=True, text=True)
if r.returncode != 0:
    sys.exit(1)
sys.exit(0 if (r.stdout or "") == "a  b\n" else 1)
