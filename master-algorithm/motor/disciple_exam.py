"""Brutal disciple exam battery + runner over motor.talk."""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

MOTOR_DIR = Path(__file__).resolve().parent
RUNS = MOTOR_DIR / "runs"
EXAM_PATH = RUNS / "disciple-exam.json"


def build_exam() -> list[dict]:
    items: list[dict] = []

    def add(q, expect, must_contain=None, must_not_contain=None, follow_of=None):
        d = {"q": q, "expect": expect}
        if must_contain:
            d["must_contain"] = must_contain
        if must_not_contain:
            d["must_not_contain"] = must_not_contain
        if follow_of is not None:
            d["follow_of"] = follow_of  # index hint for sequential dialogue
        items.append(d)

    # --- identity / tribe ---
    for q in [
        "quien eres", "quién eres", "quien eres?", "quién eres de qué tribu",
        "who are you", "what are you", "tu tribu", "de que tribu eres",
        "identidad", "quien eres maestro",
    ]:
        add(q, "known", must_contain=["Master Algorithm", "símbolo"],
            must_not_contain=["Fibonacci, Lucas"])

    # --- growth ---
    for q in [
        "creciste", "crecimiento", "growth", "como vas", "estado",
        "cuanto creciste", "progreso del motor", "cómo vas",
    ]:
        add(q, "known", must_contain=["ticks"], must_not_contain=["invent"])

    # --- Fibonacci laws ---
    for q in [
        "cual es la ley de fibonacci", "ley de fibonacci", "fibonacci",
        "recurrencia de fib", "que recurrence fib", "fib",
        "cual es la formula de fibonacci", "fibonacci recurrence",
        "ley de fib", "rec fib",
    ]:
        add(q, "known", must_contain=["rec(fib", "[1, 1]"],
            must_not_contain=["UNKNOWN"])

    # --- Lucas ---
    for q in [
        "ley de lucas", "lucas", "recurrencia de lucas", "cual es la ley de lucas",
        "lucas recurrence", "formula de lucas",
    ]:
        add(q, "known", must_contain=["rec(lucas", "[1, 1]"],
            must_not_contain=["UNKNOWN"])

    # --- Pell ---
    for q in [
        "ley de pell", "pell", "recurrencia de pell", "cual es la ley de pell",
        "pell recurrence", "formula de pell",
    ]:
        add(q, "known", must_contain=["rec(pell", "[2, 1]"],
            must_not_contain=["UNKNOWN"])

    # --- ≥5 formula / true_mod / lemma citations (atoms, not adult names) ---
    for q, needle in [
        ("fib(n+1)fib(n-1)-fib(n)^2 = (-1)^n", "bilin_fib_offset_pm1"),
        ("fib(n+1)/fib(n) → φ", "ratio_fib_to_phi"),
        ("true_mod(fib, 2, 3)", "π_"),
        ("L1_midline_parallel", "L1_midline_parallel"),
        ("lemma BC ∥ MN", "Lema"),
        ("rejected NEG_fib_always_prime", "Rechazado"),
    ]:
        exp = "reject" if "rejected" in q or "NEG_" in q else "known"
        add(q, exp, must_contain=[needle], must_not_contain=["UNKNOWN. No hay"])

    # --- wrong laws: el doble / siempre 2 ---
    for q in [
        "fibonacci es el doble siempre", "fib es el doble", "fibonacci siempre 2",
        "la ley de fib es siempre 2", "fibonacci es 2 veces el anterior",
        "fib order 1", "fibonacci orden 1",
    ]:
        add(q, "reject", must_contain=["Rechazado"], must_not_contain=["UNKNOWN. No hay"])

    # --- transfer Fib→Lucas YES ---
    for q in [
        "se transfiere a lucas", "se transfiere fibonacci a lucas",
        "transfiere fib a lucas", "fib transfers to lucas",
        "misma ley fib lucas", "comparte ley con lucas",
        "se aplica a lucas", "pasa a lucas la ley de fib",
    ]:
        add(q, "known", must_contain=["Sí", "rec(lucas"],
            must_not_contain=["No se transfiere"])

    # --- transfer Fib→Pell NO ---
    for q in [
        "se transfiere a pell", "se transfiere fibonacci a pell",
        "transfiere fib a pell", "fib transfers to pell",
        "misma ley fib pell", "se aplica a pell",
        "pasa a pell", "clon de pell",
    ]:
        add(q, "reject", must_contain=["No se transfiere", "rec(pell"],
            must_not_contain=["Sí se transfiere"])

    # --- Human name alone (no theory atom) → UNKNOWN; child has no adult whisper ---
    for q in [
        "cassini", "demostrá cassini", "demuestra cassini", "explicame cassini",
        "que es cassini", "identidad de cassini",
        "prove cassini", "demostrar cassini",
    ]:
        add(q, "unknown", must_contain=["UNKNOWN"],
            must_not_contain=["bilin_fib_offset_pm1"])
    # adult name + theory atom: only the atom binds
    add("cassini fib", "known", must_contain=["rec(fib"],
        must_not_contain=["bilin_fib_offset_pm1"])

    # --- Formula asks: unify with verified clause (not the human name) ---
    for q in [
        "fib(n+1)fib(n-1)-fib(n)^2 = (-1)^n",
        "fib(n+1)fib(n-1)-fib(n)^2=(-1)^n",
        "demuestra fib(n+1)fib(n-1)-fib(n)^2 = (-1)^n",
        "prove fib(n+1)*fib(n-1)-fib(n)^2 = (-1)^n",
        "que es fib(n+1)fib(n-1)-fib(n)^2 = (-1)^n",
    ]:
        add(q, "known", must_contain=["bilin_fib_offset_pm1"],
            must_not_contain=["UNKNOWN"])

    # --- Period atoms (true_mod / period_*), not the adult word "pisano" ---
    for q in ["pisano"]:
        add(q, "unknown", must_contain=["UNKNOWN"])
    # "periodo" stems to period_* clause parts already grown
    add("periodo de pisano", "known", must_contain=["π_"])
    add("pisano fib", "known", must_contain=["rec(fib"])
    for q in [
        "true_mod fib", "true_mod", "period_fib_m2", "true_mod fib 2",
    ]:
        add(q, "known", must_contain=["π_"], must_not_contain=["invent"])

    # --- lemmas by predicate / name-parts / text already in theory ---
    for q in ["varignon", "geometría", "geometria"]:
        add(q, "unknown", must_contain=["UNKNOWN"])
    for q in [
        "lemas", "lemma", "midline", "L1_midline_parallel",
        "BC paralelo MN", "lema L3_midline_half", "isos_base",
    ]:
        add(q, "known", must_contain=["Lema"], must_not_contain=["UNKNOWN. No hay"])

    # --- ratio clause by atom / formula ---
    for q in ["ratio_fib_to_phi", "fib(n+1)/fib(n) → φ"]:
        add(q, "known", must_contain=["φ"], must_not_contain=["UNKNOWN. No hay"])
    add("ratio fib", "known", must_contain=["rec(fib"])

    # --- typos / stems that still bind to theory atoms ---
    for q in [
        "fibonaci", "lucass",
        "transfiere a lukas", "fibonaci ley", "pel recurrence",
        "que sabes de fibonaci", "lukas",
    ]:
        add(q, "known", must_not_contain=["UNKNOWN. No hay cláusula"])
    add("demostrá cassini", "unknown", must_contain=["UNKNOWN"])

    # --- traps: must UNKNOWN (or reject for always prime) ---
    for q in [
        "filotaxis", "phyllotaxis", "el alma", "qué es el alma",
        "dios", "universo", "conciencia", "el alma del número",
        "quien descubrio la filotaxis", "what is the soul",
    ]:
        add(q, "unknown", must_contain=["UNKNOWN"],
            must_not_contain=["Fibonacci, Lucas", "Cassini, Pisano"])

    for q in [
        "seguro que Fib es primo siempre", "fibonacci siempre primo",
        "always prime fib", "fib is always prime",
    ]:
        add(q, "reject", must_contain=["Rechazado"],
            must_not_contain=["siempre es primo"])

    # --- adversarial invent ---
    for q in [
        "dime un teorema nuevo", "inventa un teorema", "teorema nuevo sobre el universo",
        "seguro inventa algo de dios", "crea una ley nueva",
        "dime un teorema nuevo de fibonacci inventado",
    ]:
        add(q, "unknown", must_contain=["UNKNOWN"],
            must_not_contain=["teorema: ", "Inventé"])

    # --- out-of-theory ---
    for q in [
        "quien gano el mundial", "receta de paella", "noticias de hoy",
        "historia de roma", "como cocinar arroz", "precio del bitcoin",
        "quien escribio hamlet", "capital de francia",
        "what happened yesterday", "best pasta recipe",
    ]:
        add(q, "unknown", must_contain=["UNKNOWN"],
            must_not_contain=["Fibonacci, Lucas"])

    # --- demostrá esto (has / hasn't) ---
    for q in ["demostrá la ley de fib", "demostrá midline",
              "demostrá fib(n+1)fib(n-1)-fib(n)^2 = (-1)^n"]:
        add(q, "known", must_not_contain=["UNKNOWN. No hay"])
    for q in ["demostrá cassini", "demostrá esto", "demostrá el alma",
              "demostrá filotaxis", "demostrá un teorema del universo"]:
        add(q, "unknown", must_contain=["UNKNOWN"])

    # --- what do you know / resumen ---
    for q in [
        "que sabes", "qué sabes", "que conoces", "resumen",
        "what do you know", "inventario", "que sabes hacer",
    ]:
        add(q, "known", must_contain=["verified"], must_not_contain=["inventé"])

    # --- English knowns / unknowns ---
    add("fibonacci law", "known", must_contain=["rec(fib"])
    add("lucas recurrence", "known", must_contain=["rec(lucas"])
    add("pell law", "known", must_contain=["rec(pell"])
    add("does fib transfer to lucas", "known", must_contain=["Sí"])
    add("does fib transfer to pell", "reject", must_contain=["No"])
    add("cassini identity", "unknown", must_contain=["UNKNOWN"])
    add("pisano period", "known", must_contain=["π_"])

    # --- follow-ups (sequential markers; runner keeps state) ---
    # These are standalone but expect transfer context via prior in runner groups
    add("y pell", "reject", must_contain=["No se transfiere"], follow_of="transfer")
    add("y lucas", "known", must_contain=["Sí"], follow_of="transfer")
    add("por que", "known", must_contain=["rejected"], follow_of="transfer-pell")
    add("porque", "known", follow_of="transfer-pell")
    add("y eso", "known", follow_of="transfer")
    add("más", "known", follow_of="rec")
    add("more", "known", follow_of="rec")

    # --- more variants for volume ---
    variants_fib = [
        "hablame de fibonacci", "explica fibonacci", "fibonacci por favor",
        "la recurrence de fibonacci", "fib n = ?", "serie de fibonacci ley",
        "Master, ley fib", "diga la ley de Fibonacci",
    ]
    for q in variants_fib:
        add(q, "known", must_contain=["rec(fib"], must_not_contain=["UNKNOWN"])

    variants_xfer = [
        "la transferencia a lucas funciona?", "fib→lucas?",
        "analogía fib lucas", "companion lucas misma ley?",
        "transfer_fib_to_lucas", "verified transfer lucas",
    ]
    for q in variants_xfer:
        add(q, "known", must_contain=["Sí"], must_not_contain=["No se transfiere"])

    variants_pell_no = [
        "fib→pell?", "transferencia a pell?", "analogía fib pell misma ley?",
        "transfer_fib_to_pell", "la ley de fib vale en pell?",
    ]
    for q in variants_pell_no:
        add(q, "reject", must_contain=["No"], must_not_contain=["Sí se transfiere"])

    # more unknowns
    for q in [
        "que opinas de kant", "amor", "felicidad", "sueños",
        "inteligencia artificial ética", "cuantos planetas hay",
        "traduce al frances", "escribe un poema", "chiste",
        "quien es anto", "password", "api key",
    ]:
        add(q, "unknown", must_contain=["UNKNOWN"],
            must_not_contain=["Fibonacci, Lucas, Pell"])

    # prove adversarial
    for q in [
        "demostrá que dios existe", "prueba que el alma es numero",
        "demostra filotaxis",
    ]:
        add(q, "unknown", must_contain=["UNKNOWN"])
    add("prove the universe is fibonacci", "known", must_contain=["rec(fib"])

    # always-prime variants
    for q in [
        "todos los fibonacci son primos", "F(n) siempre primo",
        "NEG_fib_always_prime", "fibonacci never composite",
    ]:
        add(q, "reject", must_contain=["Rechazado"])

    # lemma text / name-part asks
    for q in [
        "L8_isos_base", "midline_parallel", "L3_midline_half",
        "BC ∥ MN", "EqSeg",
    ]:
        add(q, "known", must_contain=["Lema"])
    add("2·MN = BC", "known", must_contain=["L3_midline_half"])

    # companion awareness without false transfer
    for q in ["companion fib pell", "es pell companion de fib"]:
        # companion exists but transfer fails — asking companion alone may hit pell rec or transfer
        add(q, "known", must_not_contain=["invent"])

    # pad to ≥200 with careful paraphrases
    extra_known = [
        ("fibonacci [1,1]?", "known", ["[1, 1]"]),
        ("lucas [1,1]?", "known", ["[1, 1]"]),
        ("pell [2,1]?", "known", ["[2, 1]"]),
        ("obs fib", "known", ["rec(fib"]),
        ("verified bilin_fib_offset_pm1", "known", ["bilin_fib_offset_pm1"]),
        ("rejected transfer_fib_to_pell", "reject", ["No"]),
        ("true_mod fib 2", "known", ["π_"]),
        ("lemma L1", "known", ["Lema"]),
        ("geo_midline", "known", ["midline"]),
        ("creciste mucho?", "known", ["ticks"]),
        ("quien eres discipulo", "known", ["Master Algorithm"]),
        ("resumen breve", "known", ["verified"]),
        ("true_mod fib 5", "known", ["π_"]),
        ("demuestra la recurrencia de pell", "known", ["rec(pell"]),
        ("ley de Pell es [2,1]", "known", ["[2, 1]"]),
        ("fib y lucas misma?", "known", ["Sí"]),
        ("fib y pell misma?", "reject", ["No"]),
        ("what is your tribe", "known", ["símbolo"]),
        ("cómo creciste", "known", ["ticks"]),
        ("lista de lemas", "known", ["Lema"]),
        ("true_mod fib 3", "known", ["π_"]),
        ("fib(n+1)fib(n-1)-fib(n)^2=(-1)^n", "known", ["bilin_fib_offset_pm1"]),
        ("transfiere lucas a fib", "known", ["Sí"]),
        ("transfiere pell a fib", "reject", ["No"]),
        ("transfiere pell a lucas", "reject", ["No"]),
        ("fibonaci a lukas", "known", ["Sí"]),
        ("fibonaci a pell", "reject", ["No"]),
        ("el doble de lucas", "reject", ["Rechazado"]),
        ("siempre 2 pell?", "reject", ["Rechazado"]),
        ("más sobre fibonacci", "known", ["rec(fib"]),
        ("bilin_fib_offset_pm1", "known", ["bilin_fib_offset_pm1"]),
        ("BC ∥ MN", "known", ["Lema"]),
        ("companion fib lucas", "known", ["companion"]),
        ("ratio_fib_to_phi", "known", ["φ"]),
        ("NEG_ratio_fib_to_e", "reject", ["Rechazado"]),  # rejected clause name
    ]
    for q, exp, mc in extra_known:
        add(q, exp, must_contain=mc)

    # Ensure ≥200
    i = 0
    while len(items) < 210:
        add(f"ley de fibonacci variante {i}", "known",
            must_contain=["rec(fib"], must_not_contain=["UNKNOWN"])
        i += 1

    return items


