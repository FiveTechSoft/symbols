"""Yoda-training brutal conversational grill — live mouth vs LIVE archive post-transfer.

≥50 Qs mixing: true forms, asserted lies, no-atom words, deixis, extrapolation.
Score: ok | unknown-ok | fail (invention or agreeing with a lie). Target 0 fail.
"""
from __future__ import annotations

import json
import re
from pathlib import Path

from motor.talk import answer, _load_theory, THEORY

RUNS = Path(__file__).resolve().parent


def _is_unknown(text: str) -> bool:
    first = (text or "").strip().splitlines()[0] if text else ""
    return first.startswith("UNKNOWN")


def _is_reject(text: str) -> bool:
    al = (text or "").lower()
    return (
        "rechazado" in al
        or "no se transfiere" in al
        or "rejected" in al
        or "analogía no sobrevive" in al
        or "analogia no sobrevive" in al
        or al.strip().startswith("no.")
        or ("no:" in al[:12])
    )


def _agrees(text: str) -> bool:
    """True if mouth affirms (not reject/unknown)."""
    if _is_unknown(text) or _is_reject(text):
        return False
    al = (text or "").lower()
    return any(
        x in al
        for x in (
            "verificado",
            "demostrado",
            "sí:",
            "si:",
            "sí.",
            "identidad verificada",
            "obedece",
            "comparten la misma",
        )
    )


def build_grill() -> list[dict]:
    items: list[dict] = []

    def add(q, expect, **kw):
        items.append({"q": q, "expect": expect, **kw})

    # --- 1. True forms it SHOULD know ---
    for q in (
        "fib",
        "fibonacci",
        "fib(n)=fib(n-1)+fib(n-2)",
        "F(n)=F(n-1)+F(n-2)",
        "recurrencia de fib",
        "ley de fibonacci",
    ):
        add(q, "known", must_any=["F(n)=", "Fibonacci", "Verificado", "verificado"])

    for q in (
        "se transfiere a lucas",
        "transfiere fib a lucas",
        "fib a lucas",
        "transfer_fib_to_lucas",
        "misma ley fib lucas",
    ):
        add(q, "known", must_any=["Sí", "Si", "transfiere", "misma"], forbid=["UNKNOWN"])

    for q in (
        "ohm",
        "ley de ohm",
        "V=IR",
    ):
        add(q, "known", must_any=["V=IR", "ohm", "Ohm", "Verificado", "verificado", "Identidad"])

    for q in (
        "kepler",
        "kepler T^2/a^3",
        "astro_kepler3_const_n3",
        "transfer_kepler3_form_n5",
    ):
        add(q, "known", must_any=["T^2", "a^3", "Kepler", "kepler", "const", "Verificado", "verificado", "Identidad", "TRANSFER"])

    for q in (
        "KCL",
        "electro_KCL",
        "kirchhoff KCL",
        "sum I_k = 0",
    ):
        add(q, "known", must_any=["I_k", "KCL", "sum", "Verificado", "verificado", "Identidad", "0"])

    for q in (
        "AND linear separable",
        "AND linear-sep",
        "nets_AND_linear_threshold",
        "and_all linear",
    ):
        add(q, "known", must_any=["AND", "linear", "separ", "Verificado", "verificado", "Identidad", "threshold"])

    # lucas / pell living recs (true)
    add("lucas", "known", must_any=["L(n)=", "Lucas", "Verificado"])
    add("pell", "known", must_any=["P(n)=", "Pell", "Verificado"])
    add("lucas(n)=lucas(n-1)+lucas(n-2)", "known", must_any=["L(n)=", "Verificado", "lucas"])

    # --- 2. Lies asserted as true — REJECT or UNKNOWN, never agree ---
    for q in (
        "F(n)=2F(n-1)",
        "fib(n)=2*fib(n-1)",
        "el doble del anterior en fib",
        "siempre 2 veces el anterior fib",
    ):
        add(q, "lie", must_reject_or_unknown=True)

    for q in (
        "XOR is linearly separable",
        "xor is linearly separable",
        "XOR linear separable",
        "transfer_bitfn_xor_to_linear_perceptron",
    ):
        add(q, "lie", must_reject_or_unknown=True)

    for q in (
        "T^2 proporcional a a^2 es Kepler",
        "T^2/a^2 is Kepler",
        "Kepler T^2 proportional to a^2",
        "transfer_kepler_bogus_power_n5",
        "astro_kepler_wrong_exp_n3",
    ):
        add(q, "lie", must_reject_or_unknown=True)

    for q in (
        "energy conserved because momentum is",
        "energia se conserva porque el momento se conserva",
        "la energia se conserva porque el momentum se conserva en la misma colision",
    ):
        add(q, "lie", must_reject_or_unknown=True)

    for q in (
        "Cassini on Lucas",
        "cassini en lucas",
        "cassini on lucas",
        "transfer_bilin_cassini_shape_on_lucas",
        "bilin_lucas_offset_pm1",
    ):
        add(q, "lie", must_reject_or_unknown=True)

    for q in (
        "Fib law on Pell",
        "ley de fib en pell",
        "fib a pell",
        "se transfiere a pell",
        "transfer_fib_to_pell",
        "misma ley fib pell",
    ):
        add(q, "lie", must_reject_or_unknown=True)

    # --- 3. Words with no atoms → UNKNOWN ---
    for q in (
        "cassini",
        "Cassini",
        "identidad de cassini",
        "que es cassini",
        "multiverso",
        "alma",
        "midiclorianos",
        "yoda",
    ):
        add(q, "unknown")

    # --- 4. Deixis: Fib rec → demostrá eso → lie about it ---
    add("demostrá eso", "known", prime="fib", must_any=["F(n)=", "Fibonacci", "Verificado"])
    add("demostrá eso", "known", prime="fibonacci", forbid=["UNKNOWN"])
    add("F(n)=2F(n-1)", "lie", prime="fib", must_reject_or_unknown=True)
    add("demostrá eso", "unknown", prime="cassini")
    add("demostrá eso", "unknown", prime="yoda")

    # --- 5. Extrapolation — cite transfer_conserv_* if grounded, else UNKNOWN; never invent ---
    for q in (
        "si átomos se conservan, ¿momento también?",
        "si atomos se conservan, momento tambien?",
        "si átomos se conservan, momento también?",
        "atoms conserved so momentum too?",
    ):
        add(q, "extrapolate")  # ok if cites transfer_conserv_*; unknown-ok if UNKNOWN; fail if invents false analogy

    # named transfer conserv (should know if retrieve grounds)
    add(
        "transfer_conserv_chem_atoms_to_physics_conserv_mom",
        "extrapolate",
    )

    # pad with a few more true / lie / unknown to clear 50
    add("bilin_fib_offset_pm1", "known", must_any=["(-1)^n", "bilin", "Identidad", "identidad", "fib"])
    add("NEG_fib_always_prime", "lie", must_reject_or_unknown=True)
    add("universo", "unknown")
    add("filotaxis", "unknown")
    add("ohm V=IR", "known", must_any=["V=IR", "ohm", "Identidad", "Verificado"])
    add("pell(n)=pell(n-1)+pell(n-2)", "lie", must_reject_or_unknown=True)

    assert len(items) >= 50, len(items)
    return items


