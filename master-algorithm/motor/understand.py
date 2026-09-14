"""
Form-family identity + necessary understanding.

Doctrine (Anto 2026-09-14):
  Facts must be NECESSARY to raise understanding so emergence happens.
  Intelligence = evolutionary leap under critic on EXACT LEVERS — not more
  ticks, not fatter bins, not random new operators (that is still brute).

Six levers (evidence already in live archive — only these pay):
  1. additive rec → companion world (Fib→Lucas; Fib→Pell = carving reject)
  2. linear Δ=0 conservation → another additive world (chem/phys/electro + loop Δx=action)
  3. rec → discrete Δ (calculus), not more rec clones
  4. bit compose as circuit (AND=series, OR=parallel), not perceptron dots
  5. closed-loop sign(error)→action that generalizes to a new x0
  6. form-family gate: identity ≠ defining law (cassini on Pell ≠ pell rec)

Tags per family: lever | clone | off-path.
Necessary = lever families that transferred OR whose reject carved the form.
"""

from __future__ import annotations

import re
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

MOTOR_DIR = Path(__file__).resolve().parent
DEFAULT_THEORY = MOTOR_DIR / "archive" / "theory.pl"

# Canonical lever form-family ids (the only prize path)
LEVER_FAMILIES = frozenset({
    "rec_companion",       # lever 1: additive rec → companion
    "conserv_delta0",      # lever 2: Δ=0 across additive worlds (+ loop dx)
    "rec_to_delta",        # lever 3: rec → discrete Δ
    "bit_circuit",         # lever 4: AND=series / OR=parallel / XOR compose
    "loop_taxis",          # lever 5: sign(error)→action generalizes
    "form_gate",           # lever 6: identity ≠ defining law
})

LEVER_NAMES = {
    1: ("rec_companion", "additive rec → companion world (Fib→Lucas; Fib→Pell carve)"),
    2: ("conserv_delta0", "linear Δ=0 → another additive world (chem/phys/electro + loop Δx=action)"),
    3: ("rec_to_delta", "rec → discrete Δ (calculus), not more rec clones"),
    4: ("bit_circuit", "bit compose as circuit (AND=series, OR=parallel), not perceptron dots"),
    5: ("loop_taxis", "closed-loop sign(error)→action generalizes to new x0"),
    6: ("form_gate", "form-family gate: identity ≠ defining law (cassini≠pell rec)"),
}

# Clone mills: same operator, more params / orders / gN suffixes
CLONE_FAMILIES = frozenset({
    "rec_order_clone",     # rec_fib_o3/o4 coeff pads
    "bilinear_schema",     # bilin products + gN
    "phys_mom_clone",      # collapsed into conserv but raw seed clones
    "phys_energy",         # ½mv² product clones
    "chance_entropy",      # H on k=2..6
    "modperiod_sweep",
    "geo_invent_depth",
    "linrec_scan",
})

OFF_PATH_FAMILIES = frozenset({
    "bilinear_schema",
    "phys_energy",
    "alg_mat_assoc",
    "nets_dot",
    "kepler_power",
    "chance_entropy",
    "ring_distrib",
})

# Reject → lever it carves
CARVING_REJECT_MAP: list[tuple[re.Pattern, str]] = [
    (re.compile(r"false_fib_rec_on_trib|false_fib_padded|transfer_.*pell.*reject|NEG_.*pell"), "rec_companion"),
    (re.compile(r"false_mom_as_energy|NEG_phys_momentum|NEG_chem_atoms|NEG_electro_current"), "conserv_delta0"),
    (re.compile(r"NEG_calc_delta|false_.*delta"), "rec_to_delta"),
    (re.compile(r"false_series_and_as_parallel|false_xor_as_and|false_and_as_xor|NEG_and_|NEG_xor|NEG_parity|NEG_nets_XOR_linear|transfer_bitfn_xor_to_linear"), "bit_circuit"),
    (re.compile(r"NEG_loop_policy"), "loop_taxis"),
    (re.compile(r"false_cassini_as_pell|bilin_.*bogus|form-family mismatch.*bilin|form-family mismatch.*cassini"), "form_gate"),
    (re.compile(r"false_kepler_as_ohm|kepler_bogus"), "form_gate"),  # also a form-gate style carve
]


