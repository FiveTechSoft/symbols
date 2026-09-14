"""Keep-chat4 — short continuous grill (≥25 turns) after _is_ack.

Path: Fib → demostrá eso → ok → sí → vale → Pell lie → 471 boast
→ cassini/multiverso → unit → en cristiano → gracias.

Score axes: invention-fail, robotic-fail. Target 0/0.
Ack (ok/sí/vale/dale/no) must be tag=ack, never UNKNOWN.
"""
from __future__ import annotations

import json
import re
from pathlib import Path

from motor.talk import answer, _load_theory, THEORY

RUNS = Path(__file__).resolve().parent

ROBOTIC_MARKERS = (
    "UNIT{",
    "unit{",
    "held-out",
    "ley verificada",
    "contrastamos",
    "pasos medidos",
    "transfer_accuracy",
    "jointly closes",
    "conserv residual",
    "spawn_lever",
    "está en lo demostrado",
    "esta en lo demostrado",
    "identidad verificada",
)

FACT_COUNT_RE = re.compile(
    r"\b\d{2,4}\s+hechos\b|\bhechos firmados\b|\b\d{2,4}\s+cosas firmadas\b",
    re.I,
)

ACK_WORDS = {"ok", "okay", "vale", "sí", "si", "dale", "no", "bien", "claro"}


def _is_unknown(text: str) -> bool:
    first = (text or "").strip().splitlines()[0] if text else ""
    return first.startswith("UNKNOWN")


def _is_reject(text: str) -> bool:
    al = (text or "").lower()
    return (
        "rechazado" in al
        or "no se transfiere" in al
        or "analogía no sobrevive" in al
        or "analogia no sobrevive" in al
        or al.strip().startswith("no.")
        or al.strip().startswith("no,")
        or (
            al.strip().startswith("no ")
            and "no finjo" not in al
            and "no invento" not in al
        )
    )


def _is_robotic(text: str, expect: str | None = None) -> bool:
    al = text or ""
    low = al.lower()
    if "UNIT{" in al:
        return True
    for m in ROBOTIC_MARKERS:
        if m.lower() in low or m in al:
            return True
    if expect in ("identity", "greet") and FACT_COUNT_RE.search(al or ""):
        return True
    return False


def _invents_biology(text: str) -> bool:
    al = (text or "").lower()
    if _is_unknown(text):
        return False
    bio = (
        "célula biológica",
        "celula biologica",
        "organismo vivo",
        "adn",
        "membrane lipid",
        "protocélula biológica es",
        "protocelula biologica es",
    )
    return any(b in al for b in bio)


def build_script() -> list[dict]:
    items: list[dict] = []

    def add(q, expect, **kw):
        items.append({"q": q, "expect": expect, **kw})

    # Scene A: greet + identity
    add("hola", "greet", forbid_robotic=True, forbid_any=["hechos firmados", "transfer_accuracy"])
    add("quién eres", "identity", forbid_robotic=True, forbid_any=["hechos firmados", "504", "471"], must_any=["Master"])

    # Scene B: Fib → demostrá eso → ok → sí → vale
    add("fibonacci", "known", forbid_robotic=True, must_any=["F(n)=", "Fibonacci", "suma", "anteriores"])
    add("demostrá eso", "known", forbid_robotic=True, must_any=["F(n)=", "Fibonacci", "anteriores", "suma"])
    add("ok", "banter", forbid_robotic=True, must_ack=True)
    add("sí", "banter", forbid_robotic=True, must_ack=True)
    add("vale", "banter", forbid_robotic=True, must_ack=True)

    # Scene C: lucas → Pell lie → demostrá
    add("lucas", "known", must_any=["L(n)=", "Lucas", "misma"])
    add("y pell", "lie_or_reject", must_reject=True)
    add("demostrá eso", "known", forbid_robotic=True, must_any=["Pell", "P(n)=", "analog", "rechaz", "No", "no", "sobrevive"])
    add("dale", "banter", forbid_robotic=True, must_ack=True)

    # Scene D: 471 boast
    add(
        "eres más listo porque tienes 471 hechos?",
        "level",
        forbid_robotic=True,
        must_any=["No", "palanca", "unidad", "inventario"],
        forbid_any=["UNIT{", "held-out", "ley verificada", "Contrastamos"],
    )
    add("ok", "banter", forbid_robotic=True, must_ack=True)

    # Scene E: cassini / multiverso UNKNOWN
    add("cassini", "unknown", forbid_robotic=True)
    add("multiverso", "unknown", forbid_robotic=True)

    # Scene F: lie after true + no ack
    add("ohm", "known", forbid_robotic=True, must_any=["V=IR", "Ohm", "ohm"])
    add("V=2IR", "lie", must_reject=True)
    add("no", "banter", forbid_robotic=True, must_ack=True)

    # Scene G: fib lie
    add("fibonacci", "known", must_any=["F(n)="])
    add("F(n)=2F(n-1)", "lie", must_reject=True)
    add("sí", "banter", forbid_robotic=True, must_ack=True)

    # Scene H: unit → en cristiano → gracias
    add(
        "unit_protocell_levers",
        "known",
        forbid_robotic=True,
        forbid_biology=True,
        must_any=["seis", "viajan", "paquete", "unidad", "taxis"],
    )
    add(
        "en cristiano",
        "known",
        forbid_robotic=True,
        must_any=["seis", "piezas", "paquete", "juntas", "delta", "taxis", "palanca"],
    )
    add("vale", "banter", forbid_robotic=True, must_ack=True)
    add("y la protocélula biológica?", "unknown", no_biology_invention=True, forbid_robotic=True)
    add("gracias", "thanks", forbid_robotic=True, must_any=["nada", "gusto", "Dale", "aquí", "Aqui", "seguimos"])

    assert len(items) >= 25, len(items)
    return items


