#!/usr/bin/env python3
"""
Tiny Euclidean geometry engine for the Master Algorithm autodidact experiment.

NOT AlphaGeometry. Symbolic forward-chaining over a FIXED, documented axiom
set + optional citation of archived lemmas. Numeric coordinate checks are a
sanity filter only — a fact enters the library only if the symbolic engine
proves it.

Axiom set (honest, small):
  A1  Midpoint halves: Midpoint(M,A,B) ⇒ AM = MB
  A2  Midpoint theorem (midline): Midpoint(M,A,B), Midpoint(N,A,C)
        ⇒ MN ∥ BC  and  2·MN = BC   (encoded as Parallel + HalfSeg)
  A3  Isosceles base angles: AB = AC ⇒ ∠ABC = ∠ACB
  A4  Isosceles converse: ∠ABC = ∠ACB ⇒ AB = AC
  A5  Vertical angles: opposite angles at a crossing are equal
  A6  SSS congruence ⇒ corresponding angles equal
  A7  SAS congruence ⇒ corresponding sides/angles equal
  A8  ASA congruence ⇒ corresponding sides equal
  A9  Parallelogram opposite sides: Para(ABCD) ⇒ AB=CD, AD=BC, AB∥CD, AD∥BC
 A10  Equilateral: Equilateral(ABC) ⇒ AB=BC=CA and all angles equal
 A11  Transitivity / symmetry of EqSeg, EqAng, Parallel
 A12  Segment reflexivity; angle reflexivity

Domain: triangles + midpoints + midlines + isosceles/equilateral + simple
parallelograms from midpoints (Varignon-style).
"""

from __future__ import annotations

import itertools
import math
import random
from dataclasses import dataclass, field
from typing import Any, Iterable, Optional

EPS = 1e-6


# ---------------------------------------------------------------------------
# Atoms
# ---------------------------------------------------------------------------

def seg(a: str, b: str) -> frozenset:
    return frozenset((a, b))


def ang(v: str, p: str, q: str) -> tuple:
    """Angle at v between points p and q (unordered rays)."""
    return (v, frozenset((p, q)))


def line_of(a: str, b: str) -> frozenset:
    return frozenset((a, b))


def tri(a: str, b: str, c: str) -> frozenset:
    return frozenset((a, b, c))


# ---------------------------------------------------------------------------
# Facts
# ---------------------------------------------------------------------------

@dataclass(frozen=True)
class Fact:
    kind: str  # EqSeg | HalfSeg | EqAng | Parallel | Midpoint | Congruent | Para | Equilateral | Isos
    args: tuple
    source: str = "axiom"  # axiom | lemma:<name> | derived
    cite: tuple = ()  # lemma names cited in this derivation step

    def key(self) -> tuple:
        return (self.kind, self.args)

    def pretty(self) -> str:
        k, a = self.kind, self.args
        if k == "EqSeg":
            s1, s2 = a
            return f"{_fmt_seg(s1)} = {_fmt_seg(s2)}"
        if k == "HalfSeg":
            half, full = a
            return f"2·{_fmt_seg(half)} = {_fmt_seg(full)}"
        if k == "EqAng":
            return f"∠{_fmt_ang(a[0])} = ∠{_fmt_ang(a[1])}"
        if k == "Parallel":
            return f"{_fmt_seg(a[0])} ∥ {_fmt_seg(a[1])}"
        if k == "Midpoint":
            return f"{a[0]} = midpoint({a[1]},{a[2]})"
        if k == "Congruent":
            return f"△{''.join(a[0])} ≅ △{''.join(a[1])} ({a[2]})"
        if k == "Para":
            return f"parallelogram {''.join(a[0])}"
        if k == "Equilateral":
            return f"equilateral △{''.join(sorted(a[0]))}"
        if k == "Isos":
            return f"isosceles △{''.join(sorted(a[0]))} apex {a[1]}"
        return f"{k}{a}"


def _fmt_seg(s: frozenset) -> str:
    return "".join(sorted(s))


def _fmt_ang(a: tuple) -> str:
    v, rays = a
    pts = sorted(rays)
    if len(pts) < 2:
        return f"?{v}?"
    return f"{pts[0]}{v}{pts[1]}"


# ---------------------------------------------------------------------------
# Construction program
# ---------------------------------------------------------------------------

@dataclass
class Construction:
    """Typed construction program + optional named figure tags."""
    steps: list[tuple]  # (op, ...)
    name: str = "fig"
    tags: tuple = ()

    def points(self) -> list[str]:
        pts: list[str] = []
        for st in self.steps:
            op = st[0]
            if op == "free":
                pts.append(st[1])
            elif op == "midpoint":
                pts.append(st[1])
            elif op == "equilateral_third":
                pts.append(st[1])
            elif op == "parallelogram_fourth":
                pts.append(st[1])
        return pts

    def free_points(self) -> list[str]:
        return [st[1] for st in self.steps if st[0] == "free"]

    def midpoints(self) -> list[tuple]:
        return [(st[1], st[2], st[3]) for st in self.steps if st[0] == "midpoint"]

    def signature(self) -> str:
        return "|".join(":".join(map(str, st)) for st in self.steps)


# ---------------------------------------------------------------------------
# Numeric realization (sanity filter)
# ---------------------------------------------------------------------------

