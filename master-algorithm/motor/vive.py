"""Self-improve exam harness: BookBrain live + motor tick + gap bias.

Never invents theorems. Teaching = tick + sync + recheck gaps.
"""

from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path
from typing import Any

MOTOR_DIR = Path(__file__).resolve().parent
ROOT = MOTOR_DIR.parent
BOOKBRAIN = ROOT / "bookbrain"
RUNS = MOTOR_DIR / "runs"
VIVE_JSON = RUNS / "vive.json"

# Map gap hint tokens → (world, family) for UCB bias (catalog only; no invent).
HINT_TO_ARM: dict[str, tuple[str, str]] = {
    "sequences::linear_recurrences": ("sequences", "linear_recurrences"),
    "sequences::transfer_recurrence": ("sequences", "transfer_recurrence"),
    "sequences::bilinear_fib": ("sequences", "bilinear_fib"),
    "sequences::modular_periods": ("sequences", "modular_periods"),
    "geometry::euclid_conjectures": ("geometry", "euclid_conjectures"),
    "geometry::lemma_reuse": ("geometry", "lemma_reuse"),
}


def _swipl_exam() -> tuple[str, dict[str, Any]]:
    """Run one exam round via swipl -s mejora.pl -g mejora_exam."""
    cmd = [
        "swipl",
        "-q",
        "-s",
        str(BOOKBRAIN / "mejora.pl"),
        "-g",
        "mejora_exam",
        "-t",
        "halt",
    ]
    proc = subprocess.run(
        cmd,
        cwd=str(BOOKBRAIN),
        capture_output=True,
        text=True,
        timeout=180,
    )
    out = (proc.stdout or "") + ("\n" + proc.stderr if proc.stderr else "")
    parsed = _parse_exam_output(out)
    parsed["returncode"] = proc.returncode
    return out, parsed


def _parse_exam_output(text: str) -> dict[str, Any]:
    items: list[dict[str, Any]] = []
    answers: dict[int, str] = {}
    for line in text.splitlines():
        if line.startswith("RESULT|"):
            parts = line.split("|", 3)
            if len(parts) == 4:
                _, sid, expect, rest = parts
                # rest = verdict|question  (verdict has no spaces usually)
                if "|" in rest:
                    verdict, q = rest.split("|", 1)
                else:
                    verdict, q = rest, ""
                items.append(
                    {
                        "id": int(sid),
                        "expect": expect,
                        "verdict": verdict,
                        "question": q,
                    }
                )
        elif line.startswith("ANSWER|"):
            # ANSWER|id|text...
            m = re.match(r"ANSWER\|(\d+)\|(.*)$", line)
            if m:
                answers[int(m.group(1))] = m.group(2)
        elif line.startswith("SCORELINE|"):
            p = line.split("|")
            # SCORELINE|n|known|correct_unknown|unknown_on_known|identity_ok|growth_ok
            score = {
                "n": int(p[1]),
                "known": int(p[2]),
                "correct_unknown": int(p[3]),
                "unknown_on_known": int(p[4]),
                "identity_ok": int(p[5]),
                "growth_ok": int(p[6]),
            }
        elif line.startswith("HINTLINE|"):
            raw = line[len("HINTLINE|") :].strip()
            hints = _parse_prolog_list(raw)

    for it in items:
        it["answer"] = answers.get(it["id"], "")

    if "score" not in locals():
        score = {
            "n": len(items),
            "known": sum(1 for i in items if i["verdict"] == "known"),
            "correct_unknown": sum(
                1 for i in items if i["verdict"] == "correct_unknown"
            ),
            "unknown_on_known": sum(
                1 for i in items if i["verdict"] == "unknown_on_known"
            ),
            "identity_ok": sum(1 for i in items if i["verdict"] == "identity_ok"),
            "growth_ok": sum(1 for i in items if i["verdict"] == "growth_ok"),
        }
    if "hints" not in locals():
        hints = []
        for i in items:
            if i["verdict"] == "unknown_on_known":
                hints.append(_hint_from_question(i["question"]))

    return {"items": items, "score": score, "hints": hints, "transcript": text}


def _parse_prolog_list(raw: str) -> list[str]:
    raw = raw.strip()
    if raw in ("[]", ""):
        return []
    # ['a','b'] or [a,b]
    inner = raw.strip("[]")
    parts = []
    for p in re.findall(r"'([^']+)'|([^,\s]+)", inner):
        parts.append(p[0] or p[1])
    return [x for x in parts if x]


