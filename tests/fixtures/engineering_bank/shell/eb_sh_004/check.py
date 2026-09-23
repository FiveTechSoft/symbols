#!/usr/bin/env python3
import sys
from pathlib import Path
t = Path("run.sh").read_text(encoding="utf-8")
sys.exit(0 if t.startswith("#!/bin/sh") else 1)
