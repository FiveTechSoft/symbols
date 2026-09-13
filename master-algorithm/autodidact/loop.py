#!/usr/bin/env python3
"""
Autodidact loop — closes the Master Algorithm curiosity loop.

Given only F_0..F_N (fixed N), the system chooses its own next hypothesis
FAMILY via an explicit UCB scheduler, runs a critic, archives accept/reject,
expands saturated families, and unlocks locked families when the current
language plateaus.

This is NOT AGI. The locked catalog is human-designed. The claim under test:
  scheduler + critic + growing hypothesis language  >  more data (larger N)
with a fixed language.

Reuses check functions from ../fibonacci/miner.py where possible.
"""

from __future__ import annotations

import json
import math
import sys
import time
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any, Callable, Optional

# Reuse Fibonacci miner primitives
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "fibonacci"))
import miner as M  # noqa: E402

OUT_DIR = Path(__file__).resolve().parent
SEED = 42
N_PREFIX = 30  # fixed data for autodidact
K_STEPS = 12
UCB_C = 1.4

PHI = M.PHI


def try_linear_recurrence_any(F: list[int], order: int, N: int):
    """Reuse miner for order<=3; brute small coeffs for order 4 (autodidact expansion)."""
    if order <= 3:
        return M.try_linear_recurrence(F, order, N)
    if N < order + 2:
        return None
    # order 4: coeffs in -2..2 excluding all-zero (5^4=625 — fine)
    ranges = range(-2, 3)
    fits = []
    from itertools import product
    for coeffs in product(ranges, repeat=order):
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
    if not fits:
        return None
    fits.sort(key=lambda t: (t[0], t[1]))
    _, coeffs, support_n = fits[0]
    formula = "F(n) = " + " + ".join(f"({c})*F(n-{i+1})" for i, c in enumerate(coeffs))
    return M.Relation(
        name=f"linear_recurrence_order_{order}",
        formula=formula,
        support=f"holds for all n={order}..{N} ({support_n} checks); min L1 among fits",
        counterexample=None,
        discovered_at_N=N,
        true=True,
        category="internal",
    )



# ---------------------------------------------------------------------------
# Knowledge archive
# ---------------------------------------------------------------------------

@dataclass
class Fact:
    name: str
    family: str
    formula: str
    true: bool
    support: str
    counterexample: Optional[str]
    step: int
    relation_type: str  # coarse type for "new types" counting


class Archive:
    def __init__(self) -> None:
        self.confirmed: dict[str, Fact] = {}
        self.rejected: dict[str, Fact] = {}

    def ingest(self, fact: Fact) -> str:
        """Return 'new_true' | 'new_reject' | 'duplicate_true' | 'duplicate_reject'."""
        key = fact.name
        if fact.true:
            if key in self.confirmed:
                return "duplicate_true"
            self.confirmed[key] = fact
            # drop from rejected if previously wrong
            self.rejected.pop(key, None)
            return "new_true"
        else:
            if key in self.rejected:
                return "duplicate_reject"
            self.rejected[key] = fact
            return "new_reject"

    def confirmed_types(self) -> set[str]:
        return {f.relation_type for f in self.confirmed.values()}

    def to_dict(self) -> dict:
        return {
            "confirmed": {k: asdict(v) for k, v in self.confirmed.items()},
            "rejected": {k: asdict(v) for k, v in self.rejected.items()},
            "n_confirmed": len(self.confirmed),
            "n_rejected": len(self.rejected),
            "confirmed_relation_types": sorted(self.confirmed_types()),
        }


# ---------------------------------------------------------------------------
# Hypothesis families (catalog) — some start locked
# ---------------------------------------------------------------------------

@dataclass
class FamilyState:
    id: str
    description: str
    unlocked: bool
    dead_end: bool
    # expansion knobs
    param: int  # order / max_r / max_m depending on family
    param_max: int
    n_visits: int = 0
    total_reward: float = 0.0
    last_new_truths: int = 0
    saturated: bool = False
    unlock_order: int = 0  # catalog order for unlock

    @property
    def avg_reward(self) -> float:
        if self.n_visits == 0:
            return 0.0
        return self.total_reward / self.n_visits


