"""Intelligence curves + baselines + transfer evaluation (real numbers only)."""

from __future__ import annotations

from typing import Any, TYPE_CHECKING

if TYPE_CHECKING:
    from .kernel import MotorKernel


def compute_metrics(kernel: "MotorKernel") -> dict[str, Any]:
    arch = kernel.archive
    n_verified = arch.count_verified()
    n_rejected = arch.count_rejected()
    n_types = len(arch.distinct_types_approx())
    total_decisions = n_verified + n_rejected
    reject_rate = (n_rejected / total_decisions) if total_decisions else 0.0

    # lemma reuse from geometry world if present
    geo = kernel.worlds.get("geometry")
    lemma_reuse = float(geo.lemma_reuse_rate()) if geo and hasattr(geo, "lemma_reuse_rate") else 0.0

    transfer_acc = _quick_transfer_accuracy(kernel)

    return {
        "n_verified_facts": n_verified,
        "n_distinct_types": n_types,
        "transfer_accuracy": round(transfer_acc, 4),
        "lemma_reuse_rate": round(lemma_reuse, 4),
        "reject_rate": round(reject_rate, 4),
        "n_rejected": n_rejected,
        "n_lemmas": arch.count_lemmas(),
    }


def _predict_next(vals: list[int], coeffs: list[int], n: int) -> int:
    return sum(coeffs[i] * vals[n - 1 - i] for i in range(len(coeffs)))


def _quick_transfer_accuracy(kernel: "MotorKernel") -> float:
    """
    Prefer cross-world xfer_battery intelligence metric when available;
    else fall back to Fib→Lucas/Pell next-term average (legacy 0.5 baseline).
    """
    # Live battery metric cached on meta after xfer_battery --ingest / transfer_eval
    cached = (kernel.archive.meta or {}).get("xfer_battery", {})
    if isinstance(cached, dict) and "intelligence_metric" in cached:
        return float(cached["intelligence_metric"])

    seq_world = kernel.worlds.get("sequences")
    if not seq_world:
        return 0.0
    recs = {s: c for s, c in kernel.archive.list_recs()}
    src_coeffs = recs.get("fib")
    if not src_coeffs:
        if not recs:
            return 0.0
        src_coeffs = next(iter(recs.values()))

    hits = 0
    total = 0
    for target in ("lucas", "pell"):
        if target not in seq_world.data:
            continue
        vals = seq_world.data[target]
        order = len(src_coeffs)
        for n in range(order, min(len(vals), order + 12)):
            pred = _predict_next(vals, src_coeffs, n)
            total += 1
            if pred == vals[n]:
                hits += 1
    if total == 0:
        return 0.0
    return hits / total