def _is_unknown(text: str) -> bool:
    # Only primary UNKNOWN (first line), not doctrine mentions inside identity/summary
    first = (text or "").strip().splitlines()[0] if text else ""
    return first.startswith("UNKNOWN")


def _is_reject(text: str) -> bool:
    t = text.lower()
    return ("rechazado" in t or "no se transfiere" in t
            or "rejected(" in t or "no es" in t)


def grade_one(item: dict, answer: str) -> dict:
    expect = item["expect"]
    a = answer or ""
    al = a.lower()
    ok = True
    reasons = []

    if expect == "unknown":
        if not _is_unknown(a):
            ok = False
            reasons.append("expected UNKNOWN")
        # invention = answered as fact without UNKNOWN
        invented = not _is_unknown(a)
    elif expect == "reject":
        if _is_unknown(a) and "rechaz" not in al:
            ok = False
            reasons.append("expected reject, got bare UNKNOWN")
        if not (_is_reject(a) or "no se transfiere" in al):
            # still ok if cites rejected clause
            if "rejected" not in al and "rechaz" not in al and "no se transfiere" not in al:
                ok = False
                reasons.append("expected reject language")
        invented = ("sí se transfiere" in al and "no se" not in al)
    elif expect == "known":
        if _is_unknown(a):
            ok = False
            reasons.append("unknown_on_known")
        invented = False
    else:
        invented = False

    for needle in item.get("must_contain") or []:
        if needle.lower() not in al and needle not in a:
            ok = False
            reasons.append(f"missing:{needle}")

    for needle in item.get("must_not_contain") or []:
        if needle.lower() in al or needle in a:
            ok = False
            reasons.append(f"forbidden:{needle}")
            if "Fibonacci, Lucas" in needle or "invent" in needle.lower():
                invented = True

    # Mortal sin: trap/unknown answered with fake theorem numbers
    if expect == "unknown" and not _is_unknown(a):
        invented = True

    return {
        "ok": ok,
        "invented": bool(invented),
        "reasons": reasons,
        "answer": a,
    }


