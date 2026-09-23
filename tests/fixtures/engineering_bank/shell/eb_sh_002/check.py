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


r = subprocess.run(shell + ["run.sh", "fail"], capture_output=True, text=True)
rc = r.returncode
ok = (rc == 3)
if not ok:
    print("rc", rc, "out", (r.stdout or "") + (r.stderr or ""))
sys.exit(0 if ok else 1)