def transfer_eval(kernel: "MotorKernel") -> dict[str, Any]:
    """
    Honest transfer vs no-archive baseline:
      A_transfer: use fib rec clause to predict Lucas/Pell next terms
      B_no_archive: use wrong prior [2,0] (geometric) as frozen wrong language
      Also report search-from-scratch on target alone (oracle upper bound on that seq)
    """
    seq_world = kernel.worlds.get("sequences")
    if not seq_world:
        return {"error": "no sequences world"}

    recs = {s: c for s, c in kernel.archive.list_recs()}
    fib_rec = recs.get("fib")
    results = {}

    for target in ("lucas", "pell"):
        vals = seq_world.data[target]
        # transfer from fib if present else from any other
        src = "fib" if fib_rec else (next(iter(recs), None))
        coeffs = recs.get(src) if src else None

        def accuracy(c):
            if not c:
                return 0.0
            order = len(c)
            hits = tot = 0
            for n in range(order, len(vals)):
                tot += 1
                if _predict_next(vals, c, n) == vals[n]:
                    hits += 1
            return hits / tot if tot else 0.0

        # Baseline B: no archive — frozen wrong prior
        wrong = [2, 0]
        # Baseline A: more data frozen language — use order-1 only [1] on longer prefix
        # (does not transfer knowledge)
        order1 = None
        # quick search order-1
        for a in range(-3, 4):
            if a == 0:
                continue
            if accuracy([a]) == 1.0:
                order1 = [a]
                break

        # Scratch search order-2 on target (oracle for that sequence alone)
        from .worlds.sequences import _search_recurrence

        scratch = _search_recurrence(vals, 2)

        transfer_acc = accuracy(coeffs) if coeffs else 0.0
        results[target] = {
            "transfer_from": src,
            "transfer_coeffs": coeffs,
            "transfer_accuracy": round(transfer_acc, 4),
            "baseline_no_archive_wrong_prior_[2,0]": round(accuracy(wrong), 4),
            "baseline_frozen_order1": round(accuracy(order1) if order1 else 0.0, 4),
            "scratch_order2_on_target": scratch,
            "scratch_order2_accuracy": round(accuracy(scratch) if scratch else 0.0, 4),
            "transfer_beats_no_archive": transfer_acc > accuracy(wrong),
            "note": (
                "Lucas shares fib's [1,1] law so transfer should hit; "
                "Pell is [2,1] so fib→pell transfer should FAIL — that is an honest result."
            ),
        }

    # Summary
    lucas_ok = results.get("lucas", {}).get("transfer_beats_no_archive", False)
    pell_ok = results.get("pell", {}).get("transfer_beats_no_archive", False)
    results["summary"] = {
        "lucas_transfer_beats_baseline": lucas_ok,
        "pell_transfer_beats_baseline": pell_ok,
        "absorbed_intelligence": bool(lucas_ok),
        "negative_transfer_pell_expected": not pell_ok,
    }
    # Cross-world battery (COMPARE / transfer_form)
    try:
        from .xfer_battery import run_battery

        bat = run_battery(kernel=kernel, ingest=False)
        results["xfer_battery"] = {
            "intelligence_metric": bat["intelligence_metric"],
            "n_hit": bat["n_hit"],
            "n_miss": bat["n_miss"],
            "n_skip": bat["n_skip"],
            "new_positives_besides_fib_lucas": bat["new_positives_besides_fib_lucas"],
            "required_negatives_still_failing": bat["required_negatives_still_failing"],
            "honest_positives": bat["honest_positives"],
            "honest_negatives": bat["honest_negatives"],
        }
        results["summary"]["cross_world_intelligence"] = bat["intelligence_metric"]
        results["summary"]["cross_world_new_positives"] = bat["new_positives_besides_fib_lucas"]
        kernel.archive.meta["xfer_battery"] = {
            "intelligence_metric": bat["intelligence_metric"],
            "n_hit": bat["n_hit"],
            "n_miss": bat["n_miss"],
            "required_negatives_still_failing": bat["required_negatives_still_failing"],
            "new_positives_besides_fib_lucas": bat["new_positives_besides_fib_lucas"],
        }
    except Exception as e:
        results["xfer_battery"] = {"error": str(e)}
    return results


def run_baselines(kernel: "MotorKernel") -> dict[str, Any]:
    """
    (A) more data, frozen language — mine fib with fixed families at N=20,40
        without scheduler / without archive growth across N.
    (B) no archive / no transfer — run transfer_recurrence family with empty recs.
    """
    import sys
    from pathlib import Path

    root = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(root / "fibonacci"))
    import miner as M

    # A: frozen language at growing N
    A = []
    for N in (20, 40):
        F = M.fib_prefix(N)
        types = set()
        n_true = 0
        # fixed language subset
        r = M.check_order2_classic(F, N)
        if r.true:
            n_true += 1
            types.add("linear_recurrence")
        r = M.check_ratios_to_phi(F, N)
        if r.true:
            n_true += 1
            types.add("ratio_limit")
        for rel in M.check_cassini(F, N):
            if rel.true:
                n_true += 1
                types.add(rel.name.split("_")[0])
        A.append({"N": N, "n_true": n_true, "n_types": len(types), "types": sorted(types)})

    type_growth = A[-1]["n_types"] - A[0]["n_types"] if len(A) >= 2 else 0

    # B: no-archive transfer attempt
    seq = kernel.worlds["sequences"]
    # temporarily ignore archive recs: score wrong prior on lucas
    vals = seq.data["lucas"]
    wrong = [2, 0]
    hits = tot = 0
    for n in range(2, len(vals)):
        pred = wrong[0] * vals[n - 1] + wrong[1] * vals[n - 2]
        tot += 1
        if pred == vals[n]:
            hits += 1
    B = {
        "description": "no archive: frozen wrong prior [2,0] on Lucas",
        "accuracy": round(hits / tot if tot else 0.0, 4),
        "hits": hits,
        "total": tot,
    }

    return {
        "A_more_data_frozen_language": {
            "points": A,
            "type_growth_20_to_40": type_growth,
            "verdict": (
                "extra N adds no new relation types in frozen language"
                if type_growth == 0
                else "unexpected type growth"
            ),
        },
        "B_no_archive_no_transfer": B,
    }
