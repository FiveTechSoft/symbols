"""
Sequences world: Fibonacci + Lucas + Pell — generative schema language.

Hypotheses come from operators in schema_lang (linrec scan, modperiod schema,
bilinear schema), not a frozen menu of named theorems. Transfer remains the
intelligence metric: Fib law → Lucas (succeed), Fib law → Pell (fail honestly).
"""

from __future__ import annotations

import sys
from pathlib import Path
from typing import Any, Optional

from .base import Conjecture, FamilySpec, VerifiedFact, WorldBase
from .schema_lang import (
    HypothesisLanguage,
    SchemaClass,
    generate_bilinear_candidates,
    generate_linrec_candidates,
    generate_modperiod_candidates,
    verify_bilinear,
)

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
    """Kept for metrics.transfer_eval scratch search."""
    from itertools import product

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


# Map schema class id → FamilySpec-compatible arm id (UCB still schedules arms)
_SCHEMA_TO_FAMILY = {
    "linrec_scan": "linear_recurrences",
    "transfer_horn": "transfer_recurrence",
    "ratio_scan": "ratio_limits",
    "dead_prime": "dead_end_always_prime",
    "modperiod_schema": "modular_periods",
    "bilinear_schema": "bilinear_schema",  # NEW — not in old families() list
}


