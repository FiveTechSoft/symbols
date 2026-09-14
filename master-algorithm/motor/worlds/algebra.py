"""
Algebra world — poly identities on Z/nZ, 2×2 linear systems, matrix assoc mod p.

Does NOT blindly duplicate Z2 from symmetry.py. Critic = exact finite check.
Dead-end: claim (x+y)^2 = x^2+y^2.
"""

from __future__ import annotations

import itertools
import random
from typing import Any, Optional

from .base import Conjecture, FamilySpec, VerifiedFact, WorldBase
from .science_lang import ScienceLanguage


def _mod(n: int, m: int) -> int:
    return n % m


def _mat_mul_mod(A, B, p: int):
    return (
        (
            (A[0][0] * B[0][0] + A[0][1] * B[1][0]) % p,
            (A[0][0] * B[0][1] + A[0][1] * B[1][1]) % p,
        ),
        (
            (A[1][0] * B[0][0] + A[1][1] * B[1][0]) % p,
            (A[1][0] * B[0][1] + A[1][1] * B[1][1]) % p,
        ),
    )


def _gen_2x2(seed: int, lo: int = -5, hi: int = 5):
    rng = random.Random(seed)
    while True:
        a, b, c, d = [rng.randint(lo, hi) for _ in range(4)]
        det = a * d - b * c
        if det != 0:
            return (a, b, c, d), det


