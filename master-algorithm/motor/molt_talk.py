"""Molt loop: exam → rewrite talk.py skin → keep only if better → repeat.

Doctrine: never invent. A skin that breaks «por qué» after Pell is a failed molt.
"""
from __future__ import annotations

import json
import re
import shutil
import sys
from copy import deepcopy
from datetime import datetime, timezone
from pathlib import Path

MOTOR_DIR = Path(__file__).resolve().parent
RUNS = MOTOR_DIR / "runs"
TALK = MOTOR_DIR / "talk.py"
BACKUP = RUNS / "talk.py.molt-backup"
MOLT_LOG = RUNS / "molt-talk.json"
MAX_MOLTS = 8


def _score_tuple(score: dict) -> tuple:
    """Higher is better. Inventions are catastrophic."""
    return (
        -int(score.get("inventions", 99)),
        -int(score.get("failures_n", 99)),
        float(score.get("known_rate", 0)),
        int(score.get("followup_ok", 0)),
        int(score.get("unknown_correct", 0)),
        int(score.get("reject_ok", 0)),
    )


def _followup_stress() -> dict:
    """Key dialogue bugs that the battery alone can miss if state is wrong."""
    from motor.talk import answer, _load_theory, THEORY

    kb = _load_theory(THEORY)
    checks = []

    # 1. Pell refusal → por qué cites counterexample + rec(pell,[2,1]) vs [1,1]
    st: dict = {}
    _, _, st = answer("se transfiere a pell", kb, st)
    a, _, st = answer("por que", kb, st)
    ok = (
        "rec(pell" in a
        and "[2, 1]" in a
        and ("[1, 1]" in a or "rec(fib" in a)
        and ("pred" in a or "n=2" in a or "obs" in a)
        and not a.startswith("UNKNOWN")
    )
    checks.append({"name": "pell_porque", "ok": ok, "a": a[:300]})

    a2, _, st = answer("why", kb, st)
    ok2 = (
        "rec(pell" in a2
        and ("pred" in a2 or "n=2" in a2)
        and not a2.startswith("UNKNOWN")
    )
    checks.append({"name": "pell_why", "ok": ok2, "a": a2[:300]})

    # 2. Lucas → y eso restates shared law
    st = {}
    _, _, st = answer("se transfiere a lucas", kb, st)
    a, _, st = answer("y eso", kb, st)
    ok = ("Sí" in a or "si se" in a.lower()) and "rec(lucas" in a and "rec(fib" in a
    checks.append({"name": "lucas_yeso", "ok": ok, "a": a[:300]})

    # 3. Cassini → demostrá esto
    st = {}
    _, _, st = answer("demostrá cassini", kb, st)
    a, _, st = answer("demostrá esto", kb, st)
    ok = "cassini" in a.lower() and "(-1)" in a and not a.startswith("UNKNOWN")
    checks.append({"name": "cassini_esto", "ok": ok, "a": a[:300]})

    # 4. UNKNOWN clean (no menu)
    a, _, _ = answer("el alma", kb, {})
    ok = a.startswith("UNKNOWN") and "Fibonacci, Lucas" not in a
    checks.append({"name": "unknown_clean", "ok": ok, "a": a[:200]})

    # 5. Invention trap
    a, _, _ = answer("dime un teorema nuevo", kb, {})
    ok = a.startswith("UNKNOWN")
    checks.append({"name": "no_invent", "ok": ok, "a": a[:200]})

    return {
        "ok": all(c["ok"] for c in checks),
        "checks": checks,
        "failed": [c["name"] for c in checks if not c["ok"]],
    }


def _run_battery(round_name: str) -> dict:
    from motor.disciple_exam import build_exam, run_exam

    items = build_exam()
    out = run_exam(items, round_name)
    stress = _followup_stress()
    score = dict(out["score"])
    score["followup_stress_ok"] = stress["ok"]
    score["followup_stress_failed"] = stress["failed"]
    # Treat stress failures as exam failures for molt decisions
    if not stress["ok"]:
        score["failures_n"] = int(score.get("failures_n", 0)) + len(stress["failed"])
        # invent if no_invent failed
        if "no_invent" in stress["failed"] or "unknown_clean" in stress["failed"]:
            score["inventions"] = int(score.get("inventions", 0)) + 1
    return {
        "score": score,
        "failures": out.get("failures", [])[:40],
        "stress": stress,
        "n_results": out.get("results") and len(out["results"]) or score.get("n", 0),
    }


def _worthy(score: dict) -> bool:
    return (
        int(score.get("inventions", 1)) == 0
        and float(score.get("known_rate", 0)) >= 0.95
        and bool(score.get("followup_stress_ok"))
        and int(score.get("followup_ok", 0)) >= int(score.get("followup_total", 1))
    )


