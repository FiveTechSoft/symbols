"""
Chem world — atom-count conservation on generated reaction tables; toy K.

NO full periodic table as facts. Dead-end: atoms not conserved.
"""

from __future__ import annotations

import random
from typing import Any, Optional

from .base import Conjecture, FamilySpec, VerifiedFact, WorldBase
from .science_lang import ScienceLanguage

EPS = 1e-9

# Tiny generated reactions as atom-count vectors {element: count}
# Not a periodic table — just count algebra.


def _reaction_water() -> dict:
    # 2 H2 + O2 → 2 H2O
    return {
        "reactants": {"H": 4, "O": 2},  # 2*H2 + O2
        "products": {"H": 4, "O": 2},   # 2*H2O
        "label": "2H2+O2→2H2O",
    }


def _reaction_synth(seed: int) -> dict:
    rng = random.Random(seed)
    # generate balanced A+B→C style counts
    a_H, a_O = rng.randint(1, 3), rng.randint(0, 2)
    b_H, b_O = rng.randint(0, 2), rng.randint(1, 3)
    return {
        "reactants": {"H": a_H + b_H, "O": a_O + b_O},
        "products": {"H": a_H + b_H, "O": a_O + b_O},
        "label": f"synth_s{seed}",
    }


def _reaction_unbalanced(seed: int) -> dict:
    r = _reaction_synth(seed)
    # break product H count
    r = {
        "reactants": dict(r["reactants"]),
        "products": dict(r["products"]),
        "label": f"broken_s{seed}",
    }
    r["products"]["H"] = r["products"].get("H", 0) + 1
    return r


def _eq_toy(seed: int) -> dict:
    """2-species A⇌B with K = [B]/[A]."""
    rng = random.Random(seed)
    A = rng.uniform(0.2, 2.0)
    K = rng.uniform(0.5, 3.0)
    B = K * A
    return {"A": A, "B": B, "K": K}


class ChemWorld(WorldBase):
    name = "chem"

    def __init__(self, language: Optional[ScienceLanguage] = None) -> None:
        self.language = language or ScienceLanguage.seed_for("chem")
        self._archive = None
        self._critic = None
        self.compare_log: list[dict] = []

    def bind(self, archive, critic) -> None:
        self._archive = archive
        self._critic = critic
        skin = (archive.meta or {}).get("science_skins", {}).get("chem")
        if skin and "schemas" in skin:
            try:
                from .schema_lang import SchemaClass

                self.language = ScienceLanguage(
                    schemas={k: SchemaClass.from_dict(v) for k, v in skin["schemas"].items()},
                    generation=int(skin.get("generation", 0)),
                    molt_history=list(skin.get("molt_history", [])),
                    world_tag="chem",
                )
                self.language.ensure_novelty_alive()
            except Exception:
                pass

    def persist_skin(self) -> None:
        if self._archive is None:
            return
        skins = self._archive.meta.setdefault("science_skins", {})
        skins["chem"] = self.language.snapshot()
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
            "note": "count-vector reactions only — no periodic table dump",
            "n_schema_classes": self.language.n_schema_classes(),
            "spawned": self.language.spawned_ids(),
            "generation": self.language.generation,
        }

    def hypothesize(self, family: FamilySpec, archive_confirmed: dict, step: int) -> list[Conjecture]:
        out: list[Conjecture] = []
        fid = family.id
        n = max(2, family.param)
        seed = 6000 + step * 5 + n

        if "atom_balance" in fid:
            rxns = [_reaction_water()] + [_reaction_synth(seed + i) for i in range(n)]
            for i, rx in enumerate(rxns):
                out.append(
                    Conjecture(
                        name=f"chem_atom_balance_{rx['label']}",
                        family=fid,
                        world=self.name,
                        formula=f"atom counts conserved: {rx['label']}",
                        payload={"kind": "atom_balance", "rx": rx, "schema": fid, "order": n},
                        relation_type="stoich",
                    )
                )
            # unbalanced must reject
            bad = _reaction_unbalanced(seed)
            out.append(
                Conjecture(
                    name=f"chem_unbalanced_{bad['label']}",
                    family=fid,
                    world=self.name,
                    formula="unbalanced counts conserved? (should fail)",
                    payload={"kind": "atom_balance", "rx": bad, "schema": fid},
                    relation_type="stoich_neg",
                )
            )

        elif "equilibrium_K" in fid:
            for i in range(n):
                toy = _eq_toy(seed + i)
                out.append(
                    Conjecture(
                        name=f"chem_K_def_s{seed + i}",
                        family=fid,
                        world=self.name,
                        formula="K = [B]/[A] on 2-species toy",
                        payload={"kind": "K_def", "toy": toy, "schema": fid, "order": n},
                        relation_type="equilibrium",
                    )
                )

        elif "dead_noatoms" in fid or fid.startswith("chem_dead"):
            rx = _reaction_water()
            out.append(
                Conjecture(
                    name="NEG_chem_atoms_not_conserved",
                    family=fid,
                    world=self.name,
                    formula="atom counts NOT conserved on 2H2+O2→2H2O",
                    payload={"kind": "dead_noatoms", "rx": rx, "schema": fid},
                    relation_type="dead_end:noatoms",
                )
            )
        return out

    def verify(self, conjecture: Conjecture) -> VerifiedFact:
        p = conjecture.payload
        kind = p.get("kind")
        ok = False
        cex: Optional[str] = None
        support = ""

        if kind == "atom_balance":
            rx = p["rx"]
            elems = set(rx["reactants"]) | set(rx["products"])
            ok = all(rx["reactants"].get(e, 0) == rx["products"].get(e, 0) for e in elems)
            cex = None if ok else f"mismatch {rx['reactants']} vs {rx['products']}"
            support = f"counts ok {rx['label']}" if ok else "finite fail"

        elif kind == "K_def":
            toy = p["toy"]
            pred = toy["B"] / toy["A"] if toy["A"] > 0 else float("nan")
            ok = abs(pred - toy["K"]) < 1e-9
            cex = None if ok else f"B/A={pred} != K={toy['K']}"
            support = f"K={toy['K']:.6f}" if ok else "finite fail"

        elif kind == "dead_noatoms":
            rx = p["rx"]
            elems = set(rx["reactants"]) | set(rx["products"])
            conserved = all(rx["reactants"].get(e, 0) == rx["products"].get(e, 0) for e in elems)
            ok = not conserved  # claim NOT conserved — false on water reaction
            cex = None if ok else "counts match — conservation holds (reject claim)"
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