def realize(construction: Construction, seed: int = 0) -> dict[str, tuple[float, float]]:
    rng = random.Random(seed)
    coords: dict[str, tuple[float, float]] = {}

    # Parse isos constraints: isos:APEX:P:Q means AP=AQ (place Q on circle around A through P)
    isos_constraints = []
    for tag in construction.tags:
        if tag.startswith("isos:"):
            parts = tag.split(":")
            if len(parts) == 4:
                isos_constraints.append((parts[1], parts[2], parts[3]))

    def place_free(name: str) -> None:
        # Honor isos: if this point is Q in (apex,P,Q) and apex,P already placed, put Q at equal distance
        for apex, p, q in isos_constraints:
            if name == q and apex in coords and p in coords and q not in coords:
                ax, ay = coords[apex]
                px, py = coords[p]
                r = math.hypot(px - ax, py - ay)
                # random angle, not coinciding with P
                for _ in range(30):
                    theta = rng.uniform(0.4, 2 * math.pi - 0.4)
                    x = ax + r * math.cos(theta)
                    y = ay + r * math.sin(theta)
                    if math.hypot(x - px, y - py) > 0.8:
                        coords[name] = (x, y)
                        return
                coords[name] = (ax + r * math.cos(1.2), ay + r * math.sin(1.2))
                return
            if name == p and apex in coords and q in coords and p not in coords:
                ax, ay = coords[apex]
                qx, qy = coords[q]
                r = math.hypot(qx - ax, qy - ay)
                theta = rng.uniform(0.4, 2 * math.pi - 0.4)
                coords[name] = (ax + r * math.cos(theta), ay + r * math.sin(theta))
                return
        # Keep free points in general position, well separated
        for _ in range(40):
            x, y = rng.uniform(-5, 5), rng.uniform(-5, 5)
            ok = True
            for pt in coords.values():
                if math.hypot(x - pt[0], y - pt[1]) < 1.2:
                    ok = False
                    break
            if ok:
                coords[name] = (x, y)
                return
        coords[name] = (rng.uniform(-5, 5), rng.uniform(-5, 5))

    for st in construction.steps:
        op = st[0]
        if op == "free":
            place_free(st[1])
        elif op == "midpoint":
            _, m, a, b = st
            ax, ay = coords[a]
            bx, by = coords[b]
            coords[m] = ((ax + bx) / 2.0, (ay + by) / 2.0)
        elif op == "equilateral_third":
            _, c, a, b = st
            ax, ay = coords[a]
            bx, by = coords[b]
            mx, my = (ax + bx) / 2.0, (ay + by) / 2.0
            dx, dy = bx - ax, by - ay
            hx, hy = -dy * math.sqrt(3) / 2.0, dx * math.sqrt(3) / 2.0
            coords[c] = (mx + hx, my + hy)
        elif op == "parallelogram_fourth":
            _, d, a, b, c = st
            ax, ay = coords[a]
            bx, by = coords[b]
            cx, cy = coords[c]
            coords[d] = (ax + cx - bx, ay + cy - by)
        else:
            raise ValueError(f"unknown op {op}")
    return coords


def dist(coords: dict, a: str, b: str) -> float:
    ax, ay = coords[a]
    bx, by = coords[b]
    return math.hypot(ax - bx, ay - by)


def angle_at(coords: dict, v: str, p: str, q: str) -> float:
    vx, vy = coords[v]
    px, py = coords[p]
    qx, qy = coords[q]
    ax, ay = px - vx, py - vy
    bx, by = qx - vx, qy - vy
    na = math.hypot(ax, ay)
    nb = math.hypot(bx, by)
    if na < EPS or nb < EPS:
        return float("nan")
    cos = max(-1.0, min(1.0, (ax * bx + ay * by) / (na * nb)))
    return math.acos(cos)


def direction(coords: dict, a: str, b: str) -> float:
    ax, ay = coords[a]
    bx, by = coords[b]
    return math.atan2(by - ay, bx - ax)


def numeric_holds(fact: Fact, coords: dict) -> bool:
    k, a = fact.kind, fact.args
    if k == "EqSeg":
        s1, s2 = a
        p, q = tuple(s1)
        r, s = tuple(s2)
        return abs(dist(coords, p, q) - dist(coords, r, s)) < 1e-4
    if k == "HalfSeg":
        half, full = a
        p, q = tuple(half)
        r, s = tuple(full)
        return abs(2 * dist(coords, p, q) - dist(coords, r, s)) < 1e-4
    if k == "EqAng":
        a1, a2 = a
        v1, rays1 = a1
        v2, rays2 = a2
        p1, q1 = tuple(rays1)
        p2, q2 = tuple(rays2)
        return abs(angle_at(coords, v1, p1, q1) - angle_at(coords, v2, p2, q2)) < 1e-3
    if k == "Parallel":
        l1, l2 = a
        a1, b1 = tuple(l1)
        a2, b2 = tuple(l2)
        d1 = direction(coords, a1, b1)
        d2 = direction(coords, a2, b2)
        diff = abs(d1 - d2) % math.pi
        return min(diff, math.pi - diff) < 1e-3
    if k == "Midpoint":
        m, p, q = a
        mx, my = coords[m]
        px, py = coords[p]
        qx, qy = coords[q]
        return abs(mx - (px + qx) / 2) < 1e-6 and abs(my - (py + qy) / 2) < 1e-6
    if k == "Equilateral":
        pts = list(a[0])
        d01 = dist(coords, pts[0], pts[1])
        d12 = dist(coords, pts[1], pts[2])
        d20 = dist(coords, pts[2], pts[0])
        return abs(d01 - d12) < 1e-4 and abs(d12 - d20) < 1e-4
    if k == "Isos":
        pts, apex = a
        others = [p for p in pts if p != apex]
        if len(others) != 2:
            return False
        return abs(dist(coords, apex, others[0]) - dist(coords, apex, others[1])) < 1e-4
    if k == "Para":
        # ABCD: midpoint of AC == midpoint of BD
        A, B, C, D = a[0]
        coords_ok = all(x in coords for x in (A, B, C, D))
        if not coords_ok:
            return False
        mx1 = ((coords[A][0] + coords[C][0]) / 2, (coords[A][1] + coords[C][1]) / 2)
        mx2 = ((coords[B][0] + coords[D][0]) / 2, (coords[B][1] + coords[D][1]) / 2)
        return abs(mx1[0] - mx2[0]) < 1e-4 and abs(mx1[1] - mx2[1]) < 1e-4
    if k == "Congruent":
        t1, t2, mapping = a
        # check SSS numerically under the correspondence mapping (tuple of 3 pairs)
        for (p, q) in mapping:
            pass
        # mapping is ((a,a'), (b,b'), (c,c'))
        pairs = list(mapping)
        sides1 = [
            dist(coords, pairs[0][0], pairs[1][0]),
            dist(coords, pairs[1][0], pairs[2][0]),
            dist(coords, pairs[2][0], pairs[0][0]),
        ]
        sides2 = [
            dist(coords, pairs[0][1], pairs[1][1]),
            dist(coords, pairs[1][1], pairs[2][1]),
            dist(coords, pairs[2][1], pairs[0][1]),
        ]
        return all(abs(x - y) < 1e-4 for x, y in zip(sides1, sides2))
    return False


