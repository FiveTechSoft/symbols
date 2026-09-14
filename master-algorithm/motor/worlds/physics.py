"""
Physics world — 1D elastic collision (p+E), action=reaction.

Kepler stays in astro.py — do not duplicate. Dead-end: momentum not conserved.
"""

from __future__ import annotations

import random
from typing import Any, Optional

from .base import Conjecture, FamilySpec, VerifiedFact, WorldBase
from .science_lang import ScienceLanguage
from .conserv_form import linear_constraint_holds

EPS = 1e-8


def _elastic_1d(m1: float, m2: float, u1: float, u2: float):
    """Exact 1D elastic final velocities."""
    v1 = ((m1 - m2) / (m1 + m2)) * u1 + (2 * m2 / (m1 + m2)) * u2
    v2 = (2 * m1 / (m1 + m2)) * u1 + ((m2 - m1) / (m1 + m2)) * u2
    return v1, v2


def _gen_collision(seed: int) -> dict:
    rng = random.Random(seed)
    m1 = rng.uniform(0.5, 4.0)
    m2 = rng.uniform(0.5, 4.0)
    u1 = rng.uniform(-3, 3)
    u2 = rng.uniform(-3, 3)
    v1, v2 = _elastic_1d(m1, m2, u1, u2)
    return {"m1": m1, "m2": m2, "u1": u1, "u2": u2, "v1": v1, "v2": v2}