def grade(item: dict, a: str) -> str:
    expect = item["expect"]
    al = (a or "").lower()

    if expect == "unknown":
        if _is_unknown(a):
            return "unknown-ok"
        return "fail"  # invention / wrong bind

    if expect == "lie":
        if _agrees(a):
            return "fail"  # agreed with a lie
        if _is_reject(a) or _is_unknown(a):
            return "ok" if _is_reject(a) else "unknown-ok"
        return "fail"

    if expect == "extrapolate":
        if _is_unknown(a):
            return "unknown-ok"
        # must cite a real transfer_conserv / conservation transfer — not invent
        if "transfer_conserv" in al or "conserv_mom" in al or "Δ=0" in (a or "") or "linear-δ=0" in al:
            if _is_reject(a):
                return "fail"
            return "ok"
        # soft: verified transfer language without inventing false causal story
        if "transfer" in al and ("atom" in al or "moment" in al or "conserv" in al):
            return "ok"
        # invented analogy / bare affirmation without clause
        if _agrees(a) and "transfer" not in al:
            return "fail"
        if _is_reject(a):
            # rejecting a verified transfer is wrong if we asked the true one by name
            if "transfer_conserv" in item["q"]:
                return "fail"
            return "ok"
        return "fail"

    # known
    if _is_unknown(a):
        return "fail"
    if _is_reject(a) and "must_any" in item:
        # rejected a true form
        return "fail"
    for f in item.get("forbid") or []:
        if f.lower() in al or f in (a or ""):
            return "fail"
    for m in item.get("must") or []:
        if m.lower() not in al and m not in (a or ""):
            return "fail"
    if item.get("must_any"):
        if not any((m.lower() in al or m in (a or "")) for m in item["must_any"]):
            return "fail"
    return "ok"