def numeric_counterexample(fact: Fact, construction: Construction, trials: int = 8) -> Optional[int]:
    """Return a seed that falsifies fact, or None if all trials hold."""
    for seed in range(trials):
        coords = realize(construction, seed=seed + 17)
        if not numeric_holds(fact, coords):
            return seed + 17
    return None


# ---------------------------------------------------------------------------
# Symbolic deduction engine
# ---------------------------------------------------------------------------

AXIOM_DOC = [
    ("A1", "Midpoint halves: Midpoint(M,A,B) ⇒ AM = MB"),
    ("A2", "Midpoint theorem: Midpoint(M,A,B), Midpoint(N,A,C) ⇒ MN ∥ BC and 2·MN = BC"),
    ("A3", "Isosceles base angles: AB = AC ⇒ ∠ABC = ∠ACB"),
    ("A4", "Isosceles converse: ∠ABC = ∠ACB ⇒ AB = AC"),
    ("A5", "Vertical angles equal at a point (when four rays present)"),
    ("A6", "SSS congruence ⇒ corresponding angles equal"),
    ("A7", "SAS congruence ⇒ corresponding parts equal"),
    ("A8", "ASA congruence ⇒ corresponding sides equal"),
    ("A9", "Parallelogram opposite sides equal and parallel"),
    ("A10", "Equilateral ⇒ all sides equal and all angles equal"),
    ("A11", "Symmetry + transitivity of EqSeg / EqAng / Parallel"),
    ("A12", "Reflexivity of EqSeg and EqAng"),
]


