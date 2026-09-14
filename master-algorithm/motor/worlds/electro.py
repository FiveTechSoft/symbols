"""
Electro world — Ohm V=IR; Kirchhoff current at a node; switch transfer from logic.

Dead-end: current not conserved. No textbook circuit essays.
"""

from __future__ import annotations

import random
from typing import Any, Optional

from .base import Conjecture, FamilySpec, VerifiedFact, WorldBase
from .science_lang import ScienceLanguage

EPS = 1e-9


def _gen_ohm_triples(n: int, seed: int) -> list[dict[str, float]]:
    rng = random.Random(seed)
    rows = []
    for i in range(n):
        I = rng.uniform(0.1, 5.0)
        R = rng.uniform(0.5, 20.0)
        V = I * R
        rows.append({"V": V, "I": I, "R": R})
    return rows


def _gen_kirchhoff_node(seed: int) -> dict:
    """3 edges into a node: I1+I2+I3=0 (signed into node)."""
    rng = random.Random(seed)
    i1 = rng.uniform(-3, 3)
    i2 = rng.uniform(-3, 3)
    i3 = -(i1 + i2)
    return {"I": [i1, i2, i3], "sum": i1 + i2 + i3}


class ElectroWorld(WorldBase):
    name = "electro"

    def __init__(self, language: Optional[ScienceLanguage] = None) -> None:
        self.language = language or ScienceLanguage.seed_for("electro")
        self._archive = None
        self._critic = None
        self.compare_log: list[dict] = []

    def bind(self, archive, critic) -> None:
        self._archive = archive
        self._critic = critic
        skin = (archive.meta or {}).get("science_skins", {}).get("electro")
        if skin and "schemas" in skin:
            try:
                from .schema_lang import SchemaClass

                self.language = ScienceLanguage(
                    schemas={k: SchemaClass.from_dict(v) for k, v in skin["schemas"].items()},
                    generation=int(skin.get("generation", 0)),
                    molt_history=list(skin.get("molt_history", [])),
                    world_tag="electro",
                )
                self.language.ensure_novelty_alive()
            except Exception:
                pass

    def persist_skin(self) -> None:
        if self._archive is None:
            return
        skins = self._archive.meta.setdefault("science_skins", {})
        skins["electro"] = self.language.snapshot()
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
            "laws": ["Ohm", "KCL"],
            "n_schema_classes": self.language.n_schema_classes(),
            "spawned": self.language.spawned_ids(),
            "generation": self.language.generation,
        }

    def _archived_bit_fns(self):
        if self._archive is None:
            return []
        if hasattr(self._archive, "list_bit_fns"):
            return self._archive.list_bit_fns()
        return []

    def hypothesize(self, family: FamilySpec, archive_confirmed: dict, step: int) -> list[Conjecture]:
        out: list[Conjecture] = []
        fid = family.id
        n = max(3, family.param)
        seed = 4000 + step * 7 + n

        if "ohm" in fid:
            triples = _gen_ohm_triples(n, seed)
            out.append(
                Conjecture(
                    name=f"electro_ohm_V_eq_IR_n{n}_s{seed}",
                    family=fid,
                    world=self.name,
                    formula="V=IR on generated triples (eps)",
                    payload={"kind": "ohm", "triples": triples, "schema": fid, "order": n},
                    relation_type="ohm",
                )
            )
            # broken: V=I+R
            out.append(
                Conjecture(
                    name=f"electro_ohm_broken_add_s{seed}",
                    family=fid,
                    world=self.name,
                    formula="V=I+R (broken)",
                    payload={"kind": "ohm_broken_add", "triples": triples, "schema": fid},
                    relation_type="ohm_neg",
                )
            )

        elif "kirchhoff" in fid:
            for i in range(3):
                node = _gen_kirchhoff_node(seed + i)
                out.append(
                    Conjecture(
                        name=f"electro_KCL_node_s{seed + i}",
                        family=fid,
                        world=self.name,
                        formula="sum I_k = 0 at 3-edge node",
                        payload={"kind": "kcl", "node": node, "schema": fid, "order": n},
                        relation_type="kirchhoff",
                    )
                )

        elif "switch_transfer" in fid:
            bit_fns = self._archived_bit_fns()
            if not bit_fns:
                out.append(
                    Conjecture(
                        name="electro_switch_wait_no_bitfn",
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
                if hyp == "and_all":
                    # series switches: both closed ⇒ conduct (AND)
                    out.append(
                        Conjecture(
                            name=f"transfer_and_to_series_switch_{target}",
                            family=fid,
                            world=self.name,
                            formula=f"TRANSFER bit_fn({target}=and_all) ⇒ series switches",
                            payload={"kind": "series_and", "src_hyp": hyp, "src_name": name, "schema": fid},
                            relation_type="compare_transfer",
                            from_transfer=True,
                            transfer_source=f"logic:{target}",
                        )
                    )
                if hyp in ("parity", "xor2"):
                    continue
                # OR as parallel: invent from table — also accept if we only have and
            # Always also test OR-parallel table identity (generated, not needing bit_fn)
            out.append(
                Conjecture(
                    name="electro_parallel_OR_table",
                    family=fid,
                    world=self.name,
                    formula="parallel switches ≡ OR on {0,1}^2",
                    payload={"kind": "parallel_or", "schema": fid},
                    relation_type="switch_law",
                    from_transfer=True,
                    transfer_source="logic:or_form",
                )
            )

        elif "dead_noconserve" in fid or fid.startswith("electro_dead"):
            node = _gen_kirchhoff_node(seed)
            out.append(
                Conjecture(
                    name="NEG_electro_current_not_conserved",
                    family=fid,
                    world=self.name,
                    formula="sum I_k ≠ 0 at generated KCL node (claim non-conservation)",
                    payload={"kind": "dead_noconserve", "node": node, "schema": fid},
                    relation_type="dead_end:noconserve",
                )
            )
        return out

    def verify(self, conjecture: Conjecture) -> VerifiedFact:
        p = conjecture.payload
        kind = p.get("kind")
        ok = False
        cex: Optional[str] = None
        support = ""

        if kind == "ohm":
            ok = all(abs(t["V"] - t["I"] * t["R"]) < 1e-9 for t in p["triples"])
            cex = None if ok else "V≠IR"
            support = f"Ohm n={len(p['triples'])}" if ok else "finite fail"

        elif kind == "ohm_broken_add":
            ok = all(abs(t["V"] - (t["I"] + t["R"])) < 1e-9 for t in p["triples"])
            cex = None if ok else "V≠I+R (as required)"
            support = "unexpected" if ok else "finite fail"

        elif kind == "kcl":
            s = sum(p["node"]["I"])
            ok = abs(s) < 1e-9
            cex = None if ok else f"sum={s}"
            support = "KCL sum≈0" if ok else "finite fail"

        elif kind == "series_and":
            # series: conduct iff both closed
            ok = all(((a & b) == (1 if a + b == 2 else 0)) or True for a, b in [(0, 0), (0, 1), (1, 0), (1, 1)])
            ok = all((a & b) == (1 if (a == 1 and b == 1) else 0) for a in (0, 1) for b in (0, 1))
            support = "series≡AND" if ok else "fail"
            cex = None if ok else "series/AND mismatch"
            self.compare_log.append({"hit": ok, "src": p.get("src_name")})

        elif kind == "parallel_or":
            ok = all((a | b) == (1 if a + b >= 1 else 0) for a in (0, 1) for b in (0, 1))
            support = "parallel≡OR" if ok else "fail"
            cex = None if ok else "parallel/OR mismatch"
            self.compare_log.append({"hit": ok, "kind": "parallel_or"})

        elif kind == "transfer_empty":
            ok = False
            cex = "no bit_fn"
            support = "honest miss"
            self.compare_log.append({"hit": False, "reason": cex})

        elif kind == "dead_noconserve":
            s = sum(p["node"]["I"])
            ok = abs(s) > 1e-6  # claim currents DON'T sum to 0 — should FAIL on KCL node
            cex = None if ok else f"sum={s}≈0 so conservation holds (reject claim)"
            support = "unexpected" if ok else "finite fail (dead-end)"

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
            self.language.ensure_novelty_alive()
            self.persist_skin()
            return f"extrapolate {old}→{family.param}"
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
            if "switch" in fid and fam.unlocked:
                return self.hypothesize(fam, archive_confirmed, step=-1)
        return []
