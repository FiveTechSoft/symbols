#!/usr/bin/env python3
"""Operator induction, phase 4: induce operators from labeled repair traces.

Input: the TSV written by tools/mutation_corpus.py --repair (training data
only: mutants of the bank's development workspaces; the blind batch is
never passed here). Each repaired row carries the induction pattern of the
edit the engine kept, "<primitive class>@<site>" (see src/task_ops.c,
site_ctx/prim_class), and the label: exact = the repair restored the
original file byte for byte.

Rule (docs/operator_induction.md, decisions of 2026-09-23):
  promoted  exact on >= K=3 distinct source workspaces, 0 non-exact repairs
  demoted   any non-exact repair (one failure demotes)
  otherwise unlisted (too little evidence either way)
Leave-one-out is measured, not assumed: mutation_corpus.py --loo-from
re-runs every source with a table induced from the OTHER sources only.

Output: operators.tsv (pattern, status, support, fires, fails) on stdout
or --out. Load it in the engine with SYMBOLS_OPERATORS=<file>.
"""
import argparse, csv, sys
from collections import defaultdict

K = 3


def read_rows(path):
    with open(path, encoding="utf-8") as f:
        return [r for r in csv.DictReader(f, delimiter="\t")]


def induce(rows, k=K):
    sup, fires, fails = defaultdict(set), defaultdict(int), defaultdict(int)
    for r in rows:
        pat = r.get("pattern") or ""
        if not pat or r.get("repaired") != "1":
            continue
        fires[pat] += 1
        if r.get("exact") == "1":
            sup[pat].add(r["source"])
        else:
            fails[pat] += 1
    ops = []
    for pat in sorted(fires):
        if fails[pat]:
            st = "demoted"
        elif len(sup[pat]) >= k:
            st = "promoted"
        else:
            continue
        ops.append((pat, st, len(sup[pat]), fires[pat], fails[pat]))
    return ops


def table(ops):
    out = "# pattern\tstatus\tsupport_sources\tfires\tfails  (tools/induce_operators.py, K=%d)\n" % K
    return out + "".join("%s\t%s\t%d\t%d\t%d\n" % o for o in ops)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("corpus_tsv")
    ap.add_argument("--out")
    a = ap.parse_args()
    t = table(induce(read_rows(a.corpus_tsv)))
    if a.out:
        open(a.out, "w", encoding="utf-8").write(t)
    sys.stdout.write(t)
    return 0


if __name__ == "__main__":
    sys.exit(main())