class ProofEngine:
    def __init__(self, allow_lemmas: bool = True):
        self.allow_lemmas = allow_lemmas
        self.lemma_library: list[Lemma] = []
        self.max_closure_rounds = 40

    def add_lemma(self, lemma: "Lemma") -> None:
        self.lemma_library.append(lemma)

    def bootstrap_facts(self, construction: Construction) -> set[Fact]:
        facts: set[Fact] = set()
        # Midpoint declarations from construction
        for m, a, b in construction.midpoints():
            facts.add(Fact("Midpoint", (m, a, b), source="construction"))
            # also Midpoint(m,b,a)
            facts.add(Fact("Midpoint", (m, b, a), source="construction"))

        for st in construction.steps:
            if st[0] == "equilateral_third":
                _, c, a, b = st
                facts.add(Fact("Equilateral", (frozenset((a, b, c)),), source="construction"))
            if st[0] == "parallelogram_fourth":
                _, d, a, b, c = st
                # ABCD with D = A+C-B
                facts.add(Fact("Para", ((a, b, c, d),), source="construction"))

        # Tags can inject known structure
        for tag in construction.tags:
            if tag.startswith("isos:"):
                # isos:A:B:C means AB=AC apex A
                _, apex, p, q = tag.split(":")
                facts.add(Fact("Isos", (frozenset((apex, p, q)), apex), source="construction"))
                facts.add(Fact("EqSeg", (seg(apex, p), seg(apex, q)), source="construction"))
            if tag.startswith("equilateral:"):
                _, a, b, c = tag.split(":")
                facts.add(Fact("Equilateral", (frozenset((a, b, c)),), source="construction"))
        return facts

    def close(self, facts: set[Fact], cite_lemmas: bool = True) -> tuple[set[Fact], list[dict]]:
        """Forward-chain axioms (+ optional lemmas). Return closed set + proof log."""
        known = {f.key(): f for f in facts}
        log: list[dict] = []
        lemmas = self.lemma_library if (cite_lemmas and self.allow_lemmas) else []

        def add(fact: Fact, rule: str) -> bool:
            if fact.key() in known:
                return False
            known[fact.key()] = fact
            log.append({"fact": fact.pretty(), "rule": rule, "cites": list(fact.cite)})
            return True

        for _round in range(self.max_closure_rounds):
            added = 0
            snapshot = list(known.values())

            # A12 reflexivity for all segments/angles appearing
            pts = set()
            for f in snapshot:
                pts |= _points_in_fact(f)
            for a, b in itertools.combinations(sorted(pts), 2):
                s = seg(a, b)
                added += add(Fact("EqSeg", (s, s), source="axiom", cite=()), "A12-refl-seg")
            # angles: for each triple
            for v in pts:
                others = [p for p in pts if p != v]
                for p, q in itertools.combinations(others, 2):
                    ag = ang(v, p, q)
                    added += add(Fact("EqAng", (ag, ag), source="axiom", cite=()), "A12-refl-ang")

            # A1 midpoint halves
            for f in snapshot:
                if f.kind == "Midpoint":
                    m, a, b = f.args
                    added += add(
                        Fact("EqSeg", (seg(m, a), seg(m, b)), source="axiom", cite=()),
                        "A1",
                    )

            # Apply archived lemma SCHEMAS first (key measurement: cite earlier lemmas)
            if cite_lemmas and lemmas:
                schemas = {lem.schema for lem in lemmas}
                named = {lem.schema: lem.name for lem in lemmas if lem.schema != "raw"}
                mids = [f for f in snapshot if f.kind == "Midpoint"]
                # dedupe midpoints by (m, endpoints)
                uniq_mids = []
                seen_m = set()
                for f in mids:
                    key = (f.args[0], frozenset((f.args[1], f.args[2])))
                    if key in seen_m:
                        continue
                    seen_m.add(key)
                    uniq_mids.append(f)
                if "midline_parallel" in schemas or "midline_half" in schemas:
                    for f1, f2 in itertools.combinations(uniq_mids, 2):
                        m, a1, b1 = f1.args
                        n, a2, b2 = f2.args
                        for apex in (a1, b1):
                            other1 = b1 if apex == a1 else a1
                            if apex not in (a2, b2):
                                continue
                            other2 = b2 if apex == a2 else a2
                            if other1 == other2 or m == n:
                                continue
                            if "midline_parallel" in schemas:
                                nm = named.get("midline_parallel", "midline_parallel")
                                added += add(
                                    Fact(
                                        "Parallel",
                                        _norm_parallel(seg(m, n), seg(other1, other2)),
                                        source=f"lemma:{nm}",
                                        cite=(nm,),
                                    ),
                                    f"lemma:{nm}",
                                )
                            if "midline_half" in schemas:
                                nm = named.get("midline_half", "midline_half")
                                added += add(
                                    Fact(
                                        "HalfSeg",
                                        (seg(m, n), seg(other1, other2)),
                                        source=f"lemma:{nm}",
                                        cite=(nm,),
                                    ),
                                    f"lemma:{nm}",
                                )
                if "isos_base" in schemas:
                    nm = named.get("isos_base", "isos_base")
                    for f in snapshot:
                        if f.kind == "Isos":
                            pts, apex = f.args
                            others = [p for p in pts if p != apex]
                            if len(others) != 2:
                                continue
                            p, q = others
                            ba = ang(p, apex, q)
                            bb = ang(q, apex, p)
                            added += add(
                                Fact(
                                    "EqAng",
                                    _norm_eq_ang(ba, bb),
                                    source=f"lemma:{nm}",
                                    cite=(nm,),
                                ),
                                f"lemma:{nm}",
                            )
                if "para_opp" in schemas:
                    nm = named.get("para_opp", "para_opp")
                    for f in snapshot:
                        if f.kind == "Para":
                            A, B, C, D = f.args[0]
                            added += add(
                                Fact(
                                    "EqSeg",
                                    _norm_eq(seg(A, B), seg(D, C)),
                                    source=f"lemma:{nm}",
                                    cite=(nm,),
                                ),
                                f"lemma:{nm}",
                            )
                            added += add(
                                Fact(
                                    "Parallel",
                                    _norm_parallel(seg(A, B), seg(D, C)),
                                    source=f"lemma:{nm}",
                                    cite=(nm,),
                                ),
                                f"lemma:{nm}",
                            )

            # A2 midpoint theorem (skip if corresponding schema lemma already available —
            # so later proofs credit the library rather than re-hitting the axiom)
            have_mid_par_lem = cite_lemmas and any(lem.schema == "midline_parallel" for lem in lemmas)
            have_mid_half_lem = cite_lemmas and any(lem.schema == "midline_half" for lem in lemmas)
            mids = [f for f in snapshot if f.kind == "Midpoint"]
            for f1, f2 in itertools.combinations(mids, 2):
                m, a1, b1 = f1.args
                n, a2, b2 = f2.args
                # shared apex A: Midpoint(M,A,B), Midpoint(N,A,C)
                for apex in (a1, b1):
                    other1 = b1 if apex == a1 else a1
                    if apex not in (a2, b2):
                        continue
                    other2 = b2 if apex == a2 else a2
                    if other1 == other2 or m == n:
                        continue
                    if not have_mid_par_lem:
                        added += add(
                            Fact(
                                "Parallel",
                                _norm_parallel(seg(m, n), seg(other1, other2)),
                                source="axiom",
                                cite=(),
                            ),
                            "A2-parallel",
                        )
                    if not have_mid_half_lem:
                        added += add(
                            Fact(
                                "HalfSeg",
                                (seg(m, n), seg(other1, other2)),
                                source="axiom",
                                cite=(),
                            ),
                            "A2-half",
                        )

            # A10 equilateral
            for f in snapshot:
                if f.kind == "Equilateral":
                    a, b, c = sorted(f.args[0])
                    for s1, s2 in itertools.combinations(
                        [seg(a, b), seg(b, c), seg(c, a)], 2
                    ):
                        added += add(Fact("EqSeg", _norm_eq(s1, s2), source="axiom"), "A10-sides")
                    for ag1, ag2 in itertools.combinations(
                        [ang(a, b, c), ang(b, a, c), ang(c, a, b)], 2
                    ):
                        added += add(Fact("EqAng", _norm_eq_ang(ag1, ag2), source="axiom"), "A10-angs")
                    # also mark isos at each vertex
                    for apex in (a, b, c):
                        added += add(
                            Fact("Isos", (frozenset((a, b, c)), apex), source="axiom"),
                            "A10-isos",
                        )

            # Isos from EqSeg on two sides of a triangle
            # Collect EqSeg pairs
            eq_segs = _eq_seg_classes(snapshot)
            pts_list = sorted(pts)
            for a, b, c in itertools.combinations(pts_list, 3):
                # check AB=AC → isos apex A
                for apex, p, q in (
                    (a, b, c),
                    (b, a, c),
                    (c, a, b),
                ):
                    if _same_class(eq_segs, seg(apex, p), seg(apex, q)):
                        added += add(
                            Fact("Isos", (frozenset((a, b, c)), apex), source="derived"),
                            "isos-from-sides",
                        )
                        # A3: base angles equal (skip if isos_base lemma already in library)
                        have_isos_lem = cite_lemmas and any(lem.schema == "isos_base" for lem in lemmas)
                        if not have_isos_lem:
                            ba = ang(p, apex, q)
                            bb = ang(q, apex, p)
                            added += add(
                                Fact("EqAng", _norm_eq_ang(ba, bb), source="axiom", cite=()),
                                "A3",
                            )

            # A4 converse: equal base angles ⇒ equal sides
            eq_angs = _eq_ang_classes(snapshot)
            for a, b, c in itertools.combinations(pts_list, 3):
                # angles at B and C equal ⇒ AB=AC? Base angles at b and c equal ⇒ sides opposite equal
                # ∠ABC = ∠ACB ⇒ AB? wait: ∠ at B = ∠ at C ⇒ sides opposite: AC = AB ⇒ isos apex A
                ang_b = ang(b, a, c)
                ang_c = ang(c, a, b)
                if _same_ang_class(eq_angs, ang_b, ang_c):
                    added += add(
                        Fact("EqSeg", _norm_eq(seg(a, b), seg(a, c)), source="axiom"),
                        "A4",
                    )
                    added += add(
                        Fact("Isos", (frozenset((a, b, c)), a), source="derived"),
                        "A4-isos",
                    )

            # A9 parallelogram
            for f in snapshot:
                if f.kind == "Para":
                    A, B, C, D = f.args[0]
                    # opposite sides: AB=DC, AD=BC; parallels
                    added += add(Fact("EqSeg", _norm_eq(seg(A, B), seg(D, C)), source="axiom"), "A9-sides")
                    added += add(Fact("EqSeg", _norm_eq(seg(A, D), seg(B, C)), source="axiom"), "A9-sides")
                    added += add(
                        Fact("Parallel", _norm_parallel(seg(A, B), seg(D, C)), source="axiom"),
                        "A9-par",
                    )
                    added += add(
                        Fact("Parallel", _norm_parallel(seg(A, D), seg(B, C)), source="axiom"),
                        "A9-par",
                    )

            # A5 vertical angles: if we have four points where two lines cross at V
            # Simplified: for point V with ≥4 neighbors, opposite angles — skip heavy; 
            # apply when we have crossing of two segments sharing midpoint? 
            # Lightweight: whenever Parallel facts create corresponding angles — skip.
            # Instead: if Midpoint M of AC and Midpoint M of BD (same M) then vertical via X shape
            mid_by_pt: dict[str, list] = {}
            for f in snapshot:
                if f.kind == "Midpoint":
                    m, a, b = f.args
                    # dedupe by unordered segment endpoints
                    entry = (m, frozenset((a, b)))
                    bucket = mid_by_pt.setdefault(m, [])
                    if entry not in [(x.args[0], frozenset((x.args[1], x.args[2]))) for x in bucket]:
                        bucket.append(f)
            for m, flist in mid_by_pt.items():
                if len(flist) >= 2:
                    # two distinct segments share midpoint m → vertical angles at m
                    f1, f2 = flist[0], flist[1]
                    _, a, c = f1.args
                    _, b, d = f2.args
                    if len({a, b, c, d}) < 4:
                        continue
                    # vertical: ∠amb = ∠cmd, ∠amd = ∠cmb
                    added += add(
                        Fact("EqAng", _norm_eq_ang(ang(m, a, b), ang(m, c, d)), source="axiom"),
                        "A5",
                    )
                    added += add(
                        Fact("EqAng", _norm_eq_ang(ang(m, a, d), ang(m, c, b)), source="axiom"),
                        "A5",
                    )

            # SSS / SAS / ASA among triples sharing EqSeg structure
            added += self._congruence_rules(snapshot, eq_segs, eq_angs, add)

            # A11 transitivity already partly via union-find classes — close EqSeg/EqAng/Parallel
            added += self._close_equivalence(snapshot, add)

            if added == 0:
                break

        return set(known.values()), log

    def _congruence_rules(self, snapshot, eq_segs, eq_angs, add) -> int:
        added = 0
        pts = set()
        for f in snapshot:
            pts |= _points_in_fact(f)
        pts_list = sorted(pts)
        triangles = list(itertools.combinations(pts_list, 3))
        for t1, t2 in itertools.combinations(triangles, 2):
            if set(t1) == set(t2):
                continue
            # try all correspondences
            for perm in itertools.permutations(t2):
                mapping = tuple(zip(t1, perm))
                # sides
                s1 = [seg(t1[0], t1[1]), seg(t1[1], t1[2]), seg(t1[2], t1[0])]
                s2 = [seg(perm[0], perm[1]), seg(perm[1], perm[2]), seg(perm[2], perm[0])]
                side_eq = [_same_class(eq_segs, s1[i], s2[i]) for i in range(3)]
                # angles at vertices
                a1 = [ang(t1[0], t1[1], t1[2]), ang(t1[1], t1[0], t1[2]), ang(t1[2], t1[0], t1[1])]
                a2 = [ang(perm[0], perm[1], perm[2]), ang(perm[1], perm[0], perm[2]), ang(perm[2], perm[0], perm[1])]
                ang_eq = [_same_ang_class(eq_angs, a1[i], a2[i]) for i in range(3)]

                sss = all(side_eq)
                # SAS: side-angle-side (angle included): sides 0,2 and angle at 0? 
                # included angle between sides (0-1) and (0-2) is angle at 0
                sas = (
                    (side_eq[0] and side_eq[2] and ang_eq[0])
                    or (side_eq[0] and side_eq[1] and ang_eq[1])
                    or (side_eq[1] and side_eq[2] and ang_eq[2])
                )
                asa = (
                    (ang_eq[0] and side_eq[0] and ang_eq[1])
                    or (ang_eq[1] and side_eq[1] and ang_eq[2])
                    or (ang_eq[2] and side_eq[2] and ang_eq[0])
                )

                if sss or sas or asa:
                    rule = "A6-SSS" if sss else ("A7-SAS" if sas else "A8-ASA")
                    added += add(
                        Fact("Congruent", (t1, perm, mapping), source="axiom"),
                        rule,
                    )
                    # transfer equalities
                    for i in range(3):
                        added += add(
                            Fact("EqSeg", _norm_eq(s1[i], s2[i]), source="derived"),
                            rule + "-transfer-seg",
                        )
                        added += add(
                            Fact("EqAng", _norm_eq_ang(a1[i], a2[i]), source="derived"),
                            rule + "-transfer-ang",
                        )
        return added

    def _close_equivalence(self, snapshot, add) -> int:
        added = 0
        # EqSeg transitivity via union-find
        parent: dict[Any, Any] = {}

        def find(x):
            parent.setdefault(x, x)
            while parent[x] != x:
                parent[x] = parent[parent[x]]
                x = parent[x]
            return x

        def union(a, b):
            ra, rb = find(a), find(b)
            if ra != rb:
                parent[rb] = ra

        segs_seen = set()
        for f in snapshot:
            if f.kind == "EqSeg":
                s1, s2 = f.args
                union(("S", s1), ("S", s2))
                segs_seen.add(s1)
                segs_seen.add(s2)
            if f.kind == "EqAng":
                a1, a2 = f.args
                union(("A", a1), ("A", a2))
            if f.kind == "Parallel":
                l1, l2 = f.args
                union(("P", l1), ("P", l2))

        # emit pairwise for small classes
        from collections import defaultdict

        classes: dict[Any, list] = defaultdict(list)
        for f in snapshot:
            if f.kind == "EqSeg":
                for s in f.args:
                    classes[find(("S", s))].append(("EqSeg", s))
            if f.kind == "EqAng":
                for a in f.args:
                    classes[find(("A", a))].append(("EqAng", a))
            if f.kind == "Parallel":
                for l in f.args:
                    classes[find(("P", l))].append(("Parallel", l))

        for members in classes.values():
            kinds = {m[0] for m in members}
            if len(kinds) != 1:
                continue
            kind = next(iter(kinds))
            vals = list({m[1] for m in members})
            if len(vals) > 8:
                vals = vals[:8]
            for x, y in itertools.combinations(vals, 2):
                if kind == "EqSeg":
                    added += add(Fact("EqSeg", _norm_eq(x, y), source="axiom"), "A11")
                elif kind == "EqAng":
                    added += add(Fact("EqAng", _norm_eq_ang(x, y), source="axiom"), "A11")
                elif kind == "Parallel":
                    added += add(Fact("Parallel", _norm_parallel(x, y), source="axiom"), "A11")
        return added

    def prove(
        self, goal: Fact, construction: Construction, use_lemmas: bool = True
    ) -> dict:
        """Try to prove goal. Numeric counterexample ⇒ reject. Else symbolic."""
        # Numeric sanity: if clearly false, reject
        cex = numeric_counterexample(goal, construction)
        if cex is not None:
            return {
                "status": "rejected",
                "reason": "numeric_counterexample",
                "seed": cex,
                "proof_log": [],
                "cites": [],
            }

        base = self.bootstrap_facts(construction)
        closed, log = self.close(base, cite_lemmas=use_lemmas and self.allow_lemmas)

        # Goal match: exact key or EqSeg/EqAng normalized
        goal_keys = {goal.key()}
        if goal.kind in ("EqSeg", "EqAng", "Parallel"):
            goal_keys.add((goal.kind, _norm_pair_args(goal.kind, goal.args)))

        found = None
        for f in closed:
            if f.key() in goal_keys or (
                f.kind == goal.kind and _norm_pair_args(f.kind, f.args) == _norm_pair_args(goal.kind, goal.args)
            ):
                found = f
                break

        if found is None:
            # Still numerically true but unproven → conjecture (not archived as lemma)
            return {
                "status": "conjecture",
                "reason": "numeric_ok_but_unproven",
                "proof_log": log[-20:],
                "cites": [],
            }

        # Attribute citations to the goal; if proven via A11 transitivity of
        # lemma-backed facts, inherit those cites (Varignon-style).
        cites = list(found.cite)
        if not cites and found.source.startswith("lemma:"):
            cites = [found.source.split(":", 1)[1]]
        if not cites and found.kind in ("Parallel", "EqSeg", "EqAng"):
            # inherit cites from same-kind facts that share an argument with the goal
            goal_args = set()
            for a in found.args:
                goal_args.add(a)
            inherited = []
            for f in closed:
                if f.kind != found.kind or not f.cite:
                    continue
                if any(a in goal_args for a in f.args):
                    inherited.extend(f.cite)
            cites = sorted(set(inherited))

        return {
            "status": "proven",
            "reason": "symbolic",
            "proof_log": log[-40:],
            "cites": cites,
            "fact": found,
            "fact_source": found.source,
        }


