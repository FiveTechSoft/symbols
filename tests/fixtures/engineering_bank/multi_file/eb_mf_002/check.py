#!/usr/bin/env python3
import sys
from pathlib import Path
if not Path("util.h").is_file():
    sys.exit(1)
t = Path("util.h").read_text(encoding="utf-8")
sys.exit(0 if "int helper(int x);" in t else 1)
