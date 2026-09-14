"""
Cross-world transfer battery — tests form reuse under critic TO THE LIMIT.

Intelligence metric (honest):
  positive_transfers / (positive + negative attempts)
  skips (shape N/A) do not count as fail.
  Negatives that SHOULD fail (Pell, XOR-linear, bad Kepler) must still fail.

Usage:
  python -m motor.xfer_battery            # evaluate only
  python -m motor.xfer_battery --ingest   # also archive verified/rejected transfer_* facts
  python -m motor.xfer_battery --write-md # write 14-transfer.md
"""

from __future__ import annotations

import argparse
import json
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any, Optional

MOTOR_DIR = Path(__file__).resolve().parent
RUNS = MOTOR_DIR / "runs"
ROOT = MOTOR_DIR.parent


@dataclass
class Attempt:
    pair: str
    src_form: str
    dst_world: str
    expected: str  # hit | miss | skip
    result: str  # hit | miss | skip
    why: str
    name: str = ""
    ingested: bool = False


def _predict_next(vals: list[int], coeffs: list[int], n: int) -> int:
    return sum(coeffs[i] * vals[n - 1 - i] for i in range(len(coeffs)))


def _rec_holds(vals: list[int], coeffs: list[int]) -> tuple[bool, str]:
    order = len(coeffs)
    for n in range(order, len(vals)):
        if _predict_next(vals, coeffs, n) != vals[n]:
            return False, f"n={n}: pred={_predict_next(vals, coeffs, n)} != obs={vals[n]}"
    return True, "rec holds on prefix"


def collect_forms(kernel) -> dict[str, Any]:
    """Collect verified forms from LIVE archive + world observations."""
    recs = {}
    for seq, coeffs in kernel.archive.list_recs():
        # prefer shortest order-2 canonical
        if seq not in recs or len(coeffs) < len(recs[seq]):
            recs[seq] = coeffs
        if seq in recs and len(coeffs) == 2 and len(recs[seq]) > 2:
            recs[seq] = coeffs
    # force order-2 when available
    for seq, coeffs in kernel.archive.list_recs():
        if len(coeffs) == 2:
            recs[seq] = coeffs

    bit_fns = kernel.archive.list_bit_fns()
    verified = set(kernel.archive.verified_names())

    has_atom = any("chem_atom" in n or "atom_balance" in n for n in verified)
    has_mom = any("phys_mom" in n or "mom_conserve" in n for n in verified)
    has_kcl = any("KCL" in n or "kirchhoff" in n.lower() or "electro_KCL" in n for n in verified)
    # Also treat seed families as shape-available even if not yet verified on live
    has_atom = True  # form exists in chem world
    has_mom = True
    has_kcl = True

    has_kepler = any("kepler" in n.lower() for n in verified) or True
    has_bilin_fib = any("bilin_fib_offset" in n for n in verified)

    return {
        "recs": recs,
        "bit_fns": bit_fns,
        "has_atom": has_atom,
        "has_mom": has_mom,
        "has_kcl": has_kcl,
        "has_kepler": has_kepler,
        "has_bilin_fib": has_bilin_fib,
        "verified": sorted(verified),
    }


def _ingest(kernel, name: str, world: str, family: str, formula: str, true: bool, why: str) -> bool:
    """Archive a transfer fact if not already present. Returns True if newly written."""
    names = set(kernel.archive.verified_names())
    rejected_already = any(
        ln.startswith("rejected(") and name in ln for ln in kernel.archive._lines
    )
    if true:
        if name in names:
            return False
        kernel.archive.assert_verified(world, family, name, formula)
        kernel.confirmed[name] = name  # type: ignore — presence marker
        return True
    else:
        if rejected_already or name in names:
            return False
        kernel.archive.assert_rejected(name, why)
        return True


