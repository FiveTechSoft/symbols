#!/usr/bin/env python3
import shutil, subprocess, sys, tempfile
from pathlib import Path
if not Path("CMakeLists.txt").is_file():
    sys.exit(1)
td = tempfile.mkdtemp(prefix="eb_cmake_")
try:
    r = subprocess.run(
        ["cmake", "-S", ".", "-B", td],
        capture_output=True, text=True, timeout=60)
    out = (r.stderr or "") + (r.stdout or "")
    if r.returncode != 0:
        sys.stderr.write(out)
        sys.exit(1)
    if "No project() command is present" in out:
        print(out)
        sys.exit(1)
    sys.exit(0)
finally:
    shutil.rmtree(td, ignore_errors=True)