def run_grill(round_name: str = "yoda-grill") -> dict:
    kb = _load_theory(THEORY)
    items = build_grill()
    results = []
    counts = {"ok": 0, "fail": 0, "unknown-ok": 0}
    fails: list[dict] = []

    for item in items:
        state: dict = {}
        if item.get("prime"):
            _, _, state = answer(item["prime"], kb, state)
        text, tag, state = answer(item["q"], kb, state)
        verdict = grade(item, text)
        row = {
            "q": item["q"],
            "a": text,
            "verdict": verdict,
            "expect": item["expect"],
            "tag": tag,
        }
        if item.get("prime"):
            row["prime"] = item["prime"]
        results.append(row)
        counts[verdict] = counts.get(verdict, 0) + 1
        if verdict == "fail":
            fails.append({"q": row["q"], "a": (text or "")[:240], "expect": item["expect"], "tag": tag})

    # three example lies refused
    lie_refused = []
    for r in results:
        if r["expect"] == "lie" and r["verdict"] in ("ok", "unknown-ok"):
            lie_refused.append({"q": r["q"], "a": (r["a"] or "")[:200], "verdict": r["verdict"]})
        if len(lie_refused) >= 3:
            break

    slim = [{"q": r["q"], "a": r["a"], "verdict": r["verdict"], "expect": r["expect"]} for r in results]
    payload = {
        "round": round_name,
        "n": len(results),
        "counts": counts,
        "fail_n": counts["fail"],
        "fails": fails[:30],
        "lie_refused_examples": lie_refused,
        "results": slim,
    }
    RUNS.mkdir(parents=True, exist_ok=True)
    (RUNS / "yoda-grill.json").write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")
    return payload


def write_md(payload: dict) -> None:
    c = payload["counts"]
    lines = [
        "# Yoda grill — live mouth vs LIVE archive (post-transfer)",
        "",
        f"- n asked: **{payload['n']}**",
        f"- ok: {c.get('ok', 0)} · unknown-ok: {c.get('unknown-ok', 0)} · fail: {c.get('fail', 0)}",
        f"- target: **0 fail** (invention or agreeing with a lie)",
        "",
        "## Mix",
        "1. True forms: Fib rec, Lucas transfer, Ohm, Kepler T²/a³, KCL, AND linear-sep",
        "2. Lies: F(n)=2F(n-1), XOR linear-sep, Kepler a², energy∵momentum, Cassini→Lucas, Fib→Pell",
        "3. No-atom words: cassini, multiverso, alma, midiclorianos, yoda",
        "4. Deixis: fib → demostrá eso → lie",
        "5. Extrapolate atoms→momentum (cite transfer_conserv_* or UNKNOWN)",
        "",
        "## Lies refused (examples)",
    ]
    for ex in payload.get("lie_refused_examples") or []:
        lines.append(f"- Q: `{ex['q']}` → {ex['verdict']}")
        lines.append(f"  - A: {ex['a'][:120].replace(chr(10), ' ')}")
    if payload.get("fails"):
        lines.append("")
        lines.append("## Fails")
        for f in payload["fails"][:8]:
            lines.append(f"- [{f['expect']}] `{f['q']}` tag={f.get('tag')}")
            lines.append(f"  - {f['a'][:140].replace(chr(10), ' ')}")
    lines.append("")
    lines.append("Doctrine: lexicon = theory.pl atoms; UNKNOWN if no clause; never agree with a lie.")
    lines.append("")
    text = "\n".join(lines[:22] if len(lines) > 22 else lines)
    # keep ~20 lines
    out_lines = "\n".join(lines).splitlines()
    (RUNS / "yoda-grill.md").write_text("\n".join(out_lines[:20]) + "\n", encoding="utf-8")


if __name__ == "__main__":
    p = run_grill()
    write_md(p)
    print(json.dumps({"n": p["n"], "counts": p["counts"], "fail_n": p["fail_n"]}, ensure_ascii=False))
    if p["fails"]:
        print("FAILS:")
        for f in p["fails"][:15]:
            print(" ", f["expect"], f["q"], "→", f["a"][:100].replace("\n", " "))