def form_family_id(
    world: str = "",
    family: str = "",
    name: str = "",
    formula: str = "",
) -> str:
    """Map verified/rejected fact → canonical form_family id."""
    fam = (family or "").strip()
    nm = (name or "").strip()
    w = (world or "").strip()
    low = f"{fam} {nm} {formula}".lower()

    # --- off-path / clone first (so they never steal a lever id) ---
    if fam.startswith("bilinear_schema") or "bilinear" in fam or nm.startswith("bilin_"):
        return "bilinear_schema"
    if "phys_energy" in nm or ("energy" in nm and "conserve" in nm):
        return "phys_energy"
    if "mat_assoc" in nm or "alg_mat" in nm or (fam.startswith("alg_") and "mat" in low):
        return "alg_mat_assoc"
    if fam.startswith("nets_") or nm.startswith("nets_"):
        if "xor" in nm and "linear" in low:
            return "bit_circuit"  # carve evidence, not the dot
        return "nets_dot"
    if "kepler" in nm or "kepler" in fam or (fam.startswith("astro_") and "period" not in fam):
        return "kepler_power"
    if "chance_H" in nm or fam == "chance_entropy_scan" or "entropy" in fam:
        return "chance_entropy"

    # --- lever 6: form_gate (cassini identity facts + gate rejects live here) ---
    if "cassini" in low or (nm.startswith("bilin_") and "form" in low):
        return "form_gate"

    # --- lever 2: conserv Δ=0 (+ loop dx shared additive shape) ---
    if any(
        x in nm or x in fam
        for x in (
            "phys_mom_conserve",
            "chem_atom",
            "electro_KCL",
            "electro_kirchhoff",
            "transfer_conserv",
            "conserv_",
            "chem_transfer_form",
            "phys_transfer_form",
            "electro_transfer_form",
        )
    ):
        return "conserv_delta0"
    if fam in ("phys_collision",) and "mom" in nm:
        return "conserv_delta0"
    if fam in ("chem_atom_balance", "electro_kirchhoff"):
        return "conserv_delta0"
    # loop prediction Δx=action is the SAME additive lever as conserv (doctrine lever 2)
    if nm.startswith("loop_pred_dx") or "dx_eq_action" in nm or "delta0_to_loop" in nm:
        return "conserv_delta0"

    # --- lever 5: closed-loop taxis ---
    if fam.startswith("loop_taxis") or nm.startswith("loop_taxis") or "taxis" in nm:
        return "loop_taxis"
    if fam.startswith("loop_") or nm.startswith("loop_"):
        if "pred" in nm:
            return "conserv_delta0"
        return "loop_taxis"

    # --- lever 3: rec → discrete Δ ---
    if "rec_to_delta" in nm or "transfer_rec" in nm and "delta" in nm:
        return "rec_to_delta"
    if "calc_delta" in nm or fam in ("calc_fwd_diff", "calc_ft_discrete") or (
        fam.startswith("calc_") and "delta" in low
    ):
        return "rec_to_delta"
    if "calc_ft" in nm or "fundamental" in low and "calc" in (w + fam):
        return "rec_to_delta"

    # --- lever 4: bit circuit ---
    if any(
        x in nm or x in fam
        for x in (
            "bitfn_and",
            "bit_and",
            "series_and",
            "bitfn_xor",
            "bit_xor",
            "xor2",
            "bitfn_or",
            "bit_or",
            "parallel_or",
            "parity",
            "electro_switch",
            "switch_transfer",
            "boolean_from_examples",
        )
    ):
        return "bit_circuit"
    if fam == "boolean_from_examples" or "bitfn_" in nm:
        return "bit_circuit"
    if "transfer_bitfn" in nm or "info_transfer_bitfn" in fam:
        return "bit_circuit"

    # --- lever 1: additive rec → companion (transfer) vs clone (raw rec_*) ---
    if fam == "transfer_recurrence" or (
        nm.startswith("transfer_")
        and any(s in nm for s in ("fib", "lucas", "pell"))
        and "_to_" in nm
    ):
        return "rec_companion"
    if fam in ("linear_recurrences", "linrec_scan") or fam.startswith("linrec_scan"):
        return "rec_order_clone"
    if re.match(r"rec_(fib|lucas|pell)", nm):
        return "rec_order_clone"

    # modperiod / geo invent / entropy already off or clone
    if fam.startswith("modperiod") or nm.startswith("period_"):
        return "modperiod_sweep"
    if fam.startswith("geo_invent") or nm.startswith("geo_invent"):
        return "geo_invent_depth"
    if fam in ("euclid_conjectures", "lemma_reuse") or "midline" in nm:
        return "geo_invent_depth"

    # logodds additive — related to lever-ish but not one of the 6; treat as clone/noise unless transfer
    if "logodds" in nm or fam == "chance_bayes_scan":
        return "logodds_add"

    if fam.startswith("info_") or nm.startswith("info_"):
        return "info_mi"
    if fam.startswith("sym_") or nm.startswith("sym_"):
        return "sym_invariant"
    if "action_reaction" in nm:
        return "conserv_delta0"  # equal-opposite additive constraint

    # composed lever spawns only (Δ∘conserv, taxis on additive invariant)
    if "op_delta_conserv" in fam or "delta_conserv" in nm or "Δ∘conserv" in formula:
        return "conserv_delta0"
    if "op_taxis" in fam or "taxis_on_" in nm:
        return "loop_taxis"

    # generic transfer → try inherit
    if nm.startswith("transfer_") or "transfer" in fam:
        if "conserv" in nm or "kcl" in nm or "mom" in nm or "loop" in nm:
            return "conserv_delta0"
        if "bit" in nm or "and" in nm or "xor" in nm or "or" in nm or "switch" in nm:
            return "bit_circuit"
        if "delta" in nm or "calc" in nm:
            return "rec_to_delta"
        if any(s in nm for s in ("fib", "lucas", "pell", "rec")):
            return "rec_companion"
        return "noise_transfer"

    stem = re.sub(r"_s\d+$|_g\d+$|_r\d+|_n\d+|_p\d+|_d\d+|_k\d+", "", nm)
    stem = re.sub(r"_o\d+(_-?\d+)*$", "", stem)
    return f"noise::{w}::{stem}" if w else f"noise::{stem}"