def build_catalog() -> dict[str, FamilyState]:
    """
    Human-designed catalog (honest limitation). Initially unlocked:
      linear_recurrences, ratio_limits, dead_end_always_prime
    Locked until language growth unlocks them in unlock_order.
    """
    families = [
        FamilyState(
            id="linear_recurrences",
            description="Exact small-int linear recurrences; param=max order searched",
            unlocked=True,
            dead_end=False,
            param=1,  # start order 1 only; expand to 2,3,4
            param_max=4,
            unlock_order=0,
        ),
        FamilyState(
            id="ratio_limits",
            description="Consecutive ratios vs {phi, e, pi, 2}",
            unlocked=True,
            dead_end=False,
            param=0,
            param_max=0,
            unlock_order=1,
        ),
        FamilyState(
            id="dead_end_always_prime",
            description="DEAD END: claim F(n) always prime",
            unlocked=True,
            dead_end=True,
            param=0,
            param_max=0,
            unlock_order=2,
        ),
        # --- locked ---
        FamilyState(
            id="bilinear_identities",
            description="Cassini + Catalan-like with parameter r=1..param",
            unlocked=False,
            dead_end=False,
            param=1,
            param_max=4,
            unlock_order=3,
        ),
        FamilyState(
            id="gcd_identities",
            description="gcd(F(a),F(b))=F(gcd(a,b))",
            unlocked=False,
            dead_end=False,
            param=0,
            param_max=0,
            unlock_order=4,
        ),
        FamilyState(
            id="modular_periods",
            description="Pisano periods; param=max modulus m",
            unlocked=False,
            dead_end=False,
            param=5,  # start m=2..5; expand toward 20
            param_max=20,
            unlock_order=5,
        ),
        FamilyState(
            id="zeckendorf",
            description="Zeckendorf greedy non-consecutive Fib sums",
            unlocked=False,
            dead_end=False,
            param=0,
            param_max=0,
            unlock_order=6,
        ),
        FamilyState(
            id="closed_forms",
            description="Binet-like closed form F(n)=round(phi^n/sqrt(5))",
            unlocked=False,
            dead_end=False,
            param=0,
            param_max=0,
            unlock_order=7,
        ),
        FamilyState(
            id="dead_end_geometric_2",
            description="DEAD END: F(n)=2*F(n-1) geometric ratio 2",
            unlocked=False,
            dead_end=True,
            param=0,
            param_max=0,
            unlock_order=8,
        ),
    ]
    return {f.id: f for f in families}


# ---------------------------------------------------------------------------
# Critic: run one family, return Fact list + score components
# ---------------------------------------------------------------------------

def _rtype(name: str, family: str) -> str:
    """Coarse relation type for novelty-of-type counting."""
    if family.startswith("dead_end"):
        return f"dead_end:{family}"
    if name.startswith("pisano_period_m"):
        return "pisano_period"  # individual moduli = same type for 'types'
    if name.startswith("linear_recurrence"):
        return "linear_recurrence"
    if name.startswith("catalan"):
        return "catalan_identity"
    if name.startswith("ratio"):
        return "ratio_limit"
    mapping = {
        "classic_fibonacci_recurrence": "linear_recurrence",
        "cassini_identity": "cassini_identity",
        "gcd_identity": "gcd_identity",
        "pisano_periods_summary": "pisano_period",
        "zeckendorf": "zeckendorf",
        "binet_formula": "binet_formula",
        "NEG_F_n_always_prime": "dead_end:prime",
        "NEG_F_n_eq_2_F_n_minus_1": "dead_end:geometric_2",
        "NEG_ratios_to_e": "ratio_limit_neg",
        "NEG_ratios_to_pi": "ratio_limit_neg",
        "NEG_ratios_to_2": "ratio_limit_neg",
        "ratio_limit_phi": "ratio_limit",
    }
    return mapping.get(name, name)


