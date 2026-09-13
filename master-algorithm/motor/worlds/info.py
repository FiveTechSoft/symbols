"""
Info world — mutual information / independence on tiny joints.

XOR/AND live in logic.py — reuse via bit_fn transfer; do not re-learn them here.
COMPARE: archived bit_fn(parity|xor2|and_all) → MI structure on matching joints.
"""

from __future__ import annotations

import itertools
import math
import random
from typing import Any, Optional

from .base import Conjecture, FamilySpec, VerifiedFact, WorldBase
from .science_lang import ScienceLanguage

EPS = 1e-9


def _entropy_from_probs(ps: list[float]) -> float:
    h = 0.0
    for p in ps:
        if p > 0:
            h -= p * math.log(p, 2)
    return h


def _joint_from_counts(counts: dict[tuple[int, int], int]) -> dict[tuple[int, int], float]:
    total = sum(counts.values()) or 1
    return {k: v / total for k, v in counts.items()}


def _marginals(joint: dict[tuple[int, int], float]):
    px: dict[int, float] = {}
    py: dict[int, float] = {}
    for (x, y), p in joint.items():
        px[x] = px.get(x, 0.0) + p
        py[y] = py.get(y, 0.0) + p
    return px, py


def _mi(joint: dict[tuple[int, int], float]) -> float:
    px, py = _marginals(joint)
    mi = 0.0
    for (x, y), pxy in joint.items():
        if pxy <= 0:
            continue
        qx, qy = px.get(x, 0.0), py.get(y, 0.0)
        if qx <= 0 or qy <= 0:
            continue
        mi += pxy * math.log(pxy / (qx * qy), 2)
    return mi


def _gen_independent_joint(seed: int) -> dict[tuple[int, int], float]:
    rng = random.Random(seed)
    px1 = rng.uniform(0.2, 0.8)
    py1 = rng.uniform(0.2, 0.8)
    px = {0: 1 - px1, 1: px1}
    py = {0: 1 - py1, 1: py1}
    return {(x, y): px[x] * py[y] for x in (0, 1) for y in (0, 1)}


def _gen_xor_coupled_joint(seed: int) -> dict[tuple[int, int], float]:
    """Y = X xor B with B biased noise — dependence."""
    rng = random.Random(seed)
    # Pure XOR coupling on uniform X: Y=X or Y=1-X with high prob of equality noise
    # Simpler: support only on y = x (deterministic dependence) mixed with independent
    p_dep = rng.uniform(0.7, 0.95)
    indep = _gen_independent_joint(seed + 1)
    dep = {(0, 0): 0.5, (1, 1): 0.5, (0, 1): 0.0, (1, 0): 0.0}
    return {
        k: p_dep * dep.get(k, 0.0) + (1 - p_dep) * indep.get(k, 0.0)
        for k in [(0, 0), (0, 1), (1, 0), (1, 1)]
    }


def _gen_and_related_counts() -> dict[tuple[int, int], int]:
    # Enumerate all (x,y) with z=x AND y observed as pairing (x, z) style — use (x,y) where we
    # count inputs to AND: joint over two bits that feed and_all.
    counts: dict[tuple[int, int], int] = {(0, 0): 0, (0, 1): 0, (1, 0): 0, (1, 1): 0}
    for x, y in itertools.product([0, 1], repeat=2):
        # couple: observe (x, x&y)
        counts[(x, x & y)] = counts.get((x, x & y), 0) + 1
    return counts


