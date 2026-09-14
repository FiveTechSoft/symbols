"""Keep-chat8 — ≥30 human edge cases (incomplete / typo / cuéntame / da igual).

Spine edge cases:
- incomplete: «y fib…», «lo de ohm», «aquello de la unidad»
- typo-ish fold still ok: «fibonaci», «kepler»
- «cuéntame algo» without inventing (verified crumb or ask which)
- «da igual» as redirect/ack
- still 0 invention / 0 robotic

Patch: talk.py — unit deixis, tell-me offer/cite, da igual redirect.
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

STOCK_FAIL_PHRASES = (
    "Eso cuadra; punto",
    "eso cuadra; punto",
    "y cuadra.",
)

STOCK_WATCH = (
    "Nada más pretendo",
    "nada más pretendo",
    "Firmado; sin adorno",
    "así de corto, y cuadra",
)

FACT_COUNT_RE = re.compile(
    r"\b\d{2,4}\s+hechos\b|\bhechos firmados\b|\b\d{2,4}\s+cosas firmadas\b",
    re.I,
)

_LEVER_MARKERS = (
    r"circuito\s+de\s+bits",
    r"\bbits\b",
    r"conservaci[oó]n\s+en\s*[Δδ]=?\s*0",
    r"[Δδ]\s*=\s*0",
    r"puerta\s+de\s+forma",
    r"taxis\s+del\s+bucle",
    r"compa[nñ]ero\s+de\s+recurrencia",
    r"recurrencia\s+hermana",
    r"paso\s+a\s+delta",
    r"salto\s+a\s+delta",
)
_LEVER_RES = [re.compile(p, re.I) for p in _LEVER_MARKERS]

SHORT_UNIT_MUST = ("seis", "juntas", "paquete", "unidad")

# Honest «cuéntame algo»: ask-which OR cite a signed crumb — never invent.
TELL_ASK_MARKERS = ("¿de cuál", "de cual", "fibonacci, ohm", "todo firmado")
TELL_CITE_MARKERS = (
    "f(n)=",
    "fibonacci",
    "v=ir",
    "ohm",
    "kepler",
    "t²",
    "t^2",
    "unidad",
    "seis formas",
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
        or (
            al.strip().startswith("no ")
            and "no finjo" not in al
            and "no invento" not in al
        )
    )


def _has_stock_fail(text: str) -> str | None:
    al = text or ""
    for p in STOCK_FAIL_PHRASES:
        if p in al:
            return p
    if re.search(r",\s*y cuadra\.\s*$", al, re.I):
        return "y cuadra."
    if "Eso cuadra; punto" in al or "eso cuadra; punto" in al.lower():
        return "Eso cuadra; punto"
    return None


def _stock_watch_hits(text: str) -> list[str]:
    al = text or ""
    hits = []
    for p in STOCK_WATCH:
        if p in al or p.lower() in al.lower():
            if p not in hits and p.lower() not in [h.lower() for h in hits]:
                hits.append(p)
    return hits


def _lever_hit_count(text: str) -> int:
    return sum(1 for rx in _LEVER_RES if rx.search(text or ""))


def _is_long_unit_dump(text: str) -> bool:
    al = (text or "").lower()
    if "las seis formas viajan juntas" in al:
        return True
    if "circuito de bits" in al and "taxis" in al:
        return True
    if _lever_hit_count(text) >= 4:
        return True
    return False


def _is_unit_query(q: str) -> bool:
    ql = (q or "").strip().lower()
    return ql in (
        "unit",
        "unidad",
        "la unidad",
        "unit_protocell_levers",
        "protocell",
        "unit protocell",
        "aquello de la unidad",
        "lo de la unidad",
    ) or ql.startswith("unit_protocell") or "de la unidad" in ql


def _is_explain_query(q: str) -> bool:
    ql = (q or "").strip().lower()
    return any(
        x in ql
        for x in (
            "en cristiano",
            "explica",
            "explicame",
            "como a un amigo",
            "mas simple",
            "más simple",
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
    if _has_stock_fail(al):
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


def _tell_is_honest(text: str, tag: str | None = None) -> bool:
    """True if ask-which OR cites a signed crumb; never UNKNOWN invent-silence as only path."""
    if _is_unknown(text):
        return False
    al = (text or "").lower()
    if any(m in al for m in TELL_ASK_MARKERS):
        return True
    if any(m in al for m in TELL_CITE_MARKERS):
        return True
    if tag in ("offer", "verified", "rec-fib", "rec-lucas"):
        return True
    return False


def build_script() -> list[dict]:
    items: list[dict] = []

    def add(q, expect, **kw):
        items.append({"q": q, "expect": expect, **kw})

    # Warm-up
    add("hola", "greet", forbid_robotic=True, forbid_any=["hechos firmados", "transfer_accuracy"])
    add(
        "quién eres",
        "identity",
        forbid_robotic=True,
        forbid_any=["hechos firmados", "504", "471"],
        must_any=["Master"],
    )

    # --- Incomplete / deixis ---
    add(
        "y fib…",
        "known",
        forbid_robotic=True,
        must_any=["F(n)=", "Fibonacci", "suma", "anteriores"],
        edge="incomplete",
    )
    add(
        "lo de ohm",
        "known",
        forbid_robotic=True,
        must_any=["V=IR", "Ohm", "ohm"],
        edge="incomplete",
    )
    add(
        "aquello de la unidad",
        "known",
        forbid_robotic=True,
        forbid_biology=True,
        forbid_long_unit=True,
        must_any=list(SHORT_UNIT_MUST),
        forbid_any=["UNIT{", "Circuito de bits", "las seis formas viajan"],
        edge="incomplete",
    )
    add(
        "lo de la unidad",
        "known",
        forbid_robotic=True,
        forbid_long_unit=True,
        must_any=list(SHORT_UNIT_MUST),
        edge="incomplete",
    )
    add(
        "y fib...",
        "known",
        forbid_robotic=True,
        must_any=["F(n)=", "Fibonacci", "fib"],
        edge="incomplete",
    )

    # --- Typo-ish fold ---
    add(
        "fibonaci",
        "known",
        forbid_robotic=True,
        must_any=["F(n)=", "Fibonacci", "suma", "anteriores"],
        edge="typo",
    )
    add(
        "kepler",
        "known",
        forbid_robotic=True,
        must_any=["Kepler", "T²", "a³", "a^3", "periodo"],
        edge="typo",
    )

    # --- cuéntame algo (honest: ask or cite verified) ---
    add(
        "cuéntame algo",
        "tell",
        forbid_robotic=True,
        must_honest_tell=True,
        forbid_unknown=True,
        edge="tell",
    )
    add(
        "cuéntame algo",
        "tell",
        forbid_robotic=True,
        must_honest_tell=True,
        forbid_unknown=True,
        edge="tell",
    )
    add(
        "dime algo",
        "tell",
        forbid_robotic=True,
        must_honest_tell=True,
        forbid_unknown=True,
        edge="tell",
    )

    # --- da igual as redirect/ack ---
    add("da igual", "banter", forbid_robotic=True, must_ack=True, edge="redirect")
    add(
        "fibonacci",
        "known",
        forbid_robotic=True,
        must_any=["F(n)="],
    )
    add("me da igual", "banter", forbid_robotic=True, must_ack=True, edge="redirect")

    # Mix: ack / hold / known / reject / UNKNOWN (pad ≥30)
    add("ok", "banter", forbid_robotic=True, must_ack=True)
    add("ohm", "known", forbid_robotic=True, must_any=["V=IR", "Ohm"])
    add("espera", "banter", forbid_robotic=True, must_ack=True)
    add(
        "no, Fib es 2F(n-1)",
        "lie",
        must_reject=True,
        forbid_robotic=True,
    )
    add("mm", "banter", forbid_robotic=True, must_ack=True)
    add(
        "eres un robot?",
        "identity",
        forbid_robotic=True,
        must_any=["programa", "Master", "robot"],
    )
    add("vale", "banter", forbid_robotic=True, must_ack=True)
    add("lucas", "known", must_any=["L(n)=", "Lucas", "misma"])
    add("para", "banter", forbid_robotic=True, must_ack=True)
    add("dale", "banter", forbid_robotic=True, must_ack=True)
    add("entiendo", "banter", forbid_robotic=True, must_ack=True)
    add("multiverso", "unknown", forbid_robotic=True)
    add("cassini", "unknown", forbid_robotic=True)
    add(
        "unit",
        "known",
        forbid_robotic=True,
        forbid_long_unit=True,
        must_any=list(SHORT_UNIT_MUST),
    )
    add(
        "en cristiano",
        "known",
        forbid_robotic=True,
        must_any=["seis", "piezas", "paquete", "juntas"],
        must_list_six=True,
    )
    add("ya veo", "banter", forbid_robotic=True, must_ack=True)
    add("sí", "banter", forbid_robotic=True, must_ack=True)
    add("mejor otra cosa", "banter", forbid_robotic=True, must_ack=True)
    add(
        "y la protocélula biológica?",
        "unknown",
        no_biology_invention=True,
        forbid_robotic=True,
    )
    add(
        "gracias",
        "thanks",
        forbid_robotic=True,
        must_any=["nada", "gusto", "Dale", "aquí", "Aqui", "seguimos"],
    )

    assert len(items) >= 30, len(items)
    return items


def grade(item: dict, a: str, tag: str | None = None, st: dict | None = None) -> str:
    expect = item["expect"]
    al = (a or "").lower()
    q = item.get("q") or ""

    stock = _has_stock_fail(a or "")
    if stock:
        return "robotic-fail"
    if "UNIT{" in (a or ""):
        return "robotic-fail"
    if expect == "greet" and FACT_COUNT_RE.search(a or ""):
        return "robotic-fail"

    if item.get("forbid_long_unit") or (
        _is_unit_query(q) and not _is_explain_query(q)
    ):
        if _is_long_unit_dump(a or ""):
            return "robotic-fail"

    if item.get("forbid_robotic") or expect in (
        "greet", "identity", "growth", "thanks", "level", "banter", "tell",
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
            if "UNIT{" in f or "circuito" in f.lower() or "viajan" in f.lower():
                return "robotic-fail"
            return "fail"

    if item.get("forbid_unknown") and _is_unknown(a):
        return "fail"

    if expect == "tell":
        if _invents_biology(a):
            return "invention-fail"
        if _is_robotic(a, "tell"):
            return "robotic-fail"
        if _is_unknown(a):
            # Silence here would dodge the ask — fail honesty contract
            return "fail"
        if item.get("must_honest_tell") and not _tell_is_honest(a, tag):
            return "invention-fail"
        return "ok"

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
            must = item["must_any"]
            if not any(m.lower() in al or m in (a or "") for m in must):
                if "robot" in (item.get("q") or "").lower():
                    if "master" in al or "programa" in al:
                        return "ok"
                return "fail"
        return "ok"

    if expect == "thanks":
        if _is_unknown(a):
            return "fail"
        if item.get("must_any") and not any(
            m.lower() in al or m in (a or "") for m in item["must_any"]
        ):
            return "fail"
        return "ok"

    if _is_unknown(a):
        return "fail"
    if item.get("must_reject") and not _is_reject(a):
        return "fail"
    if item.get("must_any"):
        if not any((m.lower() in al or m in (a or "")) for m in item["must_any"]):
            return "fail"
    if item.get("must_list_six"):
        if _lever_hit_count(a or "") < 3 and not (
            "bits" in al and ("taxis" in al or "delta" in al or "Δ" in (a or ""))
        ):
            return "fail"
    return "ok"


def run_grill(round_name: str = "keep-chat8") -> dict:
    kb = _load_theory(THEORY)
    items = build_script()
    results = []
    counts = {
        "ok": 0,
        "fail": 0,
        "unknown-ok": 0,
        "robotic-fail": 0,
        "invention-fail": 0,
    }
    fails: list[dict] = []
    st: dict = {}
    stiff: list[dict] = []
    stock_hits: list[dict] = []

    for item in items:
        text, tag, st = answer(item["q"], kb, st)
        verdict = grade(item, text, tag, st)
        row = {
            "q": item["q"],
            "a": text,
            "verdict": verdict,
            "expect": item["expect"],
            "tag": tag,
            "edge": item.get("edge"),
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
                    "edge": item.get("edge"),
                }
            )

        al = text or ""
        low = al.lower()
        sf = _has_stock_fail(al)
        if sf:
            stiff.append({"q": item["q"], "a": al[:220], "why": f"stock-fail:{sf}"})
            stock_hits.append({"q": item["q"], "phrase": sf, "a": al[:160]})
        for w in _stock_watch_hits(al):
            stock_hits.append({"q": item["q"], "phrase": w, "a": al[:160]})
            stiff.append({"q": item["q"], "a": al[:220], "why": f"stock-watch:{w}"})

        if item.get("forbid_long_unit") and _is_long_unit_dump(al):
            stiff.append({"q": item["q"], "a": al[:220], "why": "long-unit-dump"})
        elif item["expect"] == "banter" and _is_unknown(al):
            stiff.append({"q": item["q"], "a": al[:220], "why": "banter-unknown"})
        elif item.get("must_ack") and tag != "ack":
            stiff.append({"q": item["q"], "a": al[:220], "why": f"ack-tag={tag}"})
        elif _is_robotic(al, item["expect"]):
            stiff.append({"q": item["q"], "a": al[:220], "why": "marker"})
        elif al.count("¿") >= 2:
            stiff.append({"q": item["q"], "a": al[:220], "why": "multi-closing-q"})

    invention_n = counts.get("invention-fail", 0)
    robotic_n = counts.get("robotic-fail", 0)
    robotic_scan = sum(1 for r in results if _is_robotic(r["a"] or "", r["expect"]))
    long_unit_n = sum(
        1
        for r, it in zip(results, items)
        if (it.get("forbid_long_unit") or _is_unit_query(it["q"]))
        and not _is_explain_query(it["q"])
        and _is_long_unit_dump(r["a"] or "")
    )

    # cuéntame algo honesty report
    tell_rows = [r for r in results if r.get("expect") == "tell" or r.get("edge") == "tell"]
    tell_honest = all(_tell_is_honest(r["a"], r.get("tag")) for r in tell_rows) if tell_rows else False
    tell_any_invent = any(
        r["verdict"] == "invention-fail" or (_invents_biology(r["a"] or "") and not _is_unknown(r["a"] or ""))
        for r in tell_rows
    )
    tell_modes = []
    for r in tell_rows:
        al = (r["a"] or "").lower()
        if any(m in al for m in TELL_ASK_MARKERS):
            tell_modes.append("ask")
        elif any(m in al for m in TELL_CITE_MARKERS):
            tell_modes.append("cite")
        else:
            tell_modes.append("other")

    seen_phrases = set()
    remaining_stock = []
    for h in stock_hits:
        key = (h["phrase"].lower(), h["q"])
        if key in seen_phrases:
            continue
        seen_phrases.add(key)
        remaining_stock.append(h)

    humanish = [
        r
        for r in results
        if r["verdict"] in ("ok", "unknown-ok")
        and not _is_robotic(r["a"] or "", r["expect"])
        and r["expect"]
        in ("known", "identity", "greet", "level", "lie", "lie_or_reject", "thanks", "tell")
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

    ack_report = {}
    for word in (
        "ok",
        "sí",
        "vale",
        "dale",
        "no",
        "mm",
        "ya veo",
        "entiendo",
        "espera",
        "para",
        "mejor otra cosa",
        "da igual",
        "me da igual",
    ):
        rows = [
            r
            for r in results
            if r["q"].strip().lower().rstrip("!.") in {word, word.replace("sí", "si")}
        ]
        if not rows and word == "sí":
            rows = [r for r in results if r["q"].strip() in ("sí", "si")]
        ack_report[word] = {
            "n": len(rows),
            "all_ack_tag": all(r.get("tag") == "ack" for r in rows) if rows else None,
            "any_unknown": any(_is_unknown(r["a"] or "") for r in rows) if rows else None,
            "clean": (
                all(r.get("tag") == "ack" and not _is_unknown(r["a"] or "") for r in rows)
                if rows
                else None
            ),
        }

    edge_report = {}
    for edge in ("incomplete", "typo", "tell", "redirect"):
        rows = [r for r in results if r.get("edge") == edge]
        edge_report[edge] = {
            "n": len(rows),
            "ok": sum(1 for r in rows if r["verdict"] in ("ok", "unknown-ok")),
            "fail": sum(1 for r in rows if r["verdict"] not in ("ok", "unknown-ok")),
            "samples": [
                {"q": r["q"], "a": (r["a"] or "")[:180], "tag": r.get("tag"), "verdict": r["verdict"]}
                for r in rows[:4]
            ],
        }

    stiffest = None
    if stiff:
        stiffest = stiff[0]
    elif fails:
        stiffest = {"q": fails[0]["q"], "a": fails[0]["a"], "why": fails[0]["verdict"]}
    else:
        scored = []
        for r in results:
            if r["verdict"] not in ("ok", "unknown-ok"):
                continue
            a = r["a"] or ""
            low = a.lower()
            score = 0
            why = None
            if "nada más pretendo" in low or "eso cuadra; punto" in low or "y cuadra." in low:
                score, why = 4, "stock-closer"
            elif _is_long_unit_dump(a) and not _is_explain_query(r["q"]):
                score, why = 3, "long-unit-default"
            elif a.count("¿") >= 1 and r["expect"] not in ("greet", "tell"):
                score, why = 1, "closing-q"
            if score:
                scored.append((score, len(a), why, r))
        if scored:
            scored.sort(key=lambda t: (-t[0], -t[1]))
            _sc, _ln, why, c = scored[0]
            stiffest = {"q": c["q"], "a": (c["a"] or "")[:220], "why": why}

    unit_samples = []
    for r in results:
        if _is_unit_query(r["q"]) or _is_explain_query(r["q"]):
            unit_samples.append(
                {
                    "q": r["q"],
                    "a": r["a"],
                    "verdict": r["verdict"],
                    "long_dump": _is_long_unit_dump(r["a"] or ""),
                    "lever_hits": _lever_hit_count(r["a"] or ""),
                }
            )

    patch = (
        "talk.py: «aquello/lo de la unidad» → short unit; "
        "«cuéntame algo» ask-or-cite verified; «da igual» redirect/ack"
    )

    payload = {
        "round": round_name,
        "n": len(results),
        "counts": counts,
        "fail_n": sum(v for k, v in counts.items() if k not in ("ok", "unknown-ok")),
        "invention_n": invention_n,
        "robotic_n": robotic_n,
        "robotic_scan": robotic_scan,
        "long_unit_dump_n": long_unit_n,
        "target": "0 invention / 0 robotic",
        "score": f"{invention_n}/{robotic_n}",
        "cuentame_algo_honest": bool(tell_honest and not tell_any_invent),
        "cuentame_modes": tell_modes,
        "cuentame_samples": [
            {"q": r["q"], "a": r["a"], "tag": r.get("tag"), "verdict": r["verdict"]}
            for r in tell_rows
        ],
        "edge_report": edge_report,
        "ack_clean": ack_report,
        "fails": fails[:40],
        "unit_samples": unit_samples,
        "best_human": [
            {"q": r["q"], "a": r["a"], "expect": r["expect"], "tag": r.get("tag")}
            for r in best[:3]
        ],
        "remaining_robotic_spots": stiff[:10],
        "remaining_stock_phrases": remaining_stock,
        "stiffest": stiffest,
        "patch": patch,
        "results": [
            {
                "q": r["q"],
                "a": r["a"],
                "verdict": r["verdict"],
                "expect": r["expect"],
                "tag": r.get("tag"),
                "edge": r.get("edge"),
            }
            for r in results
        ],
    }
    (RUNS / "keep-chat8.json").write_text(
        json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    return payload


def write_md(payload: dict) -> None:
    c = payload["counts"]
    ack = payload.get("ack_clean") or {}
    edges = payload.get("edge_report") or {}
    lines = [
        "# Keep-chat8 — ≥30 human edge cases (incomplete / typo / cuéntame / da igual)",
        "",
        f"- n asked: **{payload['n']}**",
        f"- ok: {c.get('ok', 0)} · unknown-ok: {c.get('unknown-ok', 0)} · fail: {c.get('fail', 0)}",
        f"- **invention-fail: {c.get('invention-fail', 0)}** · **robotic-fail: {c.get('robotic-fail', 0)}** (scan={payload.get('robotic_scan')})",
        f"- long-unit-dump: **{payload.get('long_unit_dump_n', 0)}**",
        f"- score: **{payload.get('score')}** (invention/robotic) · target **0 / 0**",
        f"- **cuéntame algo honest: {payload.get('cuentame_algo_honest')}** · modes={payload.get('cuentame_modes')}",
        "",
        "## Edge buckets",
    ]
    for name, info in edges.items():
        lines.append(
            f"- `{name}`: n={info.get('n')} ok={info.get('ok')} fail={info.get('fail')}"
        )
        for s in info.get("samples") or []:
            lines.append(
                f"  - `{s['q']}` [{s.get('verdict')}/{s.get('tag')}] {(s.get('a') or '')[:140]}"
            )
    lines += [
        "",
        "## cuéntame algo (must ask-which OR cite verified; 0 invent)",
    ]
    for s in payload.get("cuentame_samples") or []:
        lines.append(
            f"- `{s['q']}` tag={s.get('tag')} verdict={s.get('verdict')}"
        )
        lines.append(f"  - {(s.get('a') or '')[:200].replace(chr(10), ' ')}")
    lines += [
        "",
        "## Ack / hold / redirect cleanliness",
    ]
    for w, info in ack.items():
        lines.append(
            f"- `{w}`: n={info.get('n')} clean={info.get('clean')} "
            f"all_ack_tag={info.get('all_ack_tag')} any_unknown={info.get('any_unknown')}"
        )
    lines += [
        "",
        "## Unit samples",
    ]
    for u in payload.get("unit_samples") or []:
        lines.append(
            f"- `{u['q']}` verdict={u['verdict']} long_dump={u['long_dump']} levers={u['lever_hits']}"
        )
        lines.append(f"  - {(u.get('a') or '')[:200].replace(chr(10), ' ')}")
    lines += [
        "",
        "## Hard fails (must be 0)",
        "- `UNIT{` / long default unit dump / «Eso cuadra; punto» / «y cuadra.» / invent on cuéntame / fact-count on greet",
        "",
        "## Best 3 human replies",
    ]
    for i, ex in enumerate(payload.get("best_human") or [], 1):
        lines.append(f"{i}. `{ex['q']}`")
        lines.append(f"   - {(ex['a'] or '').replace(chr(10), ' ')}")
    lines.append("")
    lines.append("## Stiffest remaining exchange")
    stf = payload.get("stiffest")
    if stf:
        lines.append(
            f"- ({stf.get('why')}) `{stf.get('q')}` → {(stf.get('a') or '')[:180]}"
        )
    else:
        lines.append("- (none flagged)")
    lines.append("")
    lines.append("## Remaining stock phrases")
    stocks = payload.get("remaining_stock_phrases") or []
    if not stocks:
        lines.append("- (none)")
    for s in stocks:
        lines.append(
            f"- `{s.get('phrase')}` on `{s.get('q')}` → {(s.get('a') or '')[:120]}"
        )
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
            lines.append(
                f"- [{f.get('verdict')}/{f['expect']}] `{f['q']}` tag={f.get('tag')} edge={f.get('edge')}"
            )
            lines.append(f"  - {(f['a'] or '')[:160].replace(chr(10), ' ')}")
    lines.append("")
    lines.append(f"## Patch: {payload.get('patch', 'none')}")
    lines.append("")
    (RUNS / "keep-chat8.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


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
                "score": p.get("score"),
                "cuentame_algo_honest": p.get("cuentame_algo_honest"),
                "cuentame_modes": p.get("cuentame_modes"),
                "long_unit_dump_n": p.get("long_unit_dump_n"),
                "stiff_n": len(p.get("remaining_robotic_spots") or []),
                "stiffest": p.get("stiffest"),
                "edge_ok": {
                    k: {"n": v["n"], "ok": v["ok"], "fail": v["fail"]}
                    for k, v in (p.get("edge_report") or {}).items()
                },
                "ack_clean": {
                    k: v.get("clean") for k, v in (p.get("ack_clean") or {}).items()
                },
                "patch": p.get("patch"),
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