def tag_family(fid: str) -> str:
    """lever | clone | off-path"""
    if fid in LEVER_FAMILIES:
        return "lever"
    if fid in OFF_PATH_FAMILIES or fid.startswith("bilinear"):
        return "off-path"
    if fid in CLONE_FAMILIES or fid.endswith("_clone") or fid.startswith("noise"):
        return "clone"
    if fid in ("modperiod_sweep", "geo_invent_depth", "linrec_scan", "logodds_add", "info_mi", "sym_invariant"):
        return "clone"
    return "clone"


def is_off_path(fid: str) -> bool:
    return tag_family(fid) == "off-path"


def parse_theory(theory_path: Path | None = None) -> tuple[list[dict], list[dict]]:
    path = Path(theory_path) if theory_path else DEFAULT_THEORY
    text = path.read_text(encoding="utf-8")
    verified: list[dict] = []
    pat = re.compile(
        r"verified\(fact\(([^,]+),\s*([^,]+),\s*'((?:\\'|[^'])*)',\s*'((?:\\'|[^'])*)'\)\)"
    )
    for m in pat.finditer(text):
        world, family, name, formula = m.group(1), m.group(2), m.group(3), m.group(4)
        name = name.replace("\\'", "'")
        formula = formula.replace("\\'", "'")
        fid = form_family_id(world, family, name, formula)
        verified.append(
            {
                "world": world,
                "family": family,
                "name": name,
                "formula": formula,
                "form_family": fid,
                "tag": tag_family(fid),
                "from_transfer": name.startswith("transfer_") or "transfer" in family,
            }
        )
    rejected: list[dict] = []
    rpat = re.compile(r"rejected\('((?:\\'|[^'])*)',\s*'((?:\\'|[^'])*)'\)")
    for m in rpat.finditer(text):
        name = m.group(1).replace("\\'", "'")
        why = m.group(2).replace("\\'", "'")
        fid = None
        for cre, fam_id in CARVING_REJECT_MAP:
            if cre.search(name) or cre.search(why):
                fid = fam_id
                break
        if fid is None:
            fid = form_family_id("", "", name, why)
        carves = (
            any(cre.search(name) or cre.search(why) for cre, _ in CARVING_REJECT_MAP)
            or name.startswith("false_")
            or "mismatch" in why.lower()
            or "form-family" in why.lower()
        )
        rejected.append(
            {"name": name, "why": why, "form_family": fid, "carves": carves, "tag": tag_family(fid)}
        )
    return verified, rejected


@dataclass
class UnderstandReport:
    n_facts: int
    n_families: int
    n_necessary: int
    n_noise_facts: int
    families: dict[str, dict] = field(default_factory=dict)
    necessary_ids: list[str] = field(default_factory=list)
    clone_counts: dict[str, int] = field(default_factory=dict)
    unique_form_score: float = 0.0
    lever_ids: list[str] = field(default_factory=list)
    noise_ids: list[str] = field(default_factory=list)

    def to_dict(self) -> dict:
        return {
            "n_facts": self.n_facts,
            "n_families": self.n_families,
            "n_necessary": self.n_necessary,
            "n_noise_facts": self.n_noise_facts,
            "necessary_ids": self.necessary_ids,
            "unique_form_score": self.unique_form_score,
            "lever_ids": self.lever_ids,
            "noise_ids": self.noise_ids,
            "clone_counts": self.clone_counts,
            "families": self.families,
            "levers": {k: list(v) for k, v in LEVER_NAMES.items()},
        }


