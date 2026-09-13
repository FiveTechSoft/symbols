"""
Chance world — tiny Bayes, log-odds additivity, entropy inequalities.

Distributions are GENERATED (not downloaded). Critic = numeric identity
within epsilon. Dead-end family required. Extrapolate to more outcomes.
COMPARE: additivity form as prior on larger tables after small-table verify.
"""

from __future__ import annotations

import math
import random
from typing import Any, Optional

from .base import Conjecture, FamilySpec, VerifiedFact, WorldBase
from .science_lang import ScienceLanguage

EPS = 1e-9


def _normalize(xs: list[float]) -> list[float]:
    s = sum(xs)
    if s <= 0:
        n = len(xs)
        return [1.0 / n] * n
    return [x / s for x in xs]


def _gen_dist(k: int, seed: int) -> list[float]:
    rng = random.Random(seed)
    raw = [rng.random() + 0.05 for _ in range(k)]
    return _normalize(raw)


def _entropy(p: list[float]) -> float:
    h = 0.0
    for x in p:
        if x > 0:
            h -= x * math.log(x, 2)
    return h


def _logit(p: float) -> float:
    p = min(max(p, 1e-12), 1 - 1e-12)
    return math.log(p / (1 - p))


def _gen_bayes_table(seed: int) -> dict[str, float]:
    """Generate a coherent 2×2 generative Bayes scenario (not a canned theorem string)."""
    rng = random.Random(seed)
    p_h = rng.uniform(0.15, 0.85)
    p_e_h = rng.uniform(0.2, 0.95)
    p_e_nh = rng.uniform(0.05, 0.8)
    p_e = p_e_h * p_h + p_e_nh * (1 - p_h)
    p_h_e = (p_e_h * p_h) / p_e if p_e > 0 else 0.0
    return {
        "p_h": p_h,
        "p_e_h": p_e_h,
        "p_e_nh": p_e_nh,
        "p_e": p_e,
        "p_h_e": p_h_e,
    }


