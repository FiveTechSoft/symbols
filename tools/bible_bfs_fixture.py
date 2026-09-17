#!/usr/bin/env python3
"""Independent BFS oracle for the bible relations corpus (Fase 2).

Regenerates the exhaustive census first committed in e1d9719
("exhaustive 2-hop conclusion census vs python fixture", 368 novel
2-hop conclusions) and extends it to ALL depths (BFS novel set:
2053 pairs = 368 at depth 2 + 1685 at depth >= 3).

Graph: directed edges child -> parent from HIJO_DE rows of
data/bible/bible_relations.tsv, self-loops dropped (the KB never
admits them). A pair (S,O) is NOVEL iff O is reachable from S
in >1 edge (depth >= 2). Direct pairs (depth 1) are excluded:
the conclusion must not be derivable directly (plain path).

Two independent methods inside this script must agree before the
fixture TSV is trusted (double-method rule).

Output: data/bible/bible_bfs_fixture.tsv with lines "S<TAB>O<TAB>D"
where D = exact BFS distance (>= 2), sorted.
"""
import os
import sys
from collections import deque

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TSV = os.path.join(ROOT, "data", "bible", "bible_relations.tsv")
OUT = os.path.join(ROOT, "data", "bible", "bible_bfs_fixture.tsv")


def load_edges(path):
    edges = []
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            parts = line.rstrip("\r\n").split("\t")
            if len(parts) < 3:
                continue
            s, rel, o = parts[0], parts[1], parts[2]
            if rel != "HIJO_DE":
                continue
            if s == o:
                continue  # self-loop: never admitted by the KB
            edges.append((s.lower(), o.lower()))
    return edges


def bfs_all(edges, use_adj):
    """Reachable-with->1-edge pairs. Method A: adjacency dict +
    deque BFS from every node. use_adj toggles the container only;
    the depth-exact double check happens in main via two passes."""
    if use_adj:
        adj = {}
        for s, o in edges:
            adj.setdefault(s, []).append(o)
        nbrs = lambda x: adj.get(x, [])
    else:
        nbrs = lambda x: [o for (s, o) in edges if s == x]

    novel = {}
    starts = sorted({s for (s, _) in edges})
    for src in starts:
        dist = {src: 0}
        q = deque([src])
        while q:
            cur = q.popleft()
            d = dist[cur]
            for nx in nbrs(cur):
                if nx not in dist:
                    dist[nx] = d + 1
                    q.append(nx)
        for dst, d in dist.items():
            if d >= 2:
                key = (src, dst)
                if key not in novel or d < novel[key]:
                    novel[key] = d
    return novel


def main():
    edges = load_edges(TSV)
    if len(edges) != 913:
        print("FAIL edge count %d != 913 (census contract)" % len(edges))
        return 1
    direct = {(s, o) for (s, o) in edges}
    # dedupe edges (repeated verses) for the BFS itself
    uniq = sorted(set(edges))
    a = bfs_all(uniq, use_adj=True)
    b = bfs_all(uniq, use_adj=False)
    if a != b:
        print("FAIL double-method mismatch: %d vs %d" % (len(a), len(b)))
        return 2
    two = sum(1 for d in a.values() if d == 2)
    deep = sum(1 for d in a.values() if d >= 3)
    print("edges=%d uniq=%d direct_pairs=%d" % (len(edges), len(uniq), len(direct)))
    print("novel 2-hop=%d deep(>=3)=%d total=%d" % (two, deep, len(a)))
    if two != 368 or len(a) != 2053:
        print("FAIL census contract: expect 368 2-hop and 2053 total")
        return 3
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for (s, o) in sorted(a):
            f.write("%s\t%s\t%d\n" % (s, o, a[(s, o)]))
    print("wrote %s (%d rows)" % (OUT, len(a)))
    return 0


if __name__ == "__main__":
    sys.exit(main())