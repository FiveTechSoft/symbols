#!/usr/bin/env python3
import sys
from pathlib import Path
t = Path("README.md").read_text(encoding="utf-8")
sys.exit(0 if t.count("```") % 2 == 0 and t.count("```") >= 2 else 1)
