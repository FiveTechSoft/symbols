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
    if r.returncode != 0:
        sys.stderr.write((r.stderr or "") + (r.stdout or ""))
        sys.exit(1)
    r = subprocess.run(
        ["cmake", "--build", td],
        capture_output=True, text=True, timeout=120)
    if r.returncode != 0:
        sys.stderr.write((r.stderr or "") + (r.stdout or ""))
        sys.exit(1)
    sys.exit(0)
finally:
    shutil.rmtree(td, ignore_errors=True)
