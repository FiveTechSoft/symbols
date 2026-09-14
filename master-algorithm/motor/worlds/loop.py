"""
Loop world — minimal closed loop: observe → act → critic.

Toy 1D agent in a light/target field. Hidden law: next_x = x + a (Δx = action).
Brightness falls with |x - x*|. Critic gates prediction error and taxis improvement
on held-out start positions (no overfitting one trajectory).
"""

from __future__ import annotations

import random
from typing import Any, Callable, Optional

from .base import Conjecture, FamilySpec, VerifiedFact, WorldBase
from .science_lang import ScienceLanguage

EPS = 1e-9
PRED_ERR_THRESH = 1e-9
TARGET = 0.0
FIELD_SCALE = 8.0


def brightness(x: float, x_star: float = TARGET, scale: float = FIELD_SCALE) -> float:
    return max(0.0, 1.0 - abs(x - x_star) / scale)


def true_step(x: float, a: float) -> float:
    """Hidden dynamics: discrete Δ(position) = action."""
    return x + a


def rollout(
    x0: float,
    policy: Callable[[float, float], float],
    T: int,
    x_star: float = TARGET,
) -> list[dict[str, float]]:
    """Observe → act → observe for T steps. Returns trajectory rows."""
    rows = []
    x = float(x0)
    for _ in range(T):
        b = brightness(x, x_star)
        a = float(policy(x, b))
        x_next = true_step(x, a)
        rows.append({"x": x, "b": b, "a": a, "x_next": x_next, "err": abs(x - x_star)})
        x = x_next
    rows.append(
        {
            "x": x,
            "b": brightness(x, x_star),
            "a": 0.0,
            "x_next": x,
            "err": abs(x - x_star),
        }
    )
    return rows


def err_reduction(traj: list[dict[str, float]]) -> float:
    """Initial |error| minus final |error| (positive = improved)."""
    return traj[0]["err"] - traj[-1]["err"]


# --- policies ---

def policy_bangbang(x: float, b: float) -> float:
    """Bang-bang taxis toward target at 0."""
    if abs(x) < 1e-12:
        return 0.0
    return -1.0 if x > 0 else 1.0


def policy_linear_gain(k: float = 0.5) -> Callable[[float, float], float]:
    def _p(x: float, b: float) -> float:
        a = -k * x
        # clip to discrete-ish steps for stability
        if a > 1:
            return 1.0
        if a < -1:
            return -1.0
        return a

    return _p


def policy_always_right(x: float, b: float) -> float:
    return 1.0


def policy_always_left(x: float, b: float) -> float:
    return -1.0


def policy_random_walk(seed: int) -> Callable[[float, float], float]:
    rng = random.Random(seed)

    def _p(x: float, b: float) -> float:
        return float(rng.choice([-1, 0, 1]))

    return _p


