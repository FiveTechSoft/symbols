#!/usr/bin/env python3
import re
import sys
from pathlib import Path
if not Path("util.h").is_file():
    sys.exit(1)
t = Path("util.h").read_text(encoding="utf-8")
# Accept parameter names or bare types: int helper(int x); / int helper(int);
pat = re.compile(r"\bint\s+helper\s*\(\s*int(?:\s+\w+)?\s*\)")
sys.exit(0 if pat.search(t) else 1)
