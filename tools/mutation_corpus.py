#!/usr/bin/env python3
"""Operator induction, phase 2: mutation corpus (training data only).

Sources: C workspaces in this repo that carry their own self-check - a main()
that builds and exits 0. By default these are the engineering bank's golden
after/ trees. The whole bank is development data since fa230f4; the new blind
batch is never passed here.

For each source it makes single-primitive mutants:
  relop     <  <=  >  >=  swapped one at a time
  arith     +  <->  -    (binary, between operands)
  logic     && <-> ||
  literal   integer literal +1 / -1
A mutant is kept as a training case only when the self-check catches it:
the build fails or the exit code becomes non-zero ("caught"). Uncaught
mutants are recorded but not used.

With --repair, symbols-agent is run on each caught mutant with a neutral
task text (no hint of the primitive or site), and the result is
recorded: did the program build and exit 0 again? Set SYMBOLS_TRACE to
collect phase-1 traces for the inducer.

Output: TSV on stdout (and --out): id, source, file, primitive, from, to,
line, caught, repaired, exact (the
repaired file equals the original byte for byte).  Summary line on stderr: MUTATION_CORPUS ...
Declared rules: primitive set above, max --per-source mutants per source.
"""
import argparse, os, re, shutil, subprocess, sys, tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BANK = ROOT / "tests" / "fixtures" / "engineering_bank"
NEUTRAL_TASK = "The program no longer builds or no longer exits 0. Fix it without changing its intended behavior."

TOKEN = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*.*?\*/|'
                   r'<<=|>>=|<=|>=|==|!=|&&|\|\||->|\+\+|--|<<|>>|[-+*/%<>=!&|^~?:;,.(){}\[\]]|'
                   r'[A-Za-z_]\w*|\d+[uUlL]*|\s+|.', re.S)


def swaps(tok, prev, nxt):
    if tok in ("<", "<=", ">", ">="):
        return [("relop", t) for t in ("<", "<=", ">", ">=") if t != tok]
    if tok in ("&&", "||"):
        return [("logic", "||" if tok == "&&" else "&&")]
    if tok in ("+", "-") and prev and (prev[-1:].isalnum() or prev in (")", "]")) and nxt:
        return [("arith", "-" if tok == "+" else "+")]
    if re.fullmatch(r"\d+", tok):
        v = int(tok)
        return [("literal", str(v + 1))] + ([("literal", str(v - 1))] if v > 0 else [])
    return []


def mutants(text):
    toks = [m.group(0) for m in TOKEN.finditer(text)]
    sig = [i for i, t in enumerate(toks) if not t.isspace() and not t.startswith(("//", "/*"))]
    line_starts_pp = set()
    ln, at_start = 1, True
    lines_of = []
    for t in toks:
        lines_of.append(ln)
        ln += t.count("\n")
    pp_lines = {i + 1 for i, l in enumerate(text.split("\n")) if l.lstrip().startswith("#")}
    for k, i in enumerate(sig):
        if lines_of[i] in pp_lines:
            continue
        prev = toks[sig[k - 1]] if k > 0 else ""
        nxt = toks[sig[k + 1]] if k + 1 < len(sig) else ""
        for prim, new in swaps(toks[i], prev, nxt):
            out = toks[:i] + [new] + toks[i + 1:]
            yield prim, toks[i], new, lines_of[i], "".join(out)


def build_run(d, timeout=10):
    srcs = sorted(p.name for p in Path(d).glob("*.c"))
    if not srcs:
        return None, None
    exe = os.path.join(d, "a.out.mut")
    try:
        c = subprocess.run(["gcc", "-std=c11", "-w", "-o", exe] + srcs, cwd=d, capture_output=True, timeout=60)
    except subprocess.TimeoutExpired:
        return 0, None
    if c.returncode != 0:
        return 0, None
    try:
        r = subprocess.run([exe], cwd=d, capture_output=True, timeout=timeout)
        rc = r.returncode
    except subprocess.TimeoutExpired:
        rc = 124
    os.remove(exe)
    return 1, rc


def sources(bank):
    for line in (bank / "index.tsv").read_text(encoding="utf-8").splitlines()[1:]:
        parts = line.split("\t")
        if len(parts) >= 3:
            d = bank / parts[2] / "after"
            if d.is_dir():
                yield parts[0], d


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bank", default=str(BANK))
    ap.add_argument("--per-source", type=int, default=12)
    ap.add_argument("--repair", action="store_true")
    ap.add_argument("--agent", default=str(ROOT / "build" / "symbols-agent"))
    ap.add_argument("--keep", help="directory to keep caught mutant workspaces")
    ap.add_argument("--out")
    a = ap.parse_args()
    rows, n_src = [], 0
    for sid, d in sources(Path(a.bank)):
        c, rc = build_run(str(d))
        if c != 1 or rc != 0 or not any("main(" in p.read_text(errors="ignore") for p in d.glob("*.c")):
            continue
        n_src += 1
        made = 0
        for cf in sorted(d.glob("*.c")):
            text = cf.read_text(encoding="utf-8", errors="ignore")
            for prim, frm, to, line, mtext in mutants(text):
                if made >= a.per_source:
                    break
                made += 1
                mid = f"{sid}_m{made:02d}"
                w = tempfile.mkdtemp(prefix="mut_")
                shutil.copytree(d, w, dirs_exist_ok=True)
                (Path(w) / cf.name).write_text(mtext, encoding="utf-8")
                mc, mrc = build_run(w)
                caught = int(mc == 0 or mrc != 0)
                repaired, exact = "", 0
                if caught and a.repair:
                    try:
                        subprocess.run([a.agent, "-w", w, NEUTRAL_TASK], capture_output=True, timeout=120)
                    except subprocess.TimeoutExpired:
                        pass
                    rc2, rrc = build_run(w)
                    repaired = str(int(rc2 == 1 and rrc == 0))
                    exact = int((Path(w) / cf.name).read_text(encoding="utf-8", errors="ignore") == text)
                if caught and a.keep:
                    shutil.copytree(w, Path(a.keep) / mid, dirs_exist_ok=True)
                shutil.rmtree(w, ignore_errors=True)
                rows.append([mid, sid, cf.name, prim, frm, to, str(line), str(caught), repaired, str(exact) if repaired else ""])
    text = "id\tsource\tfile\tprimitive\tfrom\tto\tline\tcaught\trepaired\texact\n" + "".join("\t".join(r) + "\n" for r in rows)
    if a.out:
        Path(a.out).write_text(text, encoding="utf-8")
    sys.stdout.write(text)
    caught = [r for r in rows if r[7] == "1"]
    by = {}
    for r in caught:
        b = by.setdefault(r[3], [0, 0])
        b[0] += 1
        b[1] += r[8] == "1"
    summ = " ".join(f"{k}={v[1]}/{v[0]}" for k, v in sorted(by.items())) if a.repair else ""
    print(f"MUTATION_CORPUS sources={n_src} mutants={len(rows)} caught={len(caught)} "
          f"repaired={sum(1 for r in caught if r[8] == '1') if a.repair else 'n/a'} "
          f"exact={sum(1 for r in caught if r[9] == '1') if a.repair else 'n/a'} {summ}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