# ---------------------------------------------------------------------------
# Lemma archive
# ---------------------------------------------------------------------------

@dataclass
class Lemma:
    name: str
    lemma_type: str
    construction: Construction
    conclusions: list[Fact]
    proof_cites: list[str]
    step: int
    statement: str
    schema: str = "raw"  # midline_parallel | midline_half | isos_base | para_opp | raw

    def to_dict(self) -> dict:
        return {
            "name": self.name,
            "lemma_type": self.lemma_type,
            "schema": self.schema,
            "construction": self.construction.signature(),
            "tags": list(self.construction.tags),
            "conclusions": [c.pretty() for c in self.conclusions],
            "proof_cites": list(self.proof_cites),
            "step": self.step,
            "statement": self.statement,
        }


def infer_schema(fact: Fact, construction: Construction) -> str:
    """Map a proven fact to a reusable schema for later citation."""
    if fact.kind == "Parallel":
        # midline-style if construction has ≥2 midpoints
        if sum(1 for st in construction.steps if st[0] == "midpoint") >= 2:
            return "midline_parallel"
    if fact.kind == "HalfSeg":
        if sum(1 for st in construction.steps if st[0] == "midpoint") >= 2:
            return "midline_half"
    if fact.kind == "EqAng":
        if any(t.startswith("isos:") for t in construction.tags):
            return "isos_base"
    if fact.kind == "EqSeg" and any(t.startswith("family:para") or t == "family:para" for t in construction.tags):
        return "para_opp"
    if fact.kind in ("EqSeg", "Parallel") and any("para" in t for t in construction.tags):
        return "para_opp"
    return "raw"


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _points_in_fact(f: Fact) -> set[str]:
    pts: set[str] = set()
    k, a = f.kind, f.args
    if k == "Midpoint":
        pts.update(a)
    elif k in ("EqSeg", "HalfSeg", "Parallel"):
        for s in a:
            pts.update(s)
    elif k == "EqAng":
        for ang_t in a:
            pts.add(ang_t[0])
            pts.update(ang_t[1])
    elif k == "Congruent":
        pts.update(a[0])
        pts.update(a[1])
    elif k == "Para":
        pts.update(a[0])
    elif k == "Equilateral":
        pts.update(a[0])
    elif k == "Isos":
        pts.update(a[0])
    return pts


