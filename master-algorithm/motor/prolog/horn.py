"""
Tiny self-contained Horn-clause interpreter (fallback if swipl missing).

Subset:
  - ground & non-ground terms as nested tuples: ('f', a, b) or atoms/ints/strings
  - variables: strings starting with uppercase or '_'
  - clauses: (head, [body1, body2, ...])  — empty body = fact
  - unify with occurs-check
  - backward chaining with depth limit (finite fail)

Documented limitation: no cuts, no arithmetic builtins beyond =:= via
pre-evaluated ground ints, no ISO IO. Enough to mirror critic checks that
the Python worlds already ground before querying.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Iterator, Optional, Union

Term = Any  # atom | int | str(Var) | tuple(functor, *args)


def is_var(t: Term) -> bool:
    return isinstance(t, str) and len(t) > 0 and (t[0].isupper() or t[0] == "_")


def occurs(var: str, term: Term, subst: dict) -> bool:
    term = deref(term, subst)
    if term == var:
        return True
    if isinstance(term, tuple):
        return any(occurs(var, a, subst) for a in term[1:])
    return False


def deref(term: Term, subst: dict) -> Term:
    while is_var(term) and term in subst:
        term = subst[term]
    return term


def unify(a: Term, b: Term, subst: dict) -> Optional[dict]:
    a = deref(a, subst)
    b = deref(b, subst)
    if a == b:
        return subst
    if is_var(a):
        if occurs(a, b, subst):
            return None
        subst = dict(subst)
        subst[a] = b
        return subst
    if is_var(b):
        if occurs(b, a, subst):
            return None
        subst = dict(subst)
        subst[b] = a
        return subst
    if isinstance(a, tuple) and isinstance(b, tuple):
        if a[0] != b[0] or len(a) != len(b):
            return None
        for x, y in zip(a[1:], b[1:]):
            subst = unify(x, y, subst)
            if subst is None:
                return None
        return subst
    return None


@dataclass
class HornKB:
    clauses: list[tuple[Term, list[Term]]] = field(default_factory=list)

    def assertz(self, head: Term, body: Optional[list[Term]] = None) -> None:
        self.clauses.append((head, body or []))

    def retract_all(self, functor: str) -> None:
        self.clauses = [c for c in self.clauses if not (isinstance(c[0], tuple) and c[0][0] == functor)]

    def solve(self, goals: list[Term], depth_limit: int = 64) -> Iterator[dict]:
        yield from self._bc(goals, {}, 0, depth_limit)

    def _bc(self, goals: list[Term], subst: dict, depth: int, limit: int) -> Iterator[dict]:
        if depth > limit:
            return
        if not goals:
            yield subst
            return
        g0, *rest = goals
        g0 = deref(g0, subst)
        for head, body in self.clauses:
            # rename vars per clause attempt
            head2, body2 = _rename(head, body, depth)
            s2 = unify(g0, head2, subst)
            if s2 is None:
                continue
            yield from self._bc(body2 + rest, s2, depth + 1, limit)

    def proves(self, goal: Term, depth_limit: int = 64) -> bool:
        for _ in self.solve([goal], depth_limit=depth_limit):
            return True
        return False


_counter = 0


def _rename(head: Term, body: list[Term], depth: int) -> tuple[Term, list[Term]]:
    global _counter
    mapping: dict[str, str] = {}

    def ren(t: Term) -> Term:
        nonlocal mapping
        t = t  # noqa
        if is_var(t):
            if t not in mapping:
                _counter += 1
                mapping[t] = f"{t}_d{depth}_{_counter}"
            return mapping[t]
        if isinstance(t, tuple):
            return (t[0],) + tuple(ren(a) for a in t[1:])
        return t

    return ren(head), [ren(b) for b in body]


def term_from_prologish(s: str) -> Term:
    """Minimal parser for atoms/ints/rec(fib,[1,1]) style — not full Prolog."""
    s = s.strip()
    if s.isdigit() or (s.startswith("-") and s[1:].isdigit()):
        return int(s)
    if s.startswith("'") and s.endswith("'"):
        return s[1:-1]
    return s
