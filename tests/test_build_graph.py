#!/usr/bin/env python3
"""m231: build_graph.py against labeled fixtures. Selection recall is checked
against an oracle: touch one file, rebuild, and see which test executables
were relinked. With --mutant no_closure the dependent closure is removed and
the test exits 0 only if exactly recall_basic and recall_mixed fail."""
import json, os, time, shutil, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(HERE), "tools"))
import build_graph as bg

FIX = os.path.join(HERE, "fixtures")
MUTANT = sys.argv[sys.argv.index("--mutant") + 1] if "--mutant" in sys.argv else None


def gen_name():
    for g, exe in (("Ninja", "ninja"), ("Unix Makefiles", "make")):
        if shutil.which(exe):
            return g
    return None


_N = [0]


def setup(name, tmp, build=True):
    _N[0] += 1
    tmp = os.path.join(tmp, "w%d" % _N[0])
    os.makedirs(tmp)
    src = os.path.join(tmp, name)
    shutil.copytree(os.path.join(FIX, "graph_" + name), src)
    b = os.path.join(tmp, name + "_b")
    assert bg.configure(src, b, GEN) == 0, "configure " + name
    if build:
        subprocess.run(["cmake", "--build", b], check=True, stdout=subprocess.DEVNULL)
    return src, b


def view(g):
    return {"targets": [{"name": t["name"], "type": t["type"], "sources": t["sources"],
                         "deps": t["deps"]} for t in g["targets"]],
            "tests": [{"name": t["name"], "target": t["target"], "labels": t["labels"]} for t in g["tests"]],
            "unknown": g["unknown"]}


def expected(name):
    e = json.load(open(os.path.join(FIX, "graph_" + name, "expected_graph.json")))
    e["targets"].sort(key=lambda t: t["name"]); e["tests"].sort(key=lambda t: t["name"])
    return e


def select(g, files):
    if MUTANT == "scan_off":
        return bg.select(g, files, scan=False)
    if MUTANT == "no_closure":
        g = json.loads(json.dumps(g))
        for t in g["targets"]:
            t["deps"] = []
    return bg.select(g, files)


def recall(name, tmp, stats):
    src, b = setup(name, tmp)
    g = bg.load_graph(b)
    exes = {t["name"]: os.path.join(b, t["command"][0]) if not os.path.isabs(t["command"][0]) else t["command"][0]
            for t in g["tests"] if t["command"]}
    files = sorted({s for t in g["targets"] for s in t["sources"]} |
                   {f for f in bg.scan_includes(src)[0] if f.endswith(bg.HEADER_EXT)})
    ok = True
    for f in files:
        before = {n: os.stat(p).st_mtime_ns for n, p in exes.items()}
        time.sleep(0.05)
        os.utime(os.path.join(src, f), None)
        subprocess.run(["cmake", "--build", b], check=True, stdout=subprocess.DEVNULL)
        oracle = {n for n, pth in exes.items() if os.stat(pth).st_mtime_ns != before[n]}
        sel = select(g, [f])
        got = set(sel["tests"])
        stats.append((name, f, sorted(oracle), sorted(got), sel["mode"]))
        if not oracle <= got:
            ok = False
    return ok


def main():
    global GEN
    GEN = gen_name()
    if os.name == "nt" or GEN is None:
        print("build_graph test: SKIP (no make or ninja, or Windows: not measured)")
        return 77
    fails, stats = [], []
    with tempfile.TemporaryDirectory() as tmp:
        for name in ("basic", "mixed", "incl"):
            _, b = setup(name, tmp)
            if view(bg.load_graph(b)) != expected(name):
                fails.append("graph_" + name)
        _, b = setup("script", tmp)
        g = bg.load_graph(b)
        if g["unknown"] != ["test script_ok is not mapped to a target"] or \
           select(g, ["prog.c"])["mode"] != "full":
            fails.append("graph_script")
        for name in ("basic", "mixed", "incl"):
            if not recall(name, tmp, stats):
                fails.append("recall_" + name)
        src, b = setup("mixed", tmp)
        g = bg.load_graph(b)
        rules = [select(g, ["CMakeLists.txt"])["mode"] == "full",
                 select(g, ["src/gen.c.in"])["mode"] == "full",
                 select(g, ["src/base.c"])["mode"] == "subset"]
        src3, b3 = setup("incl", tmp)
        g3 = bg.load_graph(b3)
        hdr = [select(g3, ["src/lib.h"]), select(g3, ["src/lib.c"])]
        if not (hdr[0]["mode"] == "subset" and hdr[0]["tests"] == ["inc_ok", "lib_ok"] and
                hdr[1]["tests"] == ["inc_ok", "lib_ok"] and
                select(g, ["src/base.h"])["tests"] == ["base_ok", "gen_ok"]):
            fails.append("header_scan")
        _, bu = setup("basic", tmp, build=False)
        gu = bg.load_graph(bu)
        rules.append(select(gu, ["src/core.c"])["mode"] == "full" and bool(gu["unknown"]))
        if not all(rules):
            fails.append("fullgate_rules")
        if shutil.which("ninja"):
            src2 = os.path.join(tmp, "mc"); shutil.copytree(os.path.join(FIX, "graph_basic"), src2)
            b2 = os.path.join(tmp, "mc_b"); bg.configure(src2, b2, "Ninja Multi-Config")
            gm = bg.load_graph(b2)
            if select(gm, ["src/core.c"])["mode"] != "full" or not any("configurations" in u for u in gm["unknown"]):
                fails.append("multi_config")
    sub = sel_n = 0
    for name, f, o, got, mode in stats:
        if mode == "subset":
            sub += len(got); sel_n += len(o)
    print("recall rows: %d, subset selections: selected %d tests, needed %d (precision %.2f)" %
          (len(stats), sub, sel_n, sel_n / sub if sub else 1.0))
    for r in stats:
        print("  %s %s oracle=%s selected=%s mode=%s" % r)
    print("failed checks: %s" % (" ".join(sorted(fails)) or "none"))
    if MUTANT:
        want = {"no_closure": ["header_scan", "recall_basic", "recall_incl", "recall_mixed"],
                "scan_off": ["header_scan", "recall_incl"]}[MUTANT]
        if sorted(fails) == want:
            print("MUTANT killed by exactly " + " ".join(want)); return 0
        print("MUTANT not killed by the exact set"); return 1
    if fails:
        return 1
    print("build_graph test ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