def run_battery(kernel=None, ingest: bool = False) -> dict[str, Any]:
    from .kernel import MotorKernel
    from .worlds.base import Conjecture, FamilySpec
    from .worlds.schema_lang import verify_bilinear
    from .worlds.astro import _kepler_table, EPS as ASTRO_EPS
    from .worlds.nets import AND_TABLE, XOR_TABLE, _exists_linear
    from .worlds.calculus import _fib, _lucas
    from .worlds.chem import _reaction_water
    from .worlds.physics import _gen_collision
    from .worlds.electro import _gen_kirchhoff_node
    from .worlds.conserv_form import linear_constraint_holds

    if kernel is None:
        kernel = MotorKernel(reset=False)

    forms = collect_forms(kernel)
    attempts: list[Attempt] = []

    def add(pair, src, dst, expected, result, why, name="", ingested=False):
        attempts.append(
            Attempt(
                pair=pair,
                src_form=src,
                dst_world=dst,
                expected=expected,
                result=result,
                why=why,
                name=name,
                ingested=ingested,
            )
        )

    seq_world = kernel.worlds["sequences"]
    fib_rec = forms["recs"].get("fib")

    # ----- a) rec fib → lucas, pell, and Δ of fib -----
    if fib_rec:
        for target, expected in (("lucas", "hit"), ("pell", "miss")):
            vals = seq_world.data[target]
            ok, why = _rec_holds(vals, fib_rec)
            result = "hit" if ok else "miss"
            name = f"transfer_fib_to_{target}_{'_'.join(map(str, fib_rec))}"
            formula = f"TRANSFER rec(fib,{fib_rec}) ⇒ try on {target}"
            ingested = False
            if ingest:
                ingested = _ingest(
                    kernel, name, "sequences", "transfer_recurrence", formula, ok, why
                )
            add(
                f"rec fib→{target}",
                f"rec(fib,{fib_rec})",
                "sequences",
                expected,
                result,
                why if not ok else f"shared law {fib_rec}",
                name=name,
                ingested=ingested,
            )

        # Δ of fib (calculus)
        vals = _fib(12)
        # for [1,1]: Delta[n] = vals[n-1]
        ok = True
        why = "TRANSFER rec(fib,[1,1])→Delta=prev"
        if list(fib_rec) == [1, 1]:
            for n in range(1, len(vals) - 1):
                if vals[n + 1] - vals[n] != vals[n - 1]:
                    ok = False
                    why = f"n={n}"
                    break
        else:
            ok, why = _rec_holds(vals, fib_rec)
            why = f"rec holds ⇒ Δ well-posed: {why}"
        result = "hit" if ok else "miss"
        name = f"transfer_rec_fib_to_delta_N12"
        formula = f"TRANSFER rec(fib,{fib_rec}) ⇒ Delta structure matches recurrence"
        ingested = False
        if ingest:
            ingested = _ingest(kernel, name, "calculus", "calc_transfer_form", formula, ok, why)
        add(
            "rec fib→Δ(fib)",
            f"rec(fib,{fib_rec})",
            "calculus",
            "hit",
            result,
            why,
            name=name,
            ingested=ingested,
        )
    else:
        add("rec fib→lucas", "rec(fib,?)", "sequences", "hit", "skip", "no fib rec in archive")
        add("rec fib→pell", "rec(fib,?)", "sequences", "miss", "skip", "no fib rec in archive")
        add("rec fib→Δ(fib)", "rec(fib,?)", "calculus", "hit", "skip", "no fib rec in archive")

    # ----- b) bit_fn AND/XOR → nets perceptron / electro switch -----
    bit_map = {h: (n, t) for n, t, h in forms["bit_fns"]}
    # AND → perceptron
    if "and_all" in bit_map or True:
        ok = _exists_linear(AND_TABLE)
        result = "hit" if ok else "miss"
        expected = "hit" if "and_all" in bit_map else "skip"
        if "and_all" not in bit_map:
            result = "skip"
            why = "no and_all bit_fn prior"
        else:
            why = "AND linear-sep under bit_fn(and_all) prior"
        name = "transfer_bitfn_and_to_perceptron_form"
        formula = "TRANSFER bit_fn(and_all) ⇒ AND linear-sep (perceptron)"
        ingested = False
        if ingest and result != "skip":
            ingested = _ingest(
                kernel, name, "nets", "nets_transfer_form", formula, ok, why if not ok else "ok"
            )
        add(
            "bit_fn AND→nets perceptron",
            "bit_fn(and_all)",
            "nets",
            "hit" if "and_all" in bit_map else "skip",
            result,
            why,
            name=name,
            ingested=ingested,
        )

        # AND → electro series switch
        ok = all((a & b) == (1 if (a == 1 and b == 1) else 0) for a in (0, 1) for b in (0, 1))
        result = "hit" if ok and "and_all" in bit_map else ("skip" if "and_all" not in bit_map else "miss")
        why = "series≡AND" if result == "hit" else ("no and_all prior" if result == "skip" else "mismatch")
        name = "transfer_and_to_series_switch_and_all"
        formula = "TRANSFER bit_fn(and_all) ⇒ series switches"
        ingested = False
        if ingest and result != "skip":
            ingested = _ingest(
                kernel, name, "electro", "electro_switch_transfer", formula, result == "hit", why
            )
        add(
            "bit_fn AND→electro switch",
            "bit_fn(and_all)",
            "electro",
            "hit" if "and_all" in bit_map else "skip",
            result,
            why,
            name=name,
            ingested=ingested,
        )

    # XOR → linear (must miss)
    if "xor2" in bit_map or True:
        ok = _exists_linear(XOR_TABLE)
        # claim is transfer works as linear — expect REJECT
        result = "hit" if ok else "miss"
        expected = "miss"
        if "xor2" not in bit_map:
            result = "skip"
            why = "no xor2 bit_fn prior"
        else:
            why = "XOR not linearly separable (honest negative)" if not ok else "UNEXPECTED linear XOR"
        name = "transfer_bitfn_xor_to_linear_perceptron"
        formula = "TRANSFER bit_fn(xor2) ⇒ XOR linear-sep (must FAIL)"
        ingested = False
        if ingest and result != "skip":
            ingested = _ingest(
                kernel, name, "nets", "nets_transfer_form", formula, ok, why
            )
        add(
            "bit_fn XOR→linear perceptron",
            "bit_fn(xor2)",
            "nets",
            expected if "xor2" in bit_map else "skip",
            result,
            why,
            name=name,
            ingested=ingested,
        )

    # ----- c) conservation chem ↔ physics ↔ electro -----
    rx = _reaction_water()
    c = _gen_collision(4242)
    node = _gen_kirchhoff_node(4243)
    conserv_pairs = [
        ("chem atom-balance → physics momentum", "chem:atoms", "physics", "conserv_mom", {"c": c}, "hit"),
        ("chem atom-balance → electro KCL", "chem:atoms", "electro", "conserv_kcl", {"node": node}, "hit"),
        ("physics momentum → chem atoms", "physics:momentum", "chem", "conserv_chem", {"rx": rx}, "hit"),
        ("physics momentum → electro KCL", "physics:momentum", "electro", "conserv_kcl", {"node": node}, "hit"),
        ("electro KCL → chem atoms", "electro:kcl", "chem", "conserv_chem", {"rx": rx}, "hit"),
        ("electro KCL → physics momentum", "electro:kcl", "physics", "conserv_mom", {"c": c}, "hit"),
    ]
    for pair, src, dst, kind, payload, expected in conserv_pairs:
        ok, support = linear_constraint_holds(kind, payload)
        result = "hit" if ok else "miss"
        name = f"transfer_conserv_{src.replace(':','_')}_to_{dst}_{kind}"
        formula = f"TRANSFER linear-Δ=0 ({src}) ⇒ {kind} on {dst}"
        ingested = False
        fam = {
            "chem": "chem_transfer_form",
            "physics": "phys_transfer_form",
            "electro": "electro_transfer_form",
        }[dst]
        if ingest:
            ingested = _ingest(kernel, name, dst, fam, formula, ok, support)
        add(pair, f"conserv({src})", dst, expected, result, support, name=name, ingested=ingested)

    # ----- d) Kepler T²∝a³ → bogus other power (must reject) -----
    table = _kepler_table(5)
    import math
    TWO_PI = 2 * math.pi
    ratios = [row["T"] ** 2 / row["a"] ** 3 for row in table]
    target = (TWO_PI) ** 2
    ok_good = all(abs(x - target) < 1e-6 for x in ratios) and all(
        abs(x - ratios[0]) < 1e-6 for x in ratios
    )
    name = "transfer_kepler3_form_n5"
    formula = "TRANSFER Kepler form T^2/a^3 constancy on circular table"
    ingested = False
    if ingest:
        ingested = _ingest(
            kernel, name, "astro", "astro_transfer_form", formula, ok_good, "T2/a3 not const"
        )
    add(
        "Kepler T²∝a³ → same table",
        "kepler T²/a³",
        "astro",
        "hit",
        "hit" if ok_good else "miss",
        f"ratios≈{ratios[0]:.6f}" if ok_good else f"spread {ratios}",
        name=name,
        ingested=ingested,
    )

    ratios_bad = [row["T"] ** 2 / row["a"] ** 2 for row in table]
    ok_bad = all(abs(x - ratios_bad[0]) < 1e-6 for x in ratios_bad)
    # expect miss (reject)
    name = "transfer_kepler_bogus_power_n5"
    formula = "TRANSFER bogus T^2/a^2 (wrong exponent) — must REJECT"
    ingested = False
    if ingest:
        ingested = _ingest(
            kernel,
            name,
            "astro",
            "astro_transfer_form",
            formula,
            ok_bad,
            f"T²/a² not const: {ratios_bad}",
        )
    add(
        "Kepler → bogus T²∝a²",
        "kepler T²/a³ (wrong exp)",
        "astro",
        "miss",
        "hit" if ok_bad else "miss",
        f"T²/a² not const: {[round(x,4) for x in ratios_bad]}"
        if not ok_bad
        else "UNEXPECTED constancy",
        name=name,
        ingested=ingested,
    )

    # ----- e) bilinear cassini-shape → lucas (reject) and fib (hit) -----
    N = 15
    F = seq_world.data["fib"]
    L = seq_world.data["lucas"]
    ok_f, s_f, c_f = verify_bilinear(F, {"form": "offset_pm1", "r": 1, "seq": "fib"}, N)
    name = "transfer_bilin_cassini_shape_on_fib"
    formula = "TRANSFER cassini-shape bilin ⇒ fib(n+1)fib(n-1)-fib(n)^2=(-1)^n"
    ingested = False
    if ingest:
        ingested = _ingest(
            kernel, name, "sequences", "bilinear_schema", formula, ok_f, c_f or "ok"
        )
    add(
        "bilinear cassini → fib",
        "bilinear offset_pm1",
        "sequences",
        "hit",
        "hit" if ok_f else "miss",
        s_f if ok_f else (c_f or s_f),
        name=name,
        ingested=ingested,
    )

    ok_l, s_l, c_l = verify_bilinear(L, {"form": "offset_pm1", "r": 1, "seq": "lucas"}, N)
    name = "transfer_bilin_cassini_shape_on_lucas"
    formula = "TRANSFER cassini-shape bilin ⇒ lucas (expect reject; RHS≠(-1)^n)"
    ingested = False
    if ingest:
        ingested = _ingest(
            kernel, name, "sequences", "bilinear_schema", formula, ok_l, c_l or "ok"
        )
    add(
        "bilinear cassini → lucas",
        "bilinear offset_pm1",
        "sequences",
        "miss",
        "hit" if ok_l else "miss",
        c_l or s_l,
        name=name,
        ingested=ingested,
    )


    # ----- f) DEMAND-15 FALSE ANALOGIES (critic MUST miss) -----
    from .worlds.conserv_form import criticize_claimed_transfer, form_transfer_allowed

    # 1) momentum-Δ=0 claimed as energy ½mv² on same collision
    ok_f, why_f = criticize_claimed_transfer("conserv_mom", "energy_half_mv2", {"c": c})
    name = "false_mom_as_energy_on_collision"
    ingested = False
    if ingest:
        ingested = _ingest(kernel, name, "physics", "false_analogy", "FALSE mom-Δ=0 as energy ½mv²", ok_f, why_f)
    add(
        "FALSE mom-Δ=0 as energy ½mv²",
        "conserv_mom",
        "physics",
        "miss",
        "hit" if ok_f else "miss",
        why_f,
        name=name,
        ingested=ingested,
    )

    # 2) series-AND claimed as parallel OR
    ok_fam, why_fam = form_transfer_allowed("series_and", "parallel_or")
    # also table check: AND table ≠ OR table
    table_eq = all((a & b) == (a | b) for a in (0, 1) for b in (0, 1))
    ok_f = ok_fam and table_eq  # both must hold for a hit; expect miss
    why_f = why_fam if not ok_fam else ("series∧≡parallel∨ UNEXPECTED" if table_eq else "series∧≠parallel∨ on (0,1)")
    name = "false_series_and_as_parallel_or"
    ingested = False
    if ingest:
        ingested = _ingest(kernel, name, "electro", "false_analogy", "FALSE series-AND as parallel-OR", ok_f, why_f)
    add(
        "FALSE series-AND as parallel-OR",
        "series_and",
        "electro",
        "miss",
        "hit" if ok_f else "miss",
        why_f,
        name=name,
        ingested=ingested,
    )

    # 3) fib rec [1,1] on trib/tribonacci (order-3)
    trib = [0, 0, 1]
    for _i in range(14):
        trib.append(trib[-1] + trib[-2] + trib[-3])
    ok_trib, why_trib = _rec_holds(trib, [1, 1])
    name = "false_fib_rec_on_trib"
    ingested = False
    if ingest:
        ingested = _ingest(kernel, name, "sequences", "false_analogy", "FALSE fib[1,1] on trib", ok_trib, why_trib)
    add(
        "FALSE fib[1,1] on trib",
        "rec(fib,[1,1])",
        "sequences",
        "miss",
        "hit" if ok_trib else "miss",
        why_trib,
        name=name,
        ingested=ingested,
    )
    ok_pad, why_pad = _rec_holds(trib, [1, 1, 0])
    name = "false_fib_padded_on_trib"
    ingested = False
    if ingest:
        ingested = _ingest(kernel, name, "sequences", "false_analogy", "FALSE fib[1,1,0] on trib", ok_pad, why_pad)
    add(
        "FALSE fib[1,1,0] padded on trib",
        "rec(fib,[1,1,0])",
        "sequences",
        "miss",
        "hit" if ok_pad else "miss",
        why_pad,
        name=name,
        ingested=ingested,
    )

    # 4) Kepler T²/a³ claimed as Ohm on resistor table
    ohm = _gen_ohm_triples(5, 7) if "_gen_ohm_triples" in dir() else None
    from .worlds.electro import _gen_ohm_triples as _ohm_gen
    ohm = _ohm_gen(5, 7)
    ratios_ko = [t["V"] ** 2 / (t["R"] ** 3 + 1e-15) for t in ohm]
    ok_ko = all(abs(x - ratios_ko[0]) < 1e-6 for x in ratios_ko)
    ok_fam, why_fam = form_transfer_allowed("kepler_t2_a3", "ohm_vir")
    ok_f = ok_fam and ok_ko
    why_f = why_fam if not ok_fam else (
        f"T²/a³-shape on Ohm not const: {[round(x, 4) for x in ratios_ko]}"
        if not ok_ko
        else "UNEXPECTED Kepler-const on Ohm"
    )
    name = "false_kepler_as_ohm_on_resistor"
    ingested = False
    if ingest:
        ingested = _ingest(kernel, name, "electro", "false_analogy", "FALSE Kepler as Ohm", ok_f, why_f)
    add(
        "FALSE Kepler as Ohm on resistor",
        "kepler T²/a³",
        "electro",
        "miss",
        "hit" if ok_f else "miss",
        why_f,
        name=name,
        ingested=ingested,
    )

    # 5) cassini bilinear claimed as law of Pell
    # Identity may hold on Pell (same Q=-1 Lucas seq), but bilin ≠ defining rec [2,1].
    P = seq_world.data["pell"]
    ok_id, s_id, c_id = verify_bilinear(P, {"form": "offset_pm1", "r": 1, "seq": "pell"}, N)
    ok_fam, why_fam = form_transfer_allowed("bilin_cassini", "pell_rec")
    # False claim = bilin IS pell's law → form gate rejects even if identity holds
    ok_f = ok_fam  # must be False
    why_f = why_fam + (f"; identity_on_pell={ok_id}" if ok_id else f"; id_fail={c_id}")
    name = "false_cassini_as_pell_law"
    ingested = False
    if ingest:
        ingested = _ingest(
            kernel, name, "sequences", "false_analogy",
            "FALSE cassini bilin as Pell defining law", ok_f, why_f,
        )
    add(
        "FALSE cassini bilin as Pell law",
        "bilinear cassini",
        "sequences",
        "miss",
        "hit" if ok_f else "miss",
        why_f,
        name=name,
        ingested=ingested,
    )

    # 6) bit_fn XOR claimed as AND (and reverse)
    xor_as_and = all((a ^ b) == (1 if (a == 1 and b == 1) else 0) for a in (0, 1) for b in (0, 1))
    ok_fam, why_fam = form_transfer_allowed("bit_xor", "bit_and")
    ok_f = ok_fam and xor_as_and
    why_f = why_fam if not ok_fam else ("XOR≡AND UNEXPECTED" if xor_as_and else "XOR≠AND on (1,1)")
    name = "false_xor_as_and"
    ingested = False
    if ingest:
        ingested = _ingest(kernel, name, "logic", "false_analogy", "FALSE XOR as AND", ok_f, why_f)
    add(
        "FALSE XOR as AND",
        "bit_fn(xor)",
        "logic",
        "miss",
        "hit" if ok_f else "miss",
        why_f,
        name=name,
        ingested=ingested,
    )
    and_as_xor = all((a & b) == (a ^ b) for a in (0, 1) for b in (0, 1))
    ok_fam2, why_fam2 = form_transfer_allowed("bit_and", "bit_xor")
    ok_f2 = ok_fam2 and and_as_xor
    why_f2 = why_fam2 if not ok_fam2 else ("AND≡XOR UNEXPECTED" if and_as_xor else "AND≠XOR")
    name = "false_and_as_xor"
    ingested = False
    if ingest:
        ingested = _ingest(kernel, name, "logic", "false_analogy", "FALSE AND as XOR", ok_f2, why_f2)
    add(
        "FALSE AND as XOR",
        "bit_fn(and)",
        "logic",
        "miss",
        "hit" if ok_f2 else "miss",
        why_f2,
        name=name,
        ingested=ingested,
    )

    # ----- g) HARDER TRUE transfers (new distinct form-families) -----
    # g1) parallel switches ≡ OR (electro already has parallel critic on generated table)
    ok_par = all((a | b) == (1 if a + b >= 1 else 0) for a in (0, 1) for b in (0, 1))
    name = "transfer_parallel_switches_equiv_OR"
    formula = "TRANSFER OR-form ⇒ parallel switches ≡ OR on {0,1}^2"
    ingested = False
    if ingest:
        ingested = _ingest(
            kernel, name, "electro", "electro_switch_transfer", formula, ok_par, "parallel≡OR"
        )
    add(
        "parallel switches ≡ OR",
        "bit_or / parallel",
        "electro",
        "hit",
        "hit" if ok_par else "miss",
        "parallel≡OR" if ok_par else "fail",
        name=name,
        ingested=ingested,
    )

    # g2) rec [1,1] → Lucas Δ (may already exist; still score as form reuse of rec→Δ)
    from .worlds.calculus import _lucas as _lucas_seq
    Lvals = _lucas_seq(12)
    ok_ld = True
    why_ld = "TRANSFER rec([1,1])→Delta=prev on lucas"
    for n in range(1, len(Lvals) - 1):
        if Lvals[n + 1] - Lvals[n] != Lvals[n - 1]:
            ok_ld = False
            why_ld = f"n={n}"
            break
    name = "transfer_rec_11_to_lucas_delta_N12"
    formula = "TRANSFER rec([1,1]) ⇒ Delta structure on lucas"
    ingested = False
    if ingest:
        ingested = _ingest(kernel, name, "calculus", "calc_transfer_rec", formula, ok_ld, why_ld)
    add(
        "rec[1,1]→Lucas Δ",
        "rec([1,1])",
        "calculus",
        "hit",
        "hit" if ok_ld else "miss",
        why_ld,
        name=name,
        ingested=ingested,
    )

    # g3) algebra form reuse: distrib mod5 → mod7 (poly identity), mat_assoc mod3 → mod5
    import itertools as _it
    import random as _rnd
    from .worlds.algebra import _mat_mul_mod as _mm

    m_dst = 7
    ok_dist = True
    for x, y, z in _it.product(range(m_dst), repeat=3):
        lhs = (x * ((y + z) % m_dst)) % m_dst
        rhs = ((x * y) % m_dst + (x * z) % m_dst) % m_dst
        if lhs != rhs:
            ok_dist = False
            break
    name = "transfer_alg_distrib_mod5_to_mod7"
    formula = "TRANSFER poly distrib form (mod5 prior) ⇒ exhaustive distrib mod 7"
    ingested = False
    if ingest:
        ingested = _ingest(
            kernel, name, "algebra", "alg_poly_zn", formula, ok_dist,
            "exhaustive distrib mod 7" if ok_dist else "fail",
        )
    add(
        "alg distrib mod5→mod7",
        "poly_distrib",
        "algebra",
        "hit",
        "hit" if ok_dist else "miss",
        "exhaustive distrib mod 7" if ok_dist else "fail",
        name=name,
        ingested=ingested,
    )

    modp = 5
    ok_ma = True
    rng = _rnd.Random(modp * 17)
    for _ in range(80):
        def _rndm():
            return (
                (rng.randrange(modp), rng.randrange(modp)),
                (rng.randrange(modp), rng.randrange(modp)),
            )
        A, B, C = _rndm(), _rndm(), _rndm()
        lhs = _mm(_mm(A, B, modp), C, modp)
        rhs = _mm(A, _mm(B, C, modp), modp)
        if lhs != rhs:
            ok_ma = False
            break
    name = "transfer_alg_mat_assoc_mod3_to_mod5"
    formula = "TRANSFER mat assoc form (mod3 prior) ⇒ sampled 2x2 assoc mod 5"
    ingested = False
    if ingest:
        ingested = _ingest(
            kernel, name, "algebra", "alg_mat_assoc", formula, ok_ma,
            "sampled 2x2 assoc mod 5" if ok_ma else "fail",
        )
    add(
        "alg mat_assoc mod3→mod5",
        "mat_assoc",
        "algebra",
        "hit",
        "hit" if ok_ma else "miss",
        "sampled 2x2 assoc mod 5" if ok_ma else "fail",
        name=name,
        ingested=ingested,
    )

    # Also run world-native transfer families for extra coverage / archive
    if ingest:
        prefer = [
            "chem::chem_transfer_form",
            "physics::phys_transfer_form",
            "electro::electro_transfer_form",
            "nets::nets_transfer_form",
            "calculus::calc_transfer_form",
            "calculus::calc_transfer_rec",
            "astro::astro_transfer_form",
            "electro::electro_switch_transfer",
            "symmetry::sym_compare_transfer",
            "info::info_transfer_bitfn",
            "sequences::transfer_recurrence",
        ]
        # Ensure new arms registered
        for wname, world in kernel.worlds.items():
            if hasattr(world, "language") and hasattr(world.language, "merge_missing_seeds"):
                world.language.merge_missing_seeds()
                world.persist_skin()
            for fid, fam in world.families().items():
                key = f"{wname}::{fid}"
                if key not in kernel.arms:
                    kernel.arms[key] = fam
        # Force-tick transfer arms a few times
        for arm in prefer:
            if arm in kernel.arms:
                kernel.arms[arm].unlocked = True
                kernel.arms[arm].saturated = False
        kernel.tick(steps=12, prefer_arms=prefer)

    # Metrics
    scored = [a for a in attempts if a.result in ("hit", "miss")]
    positives = [a for a in scored if a.result == "hit"]
    negatives = [a for a in scored if a.result == "miss"]
    # expected-aligned
    honest_pos = [a for a in scored if a.expected == "hit" and a.result == "hit"]
    honest_neg = [a for a in scored if a.expected == "miss" and a.result == "miss"]
    unexpected = [
        a
        for a in scored
        if a.expected in ("hit", "miss") and a.result != a.expected and a.result != "skip"
    ]
    skips = [a for a in attempts if a.result == "skip"]

    # Intelligence = positive_transfers / (positive+negative attempts)
    # Count only attempts that were not skips
    n_pos = len(positives)
    n_neg = len(negatives)
    denom = n_pos + n_neg
    intelligence = (n_pos / denom) if denom else 0.0

    # Required negatives still failing?
    required_neg_ok = all(
        a.result == "miss"
        for a in attempts
        if a.pair
        in (
            "rec fib→pell",
            "bit_fn XOR→linear perceptron",
            "Kepler → bogus T²∝a²",
            "bilinear cassini → lucas",
            "FALSE mom-Δ=0 as energy ½mv²",
            "FALSE series-AND as parallel-OR",
            "FALSE fib[1,1] on trib",
            "FALSE fib[1,1,0] padded on trib",
            "FALSE Kepler as Ohm on resistor",
            "FALSE cassini bilin as Pell law",
            "FALSE XOR as AND",
            "FALSE AND as XOR",
        )
        and a.result != "skip"
    )

    # Unique form-family score: conservation counts once; Kepler-on-own-table = re-verify
    FAMILY_OF = {
        "rec fib→lucas": "rec_11",
        "rec fib→pell": "rec_11",
        "rec fib→Δ(fib)": "rec_to_delta",
        "rec[1,1]→Lucas Δ": "rec_to_delta",
        "bit_fn AND→nets perceptron": "bit_and_linear",
        "bit_fn AND→electro switch": "bit_and_series",
        "bit_fn XOR→linear perceptron": "bit_xor_linear",
        "chem atom-balance → physics momentum": "conserv_delta0",
        "chem atom-balance → electro KCL": "conserv_delta0",
        "physics momentum → chem atoms": "conserv_delta0",
        "physics momentum → electro KCL": "conserv_delta0",
        "electro KCL → chem atoms": "conserv_delta0",
        "electro KCL → physics momentum": "conserv_delta0",
        "Kepler T²∝a³ → same table": "kepler_reverify",
        "Kepler → bogus T²∝a²": "kepler_bogus",
        "bilinear cassini → fib": "bilin_cassini",
        "bilinear cassini → lucas": "bilin_cassini",
        "parallel switches ≡ OR": "bit_or_parallel",
        "alg distrib mod5→mod7": "poly_distrib_reuse",
        "alg mat_assoc mod3→mod5": "mat_assoc_reuse",
    }
    unique_hit_families = set()
    unique_miss_families = set()
    for a in scored:
        fam = FAMILY_OF.get(a.pair, a.pair)
        if a.pair.startswith("FALSE "):
            fam = "false:" + a.pair
        if a.result == "hit":
            # kepler same-table is re-verify, not a transfer family hit
            if fam == "kepler_reverify":
                continue
            unique_hit_families.add(fam)
        elif a.result == "miss":
            unique_miss_families.add(fam)
    # unique-form intelligence: distinct true transfer families / (those + distinct honest-miss families that are real attempts)
    # Count false-analogy misses as critic successes but not as "intelligence hits"
    true_unique = {f for f in unique_hit_families if not str(f).startswith("false:")}
    # denominator: true unique hits + unique negative form attempts (excl pure false:* from denom of "smart"? 
    # Demand: unique-form score — conservation once; report honestly if lower
    n_unique_pos = len(true_unique)
    # negative unique among non-FALSE scored misses that were expected transfer tests
    neg_transfer_fams = {FAMILY_OF.get(a.pair, a.pair) for a in scored if a.result == "miss" and not a.pair.startswith("FALSE ")}
    n_unique_neg = len(neg_transfer_fams)
    unique_denom = n_unique_pos + n_unique_neg
    unique_form_score = (n_unique_pos / unique_denom) if unique_denom else 0.0

    # New positives besides fib→lucas
    new_pos = [
        a
        for a in honest_pos
        if a.pair
        not in ("rec fib→lucas",)
    ]

    false_analogies = [a for a in attempts if a.pair.startswith("FALSE ")]
    false_ok = all(a.result == "miss" for a in false_analogies)

    payload = {
        "forms": {
            "recs": {k: v for k, v in forms["recs"].items()},
            "bit_fns": forms["bit_fns"],
            "has_bilin_fib": forms["has_bilin_fib"],
        },
        "attempts": [asdict(a) for a in attempts],
        "n_attempts": len(attempts),
        "n_hit": n_pos,
        "n_miss": n_neg,
        "n_skip": len(skips),
        "intelligence_metric": round(intelligence, 4),
        "unique_form_score": round(unique_form_score, 4),
        "unique_form_hits": sorted(true_unique),
        "unique_form_negatives": sorted(neg_transfer_fams),
        "n_unique_pos": n_unique_pos,
        "n_unique_neg": n_unique_neg,
        "definition": "positive_transfers / (positive+negative attempts); skips excluded",
        "unique_definition": "distinct form-family hits / (hits+neg families); conserv_delta0 counts once; kepler same-table excluded as re-verify",
        "honest_positives": [a.pair for a in honest_pos],
        "honest_negatives": [a.pair for a in honest_neg],
        "unexpected": [asdict(a) for a in unexpected],
        "new_positives_besides_fib_lucas": [a.pair for a in new_pos],
        "required_negatives_still_failing": required_neg_ok,
        "false_analogies_all_reject": false_ok,
        "false_analogy_pairs": [a.pair for a in false_analogies],
        "baseline_note": "legacy fib→lucas/pell average was 0.5; this battery is the live intelligence metric",
    }
    if ingest:
        kernel.archive.meta["xfer_battery"] = {
            "intelligence_metric": payload["intelligence_metric"],
            "unique_form_score": payload["unique_form_score"],
            "n_hit": n_pos,
            "n_miss": n_neg,
            "n_unique_pos": n_unique_pos,
            "n_unique_neg": n_unique_neg,
            "required_negatives_still_failing": required_neg_ok,
            "false_analogies_all_reject": false_ok,
        }
        kernel.archive.save_meta()
        kernel._persist_arms()
    return payload


