#!/usr/bin/env python3
import subprocess, sys
from pathlib import Path
good = "\n".join(['#include <stdio.h>',
 'int main(void) { printf("ok\\n"); return 0; }', ""]) + "\n"
r = subprocess.run(["git", "show", "HEAD:calc.c"], capture_output=True,
                   text=True, encoding="utf-8", errors="replace")
if r.returncode != 0 or r.stdout != good:
    print("HEAD:calc.c is not the working version")
    sys.exit(1)
r = subprocess.run(
    ["gcc", "-std=c11", "-Werror=implicit-function-declaration",
     "-o", "eb_bin", "calc.c"], capture_output=True, text=True)
if r.returncode != 0:
    sys.stderr.write(r.stderr)
    sys.exit(1)
bin_path = Path("eb_bin.exe") if Path("eb_bin.exe").is_file() else Path("eb_bin")
r = subprocess.run([str(bin_path.resolve())], capture_output=True, text=True)
sys.exit(0 if r.returncode == 0 else 1)
