#!/usr/bin/env python3
"""S7: include spelling normalization, relink oracle, exact source mutants."""
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parent.parent


def load(path):
    spec = importlib.util.spec_from_file_location("bg_spelling", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def build(b):
    return subprocess.run(["cmake", "--build", str(b)], check=True,
                          capture_output=True, text=True).stdout


def checks(bg, src, b):
    fails = []
    materialized = src / "tests/t_inc.c"
    variants = {
        "macro_splice": '#define IMPLEMENTATION "../src/lib.c"\n#inc\\\nlude IMPLEMENTATION\n',
        "macro_comment": '#define IMPLEMENTATION "../src/lib.c"\n#/**/include IMPLEMENTATION\n',
        "literal_splice": '#inc\\\nlude "../src/lib.c"\n',
        "literal_comment": '#/**/include "../src/lib.c"\n',
    }
    for name, text in variants.items():
        materialized.write_text(text + 'int main(void){return lib();}\n')
        build(b)
        assert "Linking C executable" not in build(b)
        g = bg.load_graph(str(b))
        sel = bg.select(g, ["src/lib.c"])
        expected_mode = "full" if name.startswith("macro") else "subset"
        ok = sel["mode"] == expected_mode and sel["tests"] == ["inc_ok", "lib_ok"]
        if expected_mode == "full":
            ok = ok and sel["reasons"] == ["dynamic-include: tests/t_inc.c"]
        exes = {t["name"]: Path(t["command"][0]) for t in g["tests"]}
        before = {n: p.stat().st_mtime_ns for n, p in exes.items()}
        time.sleep(0.05)
        os.utime(src / "src/lib.c", None)
        build(b)
        needed = sorted(n for n, p in exes.items() if p.stat().st_mtime_ns != before[n])
        if not ok or needed != ["inc_ok", "lib_ok"] or not set(needed) <= set(sel["tests"]):
            fails.append(name)
    materialized.write_text('#include "../src/lib.c"\nint main(void){return lib();}\n')
    build(b)
    g = bg.load_graph(str(b))
    # These are inert scratch data, not compiled. The whole-tree scan must fail closed.
    bad = src / "unsupported.h"
    for name, text in (("malformed_literal", '#include "missing\n'),
                       ("unterminated_comment", '/* unfinished\n'),
                       ("include_next", '#include_next <stdio.h>\n')):
        bad.write_text(text)
        sel = bg.select(g, ["src/lib.c"])
        if sel["mode"] != "full" or not any(r.startswith("dynamic-include:") for r in sel["reasons"]):
            fails.append(name)
    bad.unlink()
    # Comment text must not create an include; quote contents must not be stripped.
    text = '/*\n#include MACRO\n*/\nchar *s="/* not a comment */";\n'
    normalized, malformed = bg.normalize_include_text(text)
    if malformed or '#include' in normalized or '"/* not a comment */"' not in normalized:
        fails.append("comments_quotes")
    # CRLF splicing has the same semantics, tested without a compiler-newline dependency.
    normalized, malformed = bg.normalize_include_text('#inc\\\r\nlude "x.h"\n')
    if malformed or not bg.INCLUDE_RX.search(normalized):
        fails.append("crlf_splice")
    return sorted(fails)


def main():
    if os.name == "nt" or not shutil.which("make"):
        print("build_graph spelling: SKIP (Windows or no make)")
        return 77
    with tempfile.TemporaryDirectory() as td:
        td = Path(td)
        src = td / "src"
        shutil.copytree(ROOT / "tests/fixtures/graph_dynamic", src)
        (src / "tests/t_inc.c").write_text((src / "tests/t_inc.c.in").read_text())
        b = td / "build"
        tool = ROOT / "tools/build_graph.py"
        bg = load(tool)
        assert bg.configure(str(src), str(b), "Unix Makefiles") == 0
        build(b)
        failed = checks(bg, src, b)
        if failed:
            print("failed checks:", failed)
            return 1
        source = tool.read_text()
        mutations = {
            "no_splice": ("    text = re.sub(r'\\\\\\r?\\n', '', text)",
                          "    text = text  # mutant", ["crlf_splice", "literal_splice", "macro_splice"]),
            "no_comments": ("        if text.startswith('/*', i):", "        if False:  # mutant",
                            ["comments_quotes", "literal_comment", "macro_comment", "unterminated_comment"]),
        }
        for name, (old, new, want) in mutations.items():
            assert source.count(old) == 1, (name, old)
            mutant = td / (name + ".py")
            mutant.write_text(source.replace(old, new))
            got = checks(load(mutant), src, b)
            if got != want:
                print("mutant exact-set mismatch", name, got, want)
                return 1
            print("MUTANT", name, "killed by exactly", " ".join(want))
    print("build_graph spelling ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