def grade(item: dict, a: str, tag: str | None = None, st: dict | None = None) -> str:
    expect = item["expect"]
    al = (a or "").lower()

    if item.get("forbid_robotic") or expect in (
        "greet", "identity", "growth", "thanks", "level", "banter",
    ):
        if _is_robotic(a, expect):
            return "robotic-fail"
    if _is_robotic(a, expect) and expect in ("known", "soft", "unknown"):
        return "robotic-fail"

    if item.get("no_biology_invention") or item.get("forbid_biology"):
        if _invents_biology(a):
            return "invention-fail"

    for f in item.get("forbid_any") or []:
        if f.lower() in al or f in (a or ""):
            if f.lower() in ("hechos firmados",) or "hecho" in f.lower():
                return "robotic-fail"
            return "fail"

    if item.get("forbid_unknown") and _is_unknown(a):
        return "fail"

    if expect == "unknown":
        return "unknown-ok" if _is_unknown(a) else "invention-fail"

    if expect == "lie" or expect == "lie_or_reject":
        if _is_reject(a) or (a or "").strip().lower().startswith("no"):
            return "ok"
        if _is_unknown(a):
            return "unknown-ok"
        if any(x in al for x in ("sí:", "si:", "verificado", "obedece", "comparten")):
            return "invention-fail"
        return "fail"

    if expect == "banter":
        if _invents_biology(a):
            return "invention-fail"
        if _is_robotic(a, "banter"):
            return "robotic-fail"
        if item.get("must_ack"):
            # ok/sí/vale must not fall through to UNKNOWN
            if _is_unknown(a) or (tag and tag == "unknown"):
                return "fail"
            if tag and tag != "ack":
                return "fail"
        if _is_unknown(a):
            return "fail"
        if any(x in al for x in ("ley verificada", "contrastamos", "held-out")):
            return "robotic-fail"
        return "ok"

    if expect == "greet":
        if _is_unknown(a):
            return "fail"
        if FACT_COUNT_RE.search(a or ""):
            return "robotic-fail"
        return "ok"

    if expect == "identity":
        if _is_unknown(a):
            return "fail"
        if FACT_COUNT_RE.search(a or ""):
            return "robotic-fail"
        if item.get("must_any"):
            if not any(m.lower() in al or m in (a or "") for m in item["must_any"]):
                return "fail"
        return "ok"

    if expect == "level":
        if "más listo" in al and "no" not in al[:40]:
            return "fail"
        if item.get("must_any") and not any(m.lower() in al for m in item["must_any"]):
            return "fail"
        return "ok"

    if expect == "thanks":
        if _is_unknown(a):
            return "fail"
        if item.get("must_any") and not any(m.lower() in al or m in (a or "") for m in item["must_any"]):
            return "fail"
        return "ok"

    # known
    if _is_unknown(a):
        return "fail"
    if item.get("must_reject") and not _is_reject(a):
        return "fail"
    if item.get("must_any"):
        if not any((m.lower() in al or m in (a or "")) for m in item["must_any"]):
            return "fail"
    return "ok"