def _hint_from_question(q: str) -> str:
    l = q.lower()
    if "recurrence" in l:
        return "sequences::linear_recurrences"
    if "transfer" in l or "pell" in l:
        return "sequences::transfer_recurrence"
    if "cassini" in l:
        return "sequences::bilinear_fib"
    if "pisano" in l:
        return "sequences::modular_periods"
    if "geo_" in l or "lemma" in l or "isos" in l:
        return "geometry::euclid_conjectures"
    return "sequences::linear_recurrences"


def _hints_to_prefer(hints: list[str]) -> list[str]:
    arms: list[str] = []
    for h in hints:
        pair = HINT_TO_ARM.get(h)
        if pair:
            arms.append(f"{pair[0]}::{pair[1]}")
    # de-dupe preserve order
    seen: set[str] = set()
    out: list[str] = []
    for a in arms:
        if a not in seen:
            seen.add(a)
            out.append(a)
    return out


def run_vive(rounds: int = 2, tick_steps: int = 5) -> dict[str, Any]:
    """Exam → tick(biased by gaps) → resync exam, R times. Persist vive.json."""
    from motor.kernel import MotorKernel

    RUNS.mkdir(parents=True, exist_ok=True)
    round_rows: list[dict[str, Any]] = []
    prefer: list[str] = []

    for r in range(1, rounds + 1):
        print(f"\n===== VIVE ROUND {r}/{rounds} =====", flush=True)
        transcript, parsed = _swipl_exam()
        score = parsed["score"]
        hints = parsed.get("hints") or []
        prefer = _hints_to_prefer(hints)

        # Always tick a few steps; bias toward gap arms when present.
        k = MotorKernel(reset=False)
        before_v = k.archive.count_verified()
        latest = k.tick(steps=tick_steps, prefer_arms=prefer or None)
        after_v = latest["n_verified_facts"]

        row = {
            "round": r,
            "score": score,
            "hints": hints,
            "prefer_arms": prefer,
            "unknown_on_known_questions": [
                i["question"]
                for i in parsed["items"]
                if i["verdict"] == "unknown_on_known"
            ],
            "qa": [
                {
                    "id": i["id"],
                    "q": i["question"],
                    "expect": i["expect"],
                    "verdict": i["verdict"],
                    "a": i.get("answer", "")[:500],
                }
                for i in parsed["items"]
            ],
            "tick_steps": tick_steps,
            "verified_before_tick": before_v,
            "verified_after_tick": after_v,
            "total_steps": latest["total_steps"],
            "n_distinct_types": latest.get("n_distinct_types"),
            "transfer_accuracy": latest.get("transfer_accuracy"),
            "transcript_tail": transcript[-4000:],
        }
        round_rows.append(row)
        print(
            f"ROUND {r} score={score} prefer={prefer} "
            f"verified {before_v}→{after_v}",
            flush=True,
        )

    # Verdict across rounds
    first, last = round_rows[0]["score"], round_rows[-1]["score"]
    known_rose = last["known"] > first["known"]
    unk_on_known_down = last["unknown_on_known"] < first["unknown_on_known"]
    true_unknowns_ok = all(
        row["score"]["correct_unknown"] >= 2  # filotaxis + alma
        for row in round_rows
    )
    theory_grew = round_rows[-1]["verified_after_tick"] > round_rows[0]["verified_before_tick"]
    verdict = {
        "known_count_rose": known_rose,
        "unknown_on_known_went_down": unk_on_known_down,
        "true_unknowns_stayed_unknown": true_unknowns_ok,
        "theory_verified_grew": theory_grew,
        "verified_span": [
            round_rows[0]["verified_before_tick"],
            round_rows[-1]["verified_after_tick"],
        ],
        "first_score": first,
        "last_score": last,
        "self_improved": known_rose or unk_on_known_down,
        "note": (
            "Exam known maxed by existing theory; ticks still grow theory.pl. "
            "Wrong-UNKNOWN gaps bias UCB; filotaxis/alma never bias."
        ),
    }

    payload = {
        "rounds": rounds,
        "tick_steps_per_round": tick_steps,
        "round_rows": round_rows,
        "verdict": verdict,
    }
    VIVE_JSON.write_text(json.dumps(payload, indent=2, ensure_ascii=False), encoding="utf-8")
    print(json.dumps({"wrote": str(VIVE_JSON), "verdict": verdict}, indent=2))
    return payload
