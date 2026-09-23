#!/usr/bin/env python3
import sys
from pathlib import Path
t = Path("main.c").read_text(encoding="utf-8") if Path("main.c").is_file() else ""
sys.exit(0 if '#include "util.h"' in t else 1)