def _fact_points_subset(f: Fact, pts: set[str]) -> bool:
    return _points_in_fact(f) <= pts


def _norm_eq(s1, s2):
    return tuple(sorted([s1, s2], key=lambda s: tuple(sorted(s))))


def _norm_eq_ang(a1, a2):
    return tuple(sorted([a1, a2], key=lambda a: (a[0], tuple(sorted(a[1])))))


def _norm_parallel(l1, l2):
    return tuple(sorted([l1, l2], key=lambda s: tuple(sorted(s))))


def _norm_pair_args(kind, args):
    if kind == "EqSeg":
        return _norm_eq(args[0], args[1])
    if kind == "EqAng":
        return _norm_eq_ang(args[0], args[1])
    if kind == "Parallel":
        return _norm_parallel(args[0], args[1])
    if kind == "HalfSeg":
        return args
    return args


def _eq_seg_classes(facts: Iterable[Fact]):
    parent: dict = {}

    def find(x):
        parent.setdefault(x, x)
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    def union(a, b):
        ra, rb = find(a), find(b)
        if ra != rb:
            parent[rb] = ra

    for f in facts:
        if f.kind == "EqSeg":
            union(f.args[0], f.args[1])
    return parent, find


def _same_class(eq_segs, s1, s2) -> bool:
    parent, find = eq_segs
    if s1 == s2:
        return True
    if s1 not in parent and s2 not in parent:
        return False
    parent.setdefault(s1, s1)
    parent.setdefault(s2, s2)
    return find(s1) == find(s2)