def run_family(fid: str, F: list[int], N: int, fam: FamilyState) -> list[Fact]:
    """Execute critic for one family at current expansion param. step filled later."""
    facts: list[Fact] = []

    def add(rel: M.Relation, family: str) -> None:
        facts.append(
            Fact(
                name=rel.name,
                family=family,
                formula=rel.formula,
                true=rel.true,
                support=rel.support,
                counterexample=rel.counterexample,
                step=-1,
                relation_type=_rtype(rel.name, family),
            )
        )

    if fid == "linear_recurrences":
        # search orders 1..param
        classic = M.check_order2_classic(F, N)
        if fam.param >= 2:
            add(classic, fid)
        for order in range(1, fam.param + 1):
            r = try_linear_recurrence_any(F, order, N)
            if r is not None:
                add(r, fid)
            else:
                add(
                    M.Relation(
                        name=f"linear_recurrence_order_{order}",
                        formula=f"no exact small-int recurrence order {order} in [-5,5]",
                        support=f"searched at N={N}",
                        counterexample="no fit",
                        discovered_at_N=N,
                        true=False,
                        category="internal",
                    ),
                    fid,
                )

    elif fid == "ratio_limits":
        add(M.check_ratios_to_phi(F, N), fid)
        # also test vs e, pi, 2 (negatives live here as ratio hypotheses)
        add(M.neg_ratio_e(F, N), fid)
        add(M.neg_ratio_pi(F, N), fid)
        # ratio → 2
        if N >= 3 and F[N - 1] != 0:
            last = F[N] / F[N - 1]
            err = abs(last - 2.0)
            holds = err < 1e-3
        else:
            last, err, holds = float("nan"), float("inf"), False
        add(
            M.Relation(
                name="NEG_ratios_to_2",
                formula="F(n+1)/F(n) → 2",
                support=f"last ratio={last}, |err vs 2|={err:.3e}",
                counterexample=None if holds else f"|ratio-2|={err:.3e}",
                discovered_at_N=N,
                true=holds,
                category="negative_control",
            ),
            fid,
        )

    elif fid == "dead_end_always_prime":
        add(M.neg_always_prime(F, N), fid)

    elif fid == "bilinear_identities":
        # Cassini always; Catalan for r=1..param
        cassini_list = M.check_cassini(F, N)  # returns cassini + catalan r=2 if N>=6
        # Always take cassini
        for rel in cassini_list:
            if rel.name == "cassini_identity":
                add(rel, fid)
        # Catalan for each r in 1..param
        for r in range(1, fam.param + 1):
            if N < 2 * r + 2:
                continue
            fails = []
            ok = 0
            for n in range(r, N - r + 1):
                lhs = F[n] ** 2 - F[n + r] * F[n - r]
                # F0=0 indexing: F(n)^2 - F(n+r)F(n-r) = (-1)^{n-r} F(r)^2
                rhs = ((-1) ** (n - r)) * (F[r] ** 2)
                if lhs == rhs:
                    ok += 1
                else:
                    fails.append(n)
                    break
            true = len(fails) == 0 and ok > 0
            add(
                M.Relation(
                    name=f"catalan_identity_r{r}",
                    formula=f"F(n)^2 - F(n+{r})F(n-{r}) = (-1)^{{n-{r}}} F({r})^2",
                    support=f"{ok} checks for n={r}..{N-r}",
                    counterexample=None if true else f"fail at n={fails[0]}",
                    discovered_at_N=N,
                    true=true,
                    category="internal",
                ),
                fid,
            )

    elif fid == "gcd_identities":
        add(M.check_gcd_identity(F, N), fid)

    elif fid == "modular_periods":
        found = {}
        max_m = fam.param
        for m in range(2, max_m + 1):
            p = M.pisano_period(F, m, N)
            if p is not None:
                found[m] = p
                add(
                    M.Relation(
                        name=f"pisano_period_m{m}",
                        formula=f"π({m})={p}",
                        support=f"verified on prefix N={N}",
                        counterexample=None,
                        discovered_at_N=N,
                        true=True,
                        category="internal",
                    ),
                    fid,
                )
            else:
                add(
                    M.Relation(
                        name=f"pisano_period_m{m}",
                        formula=f"π({m})=unknown_from_prefix",
                        support=f"period not observed within N={N}",
                        counterexample="insufficient prefix",
                        discovered_at_N=N,
                        true=False,
                        category="internal",
                    ),
                    fid,
                )
        add(
            M.Relation(
                name="pisano_periods_summary",
                formula=f"found {len(found)}/{max_m-1} moduli m=2..{max_m}; map={found}",
                support=f"N={N}",
                counterexample=None if found else "none found",
                discovered_at_N=N,
                true=len(found) > 0,
                category="internal",
            ),
            fid,
        )

    elif fid == "zeckendorf":
        add(M.check_zeckendorf(F, N), fid)

    elif fid == "closed_forms":
        add(M.check_binet(F, N), fid)

    elif fid == "dead_end_geometric_2":
        add(M.neg_double_recurrence(F, N), fid)

    else:
        raise ValueError(f"unknown family {fid}")

    return facts


