#!/usr/bin/env python3
"""Read-only build/test graph from the CMake File API and CTest, with a
conservative affected-test selection (m231, prototype).

  build_graph.py configure SRC BUILD [-G GEN]   write the codemodel query, configure
  build_graph.py graph BUILD [--config C]       print the graph as JSON
  build_graph.py select BUILD --changed F...    print the selection as JSON

Rule: anything the model cannot prove selects the full gate (mode "full").
The tests of the build must be built first: ctest omits the command of a test
whose executable does not exist, and such a test is unmapped.
"""
import argparse, collections, glob, json, os, re, subprocess, sys

SUPPORTED_GENERATORS = ("Unix Makefiles", "Ninja")
CODEMODEL_MAJOR = 2


def configure(src, build, generator=None):
    q = os.path.join(build, ".cmake", "api", "v1", "query")
    os.makedirs(q, exist_ok=True)
    open(os.path.join(q, "codemodel-v2"), "w").close()
    cmd = ["cmake", "-S", src, "-B", build]
    if generator:
        cmd += ["-G", generator]
    return subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode


def _rel(path, root):
    if os.path.isabs(path):
        try:
            return os.path.relpath(path, root).replace(os.sep, "/")
        except ValueError:
            return path
    return path.replace(os.sep, "/")


def load_graph(build, config=None):
    """Return a graph dict. Unknowns are listed in graph['unknown']; they are
    reasons to select the full gate. Never raises on missing metadata."""
    g = {"targets": [], "tests": [], "unknown": []}
    reply = os.path.join(build, ".cmake", "api", "v1", "reply")
    idx = sorted(glob.glob(os.path.join(reply, "index-*.json")))
    if not idx:
        g["unknown"].append("no File API reply (configure with the codemodel-v2 query first)")
        return g
    index = json.load(open(idx[-1]))
    gen = index.get("cmake", {}).get("generator", {})
    g["generator"] = gen.get("name")
    g["multi_config"] = bool(gen.get("multiConfig"))
    if g["generator"] not in SUPPORTED_GENERATORS:
        g["unknown"].append("generator not supported: %s" % g["generator"])
    cm = [o for o in index.get("objects", []) if o.get("kind") == "codemodel"]
    if not cm or cm[0].get("version", {}).get("major") != CODEMODEL_MAJOR:
        g["unknown"].append("codemodel major version is not %d" % CODEMODEL_MAJOR)
        return g
    model = json.load(open(os.path.join(reply, cm[0]["jsonFile"])))
    paths = model.get("paths", {})
    src_root, build_root = paths.get("source", ""), paths.get("build", build)
    g["source"] = src_root
    confs = model.get("configurations", [])
    if len(confs) != 1 and not config:
        g["unknown"].append("%d configurations and none chosen" % len(confs))
        return g
    conf = confs[0] if len(confs) == 1 else next((c for c in confs if c.get("name") == config), None)
    if conf is None:
        g["unknown"].append("configuration not found: %s" % config)
        return g
    ids = {}
    for t in conf.get("targets", []):
        j = json.load(open(os.path.join(reply, t["jsonFile"])))
        ids[j["id"]] = j["name"]
        g["targets"].append({
            "id": j["id"], "name": j["name"], "type": j["type"],
            "sources": sorted(s["path"] for s in j.get("sources", [])
                              if not s.get("isGenerated") and not os.path.isabs(s["path"])),
            "generated": sorted(_rel(s["path"], build_root) for s in j.get("sources", [])
                                if s.get("isGenerated")),
            "deps": sorted(d["id"] for d in j.get("dependencies", [])),
            "artifacts": sorted(_rel(a["path"], build_root) for a in j.get("artifacts", [])),
        })
    for t in g["targets"]:
        t["deps"] = sorted(ids.get(d, d) for d in t["deps"])
    p = subprocess.run(["ctest", "--show-only=json-v1"], cwd=build, capture_output=True, text=True)
    try:
        tj = json.loads(p.stdout)
    except ValueError:
        g["unknown"].append("ctest json unreadable")
        return g
    for t in tj.get("tests", []):
        props = {x["name"]: x["value"] for x in t.get("properties", [])}
        cmd = t.get("command")
        mapped = None
        if cmd:
            c0 = _rel(cmd[0], build_root)
            hit = [x["name"] for x in g["targets"] if c0 in x["artifacts"]]
            mapped = hit[0] if len(hit) == 1 else None
        labels = props.get("LABELS", "")
        g["tests"].append({"name": t["name"], "command": cmd, "target": mapped,
                           "labels": sorted(labels if isinstance(labels, list) else
                                            [x for x in labels.split(";") if x])})
        if mapped is None:
            g["unknown"].append("test %s is not mapped to a target" % t["name"])
    g["targets"].sort(key=lambda x: x["name"])
    g["tests"].sort(key=lambda x: x["name"])
    return g


