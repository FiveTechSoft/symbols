#!/usr/bin/env python3
"""S6: nonliteral includes fail closed; tracked templates are not scanned.
The source mutant removes the fallback and must fail the exact two checks.
"""
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parent.parent
MUTANT = "--mutant" in sys.argv


def load(path):
    spec = importlib.util.spec_from_file_location("bg_dynamic", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def build(b):
    return subprocess.run(["cmake", "--build", str(b)], check=True,
                          capture_output=True, text=True).stdout


def main():
    if os.name == "nt" or not shutil.which("make"):
        print("build_graph dynamic: SKIP (Windows or no make)")
        return 77
    fails = []
    with tempfile.TemporaryDirectory() as td:
        td = Path(td)
        tool = ROOT / "tools/build_graph.py"
        if MUTANT:
            source = tool.read_text()
            needle = '        if dynamic:\n'
            assert source.count(needle) == 1
            tool = td / "mutant.py"
            tool.write_text(source.replace(needle, '        if False:  # mutant\n'))
        bg = load(tool)
        src = td / "src"
        shutil.copytree(ROOT / "tests/fixtures/graph_dynamic", src)
        files, _, _ = bg.scan_includes(str(src))
        if "tests/t_inc.c.in" in files or bg.scan_dynamic_includes(str(src), files):
            fails.append("template_excluded")
        template = src / "tests/t_inc.c.in"
        materialized = src / "tests/t_inc.c"
        materialized.write_text(template.read_text())
        b = td / "build"
        assert bg.configure(str(src), str(b), "Unix Makefiles") == 0
        build(b)
        subprocess.run(["ctest", "--test-dir", str(b), "--output-on-failure"], check=True)
        g = bg.load_graph(str(b))
        selected = bg.select(g, ["src/lib.c"])
        if not (selected["mode"] == "full" and selected["tests"] == ["inc_ok", "lib_ok"]
                and selected["reasons"] == ["dynamic-include: tests/t_inc.c"]):
            fails.append("macro_full")
        # Settled real-build guard before the touch, then an executable-mtime oracle.
        assert "Linking C executable" not in build(b)
        exes = {t["name"]: Path(t["command"][0]) for t in g["tests"]}
        before = {n: p.stat().st_mtime_ns for n, p in exes.items()}
        time.sleep(0.05)
        os.utime(src / "src/lib.c", None)
        build(b)
        needed = sorted(n for n, p in exes.items() if p.stat().st_mtime_ns != before[n])
        if needed != ["inc_ok", "lib_ok"] or not set(needed) <= set(selected["tests"]):
            fails.append("macro_recall")
        materialized.write_text('#include "../src/lib.c"\nint main(void){return lib();}\n')
        build(b)
        literal = bg.select(bg.load_graph(str(b)), ["src/lib.c"])
        if literal != {"mode": "subset", "tests": ["inc_ok", "lib_ok"], "reasons": []}:
            fails.append("literal_subset")
        print("macro oracle needed=%s selected=%s mode=%s" %
              (needed, selected["tests"], selected["mode"]))
    print("failed checks: " + (" ".join(sorted(fails)) or "none"))
    if MUTANT:
        want = ["macro_full", "macro_recall"]
        if sorted(fails) == want:
            print("MUTANT killed by exactly " + " ".join(want))
            return 0
        print("MUTANT not killed by the exact set")
        return 1
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
