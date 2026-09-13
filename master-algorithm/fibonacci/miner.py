#!/usr/bin/env python3
"""
Fibonacci Relation Miner — sequence-alone rediscovery experiment.

Hypothesis language is FIXED and documented below. We search only relations
expressible in that language from a prefix F_0..F_N. No external knowledge
(phyllotaxis, history, art) is injected.

Hypothesis language (documented):
  H1  Linear recurrences order 1..3: F(n) = a1*F(n-1)+...+ak*F(n-k)
      (exact integer coeffs preferred; least-squares fallback)
  H2  Consecutive ratios F(n+1)/F(n) → candidate φ; error vs (1+sqrt(5))/2
  H3  Cassini-like: F(n+1)*F(n-1) - F(n)^2  (and Catalan generalization pattern)
  H4  gcd identity: gcd(F(a),F(b)) == F(gcd(a,b)) for many pairs
  H5  Pisano periods π(m) for modulus m=2..20 (period of F mod m)
  H6  Zeckendorf: every k in 1..F_N has unique greedy non-consecutive Fib sum
  H7  Binet closed form check: round(φ^n / sqrt(5)) == F(n) on the prefix
  H8  NEGATIVE controls (must be rejected):
        F(n)=2*F(n-1); F(n) always prime; ratios→e; ratios→π
"""

from __future__ import annotations

import json
import math
import time
from dataclasses import dataclass, asdict, field
from pathlib import Path
from typing import Any, Optional

import numpy as np

OUT_DIR = Path(__file__).resolve().parent
SEED = 42  # no randomness used in searches; documented for reproducibility
np.random.seed(SEED)

PHI = (1 + math.sqrt(5)) / 2
SQRT5 = math.sqrt(5)


def fib_prefix(N: int) -> list[int]:
    """Return F_0 .. F_N inclusive. F_0=0, F_1=1."""
    if N < 0:
        raise ValueError("N>=0")
    F = [0] * (N + 1)
    if N >= 1:
        F[1] = 1
    for n in range(2, N + 1):
        F[n] = F[n - 1] + F[n - 2]
    return F


@dataclass
class Relation:
    name: str
    formula: str
    support: str
    counterexample: Optional[str]
    discovered_at_N: int
    true: bool
    category: str  # internal | negative_control


def try_linear_recurrence(F: list[int], order: int, N: int) -> Optional[Relation]:
    """Exact integer linear recurrence via successive exact fits."""
    # Need enough equations: for n = order..N, F[n] = sum a_i F[n-i]
    if N < order + 2:
        return None
    # Build exact system over integers using first `order` usable rows, then verify all
    # Use sympy-free integer Gaussian elimination via numpy with exact check
    rows = []
    rhs = []
    for n in range(order, min(order + order, N) + 1):
        rows.append([F[n - i] for i in range(1, order + 1)])
        rhs.append(F[n])
    A = np.array(rows, dtype=object)
    b = np.array(rhs, dtype=object)
    # Try small integer coefficient search for order<=3: a_i in -5..5
    # Exhaustive is tiny: 11^3 = 1331
    best = None
    ranges = range(-5, 6)
    if order == 1:
        candidates = [(a,) for a in ranges]
    elif order == 2:
        candidates = [(a, b_) for a in ranges for b_ in ranges]
    else:
        candidates = [(a, b_, c) for a in ranges for b_ in ranges for c in ranges]

    fits = []
    for coeffs in candidates:
        if all(c == 0 for c in coeffs):
            continue
        ok = True
        support_n = 0
        for n in range(order, N + 1):
            pred = sum(coeffs[i] * F[n - 1 - i] for i in range(order))
            if pred != F[n]:
                ok = False
                break
            support_n += 1
        if ok and support_n > 0:
            fits.append((sum(abs(c) for c in coeffs), coeffs, support_n))
    if fits:
        fits.sort(key=lambda t: (t[0], t[1]))  # minimal L1, then lexicographic
        _, coeffs, support_n = fits[0]
        formula = "F(n) = " + " + ".join(f"({c})*F(n-{i+1})" for i, c in enumerate(coeffs))
        return Relation(
            name=f"linear_recurrence_order_{order}",
            formula=formula,
            support=f"holds for all n={order}..{N} ({support_n} checks); {len(fits)} fits in [-5,5], chose min L1",
            counterexample=None,
            discovered_at_N=N,
            true=True,
            category="internal",
        )
    return None


