"""
Sequences world: Fibonacci + Lucas + Pell.

Learning a recurrence on one sequence asserts `rec(Seq, Coeffs)` into the
Prolog archive; transfer_prior tries that clause on companion sequences
(absorb intelligence = cross-sequence Horn reuse, not the internet).

Reuses fibonacci/miner.py helpers where possible.
"""

from __future__ import annotations

import sys
from itertools import product
from pathlib import Path
from typing import Any, Optional

from .base import Conjecture, FamilySpec, VerifiedFact, WorldBase

# Reuse Fibonacci miner
_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(_ROOT / "fibonacci"))
import miner as M  # noqa: E402


def lucas_prefix(N: int) -> list[int]:
    L = [0] * (N + 1)
    if N >= 0:
        L[0] = 2
    if N >= 1:
        L[1] = 1
    for n in range(2, N + 1):
        L[n] = L[n - 1] + L[n - 2]
    return L


def pell_prefix(N: int) -> list[int]:
    P = [0] * (N + 1)
    if N >= 1:
        P[1] = 1
    for n in range(2, N + 1):
        P[n] = 2 * P[n - 1] + P[n - 2]
    return P


SEQ_BUILDERS = {
    "fib": M.fib_prefix,
    "lucas": lucas_prefix,
    "pell": pell_prefix,
}


def _search_recurrence(vals: list[int], order: int, coeff_range=range(-3, 4)) -> Optional[list[int]]:
    N = len(vals) - 1
    if N < order + 2:
        return None
    fits = []
    for coeffs in product(coeff_range, repeat=order):
        if all(c == 0 for c in coeffs):
            continue
        ok = True
        for n in range(order, N + 1):
            pred = sum(coeffs[i] * vals[n - 1 - i] for i in range(order))
            if pred != vals[n]:
                ok = False
                break
        if ok:
            fits.append((sum(abs(c) for c in coeffs), list(coeffs)))
    if not fits:
        return None
    fits.sort()
    return fits[0][1]


