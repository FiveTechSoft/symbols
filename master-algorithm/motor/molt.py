"""
Molt loop — the motor sheds its skin when saturated.

After a tick batch, detect saturation → mutate hypothesis language → run another
batch → KEEP the skin only if a metric rose; else revert. Critic still gates
every archived fact (no Python-fiat verified/1).

Usage (via run_sublime.py):
  python motor/run_sublime.py
"""

from __future__ import annotations

import json
import time
from pathlib import Path
from typing import Any, Optional

from .kernel import MotorKernel, arm_key
from .worlds.schema_lang import HypothesisLanguage

MOTOR_DIR = Path(__file__).resolve().parent
RUNS_DIR = MOTOR_DIR / "runs"
DEFAULT_SCRATCH = MOTOR_DIR / "archive-sublime"
SKIN_NAME = "skin.json"
K_STALE = 3  # ticks of no new types before saturation signal


def _metrics_snapshot(kernel: MotorKernel) -> dict[str, Any]:
    from .metrics import compute_metrics, transfer_eval

    m = compute_metrics(kernel)
    te = transfer_eval(kernel)
    seq = kernel.worlds.get("sequences")
    lang = getattr(seq, "language", None) if seq else None
    n_schema = lang.n_schema_classes() if lang else 0
    unlocked = lang.unlocked_ids() if lang else []
    lucas = te.get("lucas", {}).get("transfer_accuracy", 0.0)
    pell = te.get("pell", {}).get("transfer_accuracy", 0.0)
    return {
        "n_verified": m["n_verified_facts"],
        "n_rejected": m["n_rejected"],
        "n_types": m["n_distinct_types"],
        "reject_rate": m["reject_rate"],
        "transfer_accuracy": m["transfer_accuracy"],
        "lemma_reuse_rate": m["lemma_reuse_rate"],
        "n_schema_classes": n_schema,
        "unlocked_schemas": unlocked,
        "lucas_transfer": lucas,
        "pell_transfer": pell,
        "transfer_honest": (lucas >= 0.99 and pell <= 0.05) if (lucas or pell or True) else False,
        "language_generation": lang.generation if lang else 0,
    }


def detect_saturation(kernel: MotorKernel, prev: Optional[dict], cur: dict, batch_ticks: int) -> tuple[bool, str]:
    """
    Saturation signals:
      - a productive schema/family produced 0 new verified types this batch
      - reject_rate stuck (Δ≈0) while verified also stuck
      - transfer plateau (no change) with unlocked schemas saturated
    """
    reasons = []
    seq = kernel.worlds.get("sequences")
    lang: Optional[HypothesisLanguage] = getattr(seq, "language", None) if seq else None

    if lang:
        for sch in lang.schemas.values():
            if sch.unlocked and not sch.dead_end and sch.saturated:
                reasons.append(f"schema {sch.id} saturated")
            if sch.unlocked and not sch.dead_end and sch.ticks_no_new_type >= K_STALE:
                reasons.append(f"schema {sch.id} stale {sch.ticks_no_new_type} ticks")

    # Arm-level: all unlocked non-dead saturated
    active = [
        a for a in kernel.arms.values()
        if a.unlocked and not a.saturated and not a.dead_end
    ]
    if not active:
        reasons.append("all productive UCB arms saturated")

    if prev is not None:
        d_types = cur["n_types"] - prev["n_types"]
        d_ver = cur["n_verified"] - prev["n_verified"]
        d_rej = abs(cur["reject_rate"] - prev["reject_rate"])
        d_tr = abs(cur["transfer_accuracy"] - prev["transfer_accuracy"])
        if d_types == 0 and d_ver == 0:
            reasons.append("0 new verified and 0 new types this batch")
        if d_types == 0 and d_rej < 0.02 and d_ver <= 1:
            reasons.append("reject_rate stuck + type plateau")
        if d_tr < 1e-9 and d_types == 0:
            reasons.append("transfer plateau")

    # Always allow first molt pressure after a warm batch
    if prev is None and batch_ticks > 0:
        # soft signal if few unlocked generative schemas
        if lang and lang.n_unlocked() <= 5:
            reasons.append("seed language still narrow — molt to grow")

    if reasons:
        return True, "; ".join(reasons[:4])
    return False, "not saturated"