class LoopWorld(WorldBase):
    name = "loop"

    def __init__(self, language: Optional[ScienceLanguage] = None) -> None:
        self.language = language or ScienceLanguage.seed_for("loop")
        self._archive = None
        self._critic = None
        self.compare_log: list[dict] = []
        self.last_metrics: dict[str, Any] = {}

    def bind(self, archive, critic) -> None:
        self._archive = archive
        self._critic = critic
        skin = (archive.meta or {}).get("science_skins", {}).get("loop")
        if skin and "schemas" in skin:
            try:
                from .schema_lang import SchemaClass

                self.language = ScienceLanguage(
                    schemas={k: SchemaClass.from_dict(v) for k, v in skin["schemas"].items()},
                    generation=int(skin.get("generation", 0)),
                    molt_history=list(skin.get("molt_history", [])),
                    world_tag="loop",
                )
                self.language.ensure_novelty_alive()
                self.language.merge_missing_seeds()
            except Exception:
                pass
        else:
            if hasattr(self.language, "merge_missing_seeds"):
                self.language.merge_missing_seeds()

    def persist_skin(self) -> None:
        if self._archive is None:
            return
        skins = self._archive.meta.setdefault("science_skins", {})
        skins["loop"] = self.language.snapshot()
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
        x0 = 3.0
        traj = rollout(x0, policy_bangbang, T=4)
        return {
            "closed_loop": "observe→act→critic",
            "x0": x0,
            "brightness_x0": brightness(x0),
            "err_before": traj[0]["err"],
            "err_after_bangbang_T4": traj[-1]["err"],
            "hidden_law": "next_x = x + a",
            "n_schema_classes": self.language.n_schema_classes(),
            "generation": self.language.generation,
            **self.last_metrics,
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
        n = max(3, family.param)
        seed = 7000 + step * 11 + n
        train_starts = [-4.0, -2.0, 3.0, 5.0][:n]
        heldout_starts = [-6.0, 1.5, 7.0, -3.5]

        if "pred" in fid:
            # True law: Δx = a
            out.append(
                Conjecture(
                    name=f"loop_pred_dx_eq_action_s{seed}",
                    family=fid,
                    world=self.name,
                    formula="next_x = x + a (Δ(position)=action) on held-out starts",
                    payload={
                        "kind": "pred_dx_eq_a",
                        "train_starts": train_starts,
                        "heldout_starts": heldout_starts,
                        "T": 5,
                        "schema": fid,
                        "order": n,
                    },
                    relation_type="prediction",
                )
            )
            # Wrong law: ignore action
            out.append(
                Conjecture(
                    name=f"loop_pred_ignore_action_s{seed}",
                    family=fid,
                    world=self.name,
                    formula="next_x = x (ignore action) — false",
                    payload={
                        "kind": "pred_ignore_a",
                        "train_starts": train_starts,
                        "heldout_starts": heldout_starts,
                        "T": 5,
                        "schema": fid,
                    },
                    relation_type="prediction_neg",
                )
            )
            # Wrong law: next_x = x + 2a
            out.append(
                Conjecture(
                    name=f"loop_pred_double_action_s{seed}",
                    family=fid,
                    world=self.name,
                    formula="next_x = x + 2a — false",
                    payload={
                        "kind": "pred_double_a",
                        "train_starts": train_starts,
                        "heldout_starts": heldout_starts,
                        "T": 5,
                        "schema": fid,
                    },
                    relation_type="prediction_neg",
                )
            )
            # Overfit: claim constant next from one trajectory only
            out.append(
                Conjecture(
                    name=f"loop_pred_overfit_one_x0_s{seed}",
                    family=fid,
                    world=self.name,
                    formula="overfit one x0: claim next always = x0+1 (fails new start)",
                    payload={
                        "kind": "pred_overfit_one",
                        "fit_x0": 2.0,
                        "heldout_starts": heldout_starts,
                        "claimed_next": 3.0,
                        "schema": fid,
                    },
                    relation_type="prediction_overfit",
                )
            )

        elif "taxis" in fid:
            out.append(
                Conjecture(
                    name=f"loop_taxis_bangbang_s{seed}",
                    family=fid,
                    world=self.name,
                    formula="bang-bang toward 0 reduces |error| over T on held-out x0",
                    payload={
                        "kind": "taxis_bangbang",
                        "heldout_starts": heldout_starts,
                        "T": 8,
                        "schema": fid,
                        "order": n,
                    },
                    relation_type="taxis",
                )
            )
            out.append(
                Conjecture(
                    name=f"loop_taxis_linear_gain_s{seed}",
                    family=fid,
                    world=self.name,
                    formula="a=-k*x (k=0.5) reduces |error| over T on held-out x0",
                    payload={
                        "kind": "taxis_linear",
                        "k": 0.5,
                        "heldout_starts": heldout_starts,
                        "T": 10,
                        "schema": fid,
                    },
                    relation_type="taxis",
                )
            )
            # False: always-right increases |error| from positive x0
            out.append(
                Conjecture(
                    name=f"loop_taxis_always_right_s{seed}",
                    family=fid,
                    world=self.name,
                    formula="always a=+1 improves taxis (false — increases error)",
                    payload={
                        "kind": "taxis_always_right",
                        "heldout_starts": [2.0, 4.0, 6.0],
                        "T": 5,
                        "schema": fid,
                    },
                    relation_type="taxis_neg",
                )
            )
            out.append(
                Conjecture(
                    name=f"loop_taxis_always_left_s{seed}",
                    family=fid,
                    world=self.name,
                    formula="always a=-1 improves taxis from negative starts (false)",
                    payload={
                        "kind": "taxis_always_left",
                        "heldout_starts": [-2.0, -4.0, -5.0],
                        "T": 5,
                        "schema": fid,
                    },
                    relation_type="taxis_neg",
                )
            )

        elif "transfer" in fid:
            # Honest hit: conservation Δ=0 shape → Δx - a = 0
            out.append(
                Conjecture(
                    name="transfer_conserv_delta0_to_loop_dx_eq_a",
                    family=fid,
                    world=self.name,
                    formula="TRANSFER linear-Δ=0 ⇒ Δ(position)-action=0 (next_x=x+a)",
                    payload={
                        "kind": "transfer_delta0_dx_a",
                        "heldout_starts": heldout_starts,
                        "T": 6,
                        "schema": fid,
                        "src_form": "conserv_delta0",
                    },
                    relation_type="transfer_form",
                    from_transfer=True,
                    transfer_source="conserv:delta0",
                )
            )
            # Try rec [1,1] as prior — mostly miss on position control
            recs = self._archived_recs()
            fib11 = any(seq == "fib" and list(c) == [1, 1] for seq, c in recs)
            out.append(
                Conjecture(
                    name="transfer_rec11_to_loop_position",
                    family=fid,
                    world=self.name,
                    formula="TRANSFER rec(fib,[1,1]) ⇒ position obeys Fib recurrence (false analogy)",
                    payload={
                        "kind": "transfer_rec11_miss",
                        "has_fib11": fib11 or True,  # try shape even if absent
                        "heldout_starts": heldout_starts,
                        "schema": fid,
                        "src_form": "rec_fib_11",
                    },
                    relation_type="transfer_miss",
                    from_transfer=True,
                    transfer_source="sequences:fib[1,1]",
                )
            )
            # Wrong: claim Δx = 0 (conservation of position) while acting
            out.append(
                Conjecture(
                    name="transfer_conserv_to_loop_position_frozen",
                    family=fid,
                    world=self.name,
                    formula="TRANSFER Δ=0 ⇒ position conserved under nonzero action (false)",
                    payload={
                        "kind": "transfer_pos_frozen",
                        "heldout_starts": heldout_starts,
                        "T": 4,
                        "schema": fid,
                        "src_form": "conserv_delta0",
                    },
                    relation_type="transfer_miss",
                    from_transfer=True,
                    transfer_source="conserv:delta0_misapplied",
                )
            )

        elif "dead" in fid:
            out.append(
                Conjecture(
                    name="NEG_loop_policy_increases_error",
                    family=fid,
                    world=self.name,
                    formula="claim always-right reduces |error| from +x (dead-end)",
                    payload={
                        "kind": "dead_increase",
                        "heldout_starts": [3.0, 5.0],
                        "T": 4,
                        "schema": fid,
                    },
                    relation_type="dead_end:bad_policy",
                )
            )

        return out

    def verify(self, conjecture: Conjecture) -> VerifiedFact:
        p = conjecture.payload
        kind = p.get("kind")
        ok = False
        cex: Optional[str] = None
        support = ""

        if kind == "pred_dx_eq_a":
            # Critic: prediction error on held-out starts under random actions
            rng = random.Random(42)
            max_err = 0.0
            n_checks = 0
            for x0 in p["heldout_starts"]:
                x = float(x0)
                for _ in range(p.get("T", 5)):
                    a = float(rng.choice([-1, 0, 1]))
                    pred = x + a
                    obs = true_step(x, a)
                    err = abs(pred - obs)
                    max_err = max(max_err, err)
                    n_checks += 1
                    x = obs
            ok = max_err < PRED_ERR_THRESH
            support = f"pred_err_max={max_err:.2e} n={n_checks} heldout={p['heldout_starts']}"
            cex = None if ok else support
            self.last_metrics["pred_err_max"] = max_err
            self.compare_log.append({"kind": kind, "hit": ok, "max_err": max_err})

        elif kind == "pred_ignore_a":
            rng = random.Random(43)
            # Claim next_x = x. Should FAIL whenever a ≠ 0.
            fails = []
            for x0 in p["heldout_starts"]:
                a = 1.0
                pred = float(x0)  # ignore a
                obs = true_step(float(x0), a)
                if abs(pred - obs) >= PRED_ERR_THRESH:
                    fails.append(f"x0={x0} a={a} pred={pred} obs={obs}")
            ok = len(fails) == 0  # claim holds everywhere — should be False
            support = "unexpected hold" if ok else f"finite fail: {fails[0]}"
            cex = None if ok else support

        elif kind == "pred_double_a":
            fails = []
            for x0 in p["heldout_starts"]:
                a = 1.0
                pred = float(x0) + 2 * a
                obs = true_step(float(x0), a)
                if abs(pred - obs) >= PRED_ERR_THRESH:
                    fails.append(f"x0={x0} pred={pred} obs={obs}")
            ok = len(fails) == 0
            support = "unexpected" if ok else f"finite fail: {fails[0]}"
            cex = None if ok else support

        elif kind == "pred_overfit_one":
            # Fitted on one x0 with a=+1 → claimed_next; fails on new start
            fit_x0 = p["fit_x0"]
            claimed = p["claimed_next"]
            # "law" that always predicts claimed regardless of state
            fails = []
            for x0 in p["heldout_starts"]:
                a = 1.0
                pred = claimed  # overfit constant
                obs = true_step(float(x0), a)
                if abs(pred - obs) >= PRED_ERR_THRESH:
                    fails.append(f"new_x0={x0} pred={pred} obs={obs}")
            ok = len(fails) == 0  # should FAIL
            support = "overfit held on all" if ok else f"fails new x0: {fails[0]}"
            cex = None if ok else support
            self.compare_log.append({"kind": kind, "hit": ok, "fails": fails[:2]})

        elif kind == "taxis_bangbang":
            improvements = []
            for x0 in p["heldout_starts"]:
                traj = rollout(float(x0), policy_bangbang, T=p.get("T", 8))
                improvements.append(
                    {
                        "x0": x0,
                        "err_before": traj[0]["err"],
                        "err_after": traj[-1]["err"],
                        "delta": err_reduction(traj),
                    }
                )
            ok = all(r["delta"] > 0 or r["err_before"] < 1e-12 for r in improvements)
            support = (
                f"taxis bangbang heldout: "
                + "; ".join(
                    f"x0={r['x0']} {r['err_before']:.3g}→{r['err_after']:.3g}" for r in improvements
                )
            )
            cex = None if ok else support
            self.last_metrics["taxis_bangbang"] = improvements
            self.compare_log.append({"kind": kind, "hit": ok, "rows": improvements})

        elif kind == "taxis_linear":
            pol = policy_linear_gain(p.get("k", 0.5))
            improvements = []
            for x0 in p["heldout_starts"]:
                traj = rollout(float(x0), pol, T=p.get("T", 10))
                improvements.append(
                    {
                        "x0": x0,
                        "err_before": traj[0]["err"],
                        "err_after": traj[-1]["err"],
                        "delta": err_reduction(traj),
                    }
                )
            ok = all(r["delta"] > 0 or r["err_before"] < 1e-12 for r in improvements)
            support = (
                "taxis linear: "
                + "; ".join(
                    f"x0={r['x0']} {r['err_before']:.3g}→{r['err_after']:.3g}" for r in improvements
                )
            )
            cex = None if ok else support
            self.last_metrics["taxis_linear"] = improvements
            self.compare_log.append({"kind": kind, "hit": ok, "rows": improvements})

        elif kind == "taxis_always_right":
            # Claim: always-right reduces |error|. From +x0 it INCREASES — reject.
            rows = []
            for x0 in p["heldout_starts"]:
                traj = rollout(float(x0), policy_always_right, T=p.get("T", 5))
                rows.append(
                    {
                        "x0": x0,
                        "err_before": traj[0]["err"],
                        "err_after": traj[-1]["err"],
                        "delta": err_reduction(traj),
                    }
                )
            # Claim "improves" — true only if all deltas > 0
            ok = all(r["delta"] > 0 for r in rows)
            support = (
                "unexpected improve"
                if ok
                else "increases error: "
                + "; ".join(f"x0={r['x0']} {r['err_before']:.3g}→{r['err_after']:.3g}" for r in rows)
            )
            cex = None if ok else support

        elif kind == "taxis_always_left":
            rows = []
            for x0 in p["heldout_starts"]:
                traj = rollout(float(x0), policy_always_left, T=p.get("T", 5))
                rows.append(
                    {
                        "x0": x0,
                        "err_before": traj[0]["err"],
                        "err_after": traj[-1]["err"],
                        "delta": err_reduction(traj),
                    }
                )
            ok = all(r["delta"] > 0 for r in rows)
            support = (
                "unexpected"
                if ok
                else "increases error: "
                + "; ".join(f"x0={r['x0']} {r['err_before']:.3g}→{r['err_after']:.3g}" for r in rows)
            )
            cex = None if ok else support

        elif kind == "transfer_delta0_dx_a":
            # Δx - a = 0 on held-out rollouts under varied actions
            rng = random.Random(99)
            max_resid = 0.0
            for x0 in p["heldout_starts"]:
                x = float(x0)
                for _ in range(p.get("T", 6)):
                    a = float(rng.choice([-1, 0, 1]))
                    x_next = true_step(x, a)
                    resid = abs((x_next - x) - a)
                    max_resid = max(max_resid, resid)
                    x = x_next
            ok = max_resid < PRED_ERR_THRESH
            support = f"TRANSFER Δ=0 shape → Δx-a=0 resid_max={max_resid:.2e}"
            cex = None if ok else support
            self.compare_log.append({"kind": kind, "hit": ok, "resid": max_resid})

        elif kind == "transfer_rec11_miss":
            # Claim: positions along a bang-bang traj obey Fib recurrence — false
            traj = rollout(5.0, policy_bangbang, T=8)
            xs = [r["x"] for r in traj]
            ok_rec = True
            why = "rec[1,1] on positions"
            for n in range(2, len(xs)):
                pred = xs[n - 1] + xs[n - 2]
                if abs(pred - xs[n]) > 1e-9:
                    ok_rec = False
                    why = f"n={n}: pred={pred} != obs={xs[n]}"
                    break
            ok = ok_rec  # should be False → rejected
            support = why if not ok else "unexpected Fib on positions"
            cex = None if ok else support
            self.compare_log.append({"kind": kind, "hit": ok, "why": why})

        elif kind == "transfer_pos_frozen":
            # Claim position conserved (Δx=0) under a=±1 — false
            fails = []
            for x0 in p["heldout_starts"]:
                a = 1.0
                if abs(true_step(float(x0), a) - float(x0)) > 1e-12:
                    fails.append(f"x0={x0} moved under a=1")
            ok = len(fails) == 0
            support = "unexpected" if ok else f"finite fail: {fails[0]}"
            cex = None if ok else support

        elif kind == "dead_increase":
            # Dead-end claim: always-right reduces error from +x — should FAIL critic
            rows = []
            for x0 in p["heldout_starts"]:
                traj = rollout(float(x0), policy_always_right, T=p.get("T", 4))
                rows.append(err_reduction(traj))
            ok = all(d > 0 for d in rows)  # claim improvement — false
            support = "unexpected" if ok else f"finite fail (dead-end): deltas={rows}"
            cex = None if ok else support

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
        out: list[Conjecture] = []
        for fid, fam in self.families().items():
            if fam.unlocked and "transfer" in fid:
                out.extend(self.hypothesize(fam, archive_confirmed, step=-1))
        return out
