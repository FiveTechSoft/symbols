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

if not Path('run.sh').is_file():
    print("missing", 'run.sh')
    sys.exit(1)
r = subprocess.run(shell + ['run.sh'], capture_output=True, text=True)
out = (r.stdout or "") + (r.stderr or "")
if r.returncode != 0:
    print(out)
    sys.exit(1)
if '' and '' not in out:
    print("expected output containing", '', "got:", out)
    sys.exit(1)
sys.exit(0)