def _skin_improved(before: dict, after: dict) -> tuple[bool, str]:
    """KEEP skin if at least one metric rose (or transfer stayed honest while schemas grew)."""
    gains = []
    if after["n_verified"] > before["n_verified"]:
        gains.append(f"n_verified {before['n_verified']}→{after['n_verified']}")
    if after["n_types"] > before["n_types"]:
        gains.append(f"n_types {before['n_types']}→{after['n_types']}")
    if after["n_schema_classes"] > before["n_schema_classes"]:
        gains.append(
            f"n_schema_classes {before['n_schema_classes']}→{after['n_schema_classes']}"
        )
    # New unlocked schema that wasn't before
    before_u = set(before.get("unlocked_schemas") or [])
    after_u = set(after.get("unlocked_schemas") or [])
    newly = after_u - before_u
    if newly:
        gains.append(f"unlocked {sorted(newly)}")
    # Transfer still honest (Lucas~1, Pell~0) counts as keep if we unlocked something
    # or if lucas improved
    if after.get("lucas_transfer", 0) > before.get("lucas_transfer", 0):
        gains.append(
            f"lucas_transfer {before.get('lucas_transfer')}→{after.get('lucas_transfer')}"
        )
    # Honest transfer preserved + language grew
    if after.get("transfer_honest") and newly:
        if "unlocked" not in " ".join(gains):
            gains.append("transfer_honest+new_schema")
    # Reject rate moving toward healthy mid (not 0, not 1) after being stuck at extreme
    br, ar = before.get("reject_rate", 0), after.get("reject_rate", 0)
    if (br < 0.05 or br > 0.95) and 0.05 < ar < 0.95:
        gains.append(f"reject_rate healthier {br}→{ar}")

    if gains:
        return True, "; ".join(gains)
    return False, "no metric rose — revert skin"


def _persist_schema_clauses(kernel: MotorKernel, lang: HypothesisLanguage) -> None:
    """Write schema/2 metadata clauses (NOT verified/1)."""
    for s in lang.schemas.values():
        # schema(Id, unlocked(true/false)).
        flag = "true" if s.unlocked else "false"
        kernel.archive.append_clause(
            f"schema({s.id}, unlocked({flag})).",
            comment=f"hypothesis-language skin gen={lang.generation}",
        )
    kernel.archive.save()


def _apply_language_to_arms(kernel: MotorKernel) -> None:
    seq = kernel.worlds.get("sequences")
    if seq is None or not hasattr(seq, "sync_arms_from_language"):
        return
    seq.sync_arms_from_language(kernel.arms)
    # Also refresh geometry arms if geo_invent param changed
    geo = kernel.worlds.get("geometry")
    if geo is not None:
        for fid, fam in geo.families().items():
            key = arm_key("geometry", fid)
            if key not in kernel.arms:
                kernel.arms[key] = fam
            else:
                kernel.arms[key].unlocked = fam.unlocked
                if not fam.saturated:
                    kernel.arms[key].saturated = False
                kernel.arms[key].param = fam.param
                kernel.arms[key].param_max = fam.param_max


def molt_once(kernel: MotorKernel, reason: str) -> dict:
    """Mutate language, persist skin candidate, sync arms. Returns molt info."""
    seq = kernel.worlds["sequences"]
    lang: HypothesisLanguage = seq.language
    info = lang.mutate(reason)
    # Persist skin JSON
    skin_path = kernel.archive_dir / SKIN_NAME
    lang.save_skin(skin_path)
    seq.persist_skin()
    _persist_schema_clauses(kernel, lang)
    _apply_language_to_arms(kernel)
    kernel._persist_arms()
    return info


def revert_language(kernel: MotorKernel, backup: HypothesisLanguage) -> None:
    seq = kernel.worlds["sequences"]
    seq.language = backup.clone()
    geo = kernel.worlds.get("geometry")
    if geo is not None:
        geo.language = seq.language
    skin_path = kernel.archive_dir / SKIN_NAME
    seq.language.save_skin(skin_path)
    seq.persist_skin()
    _persist_schema_clauses(kernel, seq.language)
    _apply_language_to_arms(kernel)
    kernel._persist_arms()