def analyze(theory_path: Path | None = None) -> UnderstandReport:
    verified, rejected = parse_theory(theory_path)
    by_fam: dict[str, list[dict]] = defaultdict(list)
    for v in verified:
        by_fam[v["form_family"]].append(v)

    carving_fams: set[str] = set()
    for r in rejected:
        if r.get("carves") or r["name"].startswith("false_") or "form-family" in r["why"].lower():
            carving_fams.add(r["form_family"])

    families: dict[str, dict] = {}
    necessary: list[str] = []
    clone_counts: dict[str, int] = {}
    n_noise = 0

    for fid, facts in sorted(by_fam.items(), key=lambda x: (-len(x[1]), x[0])):
        tag = tag_family(fid)
        worlds = sorted({f["world"] for f in facts})
        transferred = any(f["from_transfer"] for f in facts) or len(worlds) > 1
        has_carve = fid in carving_fams
        n = len(facts)
        clone_counts[fid] = n
        if tag != "lever":
            n_noise += n

        # Necessary = LEVER that transferred OR whose reject carved the form
        is_necessary = tag == "lever" and (transferred or has_carve)

        families[fid] = {
            "n_facts": n,
            "worlds": worlds,
            "transferred": transferred,
            "carved_by_reject": has_carve,
            "tag": tag,
            "necessary": is_necessary,
            "example": facts[0]["name"] if facts else "",
        }
        if is_necessary:
            necessary.append(fid)

    # Lever families carved only by rejects (no verified yet) still count
    for fid in sorted(carving_fams):
        if fid in families:
            continue
        if tag_family(fid) != "lever":
            continue
        families[fid] = {
            "n_facts": 0,
            "worlds": [],
            "transferred": False,
            "carved_by_reject": True,
            "tag": "lever",
            "necessary": True,
            "example": "(carve-only)",
        }
        clone_counts[fid] = 0
        necessary.append(fid)

    n_nec = len(necessary)
    n_non_lever_fams = sum(1 for fid, m in families.items() if m["tag"] != "lever")
    score_denom = n_nec + n_non_lever_fams
    unique = (n_nec / score_denom) if score_denom else 0.0

    return UnderstandReport(
        n_facts=len(verified),
        n_families=len(families),
        n_necessary=n_nec,
        n_noise_facts=n_noise,
        families=families,
        necessary_ids=sorted(necessary),
        clone_counts=clone_counts,
        unique_form_score=round(unique, 4),
        lever_ids=sorted(fid for fid, m in families.items() if m["tag"] == "lever"),
        noise_ids=sorted(fid for fid, m in families.items() if m["tag"] != "lever"),
    )


def known_form_families(theory_path: Path | None = None) -> set[str]:
    verified, _ = parse_theory(theory_path)
    return {v["form_family"] for v in verified}


def known_lever_families(theory_path: Path | None = None) -> set[str]:
    return {f for f in known_form_families(theory_path) if tag_family(f) == "lever"}


def print_report(theory_path: Path | None = None) -> UnderstandReport:
    rep = analyze(theory_path)
    print(f"{rep.n_facts} facts → {rep.n_families} form_families → {rep.n_necessary} necessary")
    print(f"noise facts (not on a lever): {rep.n_noise_facts}")
    print(f"unique-form understanding score: {rep.unique_form_score}")
    print("6 levers:")
    for k, (fid, desc) in LEVER_NAMES.items():
        m = rep.families.get(fid)
        if m:
            print(f"  L{k} {fid:20s} n={m['n_facts']:3d} nec={m['necessary']} xfer={m['transferred']} carve={m['carved_by_reject']}")
        else:
            print(f"  L{k} {fid:20s} (absent)")
        print(f"      {desc}")
    print("necessary families:", rep.necessary_ids)
    print("top clone/off-path mills:")
    for fid, n in sorted(rep.clone_counts.items(), key=lambda x: -x[1])[:15]:
        m = rep.families[fid]
        if m["tag"] == "lever" and n < 8:
            continue
        print(f"  {fid:28s} n={n:3d}  tag={m['tag']:8s} nec={m['necessary']}")
    return rep


if __name__ == "__main__":
    import sys

    path = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_THEORY
    print_report(path)