class SequencesWorld(WorldBase):
    name = "sequences"

    def __init__(self, N: int = 24) -> None:
        self.N = N
        self.data = {k: b(N) for k, b in SEQ_BUILDERS.items()}
        self._critic = None  # set by kernel
        self._archive = None

    def bind(self, archive, critic) -> None:
        self._archive = archive
        self._critic = critic
        # seed observations into theory (idempotent)
        for seq, vals in self.data.items():
            for n, v in enumerate(vals):
                archive.assert_obs(seq, n, v)

    def families(self) -> dict[str, FamilySpec]:
        return {
            "linear_recurrences": FamilySpec(
                id="linear_recurrences",
                description="Exact small-int linear recurrences on fib/lucas/pell; param=max order",
                dead_end=False,
                param=1,
                param_max=3,
                unlocked=True,
                unlock_order=0,
            ),
            "transfer_recurrence": FamilySpec(
                id="transfer_recurrence",
                description="Try archived rec/2 clauses from companions (Horn transfer)",
                dead_end=False,
                param=0,
                param_max=0,
                unlocked=True,
                unlock_order=1,
            ),
            "ratio_limits": FamilySpec(
                id="ratio_limits",
                description="Consecutive ratios → φ on fib (and analogs)",
                dead_end=False,
                param=0,
                param_max=0,
                unlocked=True,
                unlock_order=2,
            ),
            "dead_end_always_prime": FamilySpec(
                id="dead_end_always_prime",
                description="DEAD END: claim sequence terms always prime",
                dead_end=True,
                param=0,
                param_max=0,
                unlocked=True,
                unlock_order=3,
            ),
            "modular_periods": FamilySpec(
                id="modular_periods",
                description="Pisano-style periods; param=max modulus",
                dead_end=False,
                param=3,
                param_max=10,
                unlocked=False,
                unlock_order=4,
            ),
            "bilinear_fib": FamilySpec(
                id="bilinear_fib",
                description="Cassini on Fibonacci only",
                dead_end=False,
                param=0,
                param_max=0,
                unlocked=False,
                unlock_order=5,
            ),
        }

    def observe(self) -> dict[str, Any]:
        return {
            "N": self.N,
            "sequences": {k: v[:8] + (["..."] if len(v) > 8 else []) for k, v in self.data.items()},
            "n_recs_archived": len(self._archive.list_recs()) if self._archive else 0,
        }

    def hypothesize(self, family: FamilySpec, archive_confirmed: dict, step: int) -> list[Conjecture]:
        out: list[Conjecture] = []
        fid = family.id

        if fid == "linear_recurrences":
            for seq, vals in self.data.items():
                for order in range(1, family.param + 1):
                    coeffs = _search_recurrence(vals, order)
                    if coeffs is None:
                        out.append(
                            Conjecture(
                                name=f"rec_{seq}_order_{order}_none",
                                family=fid,
                                world=self.name,
                                formula=f"no small-int recurrence order {order} on {seq}",
                                payload={"seq": seq, "coeffs": None, "order": order, "kind": "rec"},
                                relation_type="linear_recurrence",
                            )
                        )
                    else:
                        formula = f"{seq}(n) = " + " + ".join(
                            f"({c})*{seq}(n-{i+1})" for i, c in enumerate(coeffs)
                        )
                        out.append(
                            Conjecture(
                                name=f"rec_{seq}_o{order}_{'_'.join(map(str, coeffs))}",
                                family=fid,
                                world=self.name,
                                formula=formula,
                                payload={"seq": seq, "coeffs": coeffs, "order": order, "kind": "rec"},
                                relation_type="linear_recurrence",
                            )
                        )

        elif fid == "transfer_recurrence":
            if not self._critic:
                return out
            for seq in self.data:
                priors = self._critic.transfer_priors(seq)
                for coeffs, src in priors:
                    formula = (
                        f"TRANSFER rec({src},{coeffs}) ⇒ try on {seq}: "
                        + " + ".join(f"({c})*{seq}(n-{i+1})" for i, c in enumerate(coeffs))
                    )
                    out.append(
                        Conjecture(
                            name=f"transfer_{src}_to_{seq}_{'_'.join(map(str, coeffs))}",
                            family=fid,
                            world=self.name,
                            formula=formula,
                            payload={"seq": seq, "coeffs": coeffs, "kind": "rec", "src": src},
                            relation_type="transfer_recurrence",
                            from_transfer=True,
                            transfer_source=src,
                        )
                    )
            # Empty list if no priors yet — kernel scores 0; arm stays unsaturated.

        elif fid == "ratio_limits":
            # fib → φ; also false claims → e
            F = self.data["fib"]
            out.append(
                Conjecture(
                    name="ratio_fib_to_phi",
                    family=fid,
                    world=self.name,
                    formula="fib(n+1)/fib(n) → φ",
                    payload={"kind": "ratio_phi"},
                    relation_type="ratio_limit",
                )
            )
            out.append(
                Conjecture(
                    name="NEG_ratio_fib_to_e",
                    family=fid,
                    world=self.name,
                    formula="fib(n+1)/fib(n) → e",
                    payload={"kind": "ratio_e"},
                    relation_type="ratio_limit_neg",
                )
            )

        elif fid == "dead_end_always_prime":
            out.append(
                Conjecture(
                    name="NEG_fib_always_prime",
                    family=fid,
                    world=self.name,
                    formula="fib(n) always prime",
                    payload={"kind": "always_prime", "seq": "fib"},
                    relation_type="dead_end:prime",
                )
            )

        elif fid == "modular_periods":
            seq = "fib"
            vals = self.data[seq]
            max_m = family.param
            for m in range(2, max_m + 1):
                # discover period via miner helper if available
                period = M.pisano_period(vals, m, self.N)
                out.append(
                    Conjecture(
                        name=f"pisano_{seq}_m{m}",
                        family=fid,
                        world=self.name,
                        formula=f"π_{seq}({m})={period}" if period else f"π_{seq}({m})=unknown",
                        payload={"kind": "period", "seq": seq, "m": m, "period": period},
                        relation_type="modular_period",
                    )
                )

        elif fid == "bilinear_fib":
            out.append(
                Conjecture(
                    name="cassini_fib",
                    family=fid,
                    world=self.name,
                    formula="F(n+1)F(n-1)-F(n)^2 = (-1)^n",
                    payload={"kind": "cassini"},
                    relation_type="cassini_identity",
                )
            )

        return out

    def verify(self, conjecture: Conjecture) -> VerifiedFact:
        p = conjecture.payload
        kind = p.get("kind")

        if kind == "noop":
            return VerifiedFact(
                name=conjecture.name,
                family=conjecture.family,
                world=self.name,
                formula=conjecture.formula,
                true=False,
                support="no priors",
                counterexample="empty archive for transfer",
                relation_type=conjecture.relation_type,
                from_transfer=conjecture.from_transfer,
                transfer_source=conjecture.transfer_source,
            )

        if kind == "rec":
            coeffs = p.get("coeffs")
            seq = p["seq"]
            if coeffs is None:
                return VerifiedFact(
                    name=conjecture.name,
                    family=conjecture.family,
                    world=self.name,
                    formula=conjecture.formula,
                    true=False,
                    support="search exhausted",
                    counterexample="no fit",
                    relation_type=conjecture.relation_type,
                    from_transfer=conjecture.from_transfer,
                    transfer_source=conjecture.transfer_source,
                )
            obs = list(enumerate(self.data[seq]))
            ok, cex = self._critic.check_recurrence(seq, coeffs, obs)
            if ok and self._archive:
                self._archive.assert_rec(seq, coeffs)
            return VerifiedFact(
                name=conjecture.name,
                family=conjecture.family,
                world=self.name,
                formula=conjecture.formula,
                true=ok,
                support=f"Prolog holds_rec({seq},{coeffs}) on N={self.N}" if ok else "finite fail",
                counterexample=cex,
                relation_type=conjecture.relation_type,
                from_transfer=conjecture.from_transfer,
                transfer_source=conjecture.transfer_source,
            )

        if kind == "ratio_phi":
            rel = M.check_ratios_to_phi(self.data["fib"], self.N)
            return VerifiedFact(
                name=conjecture.name,
                family=conjecture.family,
                world=self.name,
                formula=conjecture.formula,
                true=rel.true,
                support=rel.support,
                counterexample=rel.counterexample,
                relation_type=conjecture.relation_type,
            )

        if kind == "ratio_e":
            rel = M.neg_ratio_e(self.data["fib"], self.N)
            return VerifiedFact(
                name=conjecture.name,
                family=conjecture.family,
                world=self.name,
                formula=conjecture.formula,
                true=rel.true,
                support=rel.support,
                counterexample=rel.counterexample,
                relation_type=conjecture.relation_type,
            )

        if kind == "always_prime":
            rel = M.neg_always_prime(self.data["fib"], self.N)
            return VerifiedFact(
                name=conjecture.name,
                family=conjecture.family,
                world=self.name,
                formula=conjecture.formula,
                true=rel.true,
                support=rel.support,
                counterexample=rel.counterexample,
                relation_type=conjecture.relation_type,
            )

        if kind == "period":
            seq, m, period = p["seq"], p["m"], p["period"]
            if period is None:
                return VerifiedFact(
                    name=conjecture.name,
                    family=conjecture.family,
                    world=self.name,
                    formula=conjecture.formula,
                    true=False,
                    support="period not observed in prefix",
                    counterexample="insufficient prefix",
                    relation_type=conjecture.relation_type,
                )
            obs = list(enumerate(self.data[seq]))
            ok, cex = self._critic.check_period(seq, m, period, obs)
            if ok and self._archive:
                self._archive.assert_mod_period(seq, m, period)
            return VerifiedFact(
                name=conjecture.name,
                family=conjecture.family,
                world=self.name,
                formula=conjecture.formula,
                true=ok,
                support=f"holds_period({seq},{m},{period})" if ok else "finite fail",
                counterexample=cex,
                relation_type=conjecture.relation_type,
            )

        if kind == "cassini":
            rels = M.check_cassini(self.data["fib"], self.N)
            rel = next(r for r in rels if r.name == "cassini_identity")
            return VerifiedFact(
                name=conjecture.name,
                family=conjecture.family,
                world=self.name,
                formula=conjecture.formula,
                true=rel.true,
                support=rel.support,
                counterexample=rel.counterexample,
                relation_type=conjecture.relation_type,
            )

        return VerifiedFact(
            name=conjecture.name,
            family=conjecture.family,
            world=self.name,
            formula=conjecture.formula,
            true=False,
            support="unknown kind",
            counterexample="unknown",
            relation_type=conjecture.relation_type,
        )


    def expand_family(self, family: FamilySpec, new_truths: int) -> str:
        # Keep transfer arm alive until some rec/2 exists to absorb.
        if family.id == "transfer_recurrence":
            if self._archive and not self._archive.list_recs():
                return "waiting for rec/2 priors — do not saturate"
            if new_truths > 0:
                family.saturated = False
                return "transfer absorbed — keep arm for other companions"
            # After successful era, allow soft saturate only if all companions have matching rec
            recs = {s for s, _ in self._archive.list_recs()} if self._archive else set()
            if {"fib", "lucas", "pell"} <= recs:
                family.saturated = True
                return "all sequences have rec — saturate transfer"
            return "priors remain for other targets — stay active"
        return super().expand_family(family, new_truths)

    def transfer_prior(self, archive_confirmed: dict) -> list[Conjecture]:
        # Handled as its own family; also exposed for metrics
        fam = self.families()["transfer_recurrence"]
        return self.hypothesize(fam, archive_confirmed, step=-1)
