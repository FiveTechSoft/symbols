"""
Astro world — generated Keplerian circular-orbit table.

obs → conjecture → critic → verified/rejected.
No Swiss-ephemeris dependency; a generated (T, a) table is enough and honest.
Dead-end: T ∝ a. Broken Kepler (wrong exponent) must reject.
"""

from __future__ import annotations

import math
from typing import Any, Optional

from .base import Conjecture, FamilySpec, VerifiedFact, WorldBase
from .science_lang import ScienceLanguage

EPS = 1e-6
TWO_PI = 2.0 * math.pi


def _kepler_table(n_bodies: int, a0: float = 1.0) -> list[dict[str, float]]:
    """Circular orbits with GM=1: T = 2π √(a³) ⇒ T²/a³ = 4π²."""
    rows = []
    for i in range(n_bodies):
        a = a0 * (1.0 + 0.5 * i)  # 1.0, 1.5, 2.0, ...
        T = TWO_PI * math.sqrt(a ** 3)
        # specific angular momentum for circular: h = √(GM a) = √a
        h = math.sqrt(a)
        rows.append({"body": i, "a": a, "T": T, "h": h, "mass": 1.0})
    return rows


class AstroWorld(WorldBase):
    name = "astro"

    def __init__(self, language: Optional[ScienceLanguage] = None) -> None:
        self.language = language or ScienceLanguage.seed_for("astro")
        self._archive = None
        self._critic = None
        self.compare_log: list[dict] = []

    def bind(self, archive, critic) -> None:
        self._archive = archive
        self._critic = critic
        skin = (archive.meta or {}).get("science_skins", {}).get("astro")
        if skin and "schemas" in skin:
            try:
                from .schema_lang import SchemaClass

                self.language = ScienceLanguage(
                    schemas={k: SchemaClass.from_dict(v) for k, v in skin["schemas"].items()},
                    generation=int(skin.get("generation", 0)),
                    molt_history=list(skin.get("molt_history", [])),
                    world_tag="astro",
                )
                self.language.ensure_novelty_alive()
            except Exception:
                pass

    def persist_skin(self) -> None:
        if self._archive is None:
            return
        skins = self._archive.meta.setdefault("science_skins", {})
        skins["astro"] = self.language.snapshot()
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
        n = max(3, min(8, self.language.schemas.get("astro_period_scan",
               type("X", (), {"order": 3})()).order if "astro_period_scan" in self.language.schemas
               else 3))
        table = _kepler_table(n)
        return {
            "n_bodies": len(table),
            "table_digest": [{"a": round(r["a"], 4), "T": round(r["T"], 4)} for r in table[:4]],
            "n_schema_classes": self.language.n_schema_classes(),
            "spawned": self.language.spawned_ids(),
            "generation": self.language.generation,
            "note": "generated Keplerian circular; no ephemeris dump",
        }

    def hypothesize(self, family: FamilySpec, archive_confirmed: dict, step: int) -> list[Conjecture]:
        out: list[Conjecture] = []
        fid = family.id
        n = max(3, family.param)
        table = _kepler_table(n)

        if "period_scan" in fid:
            out.append(
                Conjecture(
                    name=f"astro_T_increases_with_a_n{n}",
                    family=fid,
                    world=self.name,
                    formula="T increases with a on generated circular table",
                    payload={"kind": "T_mono_a", "table": table, "schema": fid, "order": n},
                    relation_type="period_scan",
                )
            )
            out.append(
                Conjecture(
                    name=f"astro_T_pos_n{n}",
                    family=fid,
                    world=self.name,
                    formula="T>0 and a>0 for all generated bodies",
                    payload={"kind": "T_a_positive", "table": table, "schema": fid, "order": n},
                    relation_type="period_scan",
                )
            )

        elif "kepler_scan" in fid:
            out.append(
                Conjecture(
                    name=f"astro_kepler3_const_n{n}",
                    family=fid,
                    world=self.name,
                    formula="T^2/a^3 constant within eps on circular table",
                    payload={"kind": "kepler3", "table": table, "schema": fid, "order": n, "eps": EPS},
                    relation_type="kepler",
                )
            )
            # broken exponent — must reject
            out.append(
                Conjecture(
                    name=f"astro_kepler_wrong_exp_n{n}",
                    family=fid,
                    world=self.name,
                    formula="T^2/a^2 constant (broken Kepler exponent)",
                    payload={"kind": "kepler_broken_exp", "table": table, "schema": fid, "order": n},
                    relation_type="kepler_neg",
                )
            )

        elif "angmom" in fid:
            out.append(
                Conjecture(
                    name=f"astro_h_eq_sqrt_a_n{n}",
                    family=fid,
                    world=self.name,
                    formula="circular specific ang-mom h = sqrt(a) (GM=1) within eps",
                    payload={"kind": "angmom_sqrt_a", "table": table, "schema": fid, "order": n},
                    relation_type="angmom",
                )
            )
            out.append(
                Conjecture(
                    name=f"astro_h2_over_a_const_n{n}",
                    family=fid,
                    world=self.name,
                    formula="h^2/a constant (=1) on circular 2-body table",
                    payload={"kind": "angmom_h2_a", "table": table, "schema": fid, "order": n},
                    relation_type="angmom",
                )
            )

        elif "dead_linear" in fid or fid.startswith("astro_dead"):
            out.append(
                Conjecture(
                    name="NEG_astro_T_prop_a",
                    family=fid,
                    world=self.name,
                    formula="T ∝ a (linear) on Keplerian circular table",
                    payload={"kind": "dead_linear", "table": table, "schema": fid},
                    relation_type="dead_end:linear_T",
                )
            )
        return out

    def verify(self, conjecture: Conjecture) -> VerifiedFact:
        p = conjecture.payload
        kind = p.get("kind")
        table = p.get("table") or []
        ok = False
        cex: Optional[str] = None
        support = ""

        if kind == "T_mono_a":
            ok = all(table[i]["T"] < table[i + 1]["T"] for i in range(len(table) - 1))
            cex = None if ok else "T not monotone in a"
            support = f"mono check n={len(table)}" if ok else "finite fail"

        elif kind == "T_a_positive":
            ok = all(r["T"] > 0 and r["a"] > 0 for r in table)
            cex = None if ok else "non-positive T or a"
            support = f"positivity n={len(table)}" if ok else "finite fail"

        elif kind == "kepler3":
            ratios = [r["T"] ** 2 / (r["a"] ** 3) for r in table]
            target = (TWO_PI) ** 2
            ok = all(abs(x - target) < 1e-6 for x in ratios) and all(
                abs(x - ratios[0]) < 1e-6 for x in ratios
            )
            cex = None if ok else f"ratios={ratios[:3]} target={target}"
            support = f"T²/a³≈4π² n={len(table)}" if ok else "finite fail"

        elif kind == "kepler_broken_exp":
            # claim T²/a² constant — FALSE for Keplerian a-variation
            ratios = [r["T"] ** 2 / (r["a"] ** 2) for r in table]
            ok = all(abs(x - ratios[0]) < 1e-6 for x in ratios)
            cex = None if ok else f"T²/a² not const: {[round(x,4) for x in ratios[:4]]}"
            support = "unexpected hold" if ok else "finite fail (wrong exponent)"

        elif kind == "angmom_sqrt_a":
            ok = all(abs(r["h"] - math.sqrt(r["a"])) < 1e-9 for r in table)
            cex = None if ok else "h≠√a"
            support = "h=√a on circular table" if ok else "finite fail"

        elif kind == "angmom_h2_a":
            ratios = [r["h"] ** 2 / r["a"] for r in table]
            ok = all(abs(x - 1.0) < 1e-9 for x in ratios)
            cex = None if ok else f"h²/a={ratios}"
            support = "h²/a=1 invariant" if ok else "finite fail"

        elif kind == "dead_linear":
            # claim T/a constant
            ratios = [r["T"] / r["a"] for r in table]
            ok = all(abs(x - ratios[0]) < 1e-6 for x in ratios)
            cex = None if ok else f"T/a not const: {[round(x,4) for x in ratios[:4]]}"
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
                sch.ticks_no_new_type = 0 if new_truths else sch.ticks_no_new_type + 1
            self.language.ensure_novelty_alive()
            self.persist_skin()
            return f"extrapolate n_bodies {old}→{family.param}"
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
