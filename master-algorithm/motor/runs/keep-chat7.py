"""Keep-chat7 — ≥25-turn free dialogue after short unit default.

Spine: unit → en cristiano → ok → Fib → espera → Ohm → contradiction → mm → robot → UNKNOWN

Unit default must be short («seis formas que solo valen juntas…»).
Explain path still lists the six.
Fail if UNIT{ or long default unit dump (lists all six levers without en cristiano).
Score axes: invention-fail, robotic-fail. Target 0/0.
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

# Long default unit dump: names the six levers in one breath (old prose).
# Allowed only after explicit explain («en cristiano» / explica).
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
    al = text or ""
    return sum(1 for rx in _LEVER_RES if rx.search(al))


def _is_long_unit_dump(text: str) -> bool:
    """True if reply lists the six levers (old long default prose)."""
    al = (text or "").lower()
    # Classic long opener
    if "las seis formas viajan juntas" in al:
        return True
    if "circuito de bits" in al and "taxis" in al:
        return True
    # ≥4 distinct lever crumbs in one reply = dump
    if _lever_hit_count(text) >= 4:
        return True
    return False


def _is_unit_query(q: str) -> bool:
    ql = (q or "").strip().lower()
    return ql in (
        "unit",
        "unit_protocell_levers",
        "protocell",
        "unit protocell",
    ) or ql.startswith("unit_protocell")


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

    # Spine: unit → en cristiano → ok → Fib → espera → Ohm → contradiction → mm → robot → UNKNOWN
    add(
        "unit",
        "known",
        forbid_robotic=True,
        forbid_biology=True,
        forbid_long_unit=True,
        must_any=list(SHORT_UNIT_MUST),
        forbid_any=["UNIT{", "Circuito de bits", "las seis formas viajan"],
    )
    add(
        "en cristiano",
        "known",
        forbid_robotic=True,
        must_any=["seis", "piezas", "paquete", "juntas"],
        must_list_six=True,
    )
    add("ok", "banter", forbid_robotic=True, must_ack=True)
    add(
        "fibonacci",
        "known",
        forbid_robotic=True,
        must_any=["F(n)=", "Fibonacci", "suma", "anteriores"],
    )
    add("espera", "banter", forbid_robotic=True, must_ack=True)
    add("ohm", "known", forbid_robotic=True, must_any=["V=IR", "Ohm", "ohm"])
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
        forbid_any=["hechos firmados", "UNIT{"],
    )
    add("multiverso", "unknown", forbid_robotic=True)

    # Pad to ≥25 — ack / hold / redirect / UNKNOWN / soft Fib / level
    add("vale", "banter", forbid_robotic=True, must_ack=True)
    add("lucas", "known", must_any=["L(n)=", "Lucas", "misma"])
    add("para", "banter", forbid_robotic=True, must_ack=True)
    add("dale", "banter", forbid_robotic=True, must_ack=True)
    add("entiendo", "banter", forbid_robotic=True, must_ack=True)
    add("fibonacci", "known", must_any=["F(n)="], forbid_robotic=True)
    add("mejor otra cosa", "banter", forbid_robotic=True, must_ack=True)
    add("cassini", "unknown", forbid_robotic=True)
    add(
        "unit_protocell_levers",
        "known",
        forbid_robotic=True,
        forbid_biology=True,
        forbid_long_unit=True,
        must_any=list(SHORT_UNIT_MUST),
        forbid_any=["UNIT{", "Circuito de bits"],
    )
    add("ya veo", "banter", forbid_robotic=True, must_ack=True)
    add(
        "eres más listo porque tienes 471 hechos?",
        "level",
        forbid_robotic=True,
        must_any=["No", "palanca", "unidad", "inventario"],
        forbid_any=["UNIT{", "held-out", "ley verificada", "Contrastamos"],
    )
    add("sí", "banter", forbid_robotic=True, must_ack=True)
    add("no me interesa", "banter", forbid_robotic=True, must_ack=True)
    add("y la protocélula biológica?", "unknown", no_biology_invention=True, forbid_robotic=True)
    add(
        "gracias",
        "thanks",
        forbid_robotic=True,
        must_any=["nada", "gusto", "Dale", "aquí", "Aqui", "seguimos"],
    )

    assert len(items) >= 25, len(items)
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

    # Long default unit dump without explain ask → robotic-fail
    if item.get("forbid_long_unit") or (
        _is_unit_query(q) and not _is_explain_query(q)
    ):
        if _is_long_unit_dump(a or ""):
            return "robotic-fail"

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
            if "UNIT{" in f or "circuito" in f.lower() or "viajan" in f.lower():
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

    if expect == "level":
        if "más listo" in al and "no" not in al[:40]:
            return "fail"
        if item.get("must_any") and not any(m.lower() in al for m in item["must_any"]):
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
        # explain path must still name several levers
        if _lever_hit_count(a or "") < 3 and not (
            "bits" in al and ("taxis" in al or "delta" in al or "Δ" in (a or ""))
        ):
            return "fail"
    return "ok"


def run_grill(round_name: str = "keep-chat7") -> dict:
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
        elif "nada más. nada más" in low:
            stiff.append({"q": item["q"], "a": al[:220], "why": "echo-closer"})
        elif item["expect"] == "known" and len(al) < 25 and not _is_unknown(al):
            stiff.append({"q": item["q"], "a": al[:220], "why": "too-short-known"})
        elif item["expect"] == "lie" and not _is_reject(al) and not _is_unknown(al):
            stiff.append({"q": item["q"], "a": al[:220], "why": "soft-reject"})

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
        in ("known", "identity", "greet", "level", "lie", "lie_or_reject", "thanks")
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
        "no me interesa",
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
            elif a.count("¿") >= 1 and r["expect"] not in ("greet",):
                score, why = 1, "closing-q"
            if score:
                scored.append((score, len(a), why, r))
        if scored:
            scored.sort(key=lambda t: (-t[0], -t[1]))
            _sc, _ln, why, c = scored[0]
            stiffest = {"q": c["q"], "a": (c["a"] or "")[:220], "why": why}

    # Capture unit default + explain samples for the report
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
        "unit_default_short": "Es la unidad: seis formas que solo valen juntas…",
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
        "patch": "none (unit default already short; explain still lists six)",
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
    (RUNS / "keep-chat7.json").write_text(
        json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    return payload


def write_md(payload: dict) -> None:
    c = payload["counts"]
    ack = payload.get("ack_clean") or {}
    lines = [
        "# Keep-chat7 — ≥25-turn free dialogue (short unit default)",
        "",
        f"- n asked: **{payload['n']}**",
        f"- ok: {c.get('ok', 0)} · unknown-ok: {c.get('unknown-ok', 0)} · fail: {c.get('fail', 0)}",
        f"- **invention-fail: {c.get('invention-fail', 0)}** · **robotic-fail: {c.get('robotic-fail', 0)}** (scan={payload.get('robotic_scan')})",
        f"- long-unit-dump: **{payload.get('long_unit_dump_n', 0)}**",
        f"- target: **0 / 0**",
        "",
        "## Unit prose",
        f"- default short: `{payload.get('unit_default_short')}`",
        "- explain («en cristiano») still lists the six levers",
        "- hard fail: `UNIT{` or long default dump (six levers without ask)",
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
        "## Mix (spine)",
        "- unit → en cristiano → ok → Fib → espera → Ohm → contradiction (2F) → mm → robot → UNKNOWN",
        "- pad: vale/lucas/para/dale/entiendo · Fib → mejor otra cosa · cassini UNKNOWN",
        "- unit again (short) → ya veo · 471-boast → sí · no me interesa · bio UNKNOWN → gracias",
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
        "- `UNIT{` / long default unit dump / «Eso cuadra; punto» / «y cuadra.» / fact-count on greet",
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
                f"- [{f.get('verdict')}/{f['expect']}] `{f['q']}` tag={f.get('tag')}"
            )
            lines.append(f"  - {(f['a'] or '')[:160].replace(chr(10), ' ')}")
    lines.append("")
    lines.append(f"## Patch: {payload.get('patch', 'none')}")
    lines.append("")
    (RUNS / "keep-chat7.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


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
                "long_unit_dump_n": p.get("long_unit_dump_n"),
                "stiff_n": len(p.get("remaining_robotic_spots") or []),
                "stiffest": p.get("stiffest"),
                "remaining_stock_phrases": p.get("remaining_stock_phrases"),
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
    if p.get("remaining_robotic_spots"):
        print("STIFF:")
        for s in p["remaining_robotic_spots"][:8]:
            print(
                " ",
                s.get("why"),
                s["q"],
                "→",
                (s["a"] or "")[:100].replace("\n", " "),
            )
