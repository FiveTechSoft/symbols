"""Keep-chat2 — conversational battery (≥40 turns). Stress DIALOGUE not math dumps.

Score axes: invention fail, robotic fail.
Robotic markers: UNIT{, held-out, ley verificada, Contrastamos, pasos medidos, dashboard-ish transfer_accuracy on greet.
Target: 0 invention / 0 robotic.
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
        or (al.strip().startswith("no ") and "no finjo" not in al and "no invento" not in al)
    )


def _is_robotic(text: str) -> bool:
    al = text or ""
    low = al.lower()
    if "UNIT{" in al:
        return True
    for m in ROBOTIC_MARKERS:
        if m.lower() in low or m in al:
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
    """Multi-turn dialogue script. Shared state across turns in a scene."""
    items: list[dict] = []

    def add(q, expect, **kw):
        items.append({"q": q, "expect": expect, **kw})

    # --- Scene A: greets / growth — no transfer_accuracy on greet ---
    add("hola", "greet", forbid_robotic=True, forbid_any=["transfer_accuracy", "0.57", "transferencia anda"])
    add("hola, cómo vas", "greet", forbid_robotic=True, forbid_any=["transfer_accuracy", "transferencia anda"])
    add("cómo vas", "growth", forbid_robotic=True, must_any=["firmad", "palanca", "unidad", "crec"])
    add("gracias", "thanks", forbid_robotic=True, forbid_unknown=True, must_any=["nada", "gusto", "Dale", "aquí", "aqui", "seguimos"])

    # --- Scene B: identity without tribe spam / count ---
    add("eres un robot?", "identity", forbid_robotic=True, forbid_any=["símbolo ·", "hechos firmados"], must_any=["programa", "Master", "robot", "no invento", "callo"])
    add("quién eres", "identity", forbid_robotic=True, forbid_any=["símbolo ·", "hechos firmados", "504"], must_any=["Master"])
    add("tu tribu", "identity", forbid_robotic=True, must_any=["símbolo", "Master"])  # stamp OK when asked

    # --- Scene C: unit → en cristiano → bio (no invention) ---
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
        forbid_biology=True,
        must_any=["seis", "piezas", "paquete", "juntas", "delta", "taxis", "palanca"],
        forbid_any=["Dale.", "De nada", "Un gusto"],
    )
    add("y la protocélula biológica?", "unknown", forbid_robotic=True, no_biology_invention=True)

    # --- Scene D: thanks then en cristiano must NOT rephrase thanks ---
    add("unit_protocell_levers", "known", forbid_robotic=True, must_any=["seis", "unidad", "paquete"])
    add("gracias", "thanks", forbid_unknown=True)
    add(
        "en cristiano",
        "known",
        forbid_robotic=True,
        must_any=["seis", "piezas", "paquete", "juntas", "unidad", "delta"],
        forbid_any=["Dale.", "De nada", "Un gusto", "Cuando quieras"],
    )

    # --- Scene E: fib dialogue — prove / why matters / confuse ---
    add("fibonacci", "known", forbid_robotic=True, must_any=["F(n)=", "Fibonacci", "suma", "anteriores"])
    add("demuéstralo", "known", forbid_robotic=True, must_any=["F(n)=", "Fibonacci"])
    add("y eso qué importa", "known", forbid_robotic=True, must_any=["Importa", "cuadra", "forma", "F(n)"])
    add("no entiendo", "known", forbid_robotic=True, must_any=["F(n)=", "anteriores", "claro", "cristiano", "término", "termino"])
    add("gracias", "thanks", forbid_unknown=True)

    # --- Scene F: lie after true (deixis) still rejects ---
    add("fibonacci", "known", must_any=["F(n)="])
    add("F(n)=2F(n-1)", "lie", must_reject=True)
    add("lucas", "known", must_any=["L(n)=", "Lucas", "misma"])
    add("L(n)=2L(n-1)", "lie", must_reject=True)

    # --- Scene G: follow-ups without same closing spam check (soft) ---
    add("ohm", "known", forbid_robotic=True, must_any=["V=IR", "Ohm", "ohm"])
    add("en cristiano", "known", forbid_robotic=True, must_any=["V=IR", "Ohm", "ohm", "Tensión", "tension", "corriente"])
    add("kepler", "known", forbid_robotic=True, must_any=["T", "a", "Kepler", "kepler"])
    add("y eso qué importa", "known", forbid_robotic=True, must_any=["Importa", "cuadra", "firmado", "rechaz"])

    # --- Scene H: more greets / soft dialogue ---
    add("hola", "greet", forbid_robotic=True, forbid_any=["transfer_accuracy", "0.57"])
    add("cómo vas", "growth", forbid_robotic=True)
    add("no entiendo", "soft", forbid_robotic=True)  # may clarify or rephrase growth-ish
    add("quién eres", "identity", forbid_any=["hechos firmados", "504"])
    add("eres un robot?", "identity", forbid_any=["símbolo ·"])

    # --- Scene I: bio pressure + unknown ---
    add("protocélula", "unknown")
    add("y la protocélula biológica?", "unknown", no_biology_invention=True)
    add("alma", "unknown")
    add("cassini", "unknown")

    # --- Scene J: transfer dialogue human ---
    add("fibonacci", "known", must_any=["F(n)="])
    add("se transfiere a lucas", "known", forbid_robotic=True, must_any=["Sí", "Si", "misma", "Lucas", "transfier"])
    add("en cristiano", "known", forbid_robotic=True, must_any=["Lucas", "Fib", "misma", "recurrencia", "arranque", "claro"])
    add("y pell", "lie_or_reject", must_reject=True)
    add("y eso qué importa", "known", forbid_robotic=True, must_any=["Importa", "no", "analog", "ley", "cuadra", "Pell", "rechaz"])

    # --- Scene K: level / count ---
    add("eres más listo porque tienes 471 hechos?", "level", forbid_robotic=True, must_any=["No", "palanca", "unidad", "inventario"])
    add("en cristiano", "known", forbid_robotic=True, must_any=["palanca", "unidad", "listo", "inventario", "hecho", "seis"])

    assert len(items) >= 40, len(items)
    return items


def grade(item: dict, a: str, st: dict | None = None) -> str:
    expect = item["expect"]
    al = (a or "").lower()

    if item.get("forbid_robotic") or expect in ("greet", "identity", "growth", "thanks", "level"):
        if _is_robotic(a):
            return "robotic-fail"
    if _is_robotic(a) and expect in ("known", "soft"):
        return "robotic-fail"

    if item.get("no_biology_invention") or item.get("forbid_biology"):
        if _invents_biology(a):
            return "invention-fail"

    for f in item.get("forbid_any") or []:
        if f.lower() in al or f in (a or ""):
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
        # agreeing is fail
        if any(x in al for x in ("sí:", "si:", "verificado", "obedece", "comparten")):
            return "invention-fail"
        return "fail"

    if expect == "greet":
        if _is_unknown(a):
            return "fail"
        if "hola" in al or "aquí" in al or "aqui" in al or "pregunta" in al:
            return "ok"
        return "ok" if not _is_unknown(a) else "fail"

    if expect == "thanks":
        if _is_unknown(a):
            return "fail"
        return "ok"

    if expect == "identity":
        if _is_unknown(a):
            return "fail"
        if item.get("must_any"):
            if not any(m.lower() in al or m in (a or "") for m in item["must_any"]):
                return "fail"
        return "ok"

    if expect == "growth":
        if _is_unknown(a):
            return "fail"
        return "ok"

    if expect == "level":
        if "más listo" in al and "no" not in al[:30]:
            return "fail"
        if item.get("must_any") and not any(m.lower() in al for m in item["must_any"]):
            return "fail"
        return "ok"

    if expect == "soft":
        # clarify OK, explain OK, unknown not preferred but not invention
        if _invents_biology(a):
            return "invention-fail"
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


def run_grill(round_name: str = "keep-chat2") -> dict:
    kb = _load_theory(THEORY)
    items = build_script()
    results = []
    counts = {"ok": 0, "fail": 0, "unknown-ok": 0, "robotic-fail": 0, "invention-fail": 0}
    fails: list[dict] = []
    st: dict = {}
    closing_qs: list[str] = []

    for item in items:
        # Continuity: same conversation state (dialogue battery)
        text, tag, st = answer(item["q"], kb, st)
        verdict = grade(item, text, st)
        # Map robotic/invention into fail bucket for target 0/0
        if verdict in ("robotic-fail", "invention-fail"):
            pass
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
            fails.append({"q": row["q"], "a": (text or "")[:320], "expect": item["expect"], "tag": tag, "verdict": verdict})

        # track trailing questions for variety note
        m = re.findall(r"¿[^?]+\?", text or "")
        closing_qs.extend(m)

    invention_n = counts.get("invention-fail", 0)
    robotic_n = counts.get("robotic-fail", 0)
    # also scan all replies for robotic even if not flagged expect
    robotic_scan = sum(1 for r in results if _is_robotic(r["a"] or ""))

    # best human replies: known/identity/greet/thanks that passed, longer-ish, no UNKNOWN
    humanish = [
        r for r in results
        if r["verdict"] in ("ok", "unknown-ok")
        and not _is_robotic(r["a"] or "")
        and r["expect"] in ("known", "identity", "greet", "thanks", "growth", "level")
        and not _is_unknown(r["a"] or "")
        and len((r["a"] or "")) > 40
    ]
    # diversify by expect
    best = []
    seen_exp = set()
    for r in humanish:
        key = (r["expect"], r["q"][:20])
        if key in seen_exp:
            continue
        seen_exp.add(key)
        best.append(r)
        if len(best) >= 8:
            break

    # remaining robotic spots (any scan hit or soft lab tone)
    soft_lab = []
    for r in results:
        al = (r["a"] or "")
        low = al.lower()
        if _is_robotic(al):
            soft_lab.append({"q": r["q"], "a": al[:200], "why": "marker"})
        elif "nada más. nada más" in low:
            soft_lab.append({"q": r["q"], "a": al[:200], "why": "echo-closer"})
        elif al.count("¿") >= 2:
            soft_lab.append({"q": r["q"], "a": al[:200], "why": "multi-closing-q"})

    payload = {
        "round": round_name,
        "n": len(results),
        "counts": counts,
        "fail_n": sum(v for k, v in counts.items() if k not in ("ok", "unknown-ok")),
        "invention_n": invention_n,
        "robotic_n": robotic_n,
        "robotic_scan": robotic_scan,
        "target": "0 invention / 0 robotic",
        "fails": fails[:40],
        "best_human": [
            {"q": r["q"], "a": r["a"], "expect": r["expect"], "tag": r.get("tag")}
            for r in best[:3]
        ],
        "remaining_robotic_spots": soft_lab[:8],
        "closing_question_samples": closing_qs[:12],
        "patch": "talk.py: thanks/confused/why-matters; identity no count; explain skips fluff→last_clause; growth no transfer_accuracy dump",
        "results": [
            {"q": r["q"], "a": r["a"], "verdict": r["verdict"], "expect": r["expect"], "tag": r.get("tag")}
            for r in results
        ],
    }
    (RUNS / "keep-chat2.json").write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")
    return payload


def write_md(payload: dict) -> None:
    c = payload["counts"]
    lines = [
        "# Keep-chat2 — dialogue battery (human mouth)",
        "",
        f"- n asked: **{payload['n']}**",
        f"- ok: {c.get('ok', 0)} · unknown-ok: {c.get('unknown-ok', 0)} · fail: {c.get('fail', 0)}",
        f"- **invention-fail: {c.get('invention-fail', 0)}** · **robotic-fail: {c.get('robotic-fail', 0)}** (scan={payload.get('robotic_scan')})",
        f"- target: **0 / 0**",
        "",
        "## Mix",
        "- greets / cómo vas / gracias (no transfer_accuracy on greet)",
        "- identity: eres un robot / quién eres (no tribe stamp unless asked; no fact-count)",
        "- unit_protocell → en cristiano → protocélula biológica (UNKNOWN, no invention)",
        "- gracias → en cristiano must rephrase substance, not thanks",
        "- demuéstralo / y eso qué importa / no entiendo",
        "- lie after true still rejects",
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
        lines.append(f"- ({s.get('why')}) `{s['q']}` → {(s['a'] or '')[:120]}")
    if payload.get("fails"):
        lines.append("")
        lines.append("## Fails")
        for f in payload["fails"][:12]:
            lines.append(f"- [{f.get('verdict')}/{f['expect']}] `{f['q']}` tag={f.get('tag')}")
            lines.append(f"  - {(f['a'] or '')[:160].replace(chr(10), ' ')}")
    lines.append("")
    lines.append(f"## Patch: {payload.get('patch', 'none')}")
    lines.append("")
    (RUNS / "keep-chat2.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    p = run_grill()
    write_md(p)
    print(json.dumps({
        "n": p["n"],
        "counts": p["counts"],
        "fail_n": p["fail_n"],
        "invention_n": p["invention_n"],
        "robotic_n": p["robotic_n"],
        "robotic_scan": p["robotic_scan"],
    }, ensure_ascii=False, indent=2))
    if p["fails"]:
        print("FAILS:")
        for f in p["fails"][:25]:
            print(" ", f.get("verdict"), f["expect"], f["q"], "→", (f["a"] or "")[:140].replace("\n", " "))
