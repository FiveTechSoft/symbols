#!/usr/bin/env python3
"""Composition census for heterogeneous rules R1 o R2 => R3 (Fase 3).

Over the deduped observed triples of data/bible/bible_relations.tsv
(same dedup the C learner performs: 624 distinct (A,REL,B) triples),
count every candidate rule (R1,R2,R3): premise instances are pairs
R1(A,B) and R2(B,C) with A != C, B != A, B != C; the rule confirms
when R3(A,C) is observed. A rule is LICENSABLE only with confirm
rate 1.0 and support >= 2 (the C discovery gate mirrors this).

Census result on the KJV corpus (2026-09-17): NO rule reaches
rate 1.0 at support >= 2 (best: HIJO_DE x REY_DE => REY_DE at
32/74 = 0.432), so the honest meta-discovery licenses ZERO
heterogeneous compositions here and the chat must stay UNKNOWN.
This script is the measurement backing that decision.
"""
import collections
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TSV = os.path.join(ROOT, "data", "bible", "bible_relations.tsv")


def load_triples(path):
    triples = set()
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            p = line.rstrip("\r\n").split("\t")
            if len(p) < 3 or p[0] == p[2]:
                continue
            triples.add((p[0].lower(), p[1], p[2].lower()))
    return triples


def census(triples):
    idx = collections.defaultdict(set)
    for (a, r, b) in triples:
        idx[r].add((a, b))
    # successor map: for each family, B -> set of C
    nxt = collections.defaultdict(lambda: collections.defaultdict(set))
    for (a, r, b) in triples:
        nxt[r][a].add(b)
    stats = {}
    for r1 in idx:
        for r2 in idx:
            for (a, b) in idx[r1]:
                for c in nxt[r2].get(b, ()):
                    if c == a:
                        continue
                    for r3 in idx:
                        k = (r1, r2, r3)
                        s = stats.setdefault(k, [0, 0])
                        s[0] += 1
                        if (a, c) in idx[r3]:
                            s[1] += 1
    return stats


def main():
    triples = load_triples(TSV)
    if len(triples) != 624:
        print("FAIL triples %d != 624" % len(triples))
        return 1
    stats = census(triples)
    licensed = [k for k, (prem, hit) in stats.items()
                if prem >= 2 and hit == prem]
    print("candidate rules evaluated:", len(stats))
    best = sorted(stats.items(), key=lambda kv: -kv[1][1] / max(kv[1][0], 1))[:3]
    for (r1, r2, r3), (prem, hit) in best:
        print("  best: %s x %s => %s  %d/%d = %.3f"
              % (r1, r2, r3, hit, prem, hit / prem))
    print("licensable rules (rate 1.0, support>=2):", len(licensed))
    if licensed:
        print("FAIL unexpected licensable rules:", licensed)
        return 2
    print("PASS zero licensable compositions: fail-closed is correct "
          "on this corpus")
    return 0


if __name__ == "__main__":
    sys.exit(main())