def run_grill(round_name: str = "keep-chat4") -> dict:
    kb = _load_theory(THEORY)
    items = build_script()
    results = []
    counts = {"ok": 0, "fail": 0, "unknown-ok": 0, "robotic-fail": 0, "invention-fail": 0}
    fails: list[dict] = []
    st: dict = {}
    stiff: list[dict] = []
    ack_clean = {"ok": True, "sí": True, "vale": True, "dale": True, "no": True}

    for item in items:
        text, tag, st = answer(item["q"], kb, st)
        verdict = grade(item, text, tag, st)
        row = {
            "q": item["q"],
            "a": text,
            "verdict": verdict,
            "expect": item["expect"],
            "tag": tag,
        }
        results.append(row)
        counts[verdict] = counts.get(verdict, 0) + 1
        if verdict not in ("ok", "unknown-ok"):
            fails.append(
                {
                    "q": row["q"],
                    "a": (text or "")[:320],
                    "expect": item["expect"],
                    "tag": tag,
                    "verdict": verdict,
                }
            )

        qn = (item["q"] or "").strip().lower().rstrip("!.")
        if qn in ack_clean:
            if _is_unknown(text) or tag != "ack":
                ack_clean[qn if qn != "si" else "sí"] = False
                if qn == "si":
                    ack_clean["sí"] = False

        al = text or ""
        low = al.lower()
        if item["expect"] == "banter" and _is_unknown(al):
            stiff.append({"q": item["q"], "a": al[:220], "why": "banter-unknown"})
        elif item.get("must_ack") and tag != "ack":
            stiff.append({"q": item["q"], "a": al[:220], "why": f"ack-tag={tag}"})
        elif _is_robotic(al, item["expect"]):
            stiff.append({"q": item["q"], "a": al[:220], "why": "marker"})
        elif al.count("¿") >= 2:
            stiff.append({"q": item["q"], "a": al[:220], "why": "multi-closing-q"})
        elif "nada más. nada más" in low:
            stiff.append({"q": item["q"], "a": al[:220], "why": "echo-closer"})

    invention_n = counts.get("invention-fail", 0)
    robotic_n = counts.get("robotic-fail", 0)
    robotic_scan = sum(1 for r in results if _is_robotic(r["a"] or "", r["expect"]))

    humanish = [
        r
        for r in results
        if r["verdict"] in ("ok", "unknown-ok")
        and not _is_robotic(r["a"] or "", r["expect"])
        and r["expect"] in ("known", "identity", "greet", "level", "lie", "lie_or_reject", "thanks")
        and not _is_unknown(r["a"] or "")
        and len((r["a"] or "")) > 40
    ]
    best = []
    seen = set()
    for r in humanish:
        key = (r["expect"], r["q"][:24])
        if key in seen:
            continue
        seen.add(key)
        best.append(r)
        if len(best) >= 6:
            break

    # summarize ack cleanliness from results
    ack_report = {}
    for word in ("ok", "sí", "vale", "dale", "no"):
        rows = [r for r in results if r["q"].strip().lower().rstrip("!.") in {word, word.replace("sí", "si")}]
        if not rows and word == "sí":
            rows = [r for r in results if r["q"].strip() in ("sí", "si")]
        ack_report[word] = {
            "n": len(rows),
            "all_ack_tag": all(r.get("tag") == "ack" for r in rows) if rows else None,
            "any_unknown": any(_is_unknown(r["a"] or "") for r in rows) if rows else None,
            "clean": (all(r.get("tag") == "ack" and not _is_unknown(r["a"] or "") for r in rows) if rows else None),
        }

    payload = {
        "round": round_name,
        "n": len(results),
        "counts": counts,
        "fail_n": sum(v for k, v in counts.items() if k not in ("ok", "unknown-ok")),
        "invention_n": invention_n,
        "robotic_n": robotic_n,
        "robotic_scan": robotic_scan,
        "target": "0 invention / 0 robotic",
        "ack_clean": ack_report,
        "fails": fails[:40],
        "best_human": [
            {"q": r["q"], "a": r["a"], "expect": r["expect"], "tag": r.get("tag")}
            for r in best[:3]
        ],
        "remaining_robotic_spots": stiff[:10],
        "patch": "_is_ack covers ok/sí/vale/dale/no; bare ack never UNKNOWN",
        "results": [
            {
                "q": r["q"],
                "a": r["a"],
                "verdict": r["verdict"],
                "expect": r["expect"],
                "tag": r.get("tag"),
            }
            for r in results
        ],
    }
    (RUNS / "keep-chat4.json").write_text(
        json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    return payload


def write_md(payload: dict) -> None:
    c = payload["counts"]
    ack = payload.get("ack_clean") or {}
    lines = [
        "# Keep-chat4 — short grill after _is_ack",
        "",
        f"- n asked: **{payload['n']}**",
        f"- ok: {c.get('ok', 0)} · unknown-ok: {c.get('unknown-ok', 0)} · fail: {c.get('fail', 0)}",
        f"- **invention-fail: {c.get('invention-fail', 0)}** · **robotic-fail: {c.get('robotic-fail', 0)}** (scan={payload.get('robotic_scan')})",
        f"- target: **0 / 0**",
        "",
        "## Ack cleanliness",
    ]
    for w, info in ack.items():
        lines.append(
            f"- `{w}`: n={info.get('n')} clean={info.get('clean')} "
            f"all_ack_tag={info.get('all_ack_tag')} any_unknown={info.get('any_unknown')}"
        )
    lines += [
        "",
        "## Mix",
        "- Fib → demostrá eso → ok → sí → vale",
        "- lucas → Pell lie → demostrá → dale",
        "- 471-boast → ok",
        "- bare cassini / multiverso → UNKNOWN",
        "- ohm lie → no ack; fib lie → sí",
        "- unit → en cristiano → vale → bio UNKNOWN → gracias",
        "",
        "## Best 3 human replies",
    ]
    for i, ex in enumerate(payload.get("best_human") or [], 1):
        lines.append(f"{i}. `{ex['q']}`")
        lines.append(f"   - {(ex['a'] or '').replace(chr(10), ' ')}")
    lines.append("")
    lines.append("## Remaining robotic / stiff spots")
    spots = payload.get("remaining_robotic_spots") or []
    if not spots:
        lines.append("- (none flagged)")
    for s in spots:
        lines.append(f"- ({s.get('why')}) `{s['q']}` → {(s['a'] or '')[:140]}")
    if payload.get("fails"):
        lines.append("")
        lines.append("## Fails")
        for f in payload["fails"][:12]:
            lines.append(f"- [{f.get('verdict')}/{f['expect']}] `{f['q']}` tag={f.get('tag')}")
            lines.append(f"  - {(f['a'] or '')[:160].replace(chr(10), ' ')}")
    lines.append("")
    lines.append(f"## Patch: {payload.get('patch', 'none')}")
    lines.append("")
    (RUNS / "keep-chat4.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    p = run_grill()
    write_md(p)
    print(
        json.dumps(
            {
                "n": p["n"],
                "counts": p["counts"],
                "fail_n": p["fail_n"],
                "invention_n": p["invention_n"],
                "robotic_n": p["robotic_n"],
                "robotic_scan": p["robotic_scan"],
                "stiff_n": len(p.get("remaining_robotic_spots") or []),
                "ack_clean": p.get("ack_clean"),
            },
            ensure_ascii=False,
            indent=2,
        )
    )
    if p["fails"]:
        print("FAILS:")
        for f in p["fails"][:25]:
            print(
                " ",
                f.get("verdict"),
                f["expect"],
                f["q"],
                "→",
                (f["a"] or "")[:140].replace("\n", " "),
            )
    if p.get("remaining_robotic_spots"):
        print("STIFF:")
        for s in p["remaining_robotic_spots"][:8]:
            print(" ", s.get("why"), s["q"], "→", (s["a"] or "")[:100].replace("\n", " "))