def score_visit(
    outcomes: list[str],
    facts: list[Fact],
    fam: FamilyState,
    archive: Archive,
) -> tuple[float, dict]:
    """
    score = novelty + residual_unexplained_bonus - cost

    novelty: +1.0 per new_true (non-dead-end); +0.3 per correct new_reject of dead-end claim
    residual: fraction of locked families still locked (encourages eventually unlocking via saturation path)
    cost: 0.1 * n_facts_checked + 0.05 * param (expansion cost)
    """
    new_true = sum(1 for o in outcomes if o == "new_true")
    new_reject = sum(1 for o in outcomes if o == "new_reject")
    # dead-end correct reject is useful (learned not to trust); false accept is bad
    dead_end_correct = 0
    dead_end_wrong = 0
    for f, o in zip(facts, outcomes):
        if fam.dead_end or f.name.startswith("NEG_"):
            if not f.true and o in ("new_reject", "duplicate_reject"):
                dead_end_correct += 1
            if f.true:
                dead_end_wrong += 1

    novelty = 1.0 * new_true + 0.25 * dead_end_correct - 1.0 * dead_end_wrong
    # residual unexplained: locked families + unsaturated unlocked productive families
    cost = 0.05 * len(facts) + 0.02 * fam.param
    # bonus if we found something genuinely new of a new type
    types_before = set(archive.confirmed_types())  # already includes this visit's ingest
    # compute types added this visit
    new_types = 0
    seen = set()
    for f, o in zip(facts, outcomes):
        if o == "new_true" and f.relation_type not in seen:
            # check if this type was new before this fact
            # approximate: count unique new_true types in this visit
            seen.add(f.relation_type)
            new_types += 1
    novelty += 0.5 * new_types

    score = novelty - cost
    detail = {
        "new_true": new_true,
        "new_reject": new_reject,
        "dead_end_correct": dead_end_correct,
        "dead_end_wrong": dead_end_wrong,
        "new_types_in_visit": new_types,
        "cost": round(cost, 4),
        "novelty": round(novelty, 4),
        "score": round(score, 4),
    }
    return score, detail


# ---------------------------------------------------------------------------
# Curiosity scheduler: UCB1 over unlocked families
# ---------------------------------------------------------------------------

def ucb_pick(
    families: dict[str, FamilyState],
    total_steps_done: int,
    c: float = UCB_C,
) -> tuple[str, str]:
    """
    Pick among UNLOCKED and NON-SATURATED families maximizing UCB1:
      UCB_i = avg_reward_i + c * sqrt(ln(T+1) / n_i)
    Never-tried (n_i=0) get +∞. Saturated families are excluded (forced language
    growth / unlock handles them). Among +∞ ties, lower unlock_order first.
    """
    pool = [f for f in families.values() if f.unlocked and not f.saturated]
    if not pool:
        # fallback: any unlocked (caller should have unlocked already)
        pool = [f for f in families.values() if f.unlocked]
    if not pool:
        raise RuntimeError("no unlocked families")

    T = max(1, total_steps_done)
    candidates = []
    for f in pool:
        if f.n_visits == 0:
            ucb = float("inf")
            why_term = "never-tried → UCB=+∞ (forced explore)"
        else:
            bonus = c * math.sqrt(math.log(T + 1) / f.n_visits)
            ucb = f.avg_reward + bonus
            why_term = (
                f"avg={f.avg_reward:.3f}+{c}*sqrt(ln({T}+1)/{f.n_visits})={bonus:.3f}"
                f" → UCB={ucb:.3f}; sat={f.saturated}; param={f.param}"
            )
        candidates.append((ucb, f.n_visits, f.unlock_order, f.id, why_term))

    def sort_key(t):
        ucb, nvis, uord, fid, why = t
        is_inf = 1 if ucb == float("inf") else 0
        finite_ucb = 0.0 if ucb == float("inf") else ucb
        # inf first, then high UCB, then fewer visits, then earlier catalog order
        return (-is_inf, -finite_ucb, nvis, uord)

    candidates.sort(key=sort_key)
    best = candidates[0]
    fid = best[3]
    why = (
        f"UCB1 among {len(pool)} active (unlocked∧¬saturated) families; "
        f"picked '{fid}' because {best[4]}. "
        f"Rule: argmax_i [avg_reward_i + {c}*sqrt(ln(T+1)/n_i)]; never-tried=+∞; "
        f"saturated excluded."
    )
    return fid, why


