#!/usr/bin/env python3
"""m236: the repository's own sidecar (tools/build_graph_inputs.json) against the
real build graph. Run in the built tree: argv[1] is the build directory. Checks:
loads_clean (the graph has no 'unknown' entry), all_declared (every test the
codemodel cannot map has a declaration), globs_live (every declared file glob
matches at least one file of the source tree). The File API reply is made by
re-running cmake on the build directory when it is missing, which rewrites the
generated build files with the same content. Mutants run on a copy of the sidecar
and exit 0 only on the exact expected failing set. Windows prints SKIP."""
import fnmatch, json, os, shutil, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(SRC, "tools"))
import build_graph as bg

MUTANT = sys.argv[sys.argv.index("--mutant") + 1] if "--mutant" in sys.argv else None
EXPECT = {"drop_decl": ["all_declared", "loads_clean"], "stale_glob": ["globs_live"]}


def source_files():
    out = []
    for d, dirs, names in os.walk(SRC):
        dirs[:] = [x for x in dirs if not x.startswith(".") and
                   not os.path.exists(os.path.join(d, x, "CMakeCache.txt"))]
        out += [os.path.relpath(os.path.join(d, n), SRC).replace(os.sep, "/") for n in names]
    return out


def main():
    if os.name == "nt":
        print("SKIP: build graph tests are not measured on Windows")
        return 77
    build = os.path.abspath(sys.argv[1])
    sidecar = os.path.join(SRC, bg.SIDECAR)
    tmp = tempfile.mkdtemp()
    try:
        if MUTANT:
            d = json.load(open(sidecar))
            if MUTANT == "drop_decl":
                d["tests"].pop(sorted(d["tests"])[0])
            elif MUTANT == "stale_glob":
                d["tests"][sorted(d["tests"])[0]]["files"].append("no_such_dir/no_such_file.*")
            sidecar = os.path.join(tmp, "inputs.json")
            json.dump(d, open(sidecar, "w"))
        reply = os.path.join(build, ".cmake", "api", "v1", "reply")
        if not os.path.isdir(reply):
            assert bg.configure(SRC, build) == 0, "cmake re-run for the File API reply failed"
        g = bg.load_graph(build, inputs=sidecar)
        fails = []
        if g["unknown"]:
            fails.append("loads_clean")
            print("unknown:", g["unknown"][:5])
        und = [t["name"] for t in g["tests"] if t["target"] is None and not t.get("declared")]
        if und:
            fails.append("all_declared")
            print("undeclared:", und[:5])
        files = source_files()
        stale = sorted({(t["name"], p) for t in g["tests"] for p in (t.get("declared") or {}).get("files", [])
                        if not any(fnmatch.fnmatchcase(f, p) for f in files)})
        if stale:
            fails.append("globs_live")
            print("stale globs:", stale[:5])
        n = sum(1 for t in g["tests"] if t.get("declared"))
        print("tests %d unmapped-declared %d targets %d" % (len(g["tests"]), n, len(g["targets"])))
        fails.sort()
        if MUTANT:
            print("failed checks:", " ".join(fails) or "none")
            return 0 if fails == EXPECT[MUTANT] else 1
        print("failed checks:", " ".join(fails) or "none")
        if not fails:
            print("build_graph_repo test ok")
        return 1 if fails else 0
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