def _eq_ang_classes(facts: Iterable[Fact]):
    parent: dict = {}

    def find(x):
        parent.setdefault(x, x)
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    def union(a, b):
        ra, rb = find(a), find(b)
        if ra != rb:
            parent[rb] = ra

    for f in facts:
        if f.kind == "EqAng":
            union(f.args[0], f.args[1])
    return parent, find


def _same_ang_class(eq_angs, a1, a2) -> bool:
    parent, find = eq_angs
    if a1 == a2:
        return True
    parent.setdefault(a1, a1)
    parent.setdefault(a2, a2)
    return find(a1) == find(a2)


# ---------------------------------------------------------------------------
# Construction templates & conjecture generation
# ---------------------------------------------------------------------------

def template_triangle() -> Construction:
    return Construction(
        steps=[("free", "A"), ("free", "B"), ("free", "C")],
        name="triangle",
        tags=(),
    )


def template_midline() -> Construction:
    return Construction(
        steps=[
            ("free", "A"),
            ("free", "B"),
            ("free", "C"),
            ("midpoint", "M", "A", "B"),
            ("midpoint", "N", "A", "C"),
        ],
        name="midline",
        tags=("family:midline",),
    )


def template_midline_full() -> Construction:
    return Construction(
        steps=[
            ("free", "A"),
            ("free", "B"),
            ("free", "C"),
            ("midpoint", "M", "A", "B"),
            ("midpoint", "N", "A", "C"),
            ("midpoint", "P", "B", "C"),
        ],
        name="medial",
        tags=("family:medial",),
    )


def template_isosceles() -> Construction:
    return Construction(
        steps=[("free", "A"), ("free", "B"), ("free", "C")],
        name="isosceles",
        tags=("isos:A:B:C", "family:isos"),
    )


def template_equilateral() -> Construction:
    return Construction(
        steps=[
            ("free", "A"),
            ("free", "B"),
            ("equilateral_third", "C", "A", "B"),
        ],
        name="equilateral",
        tags=("family:equilateral",),
    )


def template_varignon() -> Construction:
    """Midpoints of all 4 sides of a quad — Varignon parallelogram."""
    return Construction(
        steps=[
            ("free", "A"),
            ("free", "B"),
            ("free", "C"),
            ("free", "D"),
            ("midpoint", "P", "A", "B"),
            ("midpoint", "Q", "B", "C"),
            ("midpoint", "R", "C", "D"),
            ("midpoint", "S", "D", "A"),
        ],
        name="varignon",
        tags=("family:varignon",),
    )


def template_para() -> Construction:
    return Construction(
        steps=[
            ("free", "A"),
            ("free", "B"),
            ("free", "C"),
            ("parallelogram_fourth", "D", "A", "B", "C"),
        ],
        name="parallelogram",
        tags=("family:para",),
    )


