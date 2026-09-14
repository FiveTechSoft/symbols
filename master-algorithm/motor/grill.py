"""Brutal mouth grill: ≥150 Qs, verdict ok|fail|unknown-ok. No synonym tables."""
from __future__ import annotations

import json
import re
from pathlib import Path

from motor.talk import answer, _load_theory, THEORY

RUNS = Path(__file__).resolve().parent / "runs"


def _is_unknown(text: str) -> bool:
    first = (text or "").strip().splitlines()[0] if text else ""
    return first.startswith("UNKNOWN")


def _invents(a: str) -> bool:
    """Non-UNKNOWN answer that invents domain content for a trap."""
    return not _is_unknown(a)


def build_grill() -> list[dict]:
    """Each item: q, expect in {known, unknown, reject, follow}, optional must/forbid, prime."""
    items: list[dict] = []

    def add(q, expect, **kw):
        items.append({"q": q, "expect": expect, **kw})

    # --- identity / growth / summary ---
    add("quien eres", "known", must=["Master Algorithm"], must_any=["hecho", "firmado", "verific"])
    add("who are you", "known", must=["Master Algorithm"])
    add("tu tribu", "known", must=["símbolo"])
    add("what is your tribe", "known", must_any=["tribu", "símbolo", "Master Algorithm"])
    add("creciste", "known", must_any=["pasos", "Verificado", "verificado", "transfer"])
    add("crecimiento", "known", must_any=["pasos", "Verificado", "verificado", "transfer"])
    add("como vas", "known", must_any=["pasos", "Verificado", "verificado", "transfer"])
    add("que sabes", "known", must_any=["verificado", "Verificado", "Fibonacci", "firmados"])
    add("resumen", "known", must_any=["verificado", "Verificado", "Fibonacci", "firmados"])
    add("inventario", "known", must_any=["verificado", "Verificado", "Fibonacci", "firmados"])

    # --- rec heads + typos (child stem) ---
    for q in (
        "fib", "fibonacci", "fibonaci", "fibonachi", "FIB",
        "lucas", "lukas", "Lucas",
        "pell", "Pell", "pel",
        "recurrencia de fib", "ley de lucas", "rec pell",
        "cual es la ley de fib", "formula de fibonacci",
        "fib [1,1]", "lucas [1,1]", "pell [2,1]",
    ):
        add(q, "known", must_any=["F(n)=", "L(n)=", "P(n)=", "Verificado", "verificado", "ley"])

    # --- false laws → reject, NEVER Demostrado ---
    for q in (
        "fib(n)=2*fib(n-1)",
        "fib(n) = 2·fib(n-1)",
        "F(n)=2F(n-1)",
        "lucas(n)=2*lucas(n-1)",
        "pell(n)=3*pell(n-1)",
        "fib(n)=fib(n-1)+fib(n-3)",
        "el doble del anterior en fib",
        "siempre 2 veces el anterior fib",
        "order 1 fib",
        "orden 1 lucas",
        "fib es solo el anterior",
        "el doble de lucas",
        "siempre 2 pell?",
        "fib(n)=3*fib(n-1)+fib(n-2)",
        "lucas(n)=2*lucas(n-1)+lucas(n-2)",
    ):
        add(q, "reject", must=["Rechazado"], forbid=["Demostrado"])

    # --- true rec formulas ---
    for q in (
        "fib(n)=fib(n-1)+fib(n-2)",
        "fib(n) = 1*fib(n-1) + 1*fib(n-2)",
        "lucas(n)=lucas(n-1)+lucas(n-2)",
        "pell(n)=2*pell(n-1)+pell(n-2)",
        "pell(n) = (2)*pell(n-1) + (1)*pell(n-2)",
    ):
        add(q, "known", must_any=["F(n)=", "L(n)=", "P(n)=", "Verificado", "verificado"], forbid=["Rechazado"])

    # --- bilin / Cassini formula (atom name, not the word cassini) ---
    for q in (
        "fib(n+1)fib(n-1)-fib(n)^2 = (-1)^n",
        "fib(n+1)fib(n-1)-fib(n)^2=(-1)^n",
        "fib(n+1)*fib(n-1)-fib(n)^2 = (-1)^n",
        "F(n+1)F(n-1)-F(n)^2=(-1)^n",
        "bilin_fib_offset_pm1",
        "verified bilin_fib_offset_pm1",
    ):
        add(q, "known", must_any=["(-1)^n", "bilin_fib_offset_pm1", "Identidad", "identidad"], forbid=["UNKNOWN"])

    add("bilin_fib_r1", "known", must_any=["bilin_fib", "Identidad", "identidad", "fib"])
    add("bilin_fib_r2", "known", must_any=["bilin_fib", "Identidad", "identidad", "fib"])
    add("bilin_fib_bogus_const2", "reject", must=["Rechazado"])

    # --- cassini the WORD must be UNKNOWN (no synonym table) ---
    for q in (
        "cassini", "Cassini", "identidad de cassini",
        "cassini identity", "que es cassini", "ley de cassini",
        "demuestra cassini",
    ):
        add(q, "unknown", forbid=["Demostrado", "bilin_fib_offset_pm1"])
    # cassini + fib atom: child answers the living rec (cassini still unbound)
    add("cassini fib", "known", must_any=["F(n)=", "Fibonacci", "Verificado"])

    # --- alma / filotaxis / traps → UNKNOWN, no invention ---
    for q in (
        "el alma", "alma", "filotaxis", "filotaxia",
        "universo", "espiral aurea del alma",
        "dios", "amor", "conciencia", "karma",
        "teorema de pitagoras", "teorema de fermat",
        "euler", "riemann", "zeta",
        "catalan", "motzkin", "padovan", "tribonacci",
        "josephus", "collatz", "goldbach",
        "pisano",  # no synonym table
        "periodo de pisano", "numero aureo exacto",
        "inventa un teorema", "teorema nuevo", "crea una ley",
    ):
        add(q, "unknown")
    # foreign word + living atom → child answers the atom (not an alma invention)
    add("fibonacci en la naturaleza del alma", "known", must_any=["F(n)=", "Fibonacci", "Verificado"])
    add("que es el alma de fib", "known", must_any=["F(n)=", "Fibonacci", "Verificado"])
    add("filotaxis de lucas", "known", must_any=["L(n)=", "Lucas", "Verificado"])
    add("inventame algo de fib", "unknown")  # invent speech

    # --- true_mod digits ---
    for q, needle in (
        ("true_mod fib 2", "π_"),
        ("true_mod fib 3", "π_"),
        ("true_mod fib 5", "π_"),
        ("true_mod fib 7", "π_"),
        ("true_mod fib 11", "π_"),
        ("periodo fib 2", "π_"),
        ("periodo de fib modulo 5", "π_"),
        ("fib modulo 2", "π_"),
        ("fib mod 5", "π_"),
        ("true_mod lucas 2", "π_"),
        ("period_fib_m2", "π_"),
        ("period_fib_m11", "π_"),
    ):
        add(q, "known", must=[needle])

    # --- geometry captions ---
    for q in (
        "BC ∥ MN", "BC || MN", "2·MN = BC", "2*MN = BC",
        "AC ∥ MP", "BM = AM", "AD ∥ BC", "AB ∥ CD",
        "L1_midline_parallel", "L3_midline_half",
        "lemma L1", "lema L1", "lemas",
        "geo_invent", "midline",
    ):
        add(q, "known", must_any=["Lema", "lemma", "Demostrado", "∥", "MN", "BC", "AM", "MP", "CD"])

    # --- transfer ---
    for q in (
        "se transfiere a lucas",
        "transfiere fib a lucas",
        "fib a lucas",
        "fib y lucas misma?",
        "misma ley fib lucas",
        "transfer_fib_to_lucas",
        "fibonaci a lukas",
    ):
        add(q, "known", must=["Sí"], forbid=["UNKNOWN"])

    for q in (
        "se transfiere a pell",
        "transfiere fib a pell",
        "fib a pell",
        "fib y pell misma?",
        "transfiere pell a fib",
        "transfiere pell a lucas",
        "fibonaci a pell",
        "transfer_fib_to_pell",
    ):
        add(q, "reject", must=["No"], forbid=["Sí se transfiere"])

    # --- companion / ratio / rejected primes ---
    add("companion fib lucas", "known", must=["companion"])
    add("companion", "known", must=["companion"])
    add("ratio_fib_to_phi", "known", must=["φ"])
    add("NEG_fib_always_prime", "reject", must=["Rechazado"])
    add("fib siempre primo", "reject", must=["Rechazado"])
    add("siempre primo fib", "reject", must=["Rechazado"])
    add("NEG_ratio_fib_to_e", "reject", must=["Rechazado"])

    # --- Spanish + typos mix ---
    for q in (
        "demostrá la ley de fib",
        "demostra fib",
        "prueba la formula de pell",
        "que sabes de lucas",
        "mas sobre fib",
        "más lemas",
        "lista de lemas",
        "rejected transfer_pell_to_fib",
        "rechazado bilin_lucas_offset_pm1",
    ):
        add(q, "known")

    # --- follow-ups with prime dialogue ---
    add("por que", "known", prime="se transfiere a pell", must_any=["P(n)=", "F(n)=", "n=2", "analog"], forbid=["UNKNOWN"])
    add("por que", "known", prime="se transfiere a lucas", must_any=["L(n)=", "F(n)=", "misma", "ley"], forbid=["UNKNOWN"])
    add("y pell", "reject", prime="fibonacci", must=["No"])
    add("y lucas", "known", prime="fibonacci", must=["Sí"])
    add("demostrá esto", "known", prime="fibonacci", forbid=["UNKNOWN"])
    add("demostrá esto", "known", prime="fib(n+1)fib(n-1)-fib(n)^2 = (-1)^n",
        must_any=["(-1)^n", "bilin_fib_offset_pm1", "identidad", "Identidad"], forbid=["UNKNOWN"])
    add("demostrá esto", "known", prime="BC ∥ MN", forbid=["UNKNOWN"])
    add("y eso", "known", prime="se transfiere a lucas", forbid=["UNKNOWN"])
    add("mas", "known", prime="lemas", forbid=["UNKNOWN"])
    add("demostrá esto", "unknown", prime="cassini")  # after UNKNOWN, stay UNKNOWN
    add("demostrá esto", "unknown", prime="el alma")

    # --- more false + formula traps ---
    add("fib(n)^2 - fib(n+1)fib(n-1) = (-1)^(n-1) fib(1)^2", "known", must_any=["bilin_fib_r1", "(-1)", "Identidad", "identidad", "fib"])
    add("bilin_lucas_offset_pm1", "reject", must=["Rechazado"])
    add("period_fib_m6", "reject", must=["Rechazado"])
    add("period_lucas_m3", "reject", must=["Rechazado"])

    # --- pad to ≥150 with atom-grounded variants ---
    extras_known = [
        "rec_fib_o2_1_1", "rec_lucas_o2_1_1", "rec_pell_o2_2_1",
        "transfer_lucas_to_fib_1_1", "true_mod",
        "obs fib", "bitfn_parity_is_parity",
        "geo_midline_euclid_conjectures_p0", "L8_isos_base",
        "L9_EqSeg", "L10_para_opp", "L14_midline_parallel",
        "fib(n) = (1)*fib(n-1) + (1)*fib(n-2)",
        "lucas(n) = (1)*lucas(n-1) + (1)*lucas(n-2)",
        "demuestra rec_fib", "ley [1,1] fib",
        "modulo 8 fib", "fib 4 periodo",
        "companion pell", "NP ∥ NX3", "CX0 ∥ X0X1",
        "∠X0CX1 = ∠CX0X1", "MN ∥ NX2",
    ]
    for q in extras_known:
        add(q, "known")

    extras_unk = [
        "catalan numbers", "bell numbers", "stirling",
        "matriz de pascal", "transformada de fourier",
        "alma del universo", "espiral del alma",
        "cassini theorem", "identidad cassini",
        "pisano period of 13", "periodo pisano 13",
        "tribonacci recurrence", "narayana",
        "why is the sky blue", "clima", "futbol",
        "receta de cocina", "bitcoin", "chatgpt",
        "cual es la recurrencia", "schema", "verified fact",
    ]
    for q in extras_unk:
        add(q, "unknown")
    # quantum + fib atom → answers fib (child bind)
    add("quantum fibonacci", "known", must_any=["F(n)=", "Fibonacci", "Verificado"])
    add("fibonacci quantum", "known", must_any=["F(n)=", "Fibonacci", "Verificado"])

    # more reject false laws
    for q in (
        "fib(n)=fib(n-1)",
        "fib(n)=5*fib(n-1)+fib(n-2)",
        "pell(n)=pell(n-1)+pell(n-2)",
        "lucas(n)=3*lucas(n-1)",
        "el doble siempre en fibonacci",
        "2 veces el anterior lucas",
    ):
        add(q, "reject", must=["Rechazado"], forbid=["Demostrado"])

    assert len(items) >= 150, len(items)
    return items