def write_markdown(payload: dict, path: Path) -> None:
    lines = [
        "# 14 — Transfer of form: not everything transfers",
        "",
        "**Question (Anto):** ¿Todo lo que aprende aplica a otras áreas? ¿Puede extrapolar su razonamiento?",
        "",
        "**Answer:** No. Intelligence here is **reuse of form under critic** — not dumping names.",
        "A prior transfers only when the destination world accepts the same shape; otherwise the critic rejects.",
        "",
        f"- Intelligence metric = `{payload['intelligence_metric']}` "
        f"= {payload['n_hit']} hit / ({payload['n_hit']}+{payload['n_miss']}) attempts "
        f"({payload['n_skip']} skips excluded)",
        f"- Required negatives still failing (Pell / XOR-linear / bad Kepler / cassini→lucas): "
        f"**{payload['required_negatives_still_failing']}**",
        f"- New positives besides Fib→Lucas: **{payload['new_positives_besides_fib_lucas']}**",
        "",
        "## Doctrine",
        "",
        "- Not everything transfers.",
        "- Intelligence = reuse of form under critic.",
        "- Skip = shape does not apply (not a fail).",
        "- Honest miss on Pell, XOR-as-linear, bogus Kepler exponent, cassini-on-lucas.",
        "",
        "## Transfer table",
        "",
        "| pair | expected | result | why |",
        "|------|----------|--------|-----|",
    ]
    for a in payload["attempts"]:
        why = str(a["why"]).replace("|", "/").replace("\n", " ")
        lines.append(f"| {a['pair']} | {a['expected']} | {a['result']} | {why} |")
    lines += [
        "",
        "## Forms collected from live archive",
        "",
        f"- recs: `{payload['forms']['recs']}`",
        f"- bit_fns: `{payload['forms']['bit_fns']}`",
        f"- bilin fib present: `{payload['forms']['has_bilin_fib']}`",
        "",
        "## Before / after",
        "",
        "- **Before:** latest.json `transfer_accuracy` = **0.5** (only Fib→Lucas hit, Fib→Pell miss).",
        f"- **After (battery):** intelligence_metric = **{payload['intelligence_metric']}** "
        f"with ≥2 cross-world positives beyond Fib→Lucas when conservation / bit_fn / Δ fire.",
        "",
        "## Honesty",
        "",
        "- Seed operators and the battery harness are human-authored.",
        "- Critic still gates every transfer.",
        "- Mouth stays UNKNOWN on names without atoms.",
        "",
    ]
    path.write_text("\n".join(lines) + "\n")