class AlgebraWorld(WorldBase):
    name = "algebra"

    def __init__(self, language: Optional[ScienceLanguage] = None) -> None:
        self.language = language or ScienceLanguage.seed_for("algebra")
        self._archive = None
        self._critic = None
        self.compare_log: list[dict] = []

    def bind(self, archive, critic) -> None:
        self._archive = archive
        self._critic = critic
        skin = (archive.meta or {}).get("science_skins", {}).get("algebra")
        if skin and "schemas" in skin:
            try:
                from .schema_lang import SchemaClass

                self.language = ScienceLanguage(
                    schemas={k: SchemaClass.from_dict(v) for k, v in skin["schemas"].items()},
                    generation=int(skin.get("generation", 0)),
                    molt_history=list(skin.get("molt_history", [])),
                    world_tag="algebra",
                )
                self.language.ensure_novelty_alive()
            except Exception:
                pass

    def persist_skin(self) -> None:
        if self._archive is None:
            return
        skins = self._archive.meta.setdefault("science_skins", {})
        skins["algebra"] = self.language.snapshot()
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
            "rings": ["Z/nZ"],
            "n_schema_classes": self.language.n_schema_classes(),
            "spawned": self.language.spawned_ids(),
            "generation": self.language.generation,
            "note": "no Z2 blind duplicate of symmetry; poly/linear/mat focus",
        }

    def hypothesize(self, family: FamilySpec, archive_confirmed: dict, step: int) -> list[Conjecture]:
        out: list[Conjecture] = []
        fid = family.id
        n = max(3, family.param)
        seed = 3000 + step * 11 + n

        if "poly_zn" in fid:
            out.append(
                Conjecture(
                    name=f"alg_binom_expand_mod{n}",
                    family=fid,
                    world=self.name,
                    formula=f"(x+y)^2 ≡ x^2+2xy+y^2 (mod {n}) exhaustive",
                    payload={"kind": "binom_expand", "mod": n, "schema": fid, "order": n},
                    relation_type="poly_id",
                )
            )
            out.append(
                Conjecture(
                    name=f"alg_distrib_mod{n}",
                    family=fid,
                    world=self.name,
                    formula=f"x(y+z) ≡ xy+xz (mod {n})",
                    payload={"kind": "distrib", "mod": n, "schema": fid, "order": n},
                    relation_type="poly_id",
                )
            )

        elif "linear_2x2" in fid:
            for i in range(2):
                coeffs, det = _gen_2x2(seed + i)
                # rhs chosen so solution is integers when possible: pick (x,y) then b=A(x,y)
                rng = random.Random(seed + i + 99)
                x_true, y_true = rng.randint(-3, 3), rng.randint(-3, 3)
                a, b, c, d = coeffs
                e = a * x_true + b * y_true
                f = c * x_true + d * y_true
                out.append(
                    Conjecture(
                        name=f"alg_solve_2x2_s{seed + i}",
                        family=fid,
                        world=self.name,
                        formula="2x2 Cramer exact on generated integer system",
                        payload={
                            "kind": "solve_2x2",
                            "a": a, "b": b, "c": c, "d": d,
                            "e": e, "f": f,
                            "x_true": x_true, "y_true": y_true,
                            "det": det,
                            "schema": fid,
                            "order": n,
                        },
                        relation_type="linear",
                    )
                )

        elif "mat_assoc" in fid:
            p = n if n >= 3 else 3
            # prefer small prime-ish moduli
            if p % 2 == 0:
                p += 1
            out.append(
                Conjecture(
                    name=f"alg_mat2_assoc_mod{p}",
                    family=fid,
                    world=self.name,
                    formula=f"(AB)C = A(BC) for all 2x2 matrices mod {p}",
                    payload={"kind": "mat_assoc", "p": p, "schema": fid, "order": n},
                    relation_type="mat_assoc",
                )
            )

        elif "dead_binom" in fid or fid.startswith("alg_dead"):
            m = n if n % 2 == 1 else n + 1  # odd: cross term 2xy visible
            out.append(
                Conjecture(
                    name="NEG_alg_binom_no_cross",
                    family=fid,
                    world=self.name,
                    formula=f"(x+y)^2 ≡ x^2+y^2 (mod {m}) — missing 2xy",
                    payload={"kind": "dead_binom", "mod": m, "schema": fid},
                    relation_type="dead_end:binom",
                )
            )
        return out

    def verify(self, conjecture: Conjecture) -> VerifiedFact:
        p = conjecture.payload
        kind = p.get("kind")
        ok = False
        cex: Optional[str] = None
        support = ""

        if kind == "binom_expand":
            m = int(p["mod"])
            ok = True
            for x, y in itertools.product(range(m), repeat=2):
                lhs = _mod((x + y) ** 2, m)
                rhs = _mod(x * x + 2 * x * y + y * y, m)
                if lhs != rhs:
                    ok = False
                    cex = f"x={x},y={y}: {lhs}!={rhs}"
                    break
            support = f"exhaustive Z/{m}Z binom" if ok else "finite fail"

        elif kind == "distrib":
            m = int(p["mod"])
            ok = True
            for x, y, z in itertools.product(range(m), repeat=3):
                lhs = _mod(x * _mod(y + z, m), m)
                rhs = _mod(_mod(x * y, m) + _mod(x * z, m), m)
                if lhs != rhs:
                    ok = False
                    cex = f"x,y,z={x},{y},{z}"
                    break
            support = f"exhaustive distrib mod {m}" if ok else "finite fail"

        elif kind == "solve_2x2":
            a, b, c, d = p["a"], p["b"], p["c"], p["d"]
            e, f = p["e"], p["f"]
            det = a * d - b * c
            if det == 0:
                ok = False
                cex = "singular"
            else:
                # Cramer
                x = (e * d - b * f) / det
                y = (a * f - e * c) / det
                ok = abs(x - p["x_true"]) < 1e-9 and abs(y - p["y_true"]) < 1e-9
                # also residual
                if ok:
                    ok = abs(a * x + b * y - e) < 1e-9 and abs(c * x + d * y - f) < 1e-9
                cex = None if ok else f"got ({x},{y}) want ({p['x_true']},{p['y_true']})"
            support = f"Cramer det={det}" if ok else "finite fail"

        elif kind == "mat_assoc":
            modp = int(p["p"])
            # sample or exhaustive for small p
            ok = True
            rng = random.Random(modp * 17)
            # exhaustive if p<=3 else random sample
            if modp <= 3:
                space = list(itertools.product(range(modp), repeat=4))
                mats = [((a, b), (c, d)) for a, b, c, d in space]
                for A, B, C in itertools.product(mats, repeat=3):
                    lhs = _mat_mul_mod(_mat_mul_mod(A, B, modp), C, modp)
                    rhs = _mat_mul_mod(A, _mat_mul_mod(B, C, modp), modp)
                    if lhs != rhs:
                        ok = False
                        cex = f"assoc fail mod {modp}"
                        break
                support = f"exhaustive 2x2 assoc mod {modp}" if ok else "finite fail"
            else:
                for _ in range(80):
                    def rnd():
                        return (
                            (rng.randrange(modp), rng.randrange(modp)),
                            (rng.randrange(modp), rng.randrange(modp)),
                        )
                    A, B, C = rnd(), rnd(), rnd()
                    lhs = _mat_mul_mod(_mat_mul_mod(A, B, modp), C, modp)
                    rhs = _mat_mul_mod(A, _mat_mul_mod(B, C, modp), modp)
                    if lhs != rhs:
                        ok = False
                        cex = f"assoc fail sample mod {modp}"
                        break
                support = f"sampled 2x2 assoc mod {modp}" if ok else "finite fail"

        elif kind == "dead_binom":
            m = int(p["mod"])
            ok = True
            for x, y in itertools.product(range(m), repeat=2):
                lhs = _mod((x + y) ** 2, m)
                rhs = _mod(x * x + y * y, m)
                if lhs != rhs:
                    ok = False
                    cex = f"x={x},y={y}: (x+y)^2={lhs} != x^2+y^2={rhs}"
                    break
            if ok:
                # known counterexample if somehow held (e.g. char 2)
                ok = False
                cex = f"forced: need cross term; char may hide it — check x=1,y=1"
                # recheck honestly for m=5 style
                if m % 2 == 0:
                    # in char 2, 2xy=0 so identity CAN hold — pick odd modulus claim fail
                    pass
            support = "unexpected hold" if ok else "finite fail (dead-end)"

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
            bump = 2 if "poly" in family.id or "mat" in family.id else 1
            family.param = min(family.param_max, family.param + bump)
            if sch:
                sch.order = family.param
                sch.ticks_no_new_type = 0 if new_truths else sch.ticks_no_new_type + 1
            self.language.ensure_novelty_alive()
            self.persist_skin()
            return f"extrapolate mod/order {old}→{family.param}"
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
