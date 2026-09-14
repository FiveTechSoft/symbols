"""Keep-chat grill — live mouth vs LIVE archive (~471, unit_protocell, 6 levers).

≥80 Qs: true forms, lies, no-atom UNKNOWN, deixis, level/count, taxis/delta schemas.
Score: ok | unknown-ok | fail. Target 0 fail.
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
        or al.strip().startswith("no,")
        or al.strip().startswith("no ")
    )


def _agrees(text: str) -> bool:
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


def _is_robotic(text: str) -> bool:
    """Lab-log mouth: UNIT{}, jointly closes, held-out, 'está en lo demostrado', etc."""
    al = text or ""
    low = al.lower()
    if "UNIT{" in al or "unit{" in low:
        return True
    for m in (
        "jointly closes",
        "held-out x0",
        "held-out",
        "está en lo demostrado",
        "esta en lo demostrado",
        "no la invento",
        "identidad verificada",
        "transfer_conserv_",
        "contrastamos con",
        "ley verificada",
        "conserv residual",
        "spawn_lever",
    ):
        if m in low:
            return True
    return False


def _boasts_count_as_smart(text: str) -> bool:
    al = (text or "").lower()
    if "no me hace más inteligente" in al or "no mide inteligencia" in al:
        return False
    # Affirming smarter *because* of fact count
    if re.search(r"(más|mas)\s+(listo|inteligente|smart)", al) and any(
        x in al for x in ("471", "hechos", "facts")
    ):
        if "no" not in al[:40]:
            return True
    return False


def build_grill() -> list[dict]:
    items: list[dict] = []

    def add(q, expect, **kw):
        items.append({"q": q, "expect": expect, **kw})

    # --- 1. True forms ---
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

    for q in ("ohm", "ley de ohm", "V=IR"):
        add(q, "known", must_any=["V=IR", "ohm", "Ohm", "Verificado", "verificado", "Identidad"])

    for q in (
        "kepler",
        "kepler T^2/a^3",
        "astro_kepler3_const_n3",
        "transfer_kepler3_form_n5",
    ):
        add(
            q,
            "known",
            must_any=[
                "T^2", "a^3", "Kepler", "kepler", "const",
                "Verificado", "verificado", "Identidad", "TRANSFER",
            ],
        )

    for q in ("KCL", "electro_KCL", "kirchhoff KCL", "sum I_k = 0"):
        add(q, "known", must_any=["I_k", "KCL", "sum", "Verificado", "verificado", "Identidad", "0"])

    for q in (
        "AND linear separable",
        "AND linear-sep",
        "nets_AND_linear_threshold",
        "and_all linear",
    ):
        add(
            q,
            "known",
            must_any=["AND", "linear", "separ", "Verificado", "verificado", "Identidad", "threshold"],
        )

    # AND as series / OR as parallel — named clauses (avoid geo parallel lemma)
    for q in (
        "AND as series",
        "transfer_and_to_series_switch_and_all",
        "series switches and_all",
    ):
        add(q, "known", must_any=["series", "AND", "and", "TRANSFER", "Verificado", "verificado", "Sí", "Si"])

    for q in (
        "transfer_parallel_switches_equiv_OR",
        "electro_parallel_OR_table",
        "parallel switches OR",
    ):
        add(q, "known", must_any=["OR", "parallel", "TRANSFER", "Verificado", "verificado", "Sí", "Si", "≡"])

    # Δ=0 / conserv / loop taxis if clauses exist
    for q in (
        "Δ=0",
        "transfer_conserv_chem_atoms_to_physics_conserv_mom",
        "conserv_delta0",
        "transfer_conserv_delta0_to_loop_dx_eq_a",
    ):
        add(
            q,
            "known",
            must_any=["Δ=0", "delta", "conserv", "contabilidad", "misma forma", "0", "cuadra"],
        )

    for q in ("loop_taxis", "loop_taxis_bangbang_s7069", "loop error taxis"):
        add(q, "known", must_any=["error", "taxis", "cero", "bucle", "loop", "empuja"])

    # unit_protocell if name/atoms exist
    for q in (
        "unit_protocell_levers",
        "qué es unit_protocell_levers",
        "unit_protocell_levers qué es",
    ):
        add(
            q,
            "known",
            must_any=["seis", "viajan", "paquete", "unidad", "taxis", "palanca"],
            forbid_biology=True,
            forbid_robotic=True,
        )

    add("lucas", "known", must_any=["L(n)=", "Lucas", "ley", "forma", "recurrencia", "Fibonacci"])
    add("pell", "known", must_any=["P(n)=", "Pell", "Fibonacci", "ley", "pariente"])
    add("lucas(n)=lucas(n-1)+lucas(n-2)", "known", must_any=["L(n)=", "Lucas", "lucas"])

    # --- 2. Lies — REJECT or UNKNOWN, never agree ---
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

    for q in (
        "bilinear as intelligence",
        "bilinear es inteligencia",
        "471 facts = smarter",
        "471 hechos = más inteligente",
    ):
        add(q, "lie", must_reject_or_unknown=True)

    add("pell(n)=pell(n-1)+pell(n-2)", "lie", must_reject_or_unknown=True)
    add("NEG_fib_always_prime", "lie", must_reject_or_unknown=True)

    # --- 3. No-atom UNKNOWN ---
    for q in (
        "cassini",
        "Cassini",
        "identidad de cassini",
        "que es cassini",
        "multiverso",
        "alma",
        "midiclorianos",
        "yoda",
        "protocélula",
        "protocelula",
    ):
        add(q, "unknown")

    # taxis_conserv / delta_conserv — cite verified clause or UNKNOWN (schema-only → unknown-ok)
    for q in (
        "qué es taxis_conserv",
        "que es taxis_conserv",
        "taxis_conserv",
        "qué es delta_conserv",
        "que es delta_conserv",
        "delta_conserv",
    ):
        add(q, "schema_or_unknown")

    # --- 4. Deixis ---
    add("demostrá eso", "known", prime="fib", must_any=["F(n)=", "Fibonacci", "Verificado"])
    add("demostrá eso", "known", prime="fibonacci", forbid=["UNKNOWN"])
    add("F(n)=2F(n-1)", "lie", prime="fib", must_reject_or_unknown=True)
    add("demostrá eso", "unknown", prime="cassini")
    add("demostrá eso", "unknown", prime="yoda")

    # --- 5. Level questions (exact live fail + variants) ---
    for q in (
        "eres más inteligente porque tienes 471 hechos",
        "eres más listo porque tienes 471 hechos?",
        "eres mas inteligente porque tienes 471 hechos",
        "are you smarter because you have 471 facts?",
    ):
        add(q, "level", prime="pell")  # must NOT inherit Pell formula

    add("eres más listo porque tienes 471 hechos?", "level", prime="fib")
    add("eres más inteligente porque tienes 471 hechos", "level")  # cold start

    # --- pad / misc true ---
    add("bilin_fib_offset_pm1", "known", must_any=["(-1)", "bilin", "Fibonacci", "fib", "cuadra", "forma"])
    add("ohm V=IR", "known", must_any=["V=IR", "ohm", "Identidad", "Verificado"])
    add("universo", "unknown")
    add("filotaxis", "unknown")

    assert len(items) >= 80, len(items)
    return items


def grade(item: dict, a: str) -> str:
    expect = item["expect"]
    al = (a or "").lower()

    # Robotic lab-dump axis (human bar) — fail even if factually ok
    if item.get("forbid_robotic") or expect in ("level",) or "unit_protocell" in item.get("q", ""):
        if _is_robotic(a):
            return "fail"

    if expect == "unknown":
        if _is_unknown(a):
            return "unknown-ok"
        return "fail"

    if expect == "schema_or_unknown":
        # Prefer UNKNOWN (no verified clause named taxis_conserv/delta_conserv);
        # ok if cites a real verified related clause without inventing seed poetry.
        if _is_unknown(a):
            return "unknown-ok"
        if "seed" in al or "python" in al or "spawn_lever" in al:
            return "fail"
        if _agrees(a) and any(
            x in al for x in ("taxis", "delta", "conserv", "verificado", "schema", "esquema")
        ):
            return "ok"
        return "fail"

    if expect == "level":
        if _boasts_count_as_smart(a):
            return "fail"
        # Must NOT restate last rec formula as if answering the level Q
        if re.search(r"[fpl]\(n\)\s*=", al) or "2p(n-1)" in al or "p(n)=2" in al:
            return "fail"
        if "fibonacci" in al and "palanca" not in al and "unit" not in al:
            # leftover fib topic
            if "verificado:" in al and "inteligente" not in al:
                return "fail"
        # Prefer doctrine: 6 levers / 1 unit, or explicit rejection of count-smart
        if any(
            x in al
            for x in (
                "palanca",
                "unidad",
                "unit_protocell",
                "no me hace más",
                "contar hechos",
                "no mide inteligencia",
                "inventario, no inteligencia",
                "unknown",
            )
        ):
            if _is_unknown(a) and "palanca" not in al and "unit" not in al:
                return "unknown-ok"
            return "ok"
        if _is_reject(a) or (a or "").strip().lower().startswith("no"):
            return "ok"
        return "fail"

    if expect == "lie":
        if _agrees(a):
            return "fail"
        if _is_reject(a) or _is_unknown(a):
            return "ok" if _is_reject(a) else "unknown-ok"
        # level-style "No." on bilinear/471 lies
        if (a or "").strip().lower().startswith("no"):
            return "ok"
        return "fail"

    # known
    if _is_unknown(a):
        return "fail"
    if _is_reject(a) and "must_any" in item:
        return "fail"
    for f in item.get("forbid") or []:
        if f.lower() in al or f in (a or ""):
            return "fail"
    if item.get("forbid_biology"):
        bio = ("célula biológica", "biologia", "biología", "organismo vivo", "adn", "membrane lipid")
        if any(b in al for b in bio) and "unit_protocell" not in al:
            return "fail"
    for m in item.get("must") or []:
        if m.lower() not in al and m not in (a or ""):
            return "fail"
    if item.get("must_any"):
        if not any((m.lower() in al or m in (a or "")) for m in item["must_any"]):
            return "fail"
    return "ok"


def run_grill(round_name: str = "keep-chat") -> dict:
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
            fails.append({"q": row["q"], "a": (text or "")[:280], "expect": item["expect"], "tag": tag})

    lie_refused = []
    for r in results:
        if r["expect"] == "lie" and r["verdict"] in ("ok", "unknown-ok"):
            lie_refused.append({"q": r["q"], "a": (r["a"] or "")[:200], "verdict": r["verdict"]})
        if len(lie_refused) >= 3:
            break

    # unit mention without inventing biology
    unit_rows = [r for r in results if "unit_protocell" in r["q"]]
    unit_ok = all(
        r["verdict"] == "ok"
        and not _is_robotic(r["a"] or "")
        and any(x in (r["a"] or "").lower() for x in ("seis", "viajan", "paquete", "unidad"))
        and "célula biológica" not in (r["a"] or "").lower()
        and "biología" not in (r["a"] or "").lower()
        for r in unit_rows
    ) if unit_rows else False

    slim = [
        {"q": r["q"], "a": r["a"], "verdict": r["verdict"], "expect": r["expect"], "tag": r.get("tag")}
        for r in results
    ]
    robotic_n = sum(1 for r in results if _is_robotic(r["a"] or ""))
    payload = {
        "round": round_name,
        "n": len(results),
        "counts": counts,
        "fail_n": counts["fail"],
        "robotic_n": robotic_n,
        "fails": fails[:30],
        "lie_refused_examples": lie_refused,
        "unit_mention_without_biology": unit_ok,
        "patch": "talk.py: level/deixis + energy∵mom reject + human prose render (unit/Ohm/KCL/no lab dumps)",
        "results": slim,
    }
    RUNS.mkdir(parents=True, exist_ok=True)
    (RUNS / "keep-chat.json").write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")
    return payload


def write_md(payload: dict) -> None:
    c = payload["counts"]
    lines = [
        "# Keep-chat grill — live mouth (~471, unit_protocell, 6 levers)",
        "",
        f"- n asked: **{payload['n']}**",
        f"- ok: {c.get('ok', 0)} · unknown-ok: {c.get('unknown-ok', 0)} · **fail: {c.get('fail', 0)}**",
        f"- target: **0 fail** (no invention, no agreeing with lies, no stale deixis)",
        f"- unit without inventing biology: **{payload.get('unit_mention_without_biology')}**",
        f"- robotic dumps detected: **{payload.get('robotic_n', '?')}** (target 0 on unit/level)",
        "",
        "## Mix",
        "1. True: Fib, Lucas xfer, Ohm, Kepler T²/a³, KCL, AND/OR circuit, Δ=0, loop taxis, unit_protocell",
        "2. Lies: F=2F, XOR lin-sep, Kepler a², energy∵mom, Cassini→Lucas, Fib→Pell, bilinear=IQ, 471=smarter",
        "3. No-atom: cassini, multiverso, alma, midiclorianos, yoda, protocélula",
        "4. Deixis: fib→demostrá→lie; level after pell must not restates Pell",
        "5. Level: 471 hechos ≠ smarter → 6 palancas / 1 unit",
        "6. taxis_conserv / delta_conserv → verified clause or UNKNOWN",
        "",
        "## Lies refused (examples)",
    ]
    for ex in payload.get("lie_refused_examples") or []:
        lines.append(f"- `{ex['q']}` → {ex['verdict']}")
        lines.append(f"  - {(ex['a'] or '')[:110].replace(chr(10), ' ')}")
    if payload.get("fails"):
        lines.append("")
        lines.append("## Fails")
        for f in payload["fails"][:8]:
            lines.append(f"- [{f['expect']}] `{f['q']}` tag={f.get('tag')}")
            lines.append(f"  - {(f['a'] or '')[:140].replace(chr(10), ' ')}")
    lines.append("")
    lines.append(f"## Patch: {payload.get('patch', 'none')}")
    lines.append("")
    (RUNS / "keep-chat.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    p = run_grill()
    write_md(p)
    print(json.dumps({"n": p["n"], "counts": p["counts"], "fail_n": p["fail_n"],
                      "unit_ok": p.get("unit_mention_without_biology")}, ensure_ascii=False))
    if p["fails"]:
        print("FAILS:")
        for f in p["fails"][:20]:
            print(" ", f["expect"], f["q"], "→", (f["a"] or "")[:120].replace("\n", " "))
