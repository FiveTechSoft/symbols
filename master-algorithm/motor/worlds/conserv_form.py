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