def grade(item: dict, a: str) -> str:
    """Return ok | fail | unknown-ok."""
    expect = item["expect"]
    al = (a or "").lower()

    # Hard fail classes from doctrine
    if "cassini" in item["q"].lower() and "cassini" not in "bilin":
        # cassini-the-word must not be known
        if expect == "unknown" and not _is_unknown(a):
            return "fail"
        if expect == "unknown" and _is_unknown(a):
            # still check other forbids
            pass

    if expect == "unknown":
        if _is_unknown(a):
            # check forbids
            for f in item.get("forbid") or []:
                if f.lower() in al or f in (a or ""):
                    return "fail"
            return "unknown-ok"
        return "fail"  # invention

    if expect == "reject":
        if "demostrado" in al and "rechaz" not in al:
            # Demostrado for a false rec
            return "fail"
        for f in item.get("forbid") or []:
            if f.lower() in al or f in (a or ""):
                return "fail"
        ok_reject = (
            "rechazado" in al or "no se transfiere" in al
            or "rejected" in al or "analogía no sobrevive" in al
            or "analogia no sobrevive" in al
            or ("no." in al[:8]) or ("no es" in al and "unknown" not in al[:20])
        )
        if not ok_reject:
            return "fail"
        for m in item.get("must") or []:
            if m.lower() not in al and m not in (a or ""):
                return "fail"
        return "ok"

    # known / follow
    if _is_unknown(a):
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
    # bilin offset formula = identity in words/math (atom cite optional)
    qf = item["q"].replace(" ", "").lower().replace("*", "")
    if "fib(n+1)fib(n-1)-fib(n)^2" in qf or "f(n+1)f(n-1)-f(n)^2" in qf:
        if "(-1)^n" not in al and "bilin_fib_offset_pm1" not in al:
            return "fail"
    return "ok"