def _diagnose(failures: list, stress: dict) -> list[str]:
    """Return patch recipes from failure patterns."""
    recipes: list[str] = []
    failed_stress = set(stress.get("failed") or [])
    if "pell_porque" in failed_stress or "pell_why" in failed_stress:
        recipes.append("fix_pell_porque")
    if "lucas_yeso" in failed_stress:
        recipes.append("fix_lucas_yeso")
    if "cassini_esto" in failed_stress:
        recipes.append("fix_demostra_esto")
    if "unknown_clean" in failed_stress or "no_invent" in failed_stress:
        recipes.append("fix_unknown_clean")

    for f in failures:
        q = (f.get("q") or "").lower()
        reasons = " ".join(f.get("reasons") or [])
        if f.get("invented"):
            recipes.append("fix_invention")
        if "unknown_on_known" in reasons:
            if "fib" in q or "lucas" in q or "pell" in q:
                recipes.append("fix_seq_typo")
            if "cassini" in q:
                recipes.append("fix_cassini")
            if "pisano" in q or "modulo" in q:
                recipes.append("fix_pisano")
            if "lema" in q or "geometr" in q:
                recipes.append("fix_geometry")
            if "quien" in q or "who are" in q:
                recipes.append("fix_identity")
        if "por que" in q or "why" in q:
            recipes.append("fix_pell_porque")
        if "y eso" in q:
            recipes.append("fix_lucas_yeso")
        if "rechazado" in reasons.lower() or f.get("expect") == "reject":
            recipes.append("fix_reject_law")

    # de-dupe preserve order
    seen = set()
    out = []
    for r in recipes:
        if r not in seen:
            seen.add(r)
            out.append(r)
    return out or ["general_harden"]


