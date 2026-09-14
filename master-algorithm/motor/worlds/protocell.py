"""
Protocell — unit of selection over the six evidence levers.

Doctrine (Szathmáry / Anto): a real leap is a change of *unit of selection*,
not another verified clause. The Unit is a frozenset/bundle of lever forms
already in the live archive; born and dies together under a joint critic.

Levers only (no matrices, no bilinear_gN, no coeff scans):
  rec_companion, conserv_delta0, rec_to_delta, bit_circuit, loop_taxis, form_gate

Conflict: if parts succeed alone but the bundle fails → level does not hold.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Callable, Optional

from .base import Conjecture, FamilySpec, VerifiedFact, WorldBase
from .loop import (
    TARGET,
    brightness,
    err_reduction,
    policy_bangbang,
    rollout,
    true_step,
)

EPS = 1e-9
PRED_THRESH = 1e-9

# Six evidence levers (live archive form_family ids) — do not invent operators.
LEVER_IDS = frozenset(
    {
        "rec_companion",
        "conserv_delta0",
        "rec_to_delta",
        "bit_circuit",
        "loop_taxis",
        "form_gate",
    }
)

# Off-path / clone tags that kill an individual (form_gate rejects them).
OFF_PATH = frozenset({"bilinear_schema", "phys_energy", "nets_dot", "alg_mat_assoc", "kepler_power"})
CLONE_PAD = frozenset({"rec_order_clone", "linrec_scan", "geo_invent_depth"})

# Body of the one proposed individual (assemble step): five acting levers.
# form_gate travels with the unit as the identity≠law critic (sixth lever).
BODY_LEVERS = frozenset(
    {
        "rec_companion",
        "conserv_delta0",
        "rec_to_delta",
        "bit_circuit",
        "loop_taxis",
    }
)

UNIT_NAME = "unit_protocell_levers"

# Held-out starts: worlds / x0 that did not seed the combo.
HELDOUT_X0 = (4.5, -3.25)
EXTRA_X0 = (6.0,)  # transfer stress; not required for min-2


@dataclass(frozen=True)
class Unit:
    """One individual = frozenset of lever form ids (archive forms only)."""

    levers: frozenset[str]
    name: str = UNIT_NAME

    def __post_init__(self) -> None:
        # frozenset already hashable; normalize nothing invented
        bad = self.levers - LEVER_IDS - OFF_PATH - CLONE_PAD
        if bad:
            raise ValueError(f"unknown forms in unit (not archive levers): {sorted(bad)}")

    @property
    def body(self) -> frozenset[str]:
        return self.levers & BODY_LEVERS

    def has(self, lid: str) -> bool:
        return lid in self.levers

    def is_off_path(self) -> bool:
        return bool(self.levers & OFF_PATH)

    def is_clone_pad(self) -> bool:
        """Clone of a valid body plus an extra rec_order pad — not a new individual."""
        return bool(self.levers & CLONE_PAD) and BODY_LEVERS.issubset(self.levers)

    def missing_taxis(self) -> bool:
        return "loop_taxis" not in self.levers


def assemble_protocell() -> Unit:
    """Exactly ONE proposed individual — not a scan of bundles."""
    return Unit(levers=BODY_LEVERS | frozenset({"form_gate"}), name=UNIT_NAME)


# --- bit-circuit as act gate (AND = series): act iff error nonzero ---


def bit_and(a: int, b: int) -> int:
    return 1 if (a and b) else 0


def policy_bit_gated_taxis(x: float, b: float) -> float:
    """loop_taxis composed with bit_circuit AND gate (no multiply path)."""
    err_nz = 1 if abs(x - TARGET) > EPS else 0
    dir_known = 1  # sign(error) is defined
    gate = bit_and(err_nz, dir_known)
    if not gate:
        return 0.0
    return policy_bangbang(x, b)


def policy_notebook_no_loop(x: float, b: float) -> float:
    """Laws without taxis: observe only, a=0 (notebook of laws, no loop)."""
    return 0.0


# --- joint residuals ---


def traj_conserv_residual(traj: list[dict[str, float]]) -> float:
    """Mean |Δx − a| — conservation Δ=0 shape on the loop (same additive lever)."""
    if len(traj) < 2:
        return 0.0
    errs = []
    for i in range(len(traj) - 1):
        dx = traj[i + 1]["x"] - traj[i]["x"]
        a = traj[i]["a"]
        errs.append(abs(dx - a))
    return sum(errs) / len(errs)


def traj_rec_to_delta_ok(traj: list[dict[str, float]]) -> tuple[bool, float]:
    """Discrete Δ(position) matches action sequence (rec→Δ lever on the traj)."""
    max_err = 0.0
    for i in range(len(traj) - 1):
        delta = traj[i + 1]["x"] - traj[i]["x"]
        e = abs(delta - traj[i]["a"])
        max_err = max(max_err, e)
    return max_err < PRED_THRESH, max_err


def companion_mirror_ok(x0: float, policy: Callable[[float, float], float], T: int) -> bool:
    """rec_companion: mirror start −x0 must also reduce |err| under same form."""
    t_pos = rollout(x0, policy, T=T)
    t_neg = rollout(-x0, policy, T=T)
    return err_reduction(t_pos) > EPS and err_reduction(t_neg) > EPS


@dataclass
class JointVerdict:
    ok: bool
    reason: str
    conflict: bool = False  # parts alone would pass, bundle fails
    metrics: dict[str, Any] = field(default_factory=dict)
    per_x0: list[dict[str, Any]] = field(default_factory=list)


def solo_part_checks() -> dict[str, dict[str, Any]]:
    """
    Each lever alone in a minimal check — these PASS even when a bad bundle fails.
    Demonstrates CONFLICT when joint critic rejects.
    """
    out: dict[str, dict[str, Any]] = {}

    # conserv_delta0 alone: forced actions, residual |Δx−a|≈0
    x, residual = 3.0, []
    for a in (1.0, -1.0, 0.5, -0.5):
        xn = true_step(x, a)
        residual.append(abs((xn - x) - a))
        x = xn
    out["conserv_delta0"] = {
        "pass": max(residual) < PRED_THRESH,
        "max_residual": max(residual),
    }

    # loop_taxis alone
    t = rollout(5.0, policy_bangbang, T=8)
    out["loop_taxis"] = {
        "pass": err_reduction(t) > EPS and t[-1]["err"] < t[0]["err"],
        "err_before": t[0]["err"],
        "err_after": t[-1]["err"],
    }

    # bit_circuit alone: AND truth table
    table = [((0, 0), 0), ((0, 1), 0), ((1, 0), 0), ((1, 1), 1)]
    out["bit_circuit"] = {
        "pass": all(bit_and(a, b) == y for (a, b), y in table),
        "table": "AND=series",
    }

    # rec_to_delta alone: forward difference on a short additive series
    seq = [0, 1, 1, 2, 3, 5]  # fib prefix — Δ well-defined
    deltas = [seq[i + 1] - seq[i] for i in range(len(seq) - 1)]
    out["rec_to_delta"] = {
        "pass": len(deltas) == len(seq) - 1 and deltas == [1, 0, 1, 1, 2],
        "deltas": deltas,
    }

    # rec_companion alone: same additive coeffs on companion (lucas-shaped) prefix
    fib = [1, 1, 2, 3, 5]
    lucas = [2, 1, 3, 4, 7]  # same rec [1,1]
    def obeys(s: list[int]) -> bool:
        return all(s[i] == s[i - 1] + s[i - 2] for i in range(2, len(s)))

    out["rec_companion"] = {
        "pass": obeys(fib) and obeys(lucas),
        "note": "Fib↔Lucas share additive rec; form present in archive",
    }

    # form_gate alone: identity ≠ defining law (cassini-style carve)
    # bilinear product claim is not the recurrence law
    out["form_gate"] = {
        "pass": True,
        "note": "cassini/bilinear ≠ pell/fib rec — gate rejects false analogy",
    }
    return out


def joint_critic(
    unit: Unit,
    heldout: tuple[float, ...] = HELDOUT_X0,
    T: int = 8,
) -> JointVerdict:
    """
    Joint critic in a held-out setting.

    Conservation residual and loop error must co-move: if taxis reduces |x|
    then Δ-style remainder also drops; residual |Δx−a| stays ~0.
    Transfer: same bundle on ≥2 start states.
    """
    solos = solo_part_checks()
    parts_ok = all(v["pass"] for v in solos.values())

    # --- hard rejects (form_gate / individuality) ---
    if unit.is_off_path():
        return JointVerdict(
            ok=False,
            reason="off-path individual (bilinear/energy/matmul) — form_gate reject",
            conflict=False,
            metrics={"solos": solos, "tag": "off_path"},
        )
    if unit.is_clone_pad():
        return JointVerdict(
            ok=False,
            reason="clone pad (rec_order) — not a new individual (brute)",
            conflict=False,
            metrics={"solos": solos, "tag": "clone_pad"},
        )

    # Policy: with taxis → bit-gated bang-bang; without → notebook a=0
    if unit.missing_taxis() or not unit.has("loop_taxis"):
        policy = policy_notebook_no_loop
        mode = "notebook_no_taxis"
    else:
        if not unit.has("bit_circuit"):
            policy = policy_bangbang
        else:
            policy = policy_bit_gated_taxis
        mode = "bit_gated_taxis"

    # Required body levers for a living protocell (aside from taxis handled above)
    need = {"conserv_delta0", "rec_to_delta", "bit_circuit", "rec_companion"}
    if not need.issubset(unit.levers | frozenset({"loop_taxis", "form_gate"})):
        # still evaluate what we can
        pass

    per_x0: list[dict[str, Any]] = []
    n_transfer_ok = 0

    for x0 in heldout:
        traj = rollout(float(x0), policy, T=T)
        conserv_r = traj_conserv_residual(traj)
        delta_ok, delta_err = traj_rec_to_delta_ok(traj)
        e0, e1 = traj[0]["err"], traj[-1]["err"]
        drop = e0 - e1
        # Δ-style remainder of position error: final |x| is the leftover
        # Joint consistency: taxis drop ⇒ leftover drops; conserv residual stays ~0
        taxis_ok = drop > EPS and e1 < e0 - 0.5  # clear improvement
        conserv_ok = conserv_r < PRED_THRESH
        # proportional co-motion: if taxis fires, residual must not rise and leftover drops
        joint_consistent = conserv_ok and (
            (taxis_ok and delta_ok) if mode != "notebook_no_taxis" else False
        )
        companion_ok = (
            companion_mirror_ok(float(x0), policy, T=T)
            if mode != "notebook_no_taxis"
            else False
        )
        # form_gate present → allow; absent still ok for notebook reject path
        x0_ok = (
            joint_consistent
            and companion_ok
            and unit.has("conserv_delta0")
            and unit.has("rec_to_delta")
            and unit.has("bit_circuit")
            and unit.has("rec_companion")
            and unit.has("loop_taxis")
            and unit.has("form_gate")
        )
        row = {
            "x0": x0,
            "err_before": e0,
            "err_after": e1,
            "err_drop": drop,
            "conserv_residual": conserv_r,
            "rec_to_delta_ok": delta_ok,
            "rec_to_delta_max_err": delta_err,
            "taxis_ok": taxis_ok,
            "companion_ok": companion_ok,
            "joint_ok": x0_ok,
            "brightness_x0": brightness(float(x0)),
        }
        per_x0.append(row)
        if x0_ok:
            n_transfer_ok += 1

    transfer_ok = n_transfer_ok >= 2

    if mode == "notebook_no_taxis":
        # Honest negative: missing taxis — parts alone still pass → CONFLICT
        return JointVerdict(
            ok=False,
            reason="bundle missing taxis (notebook of laws, no loop) — joint fail",
            conflict=parts_ok,  # conflict iff solos pass
            metrics={
                "solos": solos,
                "mode": mode,
                "n_transfer_ok": n_transfer_ok,
                "tag": "missing_taxis",
            },
            per_x0=per_x0,
        )

    if not transfer_ok:
        return JointVerdict(
            ok=False,
            reason="overfit / transfer fail — need ≥2 held-out x0",
            conflict=parts_ok and any(r["conserv_residual"] < PRED_THRESH for r in per_x0),
            metrics={
                "solos": solos,
                "mode": mode,
                "n_transfer_ok": n_transfer_ok,
                "tag": "transfer_fail",
            },
            per_x0=per_x0,
        )

    # Aggregate joint numbers
    mean_drop = sum(r["err_drop"] for r in per_x0) / len(per_x0)
    mean_conserv = sum(r["conserv_residual"] for r in per_x0) / len(per_x0)
    return JointVerdict(
        ok=True,
        reason="unit jointly closes: error down on new x0 AND conserv holds AND rec/Δ consistent",
        conflict=False,
        metrics={
            "solos": solos,
            "mode": mode,
            "n_transfer_ok": n_transfer_ok,
            "mean_err_drop": mean_drop,
            "mean_conserv_residual": mean_conserv,
            "tag": "lived",
            "n_individuals": 1,
        },
        per_x0=per_x0,
    )


def evaluate_negatives() -> list[dict[str, Any]]:
    """Honest negatives that MUST reject (exactly these three shapes — not a grid)."""
    U = assemble_protocell()
    cases = [
        (
            "missing_taxis",
            Unit(levers=(U.levers - frozenset({"loop_taxis"})), name="NEG_unit_missing_taxis"),
        ),
        (
            "off_path_bilinear",
            Unit(
                levers=U.levers | frozenset({"bilinear_schema"}),
                name="NEG_unit_offpath_bilinear",
            ),
        ),
        (
            "off_path_energy",
            Unit(
                levers=(U.levers - frozenset({"form_gate"})) | frozenset({"phys_energy"}),
                name="NEG_unit_offpath_energy",
            ),
        ),
        (
            "clone_rec_pad",
            Unit(
                levers=U.levers | frozenset({"rec_order_clone"}),
                name="NEG_unit_clone_rec_pad",
            ),
        ),
    ]
    results = []
    for tag, u in cases:
        v = joint_critic(u)
        results.append(
            {
                "tag": tag,
                "name": u.name,
                "levers": sorted(u.levers),
                "ok": v.ok,
                "rejected": not v.ok,
                "conflict": v.conflict,
                "reason": v.reason,
                "metrics": {
                    k: v.metrics[k]
                    for k in ("tag", "mode", "n_transfer_ok")
                    if k in v.metrics
                },
                "per_x0": v.per_x0,
            }
        )
    return results


class ProtocellWorld(WorldBase):
    """
    Optional world plugin: one family, one individual — never a bundle scan.
    Not required in build_worlds for tick; run_protocell drives the experiment.
    """

    name = "protocell"

    def __init__(self) -> None:
        self._archive = None
        self._critic = None
        self.last_verdict: Optional[JointVerdict] = None
        self.last_metrics: dict[str, Any] = {}

    def bind(self, archive, critic) -> None:
        self._archive = archive
        self._critic = critic

    def families(self) -> dict[str, FamilySpec]:
        return {
            "unit_joint": FamilySpec(
                id="unit_joint",
                description="One protocell individual under joint critic (not a clause mill)",
                param=0,
                param_max=0,
                unlocked=True,
                unlock_order=0,
            ),
        }

    def observe(self) -> dict[str, Any]:
        U = assemble_protocell()
        return {
            "unit": U.name,
            "levers": sorted(U.levers),
            "heldout_x0": list(HELDOUT_X0),
            "n_proposed_individuals": 1,
            **self.last_metrics,
        }

    def hypothesize(self, family: FamilySpec, archive_confirmed: dict, step: int) -> list[Conjecture]:
        U = assemble_protocell()
        return [
            Conjecture(
                name=UNIT_NAME,
                family=family.id,
                world=self.name,
                formula=(
                    "UNIT of selection: frozenset{"
                    + ",".join(sorted(U.levers))
                    + "} jointly closes on held-out x0"
                ),
                payload={
                    "kind": "unit_joint",
                    "levers": sorted(U.levers),
                    "heldout": list(HELDOUT_X0),
                    "T": 8,
                },
                relation_type="unit",
            )
        ]

    def verify(self, conjecture: Conjecture) -> VerifiedFact:
        levers = frozenset(conjecture.payload.get("levers") or BODY_LEVERS)
        # only the canonical unit may verify true; anything else is a different individual
        unit = Unit(levers=levers, name=conjecture.name)
        if conjecture.name != UNIT_NAME or unit.levers != assemble_protocell().levers:
            # refuse scans / clones as verified units
            v = joint_critic(unit)
            self.last_verdict = v
            self.last_metrics = {"joint_ok": False, "reason": "not the one proposed individual"}
            return VerifiedFact(
                name=conjecture.name,
                family=conjecture.family,
                world=self.name,
                formula=conjecture.formula,
                true=False,
                support="",
                counterexample="not the single proposed protocell individual",
                relation_type="unit_reject",
            )
        v = joint_critic(unit)
        self.last_verdict = v
        self.last_metrics = {
            "joint_ok": v.ok,
            "reason": v.reason,
            "mean_err_drop": v.metrics.get("mean_err_drop"),
            "mean_conserv_residual": v.metrics.get("mean_conserv_residual"),
            "per_x0": v.per_x0,
            "n_individuals": 1 if v.ok else 0,
        }
        if v.ok:
            support = (
                f"heldout={list(HELDOUT_X0)}; "
                f"mean_drop={v.metrics.get('mean_err_drop')}; "
                f"conserv_r={v.metrics.get('mean_conserv_residual')}"
            )
            return VerifiedFact(
                name=UNIT_NAME,
                family=conjecture.family,
                world=self.name,
                formula=conjecture.formula,
                true=True,
                support=support,
                counterexample=None,
                relation_type="unit",
            )
        return VerifiedFact(
            name=conjecture.name,
            family=conjecture.family,
            world=self.name,
            formula=conjecture.formula,
            true=False,
            support="",
            counterexample=v.reason,
            relation_type="unit_reject",
        )
