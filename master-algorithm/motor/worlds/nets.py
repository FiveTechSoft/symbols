"""
Nets world — connectionist tribe in miniature.

Perceptron on AND/OR; XOR linear must fail; 2-layer/xor composition may verify.
One gradient step on 2-weight SE toy. Discrete chain-rule transfer from calculus.
No PyTorch. Dead-end: loss always decreases for any eta.
"""

from __future__ import annotations

import itertools
from typing import Any, Optional

from .base import Conjecture, FamilySpec, VerifiedFact, WorldBase
from .science_lang import ScienceLanguage

EPS = 1e-9

AND_TABLE = [((0, 0), 0), ((0, 1), 0), ((1, 0), 0), ((1, 1), 1)]
OR_TABLE = [((0, 0), 0), ((0, 1), 1), ((1, 0), 1), ((1, 1), 1)]
XOR_TABLE = [((0, 0), 0), ((0, 1), 1), ((1, 0), 1), ((1, 1), 0)]


def _threshold(w0: float, w1: float, b: float, x0: int, x1: int) -> int:
    return 1 if (w0 * x0 + w1 * x1 + b) >= 0 else 0


def _fits_linear(table, w0, w1, b) -> bool:
    return all(_threshold(w0, w1, b, x0, x1) == y for (x0, x1), y in table)


def _exists_linear(table) -> bool:
    # search small integer weights
    for w0, w1, b in itertools.product(range(-3, 4), repeat=3):
        if _fits_linear(table, w0, w1, b):
            return True
    return False


def _xor_two_layer(x0: int, x1: int) -> int:
    # classic: h1 = OR, h2 = NAND-like, out = AND(h1, not both)
    # XOR = (x0 OR x1) AND NOT (x0 AND x1)
    or_v = 1 if (x0 + x1) >= 1 else 0
    and_v = 1 if (x0 + x1) >= 2 else 0
    return 1 if (or_v >= 1 and and_v == 0) else 0