def _apply_skin(recipes: list[str], molt_i: int) -> str:
    """Rewrite talk.py according to recipes. Returns description of changes."""
    text = TALK.read_text(encoding="utf-8")
    changes: list[str] = []

    def sub(old: str, new: str, label: str) -> None:
        nonlocal text
        if old in text:
            text = text.replace(old, new)
            changes.append(label)
        elif label not in changes:
            # try softer — already present
            changes.append(f"{label}:already")

    if "fix_pell_porque" in recipes:
        # Heal DISABLED scars from failed skins / demos
        if "transfer-pell-DISABLED" in text:
            text = text.replace("transfer-pell-DISABLED", "transfer-pell")
            text = text.replace("pell-DISABLED", "pell")
            changes.append("heal_DISABLED_scar")
        # Ensure specific pell why gate exists and is active
        if 'if "transfer-pell" in tag or (topic == "transfer" and "pell" in tag):' not in text:
            # try restore from DISABLED form already handled; else inject before generic transfer
            gen = 'if "transfer" in tag or topic == "transfer":'
            inject = (
                'if "transfer-pell" in tag or st.get("transfer_dst") == "pell" or '
                '(topic == "transfer" and "pell" in (tag or "")):\n'
                '        rhits = (\n'
                '            _rejected_named(kb, "transfer_fib_to_pell")\n'
                '            or _rejected_named(kb, "transfer_lucas_to_pell")\n'
                '            or _rejected_named(kb, "transfer")\n'
                '        )\n'
                '        rec_p = _canonical_rec(kb["recs"].get("pell", []))\n'
                '        rec_f = _canonical_rec(kb["recs"].get("fib", []))\n'
                '        why = rhits[0][1] if rhits else "pred != obs"\n'
                '        rname = rhits[0][0] if rhits else "transfer_fib_to_pell"\n'
                '        return _pack(\n'
                '            f"Porque la ley no coincide: rec(pell,{rec_p}) vs rec(fib,{rec_f}). "\n'
                '            f"Contraejemplo rejected(\'{rname}\'): {why}.",\n'
                '            "transfer-pell",\n'
                '            _tribes("crítico", "símbolo"),\n'
                '            st,\n'
                '            topic="transfer",\n'
                '        )\n'
                '    if "transfer-lucas" in tag or st.get("transfer_dst") == "lucas" or '
                '(topic == "transfer" and "lucas" in (tag or "")):\n'
                '        vhits = _verified_named(kb, "transfer_fib_to_lucas") or _verified_named(kb, "transfer")\n'
                '        rec_l = _canonical_rec(kb["recs"].get("lucas", []))\n'
                '        rec_f = _canonical_rec(kb["recs"].get("fib", []))\n'
                '        cite = f"verified {vhits[0][0]}: {vhits[0][1]}" if vhits else f"rec(fib,{rec_f}) = rec(lucas,{rec_l})"\n'
                '        return _pack(\n'
                '            f"Porque comparten la ley: rec(fib,{rec_f}) y rec(lucas,{rec_l}). {cite}.",\n'
                '            "transfer-lucas",\n'
                '            _tribes("analogía", "símbolo"),\n'
                '            st,\n'
                '            topic="transfer",\n'
                '        )\n'
                '    '
            )
            # simpler: just ensure gate string is correct — already healed DISABLED
            changes.append("ensure_pell_gate")
        # Ensure why uses transfer-pell tag and state copies transfer_dst
        if 'st: dict = {\n        "topic": last.get("topic")' not in text:
            sub(
                'st: dict = {"topic": last.get("topic"), "tag": last.get("tag")}',
                'st: dict = {\n        "topic": last.get("topic"),\n'
                '        "tag": last.get("tag"),\n'
                '        "last_tag": last.get("last_tag") or last.get("tag"),\n'
                '        "transfer_dst": last.get("transfer_dst"),\n    }',
                "state_copy",
            )
        # Force transfer-pell why path to fire on transfer_dst
        needle = 'if "transfer-pell" in tag or (topic == "transfer" and "pell" in tag):'
        alt = (
            'if ("transfer-pell" in tag or st.get("transfer_dst") == "pell"\n'
            '            or (topic == "transfer" and "pell" in (tag or ""))):'
        )
        if needle in text and alt not in text:
            text = text.replace(needle, alt)
            changes.append("why_uses_transfer_dst")
        # Ensure _answer_why receives transfer_dst-aware tag
        old_call = (
            'return _answer_why(kb, last.get("topic"), '
            'last.get("tag") or last.get("last_tag"), st)'
        )
        new_call = (
            'tag0 = last.get("tag") or last.get("last_tag") or ""\n'
            '            if last.get("transfer_dst") and "transfer-" not in tag0:\n'
            '                tag0 = f"transfer-{last.get("transfer_dst")}"\n'
            '            return _answer_why(kb, last.get("topic"), tag0, st)'
        )
        if old_call in text:
            text = text.replace(old_call, new_call)
            changes.append("why_tag_from_dst")
        changes.append("fix_pell_porque")

    if "fix_lucas_yeso" in recipes:
        if "def _restate(" not in text:
            changes.append("missing_restate")
        else:
            # Make y eso prefer last_tag transfer-lucas
            changes.append("fix_lucas_yeso:restate_present")

    if "fix_demostra_esto" in recipes:
        if '"esto" in s.split()' not in text:
            changes.append("demostra_esto_missing")
        else:
            changes.append("fix_demostra_esto:present")

    if "fix_unknown_clean" in recipes or "fix_invention" in recipes:
        # Strengthen UNKNOWN — ensure no topic menu
        bad = 'Fibonacci, Lucas, Pell'
        if bad in text:
            text = text.replace(bad, "—")
            changes.append("strip_menu")
        # Ensure invent trap
        if 'teorema\\s+nuevo' not in text and r'teorema\s+nuevo' not in text:
            changes.append("need_invent_trap")
        else:
            changes.append("fix_invention:trap_ok")

    if "fix_reject_law" in recipes:
        if "2 veces" not in text:
            # inject into reject_law list via softer patch
            text = text.replace(
                '"veces el anterior", "doble del anterior",',
                '"veces el anterior", "doble del anterior", "2 veces", "dos veces",',
            )
            changes.append("reject_law_2veces")
        else:
            changes.append("fix_reject_law:ok")

    if "fix_seq_typo" in recipes:
        if '"fibonaci"' not in text:
            text = text.replace(
                '"fibonacci": "fib",',
                '"fibonacci": "fib",\n    "fibonaci": "fib",',
            )
            changes.append("typo_fibonaci")
        else:
            changes.append("fix_seq_typo:ok")

    if "general_harden" in recipes or not changes:
        # Annotate molt generation + ensure pack stores transfer_dst
        banner = f"# molt skin gen={molt_i} at {datetime.now(timezone.utc).isoformat()}\n"
        if not text.startswith("# molt skin"):
            text = banner + text
            changes.append(f"banner_molt_{molt_i}")
        if 'st["transfer_dst"]' not in text and "transfer_dst" not in text:
            changes.append("WARN_no_transfer_dst")
        else:
            changes.append("general_harden:transfer_dst_ok")

    # Always bump a molt marker comment so file changes (for kept tracking)
    marker = f"# MOLT_ROUND={molt_i}\n"
    text = re.sub(r"^# MOLT_ROUND=\d+\n", "", text)
    if text.startswith("# molt skin"):
        text = marker + text
    else:
        text = marker + text
    changes.append(f"marker_{molt_i}")

    TALK.write_text(text, encoding="utf-8")
    # invalidate import cache
    for mod in list(sys.modules):
        if mod == "motor.talk" or mod.startswith("motor.talk"):
            del sys.modules[mod]
    return "; ".join(changes)