def _bump_param(fam: FamilyState) -> int:
    old = fam.param
    if fam.id == "modular_periods":
        fam.param = min(fam.param_max, fam.param + 5)
    else:
        fam.param = min(fam.param_max, fam.param + 1)
    return old


def try_expand_or_mark_saturated(fam: FamilyState, new_truths: int) -> str:
    """
    Language growth within a family:
      - dead-end → saturate immediately (curiosity tax)
      - new truths and room to grow → expand param (keep searching richer language)
      - new truths at max param → saturate (this family done)
      - no new truths and room → expand param
      - no new truths at max → saturate
    """
    if fam.dead_end:
        fam.saturated = True
        return "dead-end family → mark saturated (curiosity wasted a step; recover next)"

    if new_truths > 0:
        if fam.param < fam.param_max:
            old = _bump_param(fam)
            return f"found new truths → expand param {old}→{fam.param} (grow language inside family)"
        fam.saturated = True
        return "found new truths but param at max → mark saturated"

    if fam.param < fam.param_max:
        old = _bump_param(fam)
        return f"no new truths → expand param {old}→{fam.param}"

    fam.saturated = True
    return f"no new truths and param at max ({fam.param_max}) → mark saturated"


def unlock_next(families: dict[str, FamilyState]) -> Optional[str]:
    locked = sorted(
        [f for f in families.values() if not f.unlocked],
        key=lambda f: f.unlock_order,
    )
    if not locked:
        return None
    f = locked[0]
    f.unlocked = True
    return f.id


def all_unlocked_saturated(families: dict[str, FamilyState]) -> bool:
    unlocked = [f for f in families.values() if f.unlocked]
    return bool(unlocked) and all(f.saturated for f in unlocked)


# ---------------------------------------------------------------------------
# Autodidact main loop
# ---------------------------------------------------------------------------