def run_molt_loop(
    archive_dir: Path | str | None = None,
    batch_ticks: int = 12,
    min_molts: int = 3,
    max_molts: int = 8,
    reset: bool = True,
) -> dict:
    """
    Full sublime + molt procedure on scratch archive.
    Returns log dict written to motor/runs/molt.json and sublime.json summary.
    """
    archive_dir = archive_dir or DEFAULT_SCRATCH
    t0 = time.time()
    kernel = MotorKernel(reset=reset, archive_dir=archive_dir)

    # Seed skin
    seq = kernel.worlds["sequences"]
    lang: HypothesisLanguage = seq.language
    skin_path = Path(kernel.archive_dir) / SKIN_NAME
    lang.save_skin(skin_path)
    seq.persist_skin()
    _persist_schema_clauses(kernel, lang)

    before_all = _metrics_snapshot(kernel)
    molt_log: list[dict] = []
    batch_log: list[dict] = []

    # Warm-up batch (generation 0 skin)
    warm = kernel.tick(steps=batch_ticks)
    metrics_after_warm = _metrics_snapshot(kernel)
    batch_log.append(
        {
            "phase": "warmup",
            "generation": lang.generation,
            "ticks": batch_ticks,
            "metrics": metrics_after_warm,
            "accepted_tail": warm.get("step_log_tail", [])[-3:],
        }
    )

    prev_metrics = metrics_after_warm
    molts_kept = 0
    molts_reverted = 0

    for i in range(max_molts):
        saturated, sat_reason = detect_saturation(
            kernel, prev_metrics, prev_metrics, batch_ticks
        )
        # Force molt until min_molts even if detector is shy
        if not saturated and i < min_molts:
            saturated = True
            sat_reason = f"forced molt {i+1}/{min_molts} (self-modify mandate)"
        if not saturated and molts_kept >= min_molts:
            molt_log.append(
                {
                    "molt_index": i,
                    "action": "stop",
                    "reason": "not saturated and min_molts satisfied",
                }
            )
            break

        backup = lang.clone()
        metrics_before_molt = _metrics_snapshot(kernel)
        molt_info = molt_once(kernel, sat_reason)
        lang = seq.language  # refresh ref

        # Tick batch under new skin
        batch = kernel.tick(steps=batch_ticks)
        metrics_after = _metrics_snapshot(kernel)
        keep, keep_reason = _skin_improved(metrics_before_molt, metrics_after)

        entry = {
            "molt_index": i,
            "generation": lang.generation,
            "saturation_reason": sat_reason,
            "molt_actions": molt_info.get("actions"),
            "unlocked_before": molt_info.get("unlocked_before"),
            "unlocked_after": molt_info.get("unlocked_after"),
            "metrics_before": metrics_before_molt,
            "metrics_after": metrics_after,
            "keep": keep,
            "keep_reason": keep_reason,
            "ticks": batch_ticks,
            "n_verified_delta": metrics_after["n_verified"] - metrics_before_molt["n_verified"],
            "n_types_delta": metrics_after["n_types"] - metrics_before_molt["n_types"],
        }

        if keep:
            molts_kept += 1
            entry["action"] = "keep_skin"
            # Skin already on disk
        else:
            molts_reverted += 1
            entry["action"] = "revert_skin"
            revert_language(kernel, backup)
            lang = seq.language
            # Re-measure after revert (archive facts stay — only language reverts)
            entry["metrics_after_revert"] = _metrics_snapshot(kernel)

        molt_log.append(entry)
        batch_log.append(
            {
                "phase": f"post_molt_{i}",
                "generation": lang.generation,
                "keep": keep,
                "metrics": _metrics_snapshot(kernel),
            }
        )
        prev_metrics = _metrics_snapshot(kernel)

        # Stop early if we have enough kept molts and last was revert with no saturation urgency
        if molts_kept >= min_molts and i + 1 >= min_molts and not keep:
            # try one more only if still saturated
            pass

    final = _metrics_snapshot(kernel)
    # Collect example clauses from theory
    examples = _example_clauses(kernel)

    result = {
        "archive_dir": str(kernel.archive_dir),
        "skin_path": str(skin_path),
        "batch_ticks": batch_ticks,
        "min_molts": min_molts,
        "max_molts": max_molts,
        "molts_attempted": len([m for m in molt_log if m.get("action") != "stop"]),
        "molts_kept": molts_kept,
        "molts_reverted": molts_reverted,
        "metrics_before": before_all,
        "metrics_after_warmup": metrics_after_warm,
        "metrics_final": final,
        "molt_log": molt_log,
        "batch_log": batch_log,
        "example_clauses": examples,
        "transfer_eval": kernel.archive.meta.get("transfer_eval"),
        "elapsed_s": round(time.time() - t0, 3),
        "honesty": (
            "Molt mutates hypothesis language (skin) only. Every verified/1 and rec/2 "
            "clause still requires critic acceptance. Reverted skins discard language "
            "mutations but keep critic-gated archive facts already written."
        ),
    }

    RUNS_DIR.mkdir(parents=True, exist_ok=True)
    (RUNS_DIR / "molt.json").write_text(json.dumps(result, indent=2), encoding="utf-8")

    sublime = {
        "n_schema_classes": final["n_schema_classes"],
        "n_verified": final["n_verified"],
        "n_rejected": final["n_rejected"],
        "transfer_accuracy": final["transfer_accuracy"],
        "lucas_transfer": final["lucas_transfer"],
        "pell_transfer": final["pell_transfer"],
        "reject_rate": final["reject_rate"],
        "n_types": final["n_types"],
        "lemma_reuse_rate": final["lemma_reuse_rate"],
        "language_generation": final["language_generation"],
        "unlocked_schemas": final["unlocked_schemas"],
        "molts_kept": molts_kept,
        "molts_reverted": molts_reverted,
        "metrics_before": before_all,
        "metrics_final": final,
        "example_clauses": examples,
        "elapsed_s": result["elapsed_s"],
    }
    (RUNS_DIR / "sublime.json").write_text(json.dumps(sublime, indent=2), encoding="utf-8")
    return result


