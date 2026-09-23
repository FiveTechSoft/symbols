#!/usr/bin/env python3
import subprocess, sys
from pathlib import Path
r = subprocess.run(["gcc", "-std=c11", "-Werror=implicit-function-declaration",
                    "-o", "test_foo_bin", "test_foo.c", "util.c"], capture_output=True, text=True)
if r.returncode != 0:
    sys.stderr.write(r.stderr); sys.exit(1)
r = subprocess.run(["./test_foo_bin"], capture_output=True)
sys.exit(r.returncode)