class InfoWorld(WorldBase):
    name = "info"

    def __init__(self, language: Optional[ScienceLanguage] = None) -> None:
        self.language = language or ScienceLanguage.seed_for("info")
        self._archive = None
        self._critic = None
        self.compare_log: list[dict] = []

    def bind(self, archive, critic) -> None:
        self._archive = archive
        self._critic = critic
        skin = (archive.meta or {}).get("science_skins", {}).get("info")
        if skin and "schemas" in skin:
            try:
                from .schema_lang import SchemaClass

                self.language = ScienceLanguage(
                    schemas={k: SchemaClass.from_dict(v) for k, v in skin["schemas"].items()},
                    generation=int(skin.get("generation", 0)),
                    molt_history=list(skin.get("molt_history", [])),
                    world_tag="info",
                )
                self.language.ensure_novelty_alive()
            except Exception:
                pass

    def persist_skin(self) -> None:
        if self._archive is None:
            return
        skins = self._archive.meta.setdefault("science_skins", {})
        skins["info"] = self.language.snapshot()
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
            "n_schema_classes": self.language.n_schema_classes(),
            "spawned": self.language.spawned_ids(),
            "generation": self.language.generation,
            "compare_log_len": len(self.compare_log),
            "note": "XOR/AND not re-learned; transferred from logic bit_fn when present",
        }

    def _archived_bit_fns(self) -> list[tuple[str, str, str]]:
        if self._archive is None:
            return []
        if hasattr(self._archive, "list_bit_fns"):
            return self._archive.list_bit_fns()
        import re

        out = []
        for ln in getattr(self._archive, "_lines", []):
            m = re.match(r"bit_fn\('([^']+)',\s*(\w+),\s*(\w+)\)\.", ln)
            if m:
                out.append((m.group(1), m.group(2), m.group(3)))
        return out

    def hypothesize(self, family: FamilySpec, archive_confirmed: dict, step: int) -> list[Conjecture]:
        out: list[Conjecture] = []
        fid = family.id
        order = max(2, family.param)
        seed = 2000 + step * 13 + order

        if "mi_scan" in fid or fid.startswith("info_mi"):
            indep = _gen_independent_joint(seed)
            dep = _gen_xor_coupled_joint(seed + 3)
            out.append(
                Conjecture(
                    name=f"info_MI_indep_near0_s{seed}",
                    family=fid,
                    world=self.name,
                    formula="MI(X;Y)≈0 on generated independent joint",
                    payload={"kind": "mi_indep0", "joint": {f"{a},{b}": v for (a, b), v in indep.items()},
                             "schema": fid, "order": order},
                    relation_type="mutual_info",
                )
            )
            out.append(
                Conjecture(
                    name=f"info_MI_dep_pos_s{seed}",
                    family=fid,
                    world=self.name,
                    formula="MI(X;Y)>0 on xor-coupled joint",
                    payload={"kind": "mi_dep_pos", "joint": {f"{a},{b}": v for (a, b), v in dep.items()},
                             "schema": fid, "order": order},
                    relation_type="mutual_info",
                )
            )
            out.append(
                Conjecture(
                    name=f"info_indep_factorization_s{seed}",
                    family=fid,
                    world=self.name,
                    formula="P(x,y)=P(x)P(y) on independent joint (eps)",
                    payload={"kind": "indep_factor", "joint": {f"{a},{b}": v for (a, b), v in indep.items()},
                             "schema": fid, "order": order},
                    relation_type="independence",
                )
            )
            # AND-related joint from finite enumeration (reuse logic target structure, not duplicate learning)
            and_joint = _joint_from_counts(_gen_and_related_counts())
            out.append(
                Conjecture(
                    name=f"info_MI_and_related_s{seed}",
                    family=fid,
                    world=self.name,
                    formula="MI>0 on (X, X∧Y) joint from full 2-bit table",
                    payload={
                        "kind": "mi_and_related",
                        "joint": {f"{a},{b}": v for (a, b), v in and_joint.items()},
                        "schema": fid,
                        "order": order,
                    },
                    relation_type="mutual_info",
                )
            )

        elif "transfer_bitfn" in fid or "transfer" in fid:
            bit_fns = self._archived_bit_fns()
            if not bit_fns:
                out.append(
                    Conjecture(
                        name="info_transfer_wait_no_bitfn",
                        family=fid,
                        world=self.name,
                        formula="no bit_fn prior — honest miss",
                        payload={"kind": "transfer_empty", "schema": fid},
                        relation_type="compare_miss",
                        from_transfer=True,
                        transfer_source="logic",
                    )
                )
            for name, target, hyp in bit_fns:
                if hyp in ("parity", "xor2"):
                    dep = _gen_xor_coupled_joint(hash(name) % 10000)
                    out.append(
                        Conjecture(
                            name=f"transfer_bitfn_{hyp}_{target}_to_MI",
                            family=fid,
                            world=self.name,
                            formula=f"TRANSFER bit_fn({target}={hyp}) ⇒ MI>0 on xor-coupled joint",
                            payload={
                                "kind": "transfer_xor_mi",
                                "src_hyp": hyp,
                                "src_target": target,
                                "src_name": name,
                                "joint": {f"{a},{b}": v for (a, b), v in dep.items()},
                                "schema": fid,
                            },
                            relation_type="compare_transfer",
                            from_transfer=True,
                            transfer_source=f"logic:{target}",
                        )
                    )
                elif hyp == "and_all":
                    and_joint = _joint_from_counts(_gen_and_related_counts())
                    out.append(
                        Conjecture(
                            name=f"transfer_bitfn_and_all_{target}_to_MI",
                            family=fid,
                            world=self.name,
                            formula=f"TRANSFER bit_fn({target}=and_all) ⇒ MI>0 on (X,X∧Y)",
                            payload={
                                "kind": "transfer_and_mi",
                                "src_hyp": hyp,
                                "src_target": target,
                                "src_name": name,
                                "joint": {f"{a},{b}": v for (a, b), v in and_joint.items()},
                                "schema": fid,
                            },
                            relation_type="compare_transfer",
                            from_transfer=True,
                            transfer_source=f"logic:{target}",
                        )
                    )

        elif "dead_mi0" in fid or fid.startswith("info_dead"):
            dep = _gen_xor_coupled_joint(seed)
            out.append(
                Conjecture(
                    name="NEG_info_MI_always_0",
                    family=fid,
                    world=self.name,
                    formula="MI(X;Y)=0 on xor-coupled joint",
                    payload={
                        "kind": "dead_mi0",
                        "joint": {f"{a},{b}": v for (a, b), v in dep.items()},
                        "schema": fid,
                    },
                    relation_type="dead_end:mi0",
                )
            )
        return out

    @staticmethod
    def _parse_joint(d: dict) -> dict[tuple[int, int], float]:
        out = {}
        for k, v in d.items():
            if isinstance(k, tuple):
                out[k] = float(v)
            else:
                a, b = str(k).split(",")
                out[(int(a), int(b))] = float(v)
        return out

    def verify(self, conjecture: Conjecture) -> VerifiedFact:
        p = conjecture.payload
        kind = p.get("kind")
        ok = False
        cex: Optional[str] = None
        support = ""

        if kind in ("mi_indep0", "mi_dep_pos", "mi_and_related", "indep_factor",
                    "transfer_xor_mi", "transfer_and_mi", "dead_mi0"):
            joint = self._parse_joint(p["joint"])
            mi = _mi(joint)
            if kind == "mi_indep0":
                ok = abs(mi) < 1e-6
                cex = None if ok else f"MI={mi}"
                support = f"MI={mi:.8f}≈0" if ok else "finite fail"
            elif kind in ("mi_dep_pos", "mi_and_related", "transfer_xor_mi", "transfer_and_mi"):
                ok = mi > 1e-6
                cex = None if ok else f"MI={mi} not >0"
                support = f"MI={mi:.6f}>0" if ok else "finite fail"
                if kind.startswith("transfer"):
                    self.compare_log.append(
                        {"hit": ok, "src": p.get("src_name"), "hyp": p.get("src_hyp"), "mi": mi}
                    )
            elif kind == "indep_factor":
                px, py = _marginals(joint)
                ok = True
                for (x, y), pxy in joint.items():
                    pred = px.get(x, 0.0) * py.get(y, 0.0)
                    if abs(pxy - pred) > 1e-9:
                        ok = False
                        cex = f"P({x},{y})={pxy} != {pred}"
                        break
                support = "factorization eps" if ok else "finite fail"
            elif kind == "dead_mi0":
                ok = abs(mi) < 1e-6  # claim on dependent joint — should fail
                cex = None if ok else f"MI={mi} ≠ 0"
                support = "unexpected" if ok else "finite fail (dead-end)"

        elif kind == "transfer_empty":
            ok = False
            cex = "no bit_fn in archive"
            support = "honest miss"
            self.compare_log.append({"hit": False, "reason": cex})
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
            return f"extrapolate order {old}→{family.param}"
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

    def transfer_prior(self, archive_confirmed: dict) -> list[Conjecture]:
        for fid, fam in self.families().items():
            if "transfer" in fid and fam.unlocked:
                return self.hypothesize(fam, archive_confirmed, step=-1)
        return []
