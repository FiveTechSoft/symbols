"""
Shared linear-constraint conservation form.

Shape: signed components sum to 0 (vector Δ = 0).
  chem:   reactants[e] - products[e] = 0  ∀e
  physics: p_in - p_out = 0
  electro: Σ I_k = 0 (KCL)

Transfer = reuse this shape as prior across worlds; critic accepts/rejects.
"""

from __future__ import annotations

from typing import Any

EPS = 1e-8


def chem_delta_zero(rx: dict) -> bool:
    elems = set(rx["reactants"]) | set(rx["products"])
    return all(rx["reactants"].get(e, 0) == rx["products"].get(e, 0) for e in elems)


def mom_delta_zero(c: dict) -> bool:
    p_in = c["m1"] * c["u1"] + c["m2"] * c["u2"]
    p_out = c["m1"] * c["v1"] + c["m2"] * c["v2"]
    return abs(p_in - p_out) < EPS


def kcl_sum_zero(node: dict) -> bool:
    return abs(sum(node["I"])) < EPS


def linear_constraint_holds(kind: str, payload: dict) -> tuple[bool, str]:
    """Critic for conservation-form transfer."""
    if kind == "conserv_chem":
        ok = chem_delta_zero(payload["rx"])
        return ok, "atom Δ=0" if ok else "atom Δ≠0"
    if kind == "conserv_mom":
        ok = mom_delta_zero(payload["c"])
        return ok, "momentum Δ=0" if ok else "momentum Δ≠0"
    if kind == "conserv_kcl":
        ok = kcl_sum_zero(payload["node"])
        return ok, "KCL ΣI=0" if ok else "KCL ΣI≠0"
    return False, f"unknown conserv kind {kind}"


# --- Form-family critic (false-analogy gate) ---
# Distinct families; conservation-Δ=0 is ONE family regardless of chem/phys/electro.

FORM_FAMILY = {
    "conserv_delta0": "linear_delta_zero",
    "conserv_mom": "linear_delta_zero",
    "conserv_chem": "linear_delta_zero",
    "conserv_kcl": "linear_delta_zero",
    "energy_half_mv2": "quadratic_energy",
    "series_and": "bit_and",
    "parallel_or": "bit_or",
    "rec_order2": "linear_recurrence",
    "rec_fib_11": "linear_recurrence",
    "kepler_t2_a3": "power_ratio_const",
    "ohm_vir": "linear_proportional",
    "bilin_cassini": "bilinear_identity",
    "pell_rec": "linear_recurrence",
    "bit_xor": "bit_xor",
    "bit_and": "bit_and",
    "mat_assoc": "assoc_binary_op",
    "poly_distrib": "ring_distrib",
}


def form_family_of(kind: str) -> str:
    return FORM_FAMILY.get(kind, kind)


def form_transfer_allowed(src_kind: str, dst_kind: str) -> tuple[bool, str]:
    """Reject false analogies that confuse distinct form-families.

    Same-family reuse (e.g. conserv chem→phys) is allowed; cross-family
    claims (mom as energy, series as parallel, cassini as pell-rec, …) are not.
    """
    fs, fd = form_family_of(src_kind), form_family_of(dst_kind)
    if fs == fd:
        return True, f"same form-family {fs}"
    return False, f"form-family mismatch: {src_kind}({fs}) ≠ {dst_kind}({fd})"


def energy_half_mv2_holds(c: dict) -> tuple[bool, str]:
    e_in = 0.5 * c["m1"] * c["u1"] ** 2 + 0.5 * c["m2"] * c["u2"] ** 2
    e_out = 0.5 * c["m1"] * c["v1"] ** 2 + 0.5 * c["m2"] * c["v2"] ** 2
    ok = abs(e_in - e_out) < EPS
    return ok, "energy ½mv² Δ=0" if ok else "energy Δ≠0"


def criticize_claimed_transfer(src_kind: str, dst_kind: str, payload: dict) -> tuple[bool, str]:
    """Full critic: form-family gate first, then destination check if same family."""
    ok_fam, why_fam = form_transfer_allowed(src_kind, dst_kind)
    if not ok_fam:
        return False, why_fam
    # same family — run destination critic when we have one
    if dst_kind in ("conserv_mom", "conserv_chem", "conserv_kcl"):
        return linear_constraint_holds(dst_kind, payload)
    if dst_kind == "energy_half_mv2":
        return energy_half_mv2_holds(payload["c"])
    return True, why_fam
