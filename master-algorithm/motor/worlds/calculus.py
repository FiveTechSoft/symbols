"""
Calculus world — discrete Delta / sum on sequences + polynomials.

Transfer: rec/2 from sequences as prior for Delta of that sequence.
Dead-end: claim Delta is always zero.
"""

from __future__ import annotations

from typing import Any, Optional

from .base import Conjecture, FamilySpec, VerifiedFact, WorldBase
from .science_lang import ScienceLanguage

EPS = 1e-9


def _fwd_diff(xs: list[float]) -> list[float]:
    return [xs[i + 1] - xs[i] for i in range(len(xs) - 1)]


def _prefix_sum(xs: list[float]) -> list[float]:
    out = [0.0]
    s = 0.0
    for x in xs:
        s += x
        out.append(s)
    return out  # S[0]=0, S[k]=sum(xs[:k])


def _fib(n: int) -> list[int]:
    a = [0] * (n + 1)
    if n >= 1:
        a[1] = 1
    for i in range(2, n + 1):
        a[i] = a[i - 1] + a[i - 2]
    return a


def _lucas(n: int) -> list[int]:
    a = [0] * (n + 1)
    if n >= 0:
        a[0] = 2
    if n >= 1:
        a[1] = 1
    for i in range(2, n + 1):
        a[i] = a[i - 1] + a[i - 2]
    return a


def _pow_seq(n: int, power: int) -> list[int]:
    return [i ** power for i in range(n + 1)]