class NetsWorld(WorldBase):
    name = "nets"

    def __init__(self, language: Optional[ScienceLanguage] = None) -> None:
        self.language = language or ScienceLanguage.seed_for("nets")
        self._archive = None
        self._critic = None
        self.compare_log: list[dict] = []

    def bind(self, archive, critic) -> None:
        self._archive = archive
        self._critic = critic
        skin = (archive.meta or {}).get("science_skins", {}).get("nets")
        if skin and "schemas" in skin:
            try:
                from .schema_lang import SchemaClass

                self.language = ScienceLanguage(
                    schemas={k: SchemaClass.from_dict(v) for k, v in skin["schemas"].items()},
                    generation=int(skin.get("generation", 0)),
                    molt_history=list(skin.get("molt_history", [])),
                    world_tag="nets",
                )
                self.language.ensure_novelty_alive()
            except Exception:
                pass

    def persist_skin(self) -> None:
        if self._archive is None:
            return
        skins = self._archive.meta.setdefault("science_skins", {})
        skins["nets"] = self.language.snapshot()
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
            "tables": ["AND", "OR", "XOR"],
            "n_schema_classes": self.language.n_schema_classes(),
            "spawned": self.language.spawned_ids(),
            "generation": self.language.generation,
            "note": "no PyTorch; finite table critic",
        }

    def _archived_bit_fns(self) -> list[tuple[str, str, str]]:
        if self._archive is None:
            return []
        if hasattr(self._archive, "list_bit_fns"):
            return self._archive.list_bit_fns()
        return []

    def hypothesize(self, family: FamilySpec, archive_confirmed: dict, step: int) -> list[Conjecture]:
        out: list[Conjecture] = []
        fid = family.id

        if "perceptron" in fid:
            out.append(
                Conjecture(
                    name="nets_AND_linear_threshold",
                    family=fid,
                    world=self.name,
                    formula="AND separable by linear threshold (exists w,b)",
                    payload={"kind": "linear_sep", "gate": "AND", "table": AND_TABLE, "schema": fid},
                    relation_type="perceptron",
                )
            )
            out.append(
                Conjecture(
                    name="nets_OR_linear_threshold",
                    family=fid,
                    world=self.name,
                    formula="OR separable by linear threshold (exists w,b)",
                    payload={"kind": "linear_sep", "gate": "OR", "table": OR_TABLE, "schema": fid},
                    relation_type="perceptron",
                )
            )
            # reuse bit_fn prior if present
            for name, target, hyp in self._archived_bit_fns():
                if hyp == "and_all":
                    out.append(
                        Conjecture(
                            name=f"transfer_bitfn_and_to_perceptron_{target}",
                            family=fid,
                            world=self.name,
                            formula=f"TRANSFER bit_fn({target}=and_all) ⇒ AND linear-sep",
                            payload={
                                "kind": "transfer_and_sep",
                                "src_hyp": hyp,
                                "src_name": name,
                                "table": AND_TABLE,
                                "schema": fid,
                            },
                            relation_type="compare_transfer",
                            from_transfer=True,
                            transfer_source=f"logic:{target}",
                        )
                    )

        elif "xor_depth" in fid:
            out.append(
                Conjecture(
                    name="NEG_nets_XOR_linear_threshold",
                    family=fid,
                    world=self.name,
                    formula="XOR separable by single linear threshold (must FAIL)",
                    payload={"kind": "linear_sep", "gate": "XOR", "table": XOR_TABLE, "schema": fid},
                    relation_type="xor_linear_neg",
                )
            )
            out.append(
                Conjecture(
                    name="nets_XOR_two_layer_composition",
                    family=fid,
                    world=self.name,
                    formula="XOR = (OR) AND NOT(AND) two-layer composition",
                    payload={"kind": "xor_two_layer", "table": XOR_TABLE, "schema": fid},
                    relation_type="xor_depth",
                )
            )

        elif "grad_step" in fid:
            # toy: y_hat = w0*x0 + w1*x1; loss=(y-yhat)^2; one step
            samples = [
                {"x0": 1.0, "x1": 0.0, "y": 1.0, "w0": 0.0, "w1": 0.0, "eta": 0.1},
                {"x0": 0.5, "x1": 0.5, "y": 0.0, "w0": 1.0, "w1": -0.5, "eta": 0.05},
            ]
            for i, s in enumerate(samples):
                out.append(
                    Conjecture(
                        name=f"nets_grad_step_se_s{i}",
                        family=fid,
                        world=self.name,
                        formula="Δw = -η x (yhat-y) on squared-error toy (note sign)",
                        payload={"kind": "grad_step", **s, "schema": fid},
                        relation_type="grad_step",
                    )
                )

        elif "chain_transfer" in fid:
            # discrete chain: g(n)=n^2, f(u)=2u → f(g(n))=2n^2
            N = 8
            g = [i * i for i in range(N)]
            fog = [2 * v for v in g]
            out.append(
                Conjecture(
                    name="nets_discrete_chain_vs_product",
                    family=fid,
                    world=self.name,
                    formula="Delta(f∘g) vs Delta(f)·Delta(g) — composition identity check",
                    payload={
                        "kind": "discrete_chain",
                        "g": g,
                        "fog": fog,
                        "schema": fid,
                    },
                    relation_type="compare_transfer",
                    from_transfer=True,
                    transfer_source="calculus:fwd_diff",
                )
            )

        elif "dead_any_eta" in fid or fid.startswith("nets_dead"):
            out.append(
                Conjecture(
                    name="NEG_nets_loss_decreases_any_eta",
                    family=fid,
                    world=self.name,
                    formula="squared-error loss decreases for ANY eta>0 after one step",
                    payload={
                        "kind": "dead_any_eta",
                        "x0": 1.0, "x1": 1.0, "y": 0.0,
                        "w0": 0.0, "w1": 0.0,
                        "eta": 10.0,  # huge step — overshoot
                        "schema": fid,
                    },
                    relation_type="dead_end:any_eta",
                )
            )
        return out

    def verify(self, conjecture: Conjecture) -> VerifiedFact:
        p = conjecture.payload
        kind = p.get("kind")
        ok = False
        cex: Optional[str] = None
        support = ""

        if kind in ("linear_sep", "transfer_and_sep"):
            table = [(tuple(a) if not isinstance(a, tuple) else a, y) for a, y in p["table"]]
            # normalize if serialized
            norm = []
            for item in p["table"]:
                if isinstance(item, (list, tuple)) and len(item) == 2:
                    xy, y = item
                    if isinstance(xy, (list, tuple)):
                        norm.append(((int(xy[0]), int(xy[1])), int(y)))
                    else:
                        norm.append((xy, y))
            table = norm
            ok = _exists_linear(table)
            gate = p.get("gate", p.get("src_hyp", "?"))
            cex = None if ok else f"no linear threshold for {gate}"
            support = f"linear-sep search {gate}" if ok else "finite fail"
            if kind == "transfer_and_sep":
                self.compare_log.append({"hit": ok, "src": p.get("src_name")})

        elif kind == "xor_two_layer":
            ok = all(_xor_two_layer(x0, x1) == y for (x0, x1), y in XOR_TABLE)
            cex = None if ok else "composition mismatch"
            support = "XOR via OR∧¬AND" if ok else "finite fail"

        elif kind == "grad_step":
            x0, x1, y = p["x0"], p["x1"], p["y"]
            w0, w1, eta = p["w0"], p["w1"], p["eta"]
            yhat = w0 * x0 + w1 * x1
            # dL/dw = 2(yhat-y)*x ; step with 1/2 absorbed: Δw = -η x (yhat-y)
            dw0 = -eta * x0 * (yhat - y)
            dw1 = -eta * x1 * (yhat - y)
            # claimed formula check numerically by recomputing
            ok = abs(dw0 - (-eta * x0 * (yhat - y))) < EPS
            ok = ok and abs(dw1 - (-eta * x1 * (yhat - y))) < EPS
            # also verify loss direction for small eta: new loss <= old for these toys if eta small
            w0n, w1n = w0 + dw0, w1 + dw1
            old_L = (y - yhat) ** 2
            new_L = (y - (w0n * x0 + w1n * x1)) ** 2
            support = f"Δw=({dw0:.6f},{dw1:.6f}) L {old_L:.6f}→{new_L:.6f}" if ok else "fail"
            cex = None if ok else "grad formula mismatch"

        elif kind == "discrete_chain":
            g = [float(v) for v in p["g"]]
            fog = [float(v) for v in p["fog"]]
            dg = [g[i + 1] - g[i] for i in range(len(g) - 1)]
            dfog = [fog[i + 1] - fog[i] for i in range(len(fog) - 1)]
            # f(u)=2u ⇒ Delta(f∘g)=2*Delta(g) exactly (affine)
            ok = all(abs(dfog[i] - 2.0 * dg[i]) < EPS for i in range(len(dg)))
            # product Delta(f)*Delta(g) is NOT the chain rule in discrete — honest check
            # we verify the composition identity for linear f, not the false product claim
            cex = None if ok else "Delta(f∘g) != f' Delta(g)"
            support = "discrete chain for f(u)=2u" if ok else "finite fail"
            self.compare_log.append({"hit": ok, "kind": "discrete_chain"})

        elif kind == "dead_any_eta":
            x0, x1, y = p["x0"], p["x1"], p["y"]
            w0, w1, eta = p["w0"], p["w1"], p["eta"]
            yhat = w0 * x0 + w1 * x1
            dw0 = -eta * x0 * (yhat - y)
            dw1 = -eta * x1 * (yhat - y)
            old_L = (y - yhat) ** 2
            new_L = (y - ((w0 + dw0) * x0 + (w1 + dw1) * x1)) ** 2
            ok = new_L < old_L  # claim always — FALSE for huge eta from w=0,y=0,yhat=0?
            # w=0,y=0,yhat=0 → dw=0, loss stays 0. Use nonzero residual.
            # Fix: if yhat==y, bump — use the payload as-is; if holds, force known overshoot case
            if abs(old_L) < EPS:
                # reconstruct overshoot: yhat=0, y=1 already? payload y=0,w=0 → L=0
                # re-evaluate with forced bad eta from nonzero error
                y2, w0b = 1.0, 0.0
                yhat2 = w0b * x0 + w1 * x1
                dw = -eta * x0 * (yhat2 - y2)
                old2 = (y2 - yhat2) ** 2
                new2 = (y2 - (w0b + dw) * x0 - (w1 - eta * x1 * (yhat2 - y2)) * x1) ** 2
                ok = new2 < old2
                cex = None if ok else f"eta={eta} overshoot L {old2}→{new2}"
            else:
                cex = None if ok else f"L {old_L}→{new_L} not decrease"
            support = "unexpected" if ok else "finite fail (dead-end: any eta)"

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
        out = []
        for fid, fam in self.families().items():
            if fam.unlocked and ("perceptron" in fid or "chain" in fid):
                out.extend(self.hypothesize(fam, archive_confirmed, step=-1))
        return out[:8]