class PhysicsWorld(WorldBase):
    name = "physics"

    def __init__(self, language: Optional[ScienceLanguage] = None) -> None:
        self.language = language or ScienceLanguage.seed_for("physics")
        self._archive = None
        self._critic = None
        self.compare_log: list[dict] = []

    def bind(self, archive, critic) -> None:
        self._archive = archive
        self._critic = critic
        skin = (archive.meta or {}).get("science_skins", {}).get("physics")
        if skin and "schemas" in skin:
            try:
                from .schema_lang import SchemaClass

                self.language = ScienceLanguage(
                    schemas={k: SchemaClass.from_dict(v) for k, v in skin["schemas"].items()},
                    generation=int(skin.get("generation", 0)),
                    molt_history=list(skin.get("molt_history", [])),
                    world_tag="physics",
                )
                self.language.ensure_novelty_alive()
                self.language.merge_missing_seeds()
            except Exception:
                pass
        else:
            # fresh seed language — still merge in case SCIENCE_SEED grew
            if hasattr(self.language, "merge_missing_seeds"):
                self.language.merge_missing_seeds()

    def persist_skin(self) -> None:
        if self._archive is None:
            return
        skins = self._archive.meta.setdefault("science_skins", {})
        skins["physics"] = self.language.snapshot()
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
            "systems": ["1d_elastic", "action_reaction"],
            "n_schema_classes": self.language.n_schema_classes(),
            "spawned": self.language.spawned_ids(),
            "generation": self.language.generation,
            "note": "no Kepler here — astro owns orbits",
        }

    def hypothesize(self, family: FamilySpec, archive_confirmed: dict, step: int) -> list[Conjecture]:
        out: list[Conjecture] = []
        fid = family.id
        n = max(2, family.param)
        seed = 5000 + step * 9 + n

        if "collision" in fid:
            for i in range(n):
                c = _gen_collision(seed + i)
                out.append(
                    Conjecture(
                        name=f"phys_mom_conserve_s{seed + i}",
                        family=fid,
                        world=self.name,
                        formula="m1*u1+m2*u2 = m1*v1+m2*v2 (1D elastic)",
                        payload={"kind": "mom", "c": c, "schema": fid, "order": n},
                        relation_type="collision",
                    )
                )
                out.append(
                    Conjecture(
                        name=f"phys_energy_conserve_s{seed + i}",
                        family=fid,
                        world=self.name,
                        formula="½m1 u1²+½m2 u2² = ½m1 v1²+½m2 v2²",
                        payload={"kind": "energy", "c": c, "schema": fid, "order": n},
                        relation_type="collision",
                    )
                )

        elif "action_reaction" in fid:
            rng = random.Random(seed)
            for i in range(3):
                F = rng.uniform(-10, 10)
                out.append(
                    Conjecture(
                        name=f"phys_action_reaction_s{seed + i}",
                        family=fid,
                        world=self.name,
                        formula="F_12 + F_21 = 0",
                        payload={"kind": "action_reaction", "F12": F, "F21": -F, "schema": fid},
                        relation_type="newton3",
                    )
                )
            # broken: same sign
            F = rng.uniform(1, 5)
            out.append(
                Conjecture(
                    name=f"phys_action_same_sign_broken_s{seed}",
                    family=fid,
                    world=self.name,
                    formula="F_12 = F_21 (broken)",
                    payload={"kind": "action_same", "F12": F, "F21": F, "schema": fid},
                    relation_type="newton3_neg",
                )
            )

        elif "transfer_form" in fid:
            from .chem import _reaction_water
            from .electro import _gen_kirchhoff_node
            c = _gen_collision(8011)
            out.append(
                Conjecture(
                    name="transfer_conserv_atom_shape_to_physics_mom",
                    family=fid,
                    world=self.name,
                    formula="TRANSFER linear-Δ=0 (atom-balance shape) ⇒ momentum conserved",
                    payload={"kind": "conserv_mom", "c": c, "schema": fid, "src_form": "chem:atoms"},
                    relation_type="transfer_form",
                    from_transfer=True,
                    transfer_source="chem:atoms",
                )
            )
            out.append(
                Conjecture(
                    name="transfer_conserv_kcl_shape_to_physics_mom",
                    family=fid,
                    world=self.name,
                    formula="TRANSFER linear-Δ=0 (KCL shape) ⇒ momentum conserved",
                    payload={"kind": "conserv_mom", "c": c, "schema": fid, "src_form": "electro:kcl"},
                    relation_type="transfer_form",
                    from_transfer=True,
                    transfer_source="electro:kcl",
                )
            )
            rx = _reaction_water()
            out.append(
                Conjecture(
                    name="transfer_phys_conserv_form_on_chem_atoms",
                    family=fid,
                    world=self.name,
                    formula="TRANSFER physics mom-Δ=0 form ⇒ try on chem atom balance",
                    payload={"kind": "conserv_chem", "rx": rx, "schema": fid, "src_form": "physics:momentum"},
                    relation_type="transfer_form",
                    from_transfer=True,
                    transfer_source="physics:momentum",
                )
            )
            node = _gen_kirchhoff_node(8012)
            out.append(
                Conjecture(
                    name="transfer_phys_conserv_form_on_electro_kcl",
                    family=fid,
                    world=self.name,
                    formula="TRANSFER physics mom-Δ=0 form ⇒ try on electro KCL",
                    payload={"kind": "conserv_kcl", "node": node, "schema": fid, "src_form": "physics:momentum"},
                    relation_type="transfer_form",
                    from_transfer=True,
                    transfer_source="physics:momentum",
                )
            )

        elif "dead_nomom" in fid or fid.startswith("phys_dead"):
            c = _gen_collision(seed)
            out.append(
                Conjecture(
                    name="NEG_phys_momentum_not_conserved",
                    family=fid,
                    world=self.name,
                    formula="momentum NOT conserved in generated elastic 1D",
                    payload={"kind": "dead_nomom", "c": c, "schema": fid},
                    relation_type="dead_end:nomom",
                )
            )
        return out

    def verify(self, conjecture: Conjecture) -> VerifiedFact:
        p = conjecture.payload
        kind = p.get("kind")
        ok = False
        cex: Optional[str] = None
        support = ""

        if kind == "mom":
            c = p["c"]
            p_in = c["m1"] * c["u1"] + c["m2"] * c["u2"]
            p_out = c["m1"] * c["v1"] + c["m2"] * c["v2"]
            ok = abs(p_in - p_out) < EPS
            cex = None if ok else f"p_in={p_in} p_out={p_out}"
            support = "momentum eps" if ok else "finite fail"

        elif kind == "energy":
            c = p["c"]
            e_in = 0.5 * c["m1"] * c["u1"] ** 2 + 0.5 * c["m2"] * c["u2"] ** 2
            e_out = 0.5 * c["m1"] * c["v1"] ** 2 + 0.5 * c["m2"] * c["v2"] ** 2
            ok = abs(e_in - e_out) < 1e-6
            cex = None if ok else f"E_in={e_in} E_out={e_out}"
            support = "energy eps" if ok else "finite fail"

        elif kind == "action_reaction":
            ok = abs(p["F12"] + p["F21"]) < EPS
            cex = None if ok else f"sum={p['F12']+p['F21']}"
            support = "F12+F21=0" if ok else "finite fail"

        elif kind == "action_same":
            ok = abs(p["F12"] - p["F21"]) < EPS and abs(p["F12"]) > EPS
            # equality of same-sign holds by construction — but Newton-3 requires opposite
            # The conjecture claims F12=F21 as the law; we interpret verify as "is this the law?"
            # Better: check whether F12+F21==0 under this payload — it won't if same sign
            ok = abs(p["F12"] + p["F21"]) < EPS  # will be FALSE when same sign nonzero
            cex = None if ok else f"F12={p['F12']} F21={p['F21']} not opposite"
            support = "unexpected" if ok else "finite fail"

        elif kind in ("conserv_chem", "conserv_mom", "conserv_kcl"):
            ok, support = linear_constraint_holds(kind, p)
            cex = None if ok else support
            support = f"TRANSFER_FORM {p.get('src_form','?')} → {support}"
            self.compare_log.append({"hit": ok, "kind": kind, "src": p.get("src_form")})

        elif kind == "dead_nomom":
            c = p["c"]
            p_in = c["m1"] * c["u1"] + c["m2"] * c["u2"]
            p_out = c["m1"] * c["v1"] + c["m2"] * c["v2"]
            ok = abs(p_in - p_out) > 1e-6  # claim NOT conserved — should fail
            cex = None if ok else f"|Δp|={abs(p_in-p_out)}≈0 (conservation holds)"
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
            if "transfer_form" in fid and fam.unlocked:
                return self.hypothesize(fam, archive_confirmed, step=-1)
        return []
