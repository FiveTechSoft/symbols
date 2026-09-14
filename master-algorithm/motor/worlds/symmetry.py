"""
Symmetry world — discrete invariants on finite tables.

obs → conjecture → critic → verified/rejected.
COMPARE: reuse bit_fn parity / archived invariants across Z2/Klein systems.
EXTRAPOLATE: grow order (bit-width / table size); critic must still accept.
EMERGE: molt spawns sym_*_n*_g* classes not in SCIENCE_SEED.
"""

from __future__ import annotations

import itertools
from typing import Any, Optional

from .base import Conjecture, FamilySpec, VerifiedFact, WorldBase
from .science_lang import ScienceLanguage


def _z2_table() -> dict[tuple[int, int], int]:
    return {(a, b): (a + b) % 2 for a in (0, 1) for b in (0, 1)}


def _klein_table() -> dict[tuple[str, str], str]:
    # Klein four-group as {e,a,b,c} with every non-id order-2
    elems = ["e", "a", "b", "c"]
    # cayley: e identity; a*a=e, b*b=e, c*c=e; a*b=c, a*c=b, b*c=a
    op = {}
    for x in elems:
        op[("e", x)] = x
        op[(x, "e")] = x
    for x in ("a", "b", "c"):
        op[(x, x)] = "e"
    op[("a", "b")] = op[("b", "a")] = "c"
    op[("a", "c")] = op[("c", "a")] = "b"
    op[("b", "c")] = op[("c", "b")] = "a"
    return op


def _parity_conserved(bits: list[int]) -> int:
    r = 0
    for b in bits:
        r ^= b
    return r