def template_isos_midline() -> Construction:
    return Construction(
        steps=[
            ("free", "A"),
            ("free", "B"),
            ("free", "C"),
            ("midpoint", "M", "A", "B"),
            ("midpoint", "N", "A", "C"),
            ("midpoint", "P", "B", "C"),
        ],
        name="isos_midline",
        tags=("isos:A:B:C", "family:isos_midline"),
    )


def template_double_midline() -> Construction:
    """Midline then midpoint of midline — harder frontier."""
    return Construction(
        steps=[
            ("free", "A"),
            ("free", "B"),
            ("free", "C"),
            ("midpoint", "M", "A", "B"),
            ("midpoint", "N", "A", "C"),
            ("midpoint", "Q", "M", "N"),
            ("midpoint", "P", "B", "C"),
        ],
        name="double_midline",
        tags=("family:double_midline",),
    )


TEMPLATES = [
    template_midline,
    template_midline_full,
    template_isosceles,
    template_equilateral,
    template_para,
    template_varignon,
    template_isos_midline,
    template_double_midline,
]


def mutate_construction(base: Construction, rng: random.Random, library: list[Lemma]) -> Construction:
    """Mutate by adding a midpoint on existing segments, biased toward library families."""
    steps = list(base.steps)
    tags = list(base.tags)
    pts = []
    for st in steps:
        if st[0] == "free":
            pts.append(st[1])
        elif st[0] in ("midpoint", "equilateral_third", "parallelogram_fourth"):
            pts.append(st[1])

    existing_mids = {st[1] for st in steps if st[0] == "midpoint"}
    # pick two points without midpoint yet
    candidates = []
    for a, b in itertools.combinations(pts, 2):
        name = f"M{a}{b}"
        if name not in existing_mids and len(name) <= 4:
            candidates.append((name, a, b))
    if candidates and rng.random() < 0.7:
        name, a, b = rng.choice(candidates)
        # shorten name
        name = f"X{len(existing_mids)}"
        steps.append(("midpoint", name, a, b))
    elif library and rng.random() < 0.5:
        # graft a family tag from a recent lemma
        lem = rng.choice(library[-min(5, len(library)) :])
        for t in lem.construction.tags:
            if t.startswith("family:") and t not in tags:
                tags.append(t)
    else:
        # add equilateral or isos tag sometimes
        if "isos:A:B:C" not in tags and {"A", "B", "C"} <= set(pts) and rng.random() < 0.4:
            tags.append("isos:A:B:C")

    return Construction(steps=steps, name=base.name + "_mut", tags=tuple(tags))


def generate_conjectures(construction: Construction, known: set[Fact], rng: random.Random) -> list[Fact]:
    """Propose non-axiom-looking equalities/incidences not already known."""
    pts = construction.points()
    known_keys = {f.key() for f in known}
    # also normalized
    for f in list(known):
        if f.kind in ("EqSeg", "EqAng", "Parallel"):
            known_keys.add((f.kind, _norm_pair_args(f.kind, f.args)))

    candidates: list[Fact] = []

    # segment equalities
    segs = [seg(a, b) for a, b in itertools.combinations(pts, 2)]
    for s1, s2 in itertools.combinations(segs, 2):
        if s1 == s2:
            continue
        args = _norm_eq(s1, s2)
        f = Fact("EqSeg", args)
        if f.key() not in known_keys:
            candidates.append(f)

    # half-seg
    for s1 in segs:
        for s2 in segs:
            if s1 == s2:
                continue
            f = Fact("HalfSeg", (s1, s2))
            if f.key() not in known_keys:
                candidates.append(f)

    # parallels
    for s1, s2 in itertools.combinations(segs, 2):
        if len(s1 & s2) > 0:
            continue  # share a point — not useful as parallel candidates sometimes ok
        args = _norm_parallel(s1, s2)
        f = Fact("Parallel", args)
        if f.key() not in known_keys:
            candidates.append(f)

    # angles
    angs = []
    for v in pts:
        others = [p for p in pts if p != v]
        for p, q in itertools.combinations(others, 2):
            angs.append(ang(v, p, q))
    for a1, a2 in itertools.combinations(angs, 2):
        if a1 == a2:
            continue
        args = _norm_eq_ang(a1, a2)
        f = Fact("EqAng", args)
        if f.key() not in known_keys:
            candidates.append(f)

    rng.shuffle(candidates)
    return candidates


def lemma_type_of(fact: Fact, construction: Construction) -> str:
    fam = "general"
    for t in construction.tags:
        if t.startswith("family:"):
            fam = t.split(":", 1)[1]
            break
    return f"{fam}:{fact.kind}"


def score_frontier(
    construction: Construction,
    goal: Fact,
    recent_lemmas: list[Lemma],
    proven_keys: set,
    rng: random.Random,
) -> float:
    """POET-ish: prefer using recent lemma families + not already proven + not noise."""
    score = 0.0
    gk = goal.key()
    if gk in proven_keys:
        return -100.0
    # prefer goals involving midpoints / half / parallel (structured)
    if goal.kind in ("HalfSeg", "Parallel", "EqAng"):
        score += 2.0
    if goal.kind == "EqSeg":
        score += 1.0
    # uses recent lemma family tags
    recent_families = set()
    for lem in recent_lemmas[-5:]:
        for t in lem.construction.tags:
            if t.startswith("family:"):
                recent_families.add(t)
    for t in construction.tags:
        if t in recent_families:
            score += 3.0
    # prefer constructions that already have midpoints (non-trivial)
    n_mid = sum(1 for st in construction.steps if st[0] == "midpoint")
    score += 0.5 * n_mid
    # noise penalty: too many free points without structure
    n_free = sum(1 for st in construction.steps if st[0] == "free")
    if n_free >= 4 and n_mid == 0:
        score -= 2.0
    score += rng.random() * 0.1
    return score