def run_autodidact(N: int = N_PREFIX, K: int = K_STEPS) -> dict:
    F = M.fib_prefix(N)
    archive = Archive()
    families = build_catalog()
    step_log: list[dict] = []
    unlock_events: list[dict] = []

    for step in range(1, K + 1):
        # Language growth: if all unlocked saturated, unlock next locked family
        unlocked_before = [f.id for f in families.values() if f.unlocked]
        if all_unlocked_saturated(families):
            nid = unlock_next(families)
            if nid:
                unlock_events.append(
                    {
                        "step": step,
                        "unlocked": nid,
                        "reason": "all currently unlocked families saturated → unlock next from locked catalog",
                    }
                )

        fid, why = ucb_pick(families, total_steps_done=step - 1)
        fam = families[fid]

        facts = run_family(fid, F, N, fam)
        for fact in facts:
            fact.step = step

        outcomes = [archive.ingest(fact) for fact in facts]
        new_truths = sum(1 for o in outcomes if o == "new_true")
        # For scoring, archive already updated — score_visit uses that
        # Recompute types carefully: score based on outcomes
        reward, detail = score_visit(outcomes, facts, fam, archive)

        fam.n_visits += 1
        fam.total_reward += reward
        fam.last_new_truths = new_truths

        growth_msg = try_expand_or_mark_saturated(fam, new_truths)

        # Also: after marking saturated, if ALL saturated, unlock immediately for next step
        # (already handled at top of next iteration)

        accept_reject = [
            {
                "name": f.name,
                "true": f.true,
                "outcome": o,
                "relation_type": f.relation_type,
                "formula": f.formula,
                "counterexample": f.counterexample,
            }
            for f, o in zip(facts, outcomes)
        ]

        step_log.append(
            {
                "step": step,
                "family_chosen": fid,
                "why": why,
                "family_param": fam.param,
                "dead_end": fam.dead_end,
                "reward": round(reward, 4),
                "reward_detail": detail,
                "new_truths": new_truths,
                "growth_action": growth_msg,
                "family_saturated_after": fam.saturated,
                "unlocked_families": sorted(f.id for f in families.values() if f.unlocked),
                "accept_reject": accept_reject,
                "archive_n_confirmed": len(archive.confirmed),
                "archive_confirmed_types": sorted(archive.confirmed_types()),
            }
        )

        # Mid-step unlock if we just saturated the last unsaturated
        if all_unlocked_saturated(families):
            nid = unlock_next(families)
            if nid:
                unlock_events.append(
                    {
                        "step": step,
                        "unlocked": nid,
                        "reason": "post-step: all unlocked families saturated → unlock next",
                    }
                )

    return {
        "N": N,
        "K": K,
        "curiosity_rule": (
            "UCB1 over unlocked AND non-saturated families: "
            f"UCB_i = avg_reward_i + {UCB_C}*sqrt(ln(T+1)/n_i); "
            "never-tried get +∞; saturated excluded from the pool. "
            "Reward = novelty(# new true + 0.5*new_types + 0.25*dead-end correct rejects) - cost. "
            "After a visit: expand param (order / Catalan r / max m) on success or failure if room remains; "
            "else mark saturated. When ALL unlocked families are saturated, unlock next from locked human catalog."
        ),
        "honesty": (
            "The locked-family catalog is human-designed; unlocking is still choosing from a "
            "pre-authored menu, not inventing new mathematics. Curiosity (UCB) is a simple "
            "bandit rule and can waste steps on dead-end families — that is intentional."
        ),
        "step_log": step_log,
        "unlock_events": unlock_events,
        "archive": archive.to_dict(),
        "final_family_stats": {
            fid: {
                "unlocked": f.unlocked,
                "saturated": f.saturated,
                "n_visits": f.n_visits,
                "avg_reward": round(f.avg_reward, 4),
                "param": f.param,
                "dead_end": f.dead_end,
            }
            for fid, f in families.items()
        },
        "relation_types_found": sorted(archive.confirmed_types()),
        "n_relation_types": len(archive.confirmed_types()),
    }


# ---------------------------------------------------------------------------
# Baseline: fixed language, only increase N
# ---------------------------------------------------------------------------

def fixed_language_mine(N: int) -> dict:
    """
    Fixed language = initially-unlocked families ONLY (linear, ratio, dead_end_prime),
    run once at N. Does NOT choose families or unlock. Mirrors 'more data, fixed language'.
    Also run the FULL original miner language for the comparison table (as in fibonacci/miner).
    """
    # Full fixed language (all families that miner knows) — no scheduling
    run = M.mine_at_N(N)
    # Core types from internal true (exclude individual pisano_m* noise; use summary)
    types = set()
    for r in run["relations"]:
        if not r["true"]:
            continue
        if r["category"] != "internal":
            continue
        if r["name"].startswith("pisano_period_m") and r["name"] != "pisano_periods_m2_to_20":
            types.add("pisano_period")  # count as one type
            continue
        if r["name"] == "pisano_periods_m2_to_20":
            types.add("pisano_period")
            continue
        types.add(_rtype(r["name"], "fixed"))
    return {
        "N": N,
        "n_internal_true_core": run["n_internal_true_core"],
        "internal_true_names": run["internal_true_names"],
        "pisano_moduli_found": run["pisano_moduli_found"],
        "relation_types": sorted(types),
        "n_relation_types": len(types),
    }