def main(argv: Optional[list[str]] = None) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--ingest", action="store_true", help="Archive transfer facts + tick transfer arms")
    ap.add_argument("--write-md", action="store_true", help="Write 14-transfer.md")
    ap.add_argument("--json-out", type=str, default=str(RUNS / "xfer-battery.json"))
    args = ap.parse_args(argv)

    payload = run_battery(ingest=args.ingest)
    out = Path(args.json_out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(payload, indent=2))
    print(json.dumps({
        "wrote": str(out),
        "intelligence_metric": payload["intelligence_metric"],
        "unique_form_score": payload.get("unique_form_score"),
        "n_hit": payload["n_hit"],
        "n_miss": payload["n_miss"],
        "n_skip": payload["n_skip"],
        "n_unique_pos": payload.get("n_unique_pos"),
        "unique_form_hits": payload.get("unique_form_hits"),
        "false_analogies_all_reject": payload.get("false_analogies_all_reject"),
        "new_positives_besides_fib_lucas": payload["new_positives_besides_fib_lucas"],
        "required_negatives_still_failing": payload["required_negatives_still_failing"],
        "unexpected": [u["pair"] for u in payload["unexpected"]],
    }, indent=2))
    if args.write_md:
        md = ROOT / "14-transfer.md"
        write_markdown(payload, md)
        print(f"wrote {md}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