def _example_clauses(kernel: MotorKernel, n: int = 5) -> list[dict]:
    """Pick verified + rejected clauses with reasons for the report."""
    lines = kernel.archive._lines
    verified = [ln for ln in lines if ln.startswith("verified(")]
    rejected = [ln for ln in lines if ln.startswith("rejected(")]
    recs = [ln for ln in lines if ln.startswith("rec(")]
    schemas = [ln for ln in lines if ln.startswith("schema(")]
    bilin = [ln for ln in verified if "bilin_" in ln or "bilinear" in ln]
    invent = [ln for ln in verified if "geo_invent" in ln or "invent" in ln]
    period = [ln for ln in lines if ln.startswith("true_mod(")]

    picks: list[dict] = []

    def add(kind, line, reason):
        if len(picks) >= n:
            return
        picks.append({"status": kind, "clause": line[:200], "reason": reason})

    for ln in recs[:1]:
        add("verified_rec", ln, "critic holds_rec accepted; archived rec/2")
    for ln in bilin[:1]:
        add("verified", ln, "bilinear_schema operator (NOT old canned bilinear_fib); critic accepted")
    for ln in invent[:1]:
        add("verified", ln, "geo_invent mutate_construction; engine proved then archived")
    for ln in period[:1]:
        add("verified_period", ln, "modperiod_schema search; holds_period accepted")
    # Prefer an explicit schema reject (bogus bilinear / dead-end)
    for ln in rejected:
        if "bogus" in ln:
            add("rejected", ln, "bilinear_schema bogus RHS; critic finite-fail")
            break
    for ln in rejected:
        if len(picks) >= n:
            break
        if "bogus" in ln:
            continue
        if "prime" in ln or "NEG_" in ln or "lucas" in ln:
            add("rejected", ln, "critic finite-fail / dead-end or failed transfer-shaped bilin")
    for ln in rejected:
        if len(picks) >= n:
            break
        if not any(p["clause"] == ln[:200] for p in picks):
            add("rejected", ln, "critic rejected")
    for ln in schemas[:1]:
        add("skin_meta", ln, "schema/2 language skin (not a verified theorem)")
    return picks[:n]


if __name__ == "__main__":
    result = run_molt_loop()
    print(json.dumps({
        "molts_kept": result["molts_kept"],
        "molts_reverted": result["molts_reverted"],
        "metrics_final": result["metrics_final"],
        "elapsed_s": result["elapsed_s"],
    }, indent=2))