class CalculusWorld(WorldBase):
    name = "calculus"

    def __init__(self, language: Optional[ScienceLanguage] = None) -> None:
        self.language = language or ScienceLanguage.seed_for("calculus")
        self._archive = None
        self._critic = None
        self.compare_log: list[dict] = []

    def bind(self, archive, critic) -> None:
        self._archive = archive
        self._critic = critic
        skin = (archive.meta or {}).get("science_skins", {}).get("calculus")
        if skin and "schemas" in skin:
            try:
                from .schema_lang import SchemaClass

                self.language = ScienceLanguage(
                    schemas={k: SchemaClass.from_dict(v) for k, v in skin["schemas"].items()},
                    generation=int(skin.get("generation", 0)),
                    molt_history=list(skin.get("molt_history", [])),
                    world_tag="calculus",
                )
                self.language.ensure_novelty_alive()
            except Exception:
                pass

    def persist_skin(self) -> None:
        if self._archive is None:
            return
        skins = self._archive.meta.setdefault("science_skins", {})
        skins["calculus"] = self.language.snapshot()
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
            "ops": ["fwd_diff", "prefix_sum", "power_diff"],
            "n_schema_classes": self.language.n_schema_classes(),
            "spawned": self.language.spawned_ids(),
            "generation": self.language.generation,
        }

    def _archived_recs(self) -> list[tuple[str, list[int]]]:
        if self._archive is None:
            return []
        if hasattr(self._archive, "list_recs"):
            return self._archive.list_recs()
        return []

    def hypothesize(self, family: FamilySpec, archive_confirmed: dict, step: int) -> list[Conjecture]:
        out: list[Conjecture] = []
        fid = family.id
        N = max(4, family.param)

        if "fwd_diff" in fid:
            for name, seq in (("fib", _fib(N)), ("lucas", _lucas(N))):
                out.append(
                    Conjecture(
                        name=f"calc_delta_{name}_N{N}",
                        family=fid,
                        world=self.name,
                        formula=f"Delta({name})[n] = {name}[n+1]-{name}[n] well-defined",
                        payload={"kind": "delta_defined", "seq": name, "vals": seq, "schema": fid, "order": N},
                        relation_type="fwd_diff",
                    )
                )
            # fib identity: Delta F_n = F_{n-1} for n>=1? F_{n+1}-F_n = F_{n-1}
            fib = _fib(N)
            out.append(
                Conjecture(
                    name=f"calc_delta_fib_eq_prev_N{N}",
                    family=fid,
                    world=self.name,
                    formula="Delta(fib)[n] = fib[n-1] for n>=1",
                    payload={"kind": "delta_fib_prev", "vals": fib, "schema": fid, "order": N},
                    relation_type="fwd_diff",
                )
            )

        elif "ft_discrete" in fid:
            for power in (0, 1, 2):
                f = [float(i ** power) for i in range(N)]
                out.append(
                    Conjecture(
                        name=f"calc_ft_x{power}_N{N}",
                        family=fid,
                        world=self.name,
                        formula=f"Delta(sum x^{power}) = x^{power} on prefix",
                        payload={"kind": "ft_discrete", "f": f, "schema": fid, "order": N, "power": power},
                        relation_type="ft_discrete",
                    )
                )

        elif "power_diff" in fid:
            for power in range(1, min(family.param, 5) + 1):
                xs = _pow_seq(N, power)
                out.append(
                    Conjecture(
                        name=f"calc_delta_x{power}_vs_n_x{power-1}_N{N}",
                        family=fid,
                        world=self.name,
                        formula=f"Delta(x^{power}) / (power * x^{power-1}) → 1 (eps on lattice)",
                        payload={
                            "kind": "power_diff",
                            "power": power,
                            "vals": xs,
                            "schema": fid,
                            "order": N,
                        },
                        relation_type="power_diff",
                    )
                )

        elif "transfer_rec" in fid:
            recs = self._archived_recs()
            if not recs:
                out.append(
                    Conjecture(
                        name="calc_transfer_wait_no_rec",
                        family=fid,
                        world=self.name,
                        formula="no rec/2 prior — honest miss",
                        payload={"kind": "transfer_empty", "schema": fid},
                        relation_type="compare_miss",
                        from_transfer=True,
                        transfer_source="sequences",
                    )
                )
            for seq, coeffs in recs:
                if seq not in ("fib", "lucas"):
                    continue
                vals = _fib(N) if seq == "fib" else _lucas(N)
                out.append(
                    Conjecture(
                        name=f"transfer_rec_{seq}_to_delta_N{N}",
                        family=fid,
                        world=self.name,
                        formula=f"TRANSFER rec({seq},{coeffs}) ⇒ Delta structure matches recurrence",
                        payload={
                            "kind": "transfer_rec_delta",
                            "seq": seq,
                            "coeffs": coeffs,
                            "vals": vals,
                            "schema": fid,
                            "order": N,
                        },
                        relation_type="compare_transfer",
                        from_transfer=True,
                        transfer_source=f"sequences:{seq}",
                    )
                )

        elif "dead_zero" in fid or fid.startswith("calc_dead"):
            fib = _fib(N)
            out.append(
                Conjecture(
                    name="NEG_calc_delta_always_zero",
                    family=fid,
                    world=self.name,
                    formula="Delta(fib)=0 always",
                    payload={"kind": "dead_zero", "vals": fib, "schema": fid},
                    relation_type="dead_end:delta0",
                )
            )
        return out

    def verify(self, conjecture: Conjecture) -> VerifiedFact:
        p = conjecture.payload
        kind = p.get("kind")
        ok = False
        cex: Optional[str] = None
        support = ""

        if kind == "delta_defined":
            vals = p["vals"]
            d = _fwd_diff([float(v) for v in vals])
            ok = len(d) == len(vals) - 1
            support = f"|Delta|={len(d)}" if ok else "fail"
            cex = None if ok else "len mismatch"

        elif kind == "delta_fib_prev":
            vals = p["vals"]
            ok = True
            for n in range(1, len(vals) - 1):
                if vals[n + 1] - vals[n] != vals[n - 1]:
                    ok = False
                    cex = f"n={n}: Delta={vals[n+1]-vals[n]} != fib[{n-1}]={vals[n-1]}"
                    break
            support = "Delta F_n = F_{n-1}" if ok else "finite fail"

        elif kind == "ft_discrete":
            f = p["f"]
            S = _prefix_sum(f)  # S[k]=sum(f[:k])
            # Delta(S)[k] = S[k+1]-S[k] = f[k]
            dS = _fwd_diff(S)
            ok = len(dS) == len(f) and all(abs(dS[i] - f[i]) < EPS for i in range(len(f)))
            cex = None if ok else "Delta(sum) != f"
            support = "discrete FTC on prefix" if ok else "finite fail"

        elif kind == "power_diff":
            power = int(p["power"])
            vals = [float(v) for v in p["vals"]]
            d = _fwd_diff(vals)
            # at x>=1: Delta(x^p) / (p * x^{p-1}) ≈ 1 + O(1/x)
            ok = True
            checked = 0
            for x in range(1, len(vals) - 1):
                denom = power * (x ** (power - 1))
                if denom == 0:
                    continue
                ratio = d[x] / denom
                # exact for p=1: Delta=1, denom=1
                # for p>=2 allow growing error bound: |ratio-1| < (2^p)/x roughly
                if power == 1:
                    if abs(ratio - 1.0) > EPS:
                        ok = False
                        cex = f"x={x} ratio={ratio}"
                        break
                else:
                    # check polynomial identity: (x+1)^p - x^p == sum binom
                    expected = (x + 1) ** power - x ** power
                    if abs(d[x] - expected) > EPS:
                        ok = False
                        cex = f"x={x} delta={d[x]} != {expected}"
                        break
                    # leading term check: ratio → 1
                    if x >= 3 and abs(ratio - 1.0) > 0.5 * power:
                        ok = False
                        cex = f"x={x} ratio={ratio} far from 1"
                        break
                checked += 1
            support = f"power={power} checks={checked}" if ok else "finite fail"
            if checked == 0:
                ok = False
                cex = "no checks"

        elif kind == "transfer_empty":
            ok = False
            cex = "no rec/2 in archive"
            support = "honest miss"
            self.compare_log.append({"hit": False, "reason": cex})

        elif kind == "transfer_rec_delta":
            vals = p["vals"]
            coeffs = p["coeffs"]
            # If rec is order-2 [1,1]: s[n]=s[n-1]+s[n-2]
            # then Delta[n]=s[n+1]-s[n]=s[n]-s[n-1]=Delta[n-1] ... actually
            # s[n+1]-s[n] = (s[n]+s[n-1])-s[n] = s[n-1] for [1,1]
            ok = True
            if list(coeffs) == [1, 1]:
                for n in range(1, len(vals) - 1):
                    if vals[n + 1] - vals[n] != vals[n - 1]:
                        ok = False
                        cex = f"n={n}"
                        break
                support = f"TRANSFER rec({p['seq']},[1,1])→Delta=prev" if ok else "transfer fail"
            else:
                # general: check recurrence holds ⇒ Delta consistent with it
                order = len(coeffs)
                for n in range(order, len(vals)):
                    pred = sum(coeffs[i] * vals[n - 1 - i] for i in range(order))
                    if pred != vals[n]:
                        ok = False
                        cex = f"rec fail at n={n}"
                        break
                if ok and len(vals) > order + 1:
                    support = f"TRANSFER rec({p['seq']},{coeffs}) holds ⇒ Delta well-posed"
                elif ok:
                    support = "rec holds (short)"
            self.compare_log.append({"hit": ok, "seq": p.get("seq"), "coeffs": coeffs})

        elif kind == "dead_zero":
            d = _fwd_diff([float(v) for v in p["vals"]])
            ok = all(abs(x) < EPS for x in d)
            cex = None if ok else f"Delta[0]={d[0]} != 0"
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
            return f"extrapolate N {old}→{family.param}"
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