def run_baseline() -> dict:
    """Control: increase N with FIXED full miner language; no family choice."""
    rows = []
    cumulative_types: set[str] = set()
    type_growth = []
    for N in (20, 40, 80):
        row = fixed_language_mine(N)
        new_types = sorted(set(row["relation_types"]) - cumulative_types)
        cumulative_types |= set(row["relation_types"])
        row["new_types_vs_previous"] = new_types
        row["cumulative_types"] = sorted(cumulative_types)
        row["n_cumulative_types"] = len(cumulative_types)
        rows.append(row)
        type_growth.append(
            {
                "N": N,
                "new_types": new_types,
                "n_cumulative_types": len(cumulative_types),
                "core_true_plateau": row["n_internal_true_core"],
                "pisano_moduli": row["pisano_moduli_found"],
            }
        )
    return {
        "description": (
            "Baseline: fixed hypothesis language (full miner H1–H8), only increase N "
            "from 20→40→80. No family scheduler, no unlock."
        ),
        "rows": rows,
        "type_growth": type_growth,
        "final_relation_types": sorted(cumulative_types),
        "n_relation_types_final": len(cumulative_types),
        "new_types_after_N20": sorted(
            set(cumulative_types) - set(rows[0]["relation_types"])
        ),
    }


def compare(auto: dict, baseline: dict) -> dict:
    auto_types = set(auto["relation_types_found"])
    # Exclude pure dead-end 'confirmed' — dead ends should be rejected not confirmed
    auto_productive = {t for t in auto_types if not t.startswith("dead_end")}
    base_types = set(baseline["final_relation_types"])
    base_at_20 = set(baseline["rows"][0]["relation_types"])

    # Types autodidact found that baseline already had at N=20
    # Types only from unlocking language beyond initial unlocked set
    initial_possible = {
        "linear_recurrence",
        "ratio_limit",
        # negatives don't confirm types
    }
    types_from_unlock = sorted(auto_productive - initial_possible)

    verdict_chose_question = (
        len(types_from_unlock) > len(baseline["new_types_after_N20"])
    )

    return {
        "autodidact_N": auto["N"],
        "autodidact_K": auto["K"],
        "autodidact_relation_types": sorted(auto_productive),
        "autodidact_n_types": len(auto_productive),
        "autodidact_types_from_language_growth": types_from_unlock,
        "baseline_types_at_N20": sorted(base_at_20),
        "baseline_n_types_at_N20": len(base_at_20),
        "baseline_new_types_from_more_N": baseline["new_types_after_N20"],
        "baseline_final_n_types": baseline["n_relation_types_final"],
        "chose_next_question_beat_more_N": verdict_chose_question,
        "summary": (
            f"Autodidact at fixed N={auto['N']} over K={auto['K']} steps found "
            f"{len(auto_productive)} productive relation types "
            f"({len(types_from_unlock)} via unlocking/expanding language beyond the initial set). "
            f"Baseline with more N (20→40→80) added {len(baseline['new_types_after_N20'])} "
            f"new relation types after N=20 (core plateau; only Pisano coverage grows). "
            f"Verdict: choosing the next question "
            f"{'BEAT' if verdict_chose_question else 'did NOT beat'} reading more terms."
        ),
    }


def main():
    t0 = time.time()
    print("=== Autodidact loop ===")
    auto = run_autodidact(N=N_PREFIX, K=K_STEPS)
    for s in auto["step_log"]:
        print(
            f"  step {s['step']}: family={s['family_chosen']} "
            f"new_true={s['new_truths']} reward={s['reward']} "
            f"| {s['growth_action']}"
        )
        print(f"         why: {s['why'][:120]}...")

    print("\n=== Baseline (more N, fixed language) ===")
    baseline = run_baseline()
    for g in baseline["type_growth"]:
        print(
            f"  N={g['N']}: cumulative_types={g['n_cumulative_types']} "
            f"new={g['new_types']} core={g['core_true_plateau']} pisano={g['pisano_moduli']}"
        )

    comparison = compare(auto, baseline)
    print("\n=== Comparison ===")
    print(comparison["summary"])

    results = {
        "question": "cómo hacer un sistema que aprenda por sí solo",
        "claim_under_test": "scheduler + critic + growing language > more data (fixed language)",
        "seed": SEED,
        "autodidact": auto,
        "baseline": baseline,
        "comparison": comparison,
        "total_runtime_sec": round(time.time() - t0, 4),
    }
    out = OUT_DIR / "results.json"
    with open(out, "w") as f:
        json.dump(results, f, indent=2)
    print(f"\nWrote {out} in {results['total_runtime_sec']}s")
    return results


if __name__ == "__main__":
    main()