def run_grill(round_name: str = "grill") -> dict:
    kb = _load_theory(THEORY)
    items = build_grill()
    results = []
    state: dict = {}
    counts = {"ok": 0, "fail": 0, "unknown-ok": 0}
    fail_classes: dict[str, list] = {
        "invention": [],
        "demostrado_false_rec": [],
        "cassini_known": [],
        "bilin_no_cite": [],
        "demostra_esto_unknown": [],
        "alma_filotaxis_invent": [],
        "other": [],
    }

    for i, item in enumerate(items):
        state = {}
        if item.get("prime"):
            _, _, state = answer(item["prime"], kb, state)
        text, tag, state = answer(item["q"], kb, state)
        verdict = grade(item, text)
        row = {"q": item["q"], "a": text, "verdict": verdict, "expect": item["expect"], "tag": tag}
        if item.get("prime"):
            row["prime"] = item["prime"]
        results.append(row)
        counts[verdict] = counts.get(verdict, 0) + 1

        if verdict == "fail":
            qlow = item["q"].lower()
            al = text.lower()
            if item["expect"] == "unknown" and not _is_unknown(text):
                fail_classes["invention"].append(row)
                if "alma" in qlow or "filotax" in qlow:
                    fail_classes["alma_filotaxis_invent"].append(row)
                if "cassini" in qlow:
                    fail_classes["cassini_known"].append(row)
            elif item["expect"] == "reject" and "demostrado" in al and "rechaz" not in al:
                fail_classes["demostrado_false_rec"].append(row)
            elif "bilin" in str(item.get("must")) and "bilin_fib_offset_pm1" not in al:
                fail_classes["bilin_no_cite"].append(row)
            elif "demostr" in qlow and "esto" in qlow and _is_unknown(text) and item["expect"] == "known":
                fail_classes["demostra_esto_unknown"].append(row)
            else:
                # classify bilin formula / demostra
                qf = item["q"].replace(" ", "").lower()
                if "fib(n+1)" in qf and "(-1)" in qf and "bilin_fib_offset_pm1" not in al:
                    fail_classes["bilin_no_cite"].append(row)
                elif "demostr" in qlow and "esto" in qlow and item["expect"] == "known":
                    fail_classes["demostra_esto_unknown"].append(row)
                else:
                    fail_classes["other"].append(row)

    # slim a for json size? keep full — user asked {q,a,verdict}
    slim = [{"q": r["q"], "a": r["a"], "verdict": r["verdict"]} for r in results]
    payload = {
        "round": round_name,
        "n": len(results),
        "counts": counts,
        "fail_class_counts": {k: len(v) for k, v in fail_classes.items()},
        "fail_classes": {k: [{"q": x["q"], "a": x["a"][:200], "verdict": x["verdict"]} for x in v[:20]]
                         for k, v in fail_classes.items() if v},
        "results": slim,
    }
    RUNS.mkdir(parents=True, exist_ok=True)
    path = RUNS / "grill.json"
    path.write_text(json.dumps(payload, indent=2, ensure_ascii=False), encoding="utf-8")
    return payload


if __name__ == "__main__":
    out = run_grill("round0")
    print(json.dumps({
        "n": out["n"],
        "counts": out["counts"],
        "fail_class_counts": out["fail_class_counts"],
        "fail_sample": {k: v[:5] for k, v in out.get("fail_classes", {}).items()},
    }, indent=2, ensure_ascii=False))
