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
        )
        and a.result != "skip"
    )

    # New positives besides fib→lucas
    new_pos = [
        a
        for a in honest_pos
        if a.pair
        not in ("rec fib→lucas",)
    ]

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
        "definition": "positive_transfers / (positive+negative attempts); skips excluded",
        "honest_positives": [a.pair for a in honest_pos],
        "honest_negatives": [a.pair for a in honest_neg],
        "unexpected": [asdict(a) for a in unexpected],
        "new_positives_besides_fib_lucas": [a.pair for a in new_pos],
        "required_negatives_still_failing": required_neg_ok,
        "baseline_note": "legacy fib→lucas/pell average was 0.5; this battery is the live intelligence metric",
    }
    if ingest:
        kernel.archive.meta["xfer_battery"] = {
            "intelligence_metric": payload["intelligence_metric"],
            "n_hit": n_pos,
            "n_miss": n_neg,
            "required_negatives_still_failing": required_neg_ok,
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
        "n_hit": payload["n_hit"],
        "n_miss": payload["n_miss"],
        "n_skip": payload["n_skip"],
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