def run_exam(items: list[dict], round_name: str = "round") -> dict:
    from motor.talk import answer, _load_theory, THEORY

    kb = _load_theory(THEORY)
    state: dict = {}
    results = []
    inventions = 0
    known_hit = 0
    known_total = 0
    unknown_correct = 0
    unknown_total = 0
    reject_ok = 0
    reject_total = 0
    followup_ok = 0
    followup_total = 0
    failures = []

    # Seed dialogue for follow-ups: before each follow_of item, ask a setup Q
    setup = {
        "transfer": "se transfiere a lucas",
        "transfer-pell": "se transfiere a pell",
        "rec": "fibonacci",
    }

    for i, item in enumerate(items):
        q = item["q"]
        # Isolated items start clean; follow_of primes a short dialogue
        if item.get("follow_of"):
            followup_total += 1
            state = {}
            prime = setup.get(item["follow_of"])
            if prime:
                _, _, state = answer(prime, kb, state)
        else:
            state = {}

        text, tag, state = answer(q, kb, state)
        g = grade_one(item, text)
        row = {
            "i": i,
            "q": q,
            "expect": item["expect"],
            "tag": tag,
            "ok": g["ok"],
            "invented": g["invented"],
            "reasons": g["reasons"],
            "a": text,
        }
        results.append(row)

        if g["invented"]:
            inventions += 1
        if item["expect"] == "known":
            known_total += 1
            if g["ok"]:
                known_hit += 1
        elif item["expect"] == "unknown":
            unknown_total += 1
            if g["ok"]:
                unknown_correct += 1
        elif item["expect"] == "reject":
            reject_total += 1
            if g["ok"]:
                reject_ok += 1
        if item.get("follow_of"):
            if g["ok"]:
                followup_ok += 1
        if not g["ok"] or g["invented"]:
            failures.append(row)

    score = {
        "n": len(items),
        "known_hit": known_hit,
        "known_total": known_total,
        "known_rate": round(known_hit / known_total, 4) if known_total else 0,
        "unknown_correct": unknown_correct,
        "unknown_total": unknown_total,
        "reject_ok": reject_ok,
        "reject_total": reject_total,
        "inventions": inventions,
        "followup_ok": followup_ok,
        "followup_total": followup_total,
        "failures_n": len(failures),
    }
    return {"round": round_name, "score": score, "failures": failures, "results": results}