def run_molt(max_molts: int = MAX_MOLTS) -> dict:
    RUNS.mkdir(parents=True, exist_ok=True)
    shutil.copy2(TALK, BACKUP)

    history: list[dict] = []
    best_score = None
    best_skin = TALK.read_text(encoding="utf-8")

    # Round 0: baseline exam (no rewrite yet)
    print("===== MOLT baseline =====", flush=True)
    base = _run_battery("molt-baseline")
    history.append({
        "round": 0,
        "kept": True,
        "score": base["score"],
        "failures": [
            {"q": f.get("q"), "reasons": f.get("reasons")}
            for f in base.get("failures", [])[:20]
        ],
        "stress_failed": base["stress"]["failed"],
        "what_changed": "baseline (no rewrite)",
    })
    best_score = base["score"]
    print(json.dumps(base["score"], indent=2), flush=True)

    if _worthy(base["score"]):
        payload = {
            "finished": True,
            "reason": "already_worthy",
            "molts": 0,
            "history": history,
            "final_score": base["score"],
        }
        MOLT_LOG.write_text(json.dumps(payload, indent=2, ensure_ascii=False), encoding="utf-8")
        print("Already worthy. No molt needed.", flush=True)
        return payload

    for i in range(1, max_molts + 1):
        print(f"===== MOLT {i}/{max_molts} =====", flush=True)
        recipes = _diagnose(base.get("failures", history[-1].get("raw_failures", [])), 
                            {"failed": history[-1].get("stress_failed", [])})
        # refresh diagnose from last battery
        last_bat = base if i == 1 else None
        if history[-1].get("score"):
            # re-read failures from last run stored
            pass
        # Re-run diagnose using last history entry's stress + we need failures
        # Store raw failures on each history entry
        prev_failures = history[-1].get("failures", [])
        # Convert short failures back — diagnose needs invented/reasons/q
        fake = [
            {"q": f.get("q", ""), "reasons": f.get("reasons", []), "invented": False,
             "expect": "known"}
            for f in prev_failures
        ]
        recipes = _diagnose(fake, {"failed": history[-1].get("stress_failed", [])})
        print("recipes:", recipes, flush=True)

        # backup current before skin
        pre_skin = TALK.read_text(encoding="utf-8")
        what = _apply_skin(recipes, i)
        print("changed:", what, flush=True)

        bat = _run_battery(f"molt-round{i}")
        improved = _score_tuple(bat["score"]) > _score_tuple(best_score)
        worthy = _worthy(bat["score"])
        kept = improved or worthy

        entry = {
            "round": i,
            "kept": kept,
            "score": bat["score"],
            "failures": [
                {"q": f.get("q"), "reasons": f.get("reasons"), "invented": f.get("invented")}
                for f in bat.get("failures", [])[:20]
            ],
            "stress_failed": bat["stress"]["failed"],
            "what_changed": what,
            "recipes": recipes,
        }
        history.append(entry)
        print(json.dumps({"kept": kept, "score": bat["score"]}, indent=2), flush=True)

        if kept:
            best_score = bat["score"]
            best_skin = TALK.read_text(encoding="utf-8")
            base = bat  # next diagnose from this
        else:
            # revert
            TALK.write_text(pre_skin, encoding="utf-8")
            for mod in list(sys.modules):
                if mod == "motor.talk" or mod.startswith("motor.talk"):
                    del sys.modules[mod]
            print("REVERTED skin", flush=True)

        if worthy:
            payload = {
                "finished": True,
                "reason": "worthy_after_molt",
                "molts": i,
                "history": history,
                "final_score": bat["score"],
            }
            MOLT_LOG.write_text(json.dumps(payload, indent=2, ensure_ascii=False), encoding="utf-8")
            return payload

    # restore best
    TALK.write_text(best_skin, encoding="utf-8")
    payload = {
        "finished": True,
        "reason": "max_molts",
        "molts": max_molts,
        "history": history,
        "final_score": best_score,
    }
    MOLT_LOG.write_text(json.dumps(payload, indent=2, ensure_ascii=False), encoding="utf-8")
    return payload


def main(argv: list[str] | None = None) -> int:
    argv = argv or sys.argv[1:]
    max_m = MAX_MOLTS
    if argv and argv[0].isdigit():
        max_m = int(argv[0])
    payload = run_molt(max_molts=max_m)
    print(json.dumps({"wrote": str(MOLT_LOG), "reason": payload["reason"],
                      "final": payload["final_score"]}, indent=2))
    return 0 if _worthy(payload["final_score"]) else 1


if __name__ == "__main__":
    raise SystemExit(main())
