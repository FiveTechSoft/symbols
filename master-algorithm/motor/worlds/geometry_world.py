"""
Geometry world plugin — wraps geometry/engine.py.

Lemmas archived as Prolog `lemma/3`. Goals come from axiom closure
(same idea as geometry/loop._interesting_goals), not raw random EqSeg spam.
"""

from __future__ import annotations

import sys
from pathlib import Path
from typing import Any

from .base import Conjecture, FamilySpec, VerifiedFact, WorldBase

_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(_ROOT / "geometry"))

try:
    import engine as G  # type: ignore

    HAS_GEOMETRY = True
except Exception:
    HAS_GEOMETRY = False
    G = None


class GeometryWorld(WorldBase):
    name = "geometry"

    def __init__(self) -> None:
        self._critic = None
        self._archive = None
        self._engine = None
        self._lemma_cites = 0
        self._lemma_attempts = 0
        self._n_proven = 0
        self._templates = []
        if HAS_GEOMETRY:
            self._engine = G.ProofEngine(allow_lemmas=True)
            self._templates = [
                ("midline", G.template_midline),
                ("midline_full", G.template_midline_full),
                ("isosceles", G.template_isosceles),
                ("equilateral", G.template_equilateral),
                ("varignon", G.template_varignon),
                ("para", G.template_para),
            ]

    def bind(self, archive, critic) -> None:
        self._archive = archive
        self._critic = critic
        # Rehydrate schema stubs so lemma citation survives process restart
        if HAS_GEOMETRY and self._engine is not None:
            schemas = list(archive.meta.get("geometry_schemas", []))
            for sch in schemas:
                if any(L.schema == sch for L in self._engine.lemma_library):
                    continue
                if sch == "raw":
                    continue
                c = G.template_midline()
                lem = G.Lemma(
                    name=f"restored_{sch}",
                    lemma_type=sch,
                    construction=c,
                    conclusions=[],
                    proof_cites=[],
                    step=-1,
                    statement=f"restored schema {sch}",
                    schema=sch,
                )
                self._engine.add_lemma(lem)
            self._lemma_cites = int(archive.meta.get("geometry_lemma_cites", 0))
            self._lemma_attempts = int(archive.meta.get("geometry_lemma_attempts", 0))

    def families(self) -> dict[str, FamilySpec]:
        if not HAS_GEOMETRY:
            return {
                "mod_identity": FamilySpec(
                    id="mod_identity",
                    description="Fallback modular commutativity",
                    dead_end=False,
                    param=2,
                    param_max=7,
                    unlocked=True,
                    unlock_order=0,
                ),
                "dead_end_false_mod": FamilySpec(
                    id="dead_end_false_mod",
                    description="DEAD END: n^2 ≡ 2 (mod 4)",
                    dead_end=True,
                    unlocked=True,
                    unlock_order=1,
                ),
            }
        return {
            "euclid_conjectures": FamilySpec(
                id="euclid_conjectures",
                description="Archive axiom-closure theorems; grow template param",
                dead_end=False,
                param=0,
                param_max=max(0, len(self._templates) - 1),
                unlocked=True,
                unlock_order=0,
            ),
            "lemma_reuse": FamilySpec(
                id="lemma_reuse",
                description="Re-close with lemmas enabled; measure citation",
                dead_end=False,
                param=0,
                param_max=max(0, len(self._templates) - 1),
                unlocked=True,
                unlock_order=1,
            ),
            "dead_end_false_eq": FamilySpec(
                id="dead_end_false_eq",
                description="DEAD END: AB=AC on scalene",
                dead_end=True,
                unlocked=True,
                unlock_order=2,
            ),
        }

    def observe(self) -> dict[str, Any]:
        n_lib = len(self._engine.lemma_library) if self._engine else 0
        return {
            "has_geometry": HAS_GEOMETRY,
            "n_lemmas": n_lib,
            "lemma_cites": self._lemma_cites,
            "templates": [t[0] for t in self._templates],
        }

    def hypothesize(self, family: FamilySpec, archive_confirmed: dict, step: int) -> list[Conjecture]:
        out: list[Conjecture] = []
        if not HAS_GEOMETRY:
            if family.id == "mod_identity":
                m = max(2, family.param)
                out.append(
                    Conjecture(
                        name=f"mod_add_comm_{m}",
                        family=family.id,
                        world=self.name,
                        formula=f"(a+b) mod {m} == (b+a) mod {m}",
                        payload={"kind": "mod_comm", "m": m},
                        relation_type="mod_identity",
                    )
                )
            else:
                out.append(
                    Conjecture(
                        name="NEG_sq_eq_2_mod4",
                        family=family.id,
                        world=self.name,
                        formula="n^2 ≡ 2 (mod 4) ∀n",
                        payload={"kind": "bad_mod"},
                        relation_type="dead_end:mod",
                    )
                )
            return out

        if family.id in ("euclid_conjectures", "lemma_reuse"):
            idx = min(family.param, len(self._templates) - 1)
            tname, tfn = self._templates[idx]
            out.append(
                Conjecture(
                    name=f"geo_{tname}_{family.id}_p{family.param}",
                    family=family.id,
                    world=self.name,
                    formula=f"axiom/lemma closure on {tname}",
                    payload={
                        "kind": "geo_close",
                        "template": tname,
                        "tfn": tfn,
                        "use_lemmas": family.id == "lemma_reuse",
                    },
                    relation_type=f"geo:{tname}",
                )
            )
        elif family.id == "dead_end_false_eq":
            out.append(
                Conjecture(
                    name="NEG_scalene_ab_eq_ac",
                    family=family.id,
                    world=self.name,
                    formula="EqSeg(AB,AC) on scalene triangle",
                    payload={"kind": "geo_dead"},
                    relation_type="dead_end:geo",
                )
            )
        return out

    def _archive_goal(self, goal, construction, cites, template: str) -> str:
        lt = G.lemma_type_of(goal, construction)
        schema = G.infer_schema(goal, construction)
        name = f"L{len(self._engine.lemma_library)+1}_{schema if schema != 'raw' else goal.kind}"
        lem = G.Lemma(
            name=name,
            lemma_type=lt,
            construction=construction,
            conclusions=[goal],
            proof_cites=list(cites),
            step=0,
            statement=f"In {construction.name}: {goal.pretty()}",
            schema=schema,
        )
        if schema == "raw" or not any(L.schema == schema for L in self._engine.lemma_library):
            self._engine.add_lemma(lem)
        elif cites:
            self._engine.add_lemma(lem)
        if self._archive:
            self._archive.assert_lemma(name, goal.kind, goal.pretty())
            schemas = self._archive.meta.setdefault("geometry_schemas", [])
            if lem.schema not in schemas and lem.schema != "raw":
                schemas.append(lem.schema)
            self._archive.meta["geometry_lemma_cites"] = self._lemma_cites
            self._archive.meta["geometry_lemma_attempts"] = self._lemma_attempts
            self._archive.save_meta()
        return name

    def verify(self, conjecture: Conjecture) -> VerifiedFact:
        p = conjecture.payload
        kind = p.get("kind")

        if kind == "mod_comm":
            m = p["m"]
            ok = all(((a + b) % m) == ((b + a) % m) for a in range(2 * m) for b in range(2 * m))
            if ok and self._archive:
                self._archive.assert_verified(self.name, conjecture.family, conjecture.name, conjecture.formula)
            return VerifiedFact(
                name=conjecture.name,
                family=conjecture.family,
                world=self.name,
                formula=conjecture.formula,
                true=ok,
                support=f"enumerated a,b < {2*m}",
                counterexample=None if ok else "fail",
                relation_type=conjecture.relation_type,
            )

        if kind == "bad_mod":
            cex = next((f"n={n}" for n in range(8) if (n * n) % 4 != 2), None)
            return VerifiedFact(
                name=conjecture.name,
                family=conjecture.family,
                world=self.name,
                formula=conjecture.formula,
                true=False,
                support="finite fail",
                counterexample=cex,
                relation_type=conjecture.relation_type,
            )

        if kind == "geo_dead":
            c = G.template_triangle()
            goal = G.Fact("EqSeg", (G.seg("A", "B"), G.seg("A", "C")))
            result = self._engine.prove(goal, c, use_lemmas=False)
            proven = result["status"] == "proven"
            return VerifiedFact(
                name=conjecture.name,
                family=conjecture.family,
                world=self.name,
                formula=conjecture.formula,
                true=proven,
                support=f"status={result['status']}",
                counterexample=None if proven else "finite fail (expected)",
                relation_type=conjecture.relation_type,
            )

        if kind == "geo_close":
            construction = p["tfn"]()
            use_lemmas = p["use_lemmas"]
            if use_lemmas:
                self._lemma_attempts += 1

            base = self._engine.bootstrap_facts(construction)
            closed, log = self._engine.close(base, cite_lemmas=use_lemmas and bool(self._engine.lemma_library))

            # Pick archiveable non-reflexive facts
            candidates = []
            for f in closed:
                if f.kind in ("EqSeg", "EqAng") and f.args[0] == f.args[1]:
                    continue
                if f.kind not in ("Parallel", "HalfSeg", "EqAng", "EqSeg"):
                    continue
                if f.source == "construction":
                    continue
                candidates.append(f)

            # Prefer schemeful facts
            def rank(f):
                sch = G.infer_schema(f, construction)
                return (0 if sch == "raw" else 1, f.kind)

            candidates.sort(key=rank, reverse=True)

            proved_any = False
            cited = None
            formula = conjecture.formula
            support = "empty closure"
            lem_name = None

            for goal in candidates[:8]:
                # Re-prove to get cites
                result = self._engine.prove(goal, construction, use_lemmas=use_lemmas)
                if result["status"] != "proven":
                    continue
                # Skip if already have this schema
                schema = G.infer_schema(goal, construction)
                if schema != "raw" and any(L.schema == schema for L in self._engine.lemma_library):
                    # still counts as reuse if cites present
                    cites = result.get("cites") or []
                    if use_lemmas and cites:
                        self._lemma_cites += 1
                        cited = str(cites[0])
                        proved_any = True
                        formula = goal.pretty()
                        support = f"re-proved with cites={cites}"
                        break
                    continue
                cites = result.get("cites") or []
                lem_name = self._archive_goal(goal, construction, cites, p["template"])
                self._n_proven += 1
                proved_any = True
                formula = goal.pretty()
                support = f"archived {lem_name} schema={schema}"
                if cites and use_lemmas:
                    self._lemma_cites += 1
                    cited = str(cites[0])
                if self._archive:
                    self._archive.assert_verified(
                        self.name,
                        conjecture.family,
                        f"{conjecture.name}::{lem_name}",
                        formula,
                    )
                break

            # If nothing new but closure non-empty, accept a Midpoint-halves style fact once
            if not proved_any and candidates:
                goal = candidates[0]
                result = self._engine.prove(goal, construction, use_lemmas=False)
                if result["status"] == "proven":
                    schema = G.infer_schema(goal, construction)
                    # force raw archive with unique name
                    lem_name = self._archive_goal(goal, construction, [], p["template"])
                    proved_any = True
                    formula = goal.pretty()
                    support = f"archived fallback {lem_name}"
                    if self._archive:
                        self._archive.assert_verified(
                            self.name,
                            conjecture.family,
                            f"{conjecture.name}::{lem_name}",
                            formula,
                        )

            return VerifiedFact(
                name=conjecture.name,
                family=conjecture.family,
                world=self.name,
                formula=formula,
                true=proved_any,
                support=support,
                counterexample=None if proved_any else "no new archiveable fact",
                relation_type=conjecture.relation_type,
                lemma_cited=cited,
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

    def lemma_reuse_rate(self) -> float:
        if self._lemma_attempts == 0:
            return 0.0
        return self._lemma_cites / max(1, self._lemma_attempts)