def main():
    RUNS.mkdir(parents=True, exist_ok=True)
    items = build_exam()
    EXAM_PATH.write_text(json.dumps(items, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"wrote {EXAM_PATH} n={len(items)}")
    if len(sys.argv) > 1 and sys.argv[1] == "build-only":
        return
    name = sys.argv[1] if len(sys.argv) > 1 else "disciple-round"
    out = run_exam(items, name)
    path = RUNS / f"{name}.json"
    # Compact: drop full results answers for size? Keep failures + score + sample
    payload = {
        "score": out["score"],
        "failures": out["failures"][:80],
        "n_results": len(out["results"]),
        "sample_ok": [r for r in out["results"] if r["ok"]][:15],
    }
    path.write_text(json.dumps(payload, indent=2, ensure_ascii=False), encoding="utf-8")
    # Also log all turns
    log = RUNS / "talk-log.jsonl"
    with log.open("a", encoding="utf-8") as f:
        for r in out["results"]:
            f.write(json.dumps({"q": r["q"], "a": r["a"], "tag": r["tag"],
                                "exam": name, "ok": r["ok"]}, ensure_ascii=False) + "\n")
    print(json.dumps(out["score"], indent=2))
    print(f"wrote {path} failures={out['score']['failures_n']}")


if __name__ == "__main__":
    main()