class ChanceWorld(WorldBase):
    name = "chance"

    def __init__(self, language: Optional[ScienceLanguage] = None) -> None:
        self.language = language or ScienceLanguage.seed_for("chance")
        self._archive = None
        self._critic = None
        self.compare_log: list[dict] = []
        self._accepted_identities: list[dict] = []  # for extrapolate

    def bind(self, archive, critic) -> None:
        self._archive = archive
        self._critic = critic
        skin = (archive.meta or {}).get("science_skins", {}).get("chance")
        if skin and "schemas" in skin:
            try:
                from .schema_lang import SchemaClass

                self.language = ScienceLanguage(
                    schemas={k: SchemaClass.from_dict(v) for k, v in skin["schemas"].items()},
                    generation=int(skin.get("generation", 0)),
                    molt_history=list(skin.get("molt_history", [])),
                    world_tag="chance",
                )
                self.language.ensure_novelty_alive()
            except Exception:
                pass
        self._accepted_identities = list(
            (archive.meta or {}).get("chance_accepted", [])
        )

    def persist_skin(self) -> None:
        if self._archive is None:
            return
        skins = self._archive.meta.setdefault("science_skins", {})
        skins["chance"] = self.language.snapshot()
        self._archive.meta["chance_accepted"] = self._accepted_identities[-50:]
        if hasattr(self._archive, "save_meta"):
            self._archive.save_meta()
        else:
            self._archive.save()

    def families(self) -> dict[str, FamilySpec]:
        out: dict[str, FamilySpec] = {}
        for i, (sid, sch) in enumerate(self.language.schemas.items()):
            out[sid] = FamilySpec(
                id=sid,
                description=sch.description,
                dead_end=sch.dead_end,
                param=sch.order,
                param_max=sch.order_max,
                unlocked=sch.unlocked,
                unlock_order=i,
                saturated=sch.saturated,
                n_visits=sch.n_visits,
                total_reward=sch.total_reward,
            )
        return out

    def observe(self) -> dict[str, Any]:
        return {
            "eps": EPS,
            "n_schema_classes": self.language.n_schema_classes(),
            "spawned": self.language.spawned_ids(),
            "n_accepted_identities": len(self._accepted_identities),
            "generation": self.language.generation,
        }

    def hypothesize(self, family: FamilySpec, archive_confirmed: dict, step: int) -> list[Conjecture]:
        out: list[Conjecture] = []
        fid = family.id
        k = max(2, family.param)
        seed = 1000 + step * 17 + k

        if "bayes" in fid:
            for i in range(3):
                table = _gen_bayes_table(seed + i)
                out.append(
                    Conjecture(
                        name=f"chance_bayes_id_s{seed + i}",
                        family=fid,
                        world=self.name,
                        formula="P(H|E)=P(E|H)P(H)/P(E) on generated 2x2",
                        payload={"kind": "bayes_id", "table": table, "schema": fid, "order": k},
                        relation_type="bayes_identity",
                    )
                )
                out.append(
                    Conjecture(
                        name=f"chance_logodds_add_s{seed + i}",
                        family=fid,
                        world=self.name,
                        formula="logit(post)=logit(prior)+log(LR) on generated table",
                        payload={"kind": "logodds_add", "table": table, "schema": fid, "order": k},
                        relation_type="logodds_additivity",
                    )
                )
            # broken numeric claim
            out.append(
                Conjecture(
                    name=f"chance_bayes_swap_s{seed}",
                    family=fid,
                    world=self.name,
                    formula="P(H|E)=P(H|E^c) (broken)",
                    payload={"kind": "bayes_swap_broken", "table": _gen_bayes_table(seed), "schema": fid},
                    relation_type="bayes_neg",
                )
            )

        elif "entropy" in fid:
            for i in range(3):
                dist = _gen_dist(k, seed + i)
                out.append(
                    Conjecture(
                        name=f"chance_H_nonneg_k{k}_s{seed + i}",
                        family=fid,
                        world=self.name,
                        formula=f"H(p)≥0 on generated {k}-outcome dist",
                        payload={"kind": "H_nonneg", "dist": dist, "schema": fid, "order": k},
                        relation_type="entropy_ineq",
                    )
                )
                out.append(
                    Conjecture(
                        name=f"chance_H_le_logk_k{k}_s{seed + i}",
                        family=fid,
                        world=self.name,
                        formula=f"H(p)≤log2(k) on generated {k}-outcome dist",
                        payload={"kind": "H_le_logk", "dist": dist, "k": k, "schema": fid, "order": k},
                        relation_type="entropy_ineq",
                    )
                )
            # uniform maximizes among same support (check vs a peaked dist)
            uni = [1.0 / k] * k
            peaked = _normalize([10.0] + [0.1] * (k - 1))
            out.append(
                Conjecture(
                    name=f"chance_H_uni_max_k{k}_s{seed}",
                    family=fid,
                    world=self.name,
                    formula=f"H(uniform)≥H(peaked) for k={k}",
                    payload={
                        "kind": "H_uni_ge_peaked",
                        "uni": uni,
                        "peaked": peaked,
                        "schema": fid,
                        "order": k,
                    },
                    relation_type="entropy_ineq",
                )
            )

        elif "dead_negH" in fid or fid.startswith("chance_dead"):
            dist = _gen_dist(max(2, k), seed)
            out.append(
                Conjecture(
                    name="NEG_chance_H_negative",
                    family=fid,
                    world=self.name,
                    formula="H(p)<0 on a valid probability vector",
                    payload={"kind": "dead_negH", "dist": dist, "schema": fid},
                    relation_type="dead_end:negH",
                )
            )
            # mixing increases entropy typically — claim opposite
            out.append(
                Conjecture(
                    name="NEG_chance_mix_decreases_H",
                    family=fid,
                    world=self.name,
                    formula="H((p+u)/2) < min(H(p),H(u)) always",
                    payload={
                        "kind": "dead_mix_dec",
                        "dist": dist,
                        "uni": [1.0 / len(dist)] * len(dist),
                        "schema": fid,
                    },
                    relation_type="dead_end:mix",
                )
            )

        elif "extrapolate" in fid:
            if not self._accepted_identities:
                out.append(
                    Conjecture(
                        name="chance_extrap_wait",
                        family=fid,
                        world=self.name,
                        formula="no accepted identity yet — honest wait",
                        payload={"kind": "extrap_empty", "schema": fid},
                        relation_type="extrapolate_miss",
                    )
                )
            for ident in self._accepted_identities[-4:]:
                # re-test same identity form on larger generated tables
                bigger_k = max(k, ident.get("order", 2) + 1)
                out.append(
                    Conjecture(
                        name=f"extrap_{ident.get('kind', 'id')}_k{bigger_k}",
                        family=fid,
                        world=self.name,
                        formula=f"EXTRAPOLATE {ident.get('kind')} to k={bigger_k}",
                        payload={
                            "kind": "extrap_replay",
                            "src_kind": ident.get("kind"),
                            "order": bigger_k,
                            "schema": fid,
                            "seed": seed + bigger_k,
                        },
                        relation_type="extrapolate",
                        from_transfer=True,
                        transfer_source=f"chance:{ident.get('kind')}",
                    )
                )
        return out

    def verify(self, conjecture: Conjecture) -> VerifiedFact:
        p = conjecture.payload
        kind = p.get("kind")
        ok = False
        cex: Optional[str] = None
        support = ""

        if kind == "bayes_id":
            t = p["table"]
            pred = (t["p_e_h"] * t["p_h"]) / t["p_e"] if t["p_e"] > 0 else float("nan")
            ok = abs(pred - t["p_h_e"]) < 1e-9
            cex = None if ok else f"pred={pred} != {t['p_h_e']}"
            support = f"numeric id eps<{1e-9}" if ok else "finite numeric fail"
            if ok:
                self._note_accepted("bayes_id", p.get("order", 2))

        elif kind == "logodds_add":
            t = p["table"]
            prior = t["p_h"]
            post = t["p_h_e"]
            lr = (t["p_e_h"] / t["p_e_nh"]) if t["p_e_nh"] > 0 else float("inf")
            if not math.isfinite(lr) or lr <= 0:
                ok = False
                cex = "bad LR"
            else:
                lhs = _logit(post)
                rhs = _logit(prior) + math.log(lr)
                ok = abs(lhs - rhs) < 1e-6
                cex = None if ok else f"logit {lhs} != {rhs}"
            support = "log-odds additivity eps<1e-6" if ok else "numeric fail"
            if ok:
                self._note_accepted("logodds_add", p.get("order", 2))

        elif kind == "bayes_swap_broken":
            t = p["table"]
            # P(H|E) == P(H|not E)? almost never
            p_h = t["p_h"]
            p_e = t["p_e"]
            p_h_e = t["p_h_e"]
            # P(H|¬E) = (1-P(E|H))P(H) / (1-P(E))
            p_ne = 1 - p_e
            p_h_ne = ((1 - t["p_e_h"]) * p_h) / p_ne if p_ne > 0 else float("nan")
            ok = abs(p_h_e - p_h_ne) < 1e-9
            cex = None if ok else f"P(H|E)={p_h_e} != P(H|¬E)={p_h_ne}"
            support = "unexpected equality" if ok else "finite numeric fail"

        elif kind == "H_nonneg":
            h = _entropy(p["dist"])
            ok = h >= -EPS
            cex = None if ok else f"H={h}"
            support = f"H={h:.6f}≥0" if ok else "finite fail"
            if ok:
                self._note_accepted("H_nonneg", p.get("order", 2))

        elif kind == "H_le_logk":
            h = _entropy(p["dist"])
            bound = math.log(p["k"], 2)
            ok = h <= bound + 1e-9
            cex = None if ok else f"H={h} > log2(k)={bound}"
            support = f"H={h:.6f}≤log2({p['k']})" if ok else "finite fail"
            if ok:
                self._note_accepted("H_le_logk", p.get("order", 2))

        elif kind == "H_uni_ge_peaked":
            hu = _entropy(p["uni"])
            hp = _entropy(p["peaked"])
            ok = hu + 1e-9 >= hp
            cex = None if ok else f"H(uni)={hu} < H(peaked)={hp}"
            support = f"H(uni)={hu:.6f}≥H(peaked)={hp:.6f}" if ok else "finite fail"
            if ok:
                self._note_accepted("H_uni_ge_peaked", p.get("order", 2))

        elif kind == "dead_negH":
            h = _entropy(p["dist"])
            ok = h < 0  # claim — should FAIL for valid dist
            cex = None if ok else f"H={h} not < 0"
            support = "unexpected" if ok else "finite fail (as required for dead-end)"

        elif kind == "dead_mix_dec":
            dist, uni = p["dist"], p["uni"]
            mix = _normalize([0.5 * (a + b) for a, b in zip(dist, uni)])
            hm = _entropy(mix)
            floor = min(_entropy(dist), _entropy(uni))
            ok = hm < floor  # claim always — typically FALSE (mixing raises H)
            cex = None if ok else f"H(mix)={hm} >= min={floor}"
            support = "unexpected" if ok else "finite fail"

        elif kind == "extrap_empty":
            ok = False
            cex = "no prior identity"
            support = "honest miss"

        elif kind == "extrap_replay":
            src = p.get("src_kind")
            order = int(p.get("order", 3))
            seed = int(p.get("seed", 0))
            if src in ("H_nonneg", "H_le_logk", "H_uni_ge_peaked"):
                dist = _gen_dist(order, seed)
                h = _entropy(dist)
                if src == "H_nonneg":
                    ok = h >= -EPS
                elif src == "H_le_logk":
                    ok = h <= math.log(order, 2) + 1e-9
                else:
                    uni = [1.0 / order] * order
                    peaked = _normalize([10.0] + [0.1] * (order - 1))
                    ok = _entropy(uni) + 1e-9 >= _entropy(peaked)
                support = f"EXTRAPOLATE {src} k={order}" if ok else "extrap fail"
                cex = None if ok else support
                self.compare_log.append({"hit": ok, "kind": "extrap", "src": src, "k": order})
            elif src in ("bayes_id", "logodds_add"):
                t = _gen_bayes_table(seed)
                if src == "bayes_id":
                    pred = (t["p_e_h"] * t["p_h"]) / t["p_e"]
                    ok = abs(pred - t["p_h_e"]) < 1e-9
                else:
                    lr = t["p_e_h"] / t["p_e_nh"]
                    ok = abs(_logit(t["p_h_e"]) - (_logit(t["p_h"]) + math.log(lr))) < 1e-6
                support = f"EXTRAPOLATE {src} new table" if ok else "extrap fail"
                cex = None if ok else support
                self.compare_log.append({"hit": ok, "kind": "extrap", "src": src})
            else:
                ok = False
                cex = f"unknown src {src}"
        else:
            ok = False
            cex = f"unknown kind {kind}"

        self.persist_skin()
        return VerifiedFact(
            name=conjecture.name,
            family=conjecture.family,
            world=self.name,
            formula=conjecture.formula,
            true=ok,
            support=support or ("ok" if ok else "fail"),
            counterexample=cex,
            relation_type=conjecture.relation_type,
            from_transfer=conjecture.from_transfer,
            transfer_source=conjecture.transfer_source,
        )

    def _note_accepted(self, kind: str, order: int) -> None:
        self._accepted_identities.append({"kind": kind, "order": order})

    def expand_family(self, family: FamilySpec, new_truths: int) -> str:
        sch = self.language.schemas.get(family.id)
        if family.dead_end:
            family.saturated = True
            if sch:
                sch.saturated = True
            return "dead-end → saturate (curiosity tax)"
        if family.param < family.param_max:
            old = family.param
            family.param = min(family.param_max, family.param + 1)
            if sch:
                sch.order = family.param
                if new_truths == 0:
                    sch.ticks_no_new_type += 1
                else:
                    sch.ticks_no_new_type = 0
            self.language.ensure_novelty_alive()
            self.persist_skin()
            return f"extrapolate k {old}→{family.param}"
        family.saturated = True
        if sch:
            sch.saturated = True
        self.persist_skin()
        return "param at max → saturate (molt/spawn — no done)"

    def sync_arms(self, arms: dict) -> None:
        from ..kernel import arm_key

        for fam_id, fam in self.families().items():
            key = arm_key(self.name, fam_id)
            if key not in arms:
                arms[key] = fam
            else:
                arms[key].unlocked = fam.unlocked
                arms[key].param = fam.param
                arms[key].param_max = fam.param_max
                if not fam.saturated:
                    arms[key].saturated = False
                else:
                    arms[key].saturated = fam.saturated