SCAN_EXT = (".c", ".h", ".cc", ".cpp", ".cxx", ".hpp", ".hh", ".inc", ".inl")
HEADER_EXT = (".h", ".hpp", ".hh", ".inc", ".inl")
INCLUDE_RX = re.compile(r'^[ \t]*#[ \t]*include[ \t]*([<"])([^>"\n]+)[>"]', re.M)


def scan_includes(root):
    """Textual include scan of the source tree. Returns (files, includers,
    unresolved): files is the set of scanned paths (relative, '/'), includers
    maps a basename to the files that include something with that basename,
    unresolved lists quoted includes whose basename matches no scanned file.
    Matching is by basename only (over-approximation), no -I resolution."""
    files, incs = set(), {}
    for d, dirs, names in os.walk(root):
        dirs[:] = [x for x in dirs if not x.startswith(".") and
                   not os.path.exists(os.path.join(d, x, "CMakeCache.txt"))]
        for n in names:
            if n.endswith(SCAN_EXT):
                rel = os.path.relpath(os.path.join(d, n), root).replace(os.sep, "/")
                files.add(rel)
                try:
                    text = open(os.path.join(d, n), encoding="utf-8", errors="replace").read()
                except OSError:
                    incs[rel] = None
                    continue
                incs[rel] = [(m.group(1), os.path.basename(m.group(2))) for m in INCLUDE_RX.finditer(text)]
    bases = {os.path.basename(f) for f in files}
    includers, unresolved = collections.defaultdict(set), []
    for f, lst in incs.items():
        if lst is None:
            unresolved.append("%s: unreadable" % f)
            continue
        for kind, b in lst:
            includers[b].add(f)
            if kind == '"' and b not in bases:
                unresolved.append("%s: %s" % (f, b))
    return files, includers, sorted(unresolved)


def select(g, changed, scan=True):
    """Changed paths are relative to the source root. Returns
    {mode: 'subset'|'full', tests: [...], reasons: [...]}. With scan, a changed
    file also affects every file that includes its basename, transitively."""
    if g["unknown"]:
        return {"mode": "full", "tests": [t["name"] for t in g["tests"]], "reasons": list(g["unknown"])}
    full = lambda why: {"mode": "full", "tests": [t["name"] for t in g["tests"]], "reasons": why}
    owners = {}
    for t in g["targets"]:
        for s in t["sources"]:
            owners.setdefault(s, set()).add(t["name"])
    scanned, includers = set(), {}
    if scan and g.get("source"):
        scanned, includers, unresolved = scan_includes(g["source"])
        if unresolved:
            return full(["unresolved include: %s" % u for u in unresolved[:5]])
    hit = set()
    for f in changed:
        f = f.replace(os.sep, "/")
        base = os.path.basename(f)
        if base == "CMakeLists.txt" or base == "CMakeCache.txt" or f.endswith(".cmake"):
            return full(["build definition changed: %s" % f])
        if f in owners:
            hit |= owners[f]
        elif not (f in scanned and includers.get(base)):
            return full(["changed file is in no target and has no includer: %s" % f])
        closure, todo = {f}, [f]
        while todo:
            for i in includers.get(os.path.basename(todo.pop()), ()):
                if i not in closure:
                    closure.add(i)
                    todo.append(i)
                    hit |= owners.get(i, set())
    rdeps = {}
    for t in g["targets"]:
        for d in t["deps"]:
            rdeps.setdefault(d, set()).add(t["name"])
    todo, seen = list(hit), set(hit)
    while todo:
        for r in rdeps.get(todo.pop(), ()):
            if r not in seen:
                seen.add(r)
                todo.append(r)
    return {"mode": "subset", "tests": sorted(t["name"] for t in g["tests"] if t["target"] in seen),
            "reasons": []}


def main(argv):
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    c = sub.add_parser("configure"); c.add_argument("src"); c.add_argument("build"); c.add_argument("-G")
    g = sub.add_parser("graph"); g.add_argument("build"); g.add_argument("--config")
    s = sub.add_parser("select"); s.add_argument("build"); s.add_argument("--config")
    s.add_argument("--changed", nargs="+", required=True)
    a = ap.parse_args(argv)
    if a.cmd == "configure":
        return configure(a.src, a.build, a.G)
    gr = load_graph(a.build, a.config)
    print(json.dumps(gr if a.cmd == "graph" else select(gr, a.changed), indent=1, sort_keys=True))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