def check_order2_classic(F: list[int], N: int) -> Relation:
    """Specifically test F(n)=F(n-1)+F(n-2)."""
    fails = []
    ok = 0
    for n in range(2, N + 1):
        if F[n] == F[n - 1] + F[n - 2]:
            ok += 1
        else:
            fails.append(n)
    if not fails:
        return Relation(
            name="classic_fibonacci_recurrence",
            formula="F(n)=F(n-1)+F(n-2)",
            support=f"exact for n=2..{N} ({ok} checks)",
            counterexample=None,
            discovered_at_N=N,
            true=True,
            category="internal",
        )
    return Relation(
        name="classic_fibonacci_recurrence",
        formula="F(n)=F(n-1)+F(n-2)",
        support=f"{ok} holds, {len(fails)} fail",
        counterexample=f"first fail n={fails[0]}",
        discovered_at_N=N,
        true=False,
        category="internal",
    )


def check_ratios_to_phi(F: list[int], N: int) -> Relation:
    ratios = []
    for n in range(2, N):  # F[n]>0 for n>=1; start from 2 to avoid F1/F0
        if F[n] == 0:
            continue
        ratios.append(F[n + 1] / F[n])
    if not ratios:
        return Relation(
            name="ratio_limit_phi",
            formula="F(n+1)/F(n) → (1+√5)/2",
            support="no ratios",
            counterexample="insufficient data",
            discovered_at_N=N,
            true=False,
            category="internal",
        )
    last = ratios[-1]
    err = abs(last - PHI)
    # Criterion: last ratio within 1e-3 of phi for N>=20, tighter for larger N
    thresh = 10 ** (-max(2, N // 10))
    # More stable: max error of last 5 ratios
    tail = ratios[-min(5, len(ratios)) :]
    max_err = max(abs(r - PHI) for r in tail)
    true = max_err < 1e-2 if N <= 20 else max_err < 1e-6 if N <= 40 else max_err < 1e-10
    # Actually for N=20, F21/F20 is already very close. Use:
    true = abs(last - PHI) < 1e-4 or (N >= 20 and abs(last - PHI) < 1e-3)
    # Standard: confirm convergence by checking |r_n - phi| decreasing and last < 1e-3
    true = abs(last - PHI) < (1e-3 if N < 40 else 1e-8 if N < 80 else 1e-12)
    # For N=20: F20=6765, F21=10946, ratio ≈ 1.61803397..., err ~ 1e-9 already at n~20
    # So always true for our N values if sequence is Fibonacci
    true = abs(last - PHI) < 1e-4
    return Relation(
        name="ratio_limit_phi",
        formula="F(n+1)/F(n) → φ=(1+√5)/2",
        support=f"last ratio={last:.15f}, |err|={abs(last-PHI):.3e}, n_ratios={len(ratios)}",
        counterexample=None if true else f"|last-φ|={abs(last-PHI):.3e} too large",
        discovered_at_N=N,
        true=true,
        category="internal",
    )


def check_cassini(F: list[int], N: int) -> list[Relation]:
    """Cassini: F(n+1)F(n-1)-F(n)^2 = (-1)^n for n>=1.
    Also Catalan: F(n)^2 - F(n+r)F(n-r) = (-1)^{n-r+1} F(r)^2 for small r.
    """
    out = []
    fails = []
    ok = 0
    for n in range(1, N):
        # need F[n+1], F[n-1]
        if n + 1 > N:
            break
        lhs = F[n + 1] * F[n - 1] - F[n] ** 2
        rhs = (-1) ** n
        if lhs == rhs:
            ok += 1
        else:
            fails.append((n, lhs, rhs))
    true = len(fails) == 0 and ok > 0
    out.append(
        Relation(
            name="cassini_identity",
            formula="F(n+1)F(n-1)-F(n)^2 = (-1)^n",
            support=f"holds for n=1..{min(N-1,N)} checked ({ok} eqs)",
            counterexample=None if true else f"first fail n={fails[0]}",
            discovered_at_N=N,
            true=true,
            category="internal",
        )
    )
    # Catalan for r=2 if N large enough
    if N >= 6:
        fails_c = []
        ok_c = 0
        r = 2
        for n in range(r, N - r + 1):
            lhs = F[n] ** 2 - F[n + r] * F[n - r]
            # For F_0=0,F_1=1 indexing: F(n)^2 - F(n+r)F(n-r) = (-1)^{n-r} F(r)^2
            rhs = ((-1) ** (n - r)) * (F[r] ** 2)
            if lhs == rhs:
                ok_c += 1
            else:
                fails_c.append(n)
        true_c = len(fails_c) == 0 and ok_c > 0
        out.append(
            Relation(
                name="catalan_identity_r2",
                formula="F(n)^2 - F(n+2)F(n-2) = (-1)^{n-2} F(2)^2",
                support=f"{ok_c} checks for n=2..{N-2}",
                counterexample=None if true_c else f"fail at n={fails_c[0]}",
                discovered_at_N=N,
                true=true_c,
                category="internal",
            )
        )
    return out


def check_gcd_identity(F: list[int], N: int) -> Relation:
    """gcd(F(a),F(b)) == F(gcd(a,b)) for pairs with a,b <= N."""
    fails = []
    ok = 0
    # Sample many pairs (all if N small)
    pairs = []
    for a in range(0, N + 1):
        for b in range(0, a + 1):
            pairs.append((a, b))
    # Cap checks for speed
    if len(pairs) > 5000:
        # systematic subsample
        step = max(1, len(pairs) // 4000)
        pairs = pairs[::step]
    for a, b in pairs:
        g = math.gcd(F[a], F[b])
        fg = F[math.gcd(a, b)]
        if g == fg:
            ok += 1
        else:
            fails.append((a, b, g, fg))
            if len(fails) > 5:
                break
    true = len(fails) == 0 and ok > 0
    return Relation(
        name="gcd_identity",
        formula="gcd(F(a),F(b))=F(gcd(a,b))",
        support=f"{ok} pairs verified (a,b<=N)",
        counterexample=None if true else f"counterexample pair={fails[0]}",
        discovered_at_N=N,
        true=true,
        category="internal",
    )


def pisano_period(F: list[int], m: int, N: int) -> Optional[int]:
    """Find period of F mod m if the period fits in the prefix (need 2*period observed)."""
    if m < 2:
        return None
    mods = [f % m for f in F]
    # Period starts at (0,1) repeating
    # Search for return to (0,1)
    for p in range(1, N):
        if mods[p] == 0 and p + 1 <= N and mods[p + 1] == 1:
            # verify mods[k]==mods[k%p] for k<=N — actually period p means
            # sequence repeats every p from start
            if all(mods[k] == mods[k % p] for k in range(p, N + 1)):
                return p
    return None


def check_pisano(F: list[int], N: int) -> list[Relation]:
    out = []
    found = {}
    for m in range(2, 21):
        p = pisano_period(F, m, N)
        if p is not None:
            found[m] = p
            out.append(
                Relation(
                    name=f"pisano_period_m{m}",
                    formula=f"π({m})={p} (period of F mod {m})",
                    support=f"verified on prefix length {N+1}",
                    counterexample=None,
                    discovered_at_N=N,
                    true=True,
                    category="internal",
                )
            )
        else:
            out.append(
                Relation(
                    name=f"pisano_period_m{m}",
                    formula=f"π({m})=unknown_from_prefix",
                    support=f"period not fully observed within N={N}",
                    counterexample="insufficient prefix to confirm period",
                    discovered_at_N=N,
                    true=False,
                    category="internal",
                )
            )
    # Aggregate summary relation
    n_found = sum(1 for m in range(2, 21) if m in found)
    out.insert(
        0,
        Relation(
            name="pisano_periods_m2_to_20",
            formula=f"found periods for {n_found}/19 moduli; map={found}",
            support=f"N={N}",
            counterexample=None if n_found > 0 else "none found",
            discovered_at_N=N,
            true=n_found > 0,
            category="internal",
        ),
    )
    return out


def zeckendorf_greedy(k: int, F: list[int]) -> list[int]:
    """Return Fib numbers (values) used in Zeckendorf representation of k."""
    # Use F descending, skip F_0=0, F_1=1 duplicate with F_2=1 — use indices >=2, prefer larger
    # Standard: use F_2=1, F_3=2, ... no two consecutive
    idxs = [i for i in range(len(F) - 1, 1, -1) if F[i] > 0]
    parts = []
    rem = k
    last_used = None
    for i in idxs:
        if F[i] <= rem and (last_used is None or last_used != i + 1):
            parts.append(F[i])
            rem -= F[i]
            last_used = i
            if rem == 0:
                break
    return parts if rem == 0 else []


def check_zeckendorf(F: list[int], N: int) -> Relation:
    """Every integer 1..F_N has a greedy Zeckendorf representation using the prefix."""
    if N < 3:
        return Relation(
            name="zeckendorf",
            formula="every k in 1..F_N = unique non-consecutive Fib sum (greedy)",
            support="N too small",
            counterexample="N<3",
            discovered_at_N=N,
            true=False,
            category="internal",
        )
    upper = F[N]
    # Cap for runtime: check up to min(upper, 5000) but also sample if huge
    limit = min(upper, 8000)
    fails = []
    ok = 0
    for k in range(1, limit + 1):
        parts = zeckendorf_greedy(k, F)
        if not parts or sum(parts) != k:
            fails.append(k)
            if len(fails) > 3:
                break
            continue
        # check non-consecutive indices
        # map values back — allow duplicate value 1 only once from F2
        ok += 1
        # verify no two consecutive Fib indices
        # rebuild indices
        rem = k
        idxs_used = []
        for i in range(len(F) - 1, 1, -1):
            if F[i] <= rem and (not idxs_used or idxs_used[-1] != i + 1):
                # wait, we iterate descending so last appended is previous larger
                if idxs_used and idxs_used[-1] - i == 1:
                    continue
                idxs_used.append(i)
                rem -= F[i]
                if rem == 0:
                    break
        consecutive = any(idxs_used[j] - idxs_used[j + 1] == 1 for j in range(len(idxs_used) - 1))
        if consecutive or rem != 0:
            fails.append(k)
            ok -= 1
            if len(fails) > 3:
                break
    true = len(fails) == 0 and ok > 0
    return Relation(
        name="zeckendorf",
        formula="every k in 1..min(F_N,8000) has greedy non-consecutive Fib sum",
        support=f"{ok} integers represented successfully (limit={limit})",
        counterexample=None if true else f"fail k={fails[0]}",
        discovered_at_N=N,
        true=true,
        category="internal",
    )


def check_binet(F: list[int], N: int) -> Relation:
    """Binet: F(n) = round(φ^n / √5) for n>=0 (exact for integer n)."""
    fails = []
    ok = 0
    for n in range(0, N + 1):
        # Use high precision via float is ok up to ~n=70; for n=80 use integer via rounding carefully
        if n <= 70:
            approx = round((PHI ** n) / SQRT5)
        else:
            # use decimal-ish via numpy float64 fails; use integer recurrence identity as Binet check alternate:
            # For large n use sympy-free: F_n = nearest integer to phi^n/sqrt5
            # python floats lose precision; use from math with care — actually use integer formula via rounding of
            # Decimal
            from decimal import Decimal, getcontext

            getcontext().prec = 80
            phi_d = (Decimal(1) + Decimal(5).sqrt()) / 2
            val = (phi_d ** n) / Decimal(5).sqrt()
            approx = int(val.to_integral_value(rounding="ROUND_HALF_UP"))
        if approx == F[n]:
            ok += 1
        else:
            fails.append((n, approx, F[n]))
            if len(fails) > 3:
                break
    true = len(fails) == 0 and ok > 0
    return Relation(
        name="binet_formula",
        formula="F(n)=round(φ^n / √5)",
        support=f"{ok}/{N+1} indices match",
        counterexample=None if true else f"fail at n={fails[0]}",
        discovered_at_N=N,
        true=true,
        category="internal",
    )


# ---- Negative controls ----

def neg_double_recurrence(F: list[int], N: int) -> Relation:
    fails = []
    ok = 0
    for n in range(1, N + 1):
        if F[n] == 2 * F[n - 1]:
            ok += 1
        else:
            fails.append(n)
            break
    true = len(fails) == 0 and ok == N
    return Relation(
        name="NEG_F_n_eq_2_F_n_minus_1",
        formula="F(n)=2*F(n-1)",
        support=f"checked n=1..{N}",
        counterexample=None if true else f"fails at n={fails[0]}: F={F[fails[0]]} != 2*{F[fails[0]-1]}",
        discovered_at_N=N,
        true=false_as_rejected(true),
        category="negative_control",
    )


def false_as_rejected(holds: bool) -> bool:
    """For negative controls, 'true' means the FALSE claim was incorrectly accepted.
    We set Relation.true = holds of the claim; miner should have true=False for negs.
    """
    return holds


def neg_always_prime(F: list[int], N: int) -> Relation:
    def is_prime(x: int) -> bool:
        if x < 2:
            return False
        if x % 2 == 0:
            return x == 2
        d = 3
        while d * d <= x:
            if x % d == 0:
                return False
            d += 2
        return True

    fails = []
    for n in range(0, N + 1):
        if not is_prime(F[n]):
            fails.append((n, F[n]))
            break
    holds = len(fails) == 0
    return Relation(
        name="NEG_F_n_always_prime",
        formula="F(n) is prime for all n",
        support=f"checked n=0..{N}",
        counterexample=None if holds else f"F({fails[0][0]})={fails[0][1]} not prime",
        discovered_at_N=N,
        true=holds,
        category="negative_control",
    )


def neg_ratio_e(F: list[int], N: int) -> Relation:
    if N < 3 or F[N - 1] == 0:
        last = float("nan")
    else:
        last = F[N] / F[N - 1]
    err = abs(last - math.e)
    holds = err < 1e-3
    return Relation(
        name="NEG_ratios_to_e",
        formula="F(n+1)/F(n) → e",
        support=f"last ratio={last}, |err vs e|={err:.3e}",
        counterexample=None if holds else f"|ratio-e|={err:.3e} (e={math.e:.6f}, φ={PHI:.6f})",
        discovered_at_N=N,
        true=holds,
        category="negative_control",
    )


def neg_ratio_pi(F: list[int], N: int) -> Relation:
    if N < 3 or F[N - 1] == 0:
        last = float("nan")
    else:
        last = F[N] / F[N - 1]
    err = abs(last - math.pi)
    holds = err < 1e-3
    return Relation(
        name="NEG_ratios_to_pi",
        formula="F(n+1)/F(n) → π",
        support=f"last ratio={last}, |err vs π|={err:.3e}",
        counterexample=None if holds else f"|ratio-π|={err:.3e}",
        discovered_at_N=N,
        true=holds,
        category="negative_control",
    )


def mine_at_N(N: int) -> dict[str, Any]:
    F = fib_prefix(N)
    relations: list[Relation] = []

    relations.append(check_order2_classic(F, N))
    for order in (1, 2, 3):
        r = try_linear_recurrence(F, order, N)
        if r is not None:
            relations.append(r)
        else:
            relations.append(
                Relation(
                    name=f"linear_recurrence_order_{order}",
                    formula=f"no exact small-int recurrence order {order} in coeff range [-5,5]",
                    support=f"searched at N={N}",
                    counterexample="no fit" if order != 2 else None,
                    discovered_at_N=N,
                    true=False,
                    category="internal",
                )
            )
    relations.append(check_ratios_to_phi(F, N))
    relations.extend(check_cassini(F, N))
    relations.append(check_gcd_identity(F, N))
    relations.extend(check_pisano(F, N))
    relations.append(check_zeckendorf(F, N))
    relations.append(check_binet(F, N))

    # negatives
    relations.append(neg_double_recurrence(F, N))
    relations.append(neg_always_prime(F, N))
    relations.append(neg_ratio_e(F, N))
    relations.append(neg_ratio_pi(F, N))

    # Count TRUE internal relations (exclude per-modulus pisano detail noise: count summary + non-pisano_m*)
    internal_true = [
        r
        for r in relations
        if r.category == "internal"
        and r.true
        and not r.name.startswith("pisano_period_m")  # individual; keep summary
    ]
    # Also count how many individual pisano found
    pisano_found = sum(
        1 for r in relations if r.name.startswith("pisano_period_m") and r.true
    )
    neg_correctly_rejected = [
        r for r in relations if r.category == "negative_control" and not r.true
    ]
    neg_false_accept = [r for r in relations if r.category == "negative_control" and r.true]

    return {
        "N": N,
        "prefix_tail": F[max(0, N - 5) :],
        "relations": [asdict(r) for r in relations],
        "n_internal_true_core": len(internal_true),
        "internal_true_names": [r.name for r in internal_true],
        "pisano_moduli_found": pisano_found,
        "n_neg_rejected": len(neg_correctly_rejected),
        "n_neg_false_accept": len(neg_false_accept),
        "F_N": F[N],
    }


# Gold list — only well-known, sure facts
GOLD = [
    {
        "id": "recurrence",
        "fact": "F(n)=F(n-1)+F(n-2) with F(0)=0,F(1)=1",
        "status_rule": "rediscovered",
        "miner_names": ["classic_fibonacci_recurrence", "linear_recurrence_order_2"],
    },
    {
        "id": "phi_ratio",
        "fact": "F(n+1)/F(n) → φ=(1+√5)/2",
        "status_rule": "rediscovered",
        "miner_names": ["ratio_limit_phi"],
    },
    {
        "id": "cassini",
        "fact": "F(n+1)F(n-1)-F(n)^2 = (-1)^n",
        "status_rule": "rediscovered",
        "miner_names": ["cassini_identity"],
    },
    {
        "id": "gcd",
        "fact": "gcd(F(a),F(b))=F(gcd(a,b))",
        "status_rule": "rediscovered",
        "miner_names": ["gcd_identity"],
    },
    {
        "id": "pisano",
        "fact": "F mod m is periodic (Pisano period π(m))",
        "status_rule": "rediscovered",
        "miner_names": ["pisano_periods_m2_to_20"],
    },
    {
        "id": "zeckendorf",
        "fact": "Every positive integer has a unique Zeckendorf (non-consecutive Fib) representation",
        "status_rule": "rediscovered",
        "miner_names": ["zeckendorf"],
    },
    {
        "id": "binet",
        "fact": "F(n)=round(φ^n/√5)",
        "status_rule": "rediscovered",
        "miner_names": ["binet_formula"],
    },
    {
        "id": "catalan",
        "fact": "Catalan identity F(n)^2-F(n+r)F(n-r)=(-1)^{n-r+1}F(r)^2 (r=2 checked)",
        "status_rule": "rediscovered",
        "miner_names": ["catalan_identity_r2"],
    },
    {
        "id": "phyllotaxis",
        "fact": "Fibonacci numbers appear in plant phyllotaxis / spiral counts",
        "status_rule": "needs-external-data",
        "miner_names": [],
    },
    {
        "id": "art_history",
        "fact": "Golden ratio / Fibonacci used in art, architecture, historical aesthetics",
        "status_rule": "needs-external-data",
        "miner_names": [],
    },
    {
        "id": "fibonacci_naming",
        "fact": "Named after Leonardo of Pisa (Fibonacci); Liber Abaci rabbit problem",
        "status_rule": "needs-external-data",
        "miner_names": [],
    },
    {
        "id": "entry_point",
        "fact": "Wall–Sun–Sun conjecture / entry point properties beyond simple search",
        "status_rule": "not-in-search-space",
        "miner_names": [],
    },
    {
        "id": "tiling",
        "fact": "F(n+1) counts tilings of an n-board by 1×1 and 1×2 tiles (combinatorial interpretation)",
        "status_rule": "not-in-search-space",
        "miner_names": [],
    },
]


def evaluate_gold(runs: dict[int, dict]) -> list[dict]:
    rows = []
    # Use largest N for rediscovery judgment
    max_N = max(runs.keys())
    rels = {r["name"]: r for r in runs[max_N]["relations"]}
    for g in GOLD:
        if g["status_rule"] in ("needs-external-data", "not-in-search-space"):
            mark = g["status_rule"]
            found_at = None
        else:
            found = False
            found_at = None
            for name in g["miner_names"]:
                for N, run in sorted(runs.items()):
                    rmap = {r["name"]: r for r in run["relations"]}
                    if name in rmap and rmap[name]["true"]:
                        found = True
                        found_at = N if found_at is None else min(found_at, N)
            mark = "rediscovered" if found else "in-search-space-but-missed"
        rows.append(
            {
                "id": g["id"],
                "fact": g["fact"],
                "mark": mark,
                "first_found_at_N": found_at,
            }
        )
    return rows


def main():
    t0 = time.time()
    Ns = [20, 40, 80]
    runs = {}
    table = []
    for N in Ns:
        t1 = time.time()
        run = mine_at_N(N)
        run["runtime_sec"] = round(time.time() - t1, 4)
        runs[N] = run
        table.append(
            {
                "N": N,
                "n_internal_true_core": run["n_internal_true_core"],
                "internal_true_names": run["internal_true_names"],
                "pisano_moduli_found": run["pisano_moduli_found"],
                "n_neg_rejected": run["n_neg_rejected"],
                "n_neg_false_accept": run["n_neg_false_accept"],
                "F_N": run["F_N"],
                "runtime_sec": run["runtime_sec"],
            }
        )
        print(f"N={N}: core_true={run['n_internal_true_core']} pisano={run['pisano_moduli_found']} "
              f"neg_rej={run['n_neg_rejected']} time={run['runtime_sec']}s names={run['internal_true_names']}")

    gold_rows = evaluate_gold(runs)
    results = {
        "hypothesis": (
            "If we gave an AI the Fibonacci sequence to learn everything possible about it, "
            "searching all kinds of relations and connections, could it learn until it becomes an expert system?"
        ),
        "theory": {
            "recurrence_makes_extra_terms_redundant": True,
            "expertise_needs_hypothesis_language_plus_search": True,
            "sequence_alone_can_rediscover_internal_facts": True,
            "sequence_alone_cannot_discover_external_meaning": True,
        },
        "hypothesis_language": __doc__,
        "seed": SEED,
        "Ns": Ns,
        "table_true_vs_N": table,
        "gold_coverage": gold_rows,
        "runs": {str(k): v for k, v in runs.items()},
        "total_runtime_sec": round(time.time() - t0, 4),
    }
    out = OUT_DIR / "results.json"
    with open(out, "w") as f:
        json.dump(results, f, indent=2)
    print(f"Wrote {out}")
    print(f"Total runtime {results['total_runtime_sec']}s")
    return results


if __name__ == "__main__":
    main()