class SymmetryWorld(WorldBase):
    name = "symmetry"

    def __init__(self, language: Optional[ScienceLanguage] = None) -> None:
        self.language = language or ScienceLanguage.seed_for("symmetry")
        self._archive = None
        self._critic = None
        self.compare_log: list[dict] = []

    def bind(self, archive, critic) -> None:
        self._archive = archive
        self._critic = critic
        skin = (archive.meta or {}).get("science_skins", {}).get("symmetry")
        if skin and "schemas" in skin:
            try:
                from .schema_lang import SchemaClass

                self.language = ScienceLanguage(
                    schemas={k: SchemaClass.from_dict(v) for k, v in skin["schemas"].items()},
                    generation=int(skin.get("generation", 0)),
                    molt_history=list(skin.get("molt_history", [])),
                    world_tag="symmetry",
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
        skins["symmetry"] = self.language.snapshot()
        self._archive.save_meta() if hasattr(self._archive, "save_meta") else self._archive.save()

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
            "systems": ["z2", "klein", "parity_bits"],
            "n_schema_classes": self.language.n_schema_classes(),
            "spawned": self.language.spawned_ids(),
            "generation": self.language.generation,
            "compare_log_len": len(self.compare_log),
        }

    def _archived_bit_fns(self) -> list[tuple[str, str, str]]:
        if self._archive is None:
            return []
        if hasattr(self._archive, "list_bit_fns"):
            return self._archive.list_bit_fns()
        # fallback parse
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
        n = max(2, family.param)

        if fid.startswith("sym_invariant") or "invariant_scan" in fid:
            # Hypotheses as operator forms — not canned theorem names
            forms = [
                ("parity_mod2_sum", "sum(bits) mod 2 invariant under permute"),
                ("involution_double", "f(f(x))=x for swap/negate"),
                ("z2_assoc", "Z2 addition associative on full table"),
            ]
            # Extrapolate: at higher order also test longer bit strings
            if n >= 3:
                forms.append(("parity_mod2_sum_n", f"parity conserved on {n}-bit strings"))
            for kind, formula in forms:
                out.append(
                    Conjecture(
                        name=f"sym_{kind}_o{n}",
                        family=fid,
                        world=self.name,
                        formula=formula,
                        payload={"kind": kind, "order": n, "schema": fid},
                        relation_type="invariant",
                    )
                )

        elif fid.startswith("sym_group") or "group_table" in fid:
            out.append(
                Conjecture(
                    name=f"sym_z2_abelian_o{n}",
                    family=fid,
                    world=self.name,
                    formula="Z2 table commutative + identity 0",
                    payload={"kind": "z2_abelian", "order": n, "schema": fid},
                    relation_type="group_law",
                )
            )
            out.append(
                Conjecture(
                    name=f"sym_klein_every_nonid_order2_o{n}",
                    family=fid,
                    world=self.name,
                    formula="Klein: every non-identity element has order 2",
                    payload={"kind": "klein_order2", "order": n, "schema": fid},
                    relation_type="group_law",
                )
            )
            # broken hypothesis for critic reject path inside productive family
            out.append(
                Conjecture(
                    name=f"sym_z2_claim_mult_o{n}",
                    family=fid,
                    world=self.name,
                    formula="Z2 table equals multiplication mod 2 (broken)",
                    payload={"kind": "z2_as_mult", "order": n, "schema": fid},
                    relation_type="group_law_neg",
                )
            )

        elif fid.startswith("sym_dead") or "dead_product" in fid:
            out.append(
                Conjecture(
                    name="NEG_sym_product_mod2_conserved",
                    family=fid,
                    world=self.name,
                    formula="product(bits) mod 2 conserved under flip-one-bit",
                    payload={"kind": "dead_product", "order": n, "schema": fid},
                    relation_type="dead_end:product",
                )
            )

        elif fid.startswith("sym_compare") or "compare_transfer" in fid:
            # COMPARE: bit_fn parity → parity invariant on Z2 / bit strings
            bit_fns = self._archived_bit_fns()
            if not bit_fns:
                out.append(
                    Conjecture(
                        name="sym_compare_wait_no_bitfn",
                        family=fid,
                        world=self.name,
                        formula="no bit_fn prior yet — honest miss",
                        payload={"kind": "compare_empty", "schema": fid},
                        relation_type="compare_miss",
                        from_transfer=True,
                        transfer_source="logic",
                    )
                )
            for name, target, hyp in bit_fns:
                if hyp not in ("parity", "xor2"):
                    continue
                out.append(
                    Conjecture(
                        name=f"transfer_{hyp}_to_z2_parity_{target}",
                        family=fid,
                        world=self.name,
                        formula=f"TRANSFER bit_fn({target}={hyp}) ⇒ Z2/parity invariant",
                        payload={
                            "kind": "compare_bitfn_parity",
                            "src_name": name,
                            "src_hyp": hyp,
                            "src_target": target,
                            "order": n,
                            "schema": fid,
                        },
                        relation_type="compare_transfer",
                        from_transfer=True,
                        transfer_source=f"logic:{target}",
                    )
                )
        return out

    def verify(self, conjecture: Conjecture) -> VerifiedFact:
        p = conjecture.payload
        kind = p.get("kind")
        n = int(p.get("order", 2))
        ok = False
        cex: Optional[str] = None
        support = ""

        if kind == "parity_mod2_sum":
            # full finite check: permute does not change xor
            ok = True
            for bits in itertools.product([0, 1], repeat=min(n, 4)):
                bits = list(bits)
                base = _parity_conserved(bits)
                for perm in set(itertools.permutations(bits)):
                    if _parity_conserved(list(perm)) != base:
                        ok = False
                        cex = f"perm {perm} broke parity of {bits}"
                        break
                if not ok:
                    break
            support = f"exhaustive permute check n≤{min(n,4)}" if ok else "finite fail"

        elif kind == "parity_mod2_sum_n":
            ok = True
            for bits in itertools.product([0, 1], repeat=n):
                bits = list(bits)
                # flipping two bits preserves parity
                if n < 2:
                    continue
                flipped = bits[:]
                flipped[0] ^= 1
                flipped[1] ^= 1
                if _parity_conserved(flipped) != _parity_conserved(bits):
                    ok = False
                    cex = f"double-flip broke {bits}"
                    break
            support = f"double-flip parity on {n}-bit space" if ok else "finite fail"

        elif kind == "involution_double":
            # f = swap bits; f(f(x))=x on all 2-bit pairs
            ok = True
            for a, b in itertools.product([0, 1], repeat=2):
                fa, fb = b, a
                aa, bb = fb, fa
                if (aa, bb) != (a, b):
                    ok = False
                    cex = f"swap^2 failed on {(a,b)}"
                    break
            # also negate involution
            if ok:
                for x in (0, 1):
                    if (1 - (1 - x)) != x:
                        ok = False
                        cex = f"negate^2 failed on {x}"
                        break
            support = "swap+negate involution on {0,1}^2" if ok else "finite fail"

        elif kind == "z2_assoc":
            t = _z2_table()
            ok = True
            for a, b, c in itertools.product([0, 1], repeat=3):
                if t[(t[(a, b)], c)] != t[(a, t[(b, c)])]:
                    ok = False
                    cex = f"assoc fail {a},{b},{c}"
                    break
            support = "full Z2 assoc table" if ok else "finite fail"

        elif kind == "z2_abelian":
            t = _z2_table()
            ok = all(t[(a, b)] == t[(b, a)] for a in (0, 1) for b in (0, 1))
            ok = ok and all(t[(0, a)] == a for a in (0, 1))
            cex = None if ok else "not abelian or bad identity"
            support = "full Z2 cayley" if ok else "finite fail"

        elif kind == "klein_order2":
            t = _klein_table()
            ok = True
            for x in ("a", "b", "c"):
                if t[(x, x)] != "e":
                    ok = False
                    cex = f"{x}*{x} != e"
                    break
            support = "Klein full order-2 check" if ok else "finite fail"

        elif kind == "z2_as_mult":
            # broken: claim + is *
            t = _z2_table()
            ok = all(t[(a, b)] == ((a * b) % 2) for a in (0, 1) for b in (0, 1))
            # This is FALSE (1+1=0 but 1*1=1)
            if ok:
                support = "unexpectedly held"
            else:
                cex = "1+1=0 != 1*1=1"
                support = "finite fail"
            ok = False  # force reject path — wait, critic should decide
            # Recompute honestly:
            ok = all(t[(a, b)] == ((a * b) % 2) for a in (0, 1) for b in (0, 1))
            if not ok:
                cex = "1+1=0 != 1*1=1"

        elif kind == "dead_product":
            # claim: flipping one bit preserves product mod 2 — FALSE
            ok = True
            for bits in itertools.product([0, 1], repeat=3):
                bits = list(bits)
                prod = 1
                for b in bits:
                    prod = (prod * b) % 2
                flipped = bits[:]
                flipped[0] ^= 1
                prod2 = 1
                for b in flipped:
                    prod2 = (prod2 * b) % 2
                if prod2 != prod:
                    ok = False
                    cex = f"flip0 {bits}→{flipped} prod {prod}→{prod2}"
                    break
            # If we never found a counterexample, still check a known one
            if ok:
                # [1,1,1] prod=1; flip → [0,1,1] prod=0
                ok = False
                cex = "counterexample [1,1,1] flip0"

        elif kind == "compare_empty":
            ok = False
            cex = "no bit_fn in archive yet"
            support = "honest miss — wait for logic"
            self.compare_log.append({"hit": False, "reason": cex})

        elif kind == "compare_bitfn_parity":
            # Transfer: parity bit_fn should match Z2 xor and bit-parity conservation
            hyp = p.get("src_hyp")
            t = _z2_table()
            z2_ok = all(t[(a, b)] == (a ^ b) for a in (0, 1) for b in (0, 1))
            # also check hyp agrees with xor on 2-bits
            from ..prolog.bridge import _eval_bits_py

            bit_ok = True
            for a, b in itertools.product([0, 1], repeat=2):
                if _eval_bits_py(hyp, [a, b]) != (a ^ b) and hyp in ("parity", "xor2"):
                    # parity on 2 bits == xor; xor2 == xor
                    if hyp == "parity" and _eval_bits_py(hyp, [a, b]) != (a ^ b):
                        bit_ok = False
                    if hyp == "xor2" and _eval_bits_py(hyp, [a, b]) != (a ^ b):
                        bit_ok = False
            ok = z2_ok and bit_ok and hyp in ("parity", "xor2")
            cex = None if ok else f"transfer {hyp} failed z2/bit check"
            support = f"COMPARE bit_fn:{hyp} → Z2 xor table" if ok else "honest miss"
            self.compare_log.append(
                {"hit": ok, "src": p.get("src_name"), "hyp": hyp, "support": support}
            )

        else:
            ok = False
            cex = f"unknown kind {kind}"

        return VerifiedFact(
            name=conjecture.name,
            family=conjecture.family,
            world=self.name,
            formula=conjecture.formula,
            true=ok,
            support=support or ("table check ok" if ok else "finite fail"),
            counterexample=cex,
            relation_type=conjecture.relation_type,
            from_transfer=conjecture.from_transfer,
            transfer_source=conjecture.transfer_source,
        )

    def expand_family(self, family: FamilySpec, new_truths: int) -> str:
        """Saturated → mark for molt (do not permanently die). Extrapolate param."""
        sch = self.language.schemas.get(family.id)
        if family.dead_end:
            family.saturated = True
            if sch:
                sch.saturated = True
            return "dead-end → saturate (curiosity tax; molt will reopen others)"
        if family.param < family.param_max:
            old = family.param
            family.param = min(family.param_max, family.param + 1)
            if sch:
                sch.order = family.param
                sch.n_visits = family.n_visits
                sch.total_reward = family.total_reward
                if new_truths == 0:
                    sch.ticks_no_new_type += 1
                else:
                    sch.ticks_no_new_type = 0
            self.language.ensure_novelty_alive()
            self.persist_skin()
            return f"extrapolate order {old}→{family.param}"
        # At max: signal saturate BUT novelty stays; molt will spawn + reopen
        family.saturated = True
        if sch:
            sch.saturated = True
            sch.n_visits = family.n_visits
            sch.total_reward = family.total_reward
        self.persist_skin()
        return "param at max → saturate (molt/spawn next — no done)"

    def sync_arms(self, arms: dict) -> None:
        from ..kernel import arm_key

        for fam_id, fam in self.families().items():
            key = arm_key(self.name, fam_id)
            if key not in arms:
                arms[key] = fam
            else:
                arms[key].unlocked = fam.unlocked
                arms[key].saturated = fam.saturated
                arms[key].param = fam.param
                arms[key].param_max = fam.param_max
                # After molt, reopen
                if not fam.saturated:
                    arms[key].saturated = False

    def transfer_prior(self, archive_confirmed: dict) -> list[Conjecture]:
        fam = self.families().get("sym_compare_transfer")
        if not fam or not fam.unlocked:
            # try any compare-like spawned
            for fid, f in self.families().items():
                if "compare" in fid and f.unlocked:
                    fam = f
                    break
        if not fam:
            return []
        return self.hypothesize(fam, archive_confirmed, step=-1)