class SequencesWorld(WorldBase):
    name = "sequences"

    def __init__(self, N: int = 24, language: Optional[HypothesisLanguage] = None) -> None:
        self.N = N
        self.data = {k: b(N) for k, b in SEQ_BUILDERS.items()}
        self._critic = None
        self._archive = None
        self.language = language or HypothesisLanguage.seed()

    def bind(self, archive, critic) -> None:
        self._archive = archive
        self._critic = critic
        for seq, vals in self.data.items():
            for n, v in enumerate(vals):
                archive.assert_obs(seq, n, v)
        # Restore language skin from archive meta if present
        skin = archive.meta.get("hypothesis_skin")
        if skin and "schemas" in skin:
            try:
                self.language = HypothesisLanguage(
                    schemas={
                        k: SchemaClass.from_dict(v) for k, v in skin["schemas"].items()
                    },
                    generation=int(skin.get("generation", 0)),
                    molt_history=list(skin.get("molt_history", [])),
                    seen_coeff_fingerprints=list(skin.get("seen_coeff_fingerprints", [])),
                )
            except Exception:
                pass

    def persist_skin(self) -> None:
        if self._archive is not None:
            self._archive.meta["hypothesis_skin"] = self.language.snapshot()
            # Also write schema/2 style facts into meta (not verified theorems)
            self._archive.meta["schema_classes"] = [
                {"id": s.id, "unlocked": s.unlocked, "origin": s.origin}
                for s in self.language.schemas.values()
            ]
            self._archive.save_meta()

    def families(self) -> dict[str, FamilySpec]:
        """UCB arms derived from the live hypothesis language (skin)."""
        out: dict[str, FamilySpec] = {}
        lang = self.language
        for sid, sch in lang.schemas.items():
            if sid.startswith("geo_"):
                continue  # geometry owns geo_invent
            # Mutant linrec variants also become arms
            if sid.startswith("linrec_scan"):
                fam_id = "linear_recurrences" if sid == "linrec_scan" else sid
            else:
                fam_id = _SCHEMA_TO_FAMILY.get(sid, sid)
            out[fam_id] = FamilySpec(
                id=fam_id,
                description=sch.description,
                dead_end=sch.dead_end,
                param=sch.order if "linrec" in sid else (
                    sch.m_max if "modperiod" in sid else (
                        sch.r_max if "bilinear" in sid else 0
                    )
                ),
                param_max=sch.order_max if "linrec" in sid else (
                    sch.m_cap if "modperiod" in sid else (
                        sch.r_cap if "bilinear" in sid else 0
                    )
                ),
                unlocked=sch.unlocked,
                unlock_order=list(lang.schemas.keys()).index(sid),
                saturated=sch.saturated,
                n_visits=sch.n_visits,
                total_reward=sch.total_reward,
            )
        return out

    def _schema_for_family(self, fam_id: str) -> Optional[SchemaClass]:
        # reverse map
        for sid, mapped in _SCHEMA_TO_FAMILY.items():
            if mapped == fam_id and sid in self.language.schemas:
                return self.language.schemas[sid]
        if fam_id in self.language.schemas:
            return self.language.schemas[fam_id]
        for prefix in ("linrec_scan", "bilinear_schema", "modperiod_schema"):
            if fam_id.startswith(prefix) and fam_id in self.language.schemas:
                return self.language.schemas[fam_id]
        return None

    def observe(self) -> dict[str, Any]:
        return {
            "N": self.N,
            "sequences": {k: v[:8] + (["..."] if len(v) > 8 else []) for k, v in self.data.items()},
            "n_recs_archived": len(self._archive.list_recs()) if self._archive else 0,
            "n_schema_classes": self.language.n_schema_classes(),
            "unlocked_schemas": self.language.unlocked_ids(),
            "language_generation": self.language.generation,
        }

    def hypothesize(self, family: FamilySpec, archive_confirmed: dict, step: int) -> list[Conjecture]:
        out: list[Conjecture] = []
        fid = family.id
        sch = self._schema_for_family(fid)

        if fid == "linear_recurrences" or (sch and sch.id.startswith("linrec_scan")):
            if sch is None:
                sch = self.language.schemas.get("linrec_scan")
            if sch is None:
                return out
            prefer_novel = sch.novelty_bonus > 0 or self.language.preferred_novel_coeffs()
            for seq, vals in self.data.items():
                cands = generate_linrec_candidates(
                    vals,
                    sch,
                    seq,
                    prefer_novel=prefer_novel,
                    seen=self.language.seen_coeff_fingerprints,
                )
                for c in cands:
                    out.append(
                        Conjecture(
                            name=c["name"],
                            family=fid,
                            world=self.name,
                            formula=c["formula"],
                            payload=c,
                            relation_type=c["relation_type"],
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
                            payload={"seq": seq, "coeffs": coeffs, "kind": "rec", "src": src, "schema": "transfer_horn"},
                            relation_type="transfer_recurrence",
                            from_transfer=True,
                            transfer_source=src,
                        )
                    )

        elif fid == "ratio_limits":
            out.append(
                Conjecture(
                    name="ratio_fib_to_phi",
                    family=fid,
                    world=self.name,
                    formula="fib(n+1)/fib(n) → φ",
                    payload={"kind": "ratio_phi", "schema": "ratio_scan"},
                    relation_type="ratio_limit",
                )
            )
            out.append(
                Conjecture(
                    name="NEG_ratio_fib_to_e",
                    family=fid,
                    world=self.name,
                    formula="fib(n+1)/fib(n) → e",
                    payload={"kind": "ratio_e", "schema": "ratio_scan"},
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
                    payload={"kind": "always_prime", "seq": "fib", "schema": "dead_prime"},
                    relation_type="dead_end:prime",
                )
            )

        elif fid == "modular_periods" or fid.startswith("modperiod_schema") or (sch and "modperiod" in sch.id):
            if sch is None:
                sch = self.language.schemas.get(fid) or self.language.schemas.get("modperiod_schema")
            if sch is None or not sch.unlocked:
                return out
            for seq in ("fib", "lucas"):
                cands = generate_modperiod_candidates(
                    self.data[seq], sch, seq, self.N, M.pisano_period
                )
                for c in cands:
                    out.append(
                        Conjecture(
                            name=c["name"],
                            family=fid,
                            world=self.name,
                            formula=c["formula"],
                            payload=c,
                            relation_type=c["relation_type"],
                        )
                    )

        elif fid == "bilinear_schema" or fid.startswith("bilinear_schema") or (sch and "bilinear" in sch.id):
            if sch is None:
                sch = self.language.schemas.get(fid) or self.language.schemas.get("bilinear_schema")
            if sch is None or not sch.unlocked:
                return out
            # Search schema params on fib (and lucas for transfer-flavored check)
            for seq in ("fib", "lucas"):
                cands = generate_bilinear_candidates(self.data[seq], sch, seq, self.N)
                for c in cands:
                    out.append(
                        Conjecture(
                            name=c["name"],
                            family=fid,
                            world=self.name,
                            formula=c["formula"],
                            payload=c,
                            relation_type=c["relation_type"],
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
                self.language.note_coeffs(coeffs)
                self.persist_skin()
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

        if kind == "bilinear":
            seq = p["seq"]
            ok, support, cex = verify_bilinear(self.data[seq], p, self.N)
            # Critic gate: only archive if true AND not a bogus/neg form
            if ok and self._archive and p.get("form") != "bogus_const":
                self._archive.assert_verified(
                    self.name, conjecture.family, conjecture.name, conjecture.formula
                )
            return VerifiedFact(
                name=conjecture.name,
                family=conjecture.family,
                world=self.name,
                formula=conjecture.formula,
                true=ok,
                support=support,
                counterexample=cex,
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
        sch = self._schema_for_family(family.id)
        if family.id == "transfer_recurrence":
            if self._archive and not self._archive.list_recs():
                return "waiting for rec/2 priors — do not saturate"
            if new_truths > 0:
                family.saturated = False
                return "transfer absorbed — keep arm for other companions"
            recs = {s for s, _ in self._archive.list_recs()} if self._archive else set()
            if {"fib", "lucas", "pell"} <= recs:
                family.saturated = True
                if sch:
                    sch.saturated = True
                return "all sequences have rec — saturate transfer"
            return "priors remain for other targets — stay active"

        # Sync param growth into schema language
        if sch and not sch.dead_end:
            if family.param < family.param_max:
                old = family.param
                bump = 2 if "modular" in family.id else 1
                family.param = min(family.param_max, family.param + bump)
                if "linrec" in sch.id or family.id == "linear_recurrences":
                    sch.order = family.param
                elif "modperiod" in sch.id or family.id == "modular_periods":
                    sch.m_max = family.param
                elif "bilinear" in sch.id:
                    sch.r_max = family.param
                sch.n_visits = family.n_visits
                sch.total_reward = family.total_reward
                if new_truths == 0:
                    sch.ticks_no_new_type += 1
                else:
                    sch.ticks_no_new_type = 0
                self.persist_skin()
                return f"expand schema param {old}→{family.param}"
            family.saturated = True
            sch.saturated = True
            sch.n_visits = family.n_visits
            sch.total_reward = family.total_reward
            self.persist_skin()
            return "param at max → saturate schema"
        return super().expand_family(family, new_truths)

    def transfer_prior(self, archive_confirmed: dict) -> list[Conjecture]:
        fam = self.families().get("transfer_recurrence")
        if not fam:
            return []
        return self.hypothesize(fam, archive_confirmed, step=-1)

    def sync_arms_from_language(self, arms: dict) -> None:
        """After molt: ensure kernel arms reflect newly unlocked schemas."""
        from ..kernel import arm_key

        for fam_id, fam in self.families().items():
            key = arm_key(self.name, fam_id)
            if key not in arms:
                arms[key] = fam
            else:
                # Unlock / unsaturate if language says so
                arms[key].unlocked = fam.unlocked
                if not fam.saturated:
                    arms[key].saturated = False
                arms[key].param = fam.param
                arms[key].param_max = fam.param_max
