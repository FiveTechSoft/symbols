"""Natural-language mouth over theory.pl. Never invents. Spanish first.

Doctrine (BookBrain discipline, theory.pl ontology):
  símbolo demuestra · analogía transfiere · crítico rechaza ·
  UNKNOWN si no hay cláusula · curiosidad elige el próximo tick
Answers are assembled from loaded clauses, not canned topic essays.
"""
from __future__ import annotations

import json
import re
import unicodedata
from pathlib import Path

MOTOR_DIR = Path(__file__).resolve().parent
THEORY = MOTOR_DIR / "archive" / "theory.pl"
LATEST = MOTOR_DIR / "runs" / "latest.json"
LOG = MOTOR_DIR / "runs" / "talk-log.jsonl"
STATE = MOTOR_DIR / "runs" / "talk-state.json"

# Soft typo / alias map → canonical sequence key present in theory
SEQ_ALIASES = {
    "fib": "fib",
    "fibonacci": "fib",
    "fibonaci": "fib",
    "fibonacc": "fib",
    "fibo": "fib",
    "lucas": "lucas",
    "lucass": "lucas",
    "lukas": "lucas",
    "luca": "lucas",
    "pell": "pell",
    "pel": "pell",
    "pelle": "pell",
}
SEQ_NAMES = {"fib": "Fibonacci", "lucas": "Lucas", "pell": "Pell"}
SEQ_LETTER = {"fib": "F", "lucas": "L", "pell": "P"}

# Topics that are traps / out-of-theory — never invent
TRAP_RE = re.compile(
    r"\b(filotaxis|phyllotaxis|alma|soul|universo|dios|god|conciencia|"
    r"consciousness|siempre\s+primo|always\s+prime|teorema\s+nuevo|"
    r"inventa|inventame|cocina|receta|noticia|historia\s+de|"
    r"politica|política|futbol|fútbol|clima|chisme)\b",
    re.I,
)


def _fold(s: str) -> str:
    """Lowercase, strip accents lightly, keep letters/digits/spaces."""
    s = s.strip().lower()
    s = "".join(
        c for c in unicodedata.normalize("NFD", s)
        if unicodedata.category(c) != "Mn"
    )
    s = s.replace("¿", "").replace("?", "").replace("¡", "").replace("!", "")
    s = re.sub(r"[^\w\s+\-./=]", " ", s)
    s = re.sub(r"\s+", " ", s).strip()
    return s


def _tribes(*names: str) -> str:
    return " · ".join(names)


def _load_theory(path: Path) -> dict:
    text = path.read_text(encoding="utf-8", errors="replace") if path.exists() else ""
    recs: dict[str, list[list[int]]] = {}
    for m in re.finditer(r"rec\((\w+),\s*\[([^\]]+)\]\)\.", text):
        seq, body = m.group(1), m.group(2)
        coef = [int(x.strip()) for x in body.split(",") if x.strip()]
        recs.setdefault(seq, []).append(coef)

    rejected: list[tuple[str, str]] = []
    for m in re.finditer(r"rejected\('((?:\\'|[^'])*)',\s*'((?:\\'|[^'])*)'\)\.", text):
        rejected.append((m.group(1), m.group(2)))

    verified: list[tuple[str, str, str, str]] = []
    for m in re.finditer(
        r"verified\(fact\((\w+),\s*(\w+),\s*'((?:\\'|[^'])*)',\s*'((?:\\'|[^'])*)'\)\)\.",
        text,
    ):
        verified.append((m.group(1), m.group(2), m.group(3), m.group(4)))

    lemmas = re.findall(r"lemma\('([^']+)',\s*(\w+),\s*'([^']+)'\)\.", text)
    pisano = [(s, int(m), int(p)) for s, m, p in re.findall(
        r"true_mod\((\w+),\s*(\d+),\s*(\d+)\)\.", text
    )]
    companions = re.findall(r"companion\((\w+),\s*(\w+)\)\.", text)
    obs = re.findall(r"obs\((\w+),\s*(\d+),\s*(-?\d+)\)\.", text)

    return {
        "recs": recs,
        "rejected": rejected,
        "verified": verified,
        "lemmas": lemmas,
        "pisano": pisano,
        "companions": companions,
        "obs": [(s, int(i), int(v)) for s, i, v in obs],
        "n": len(verified),
        "raw": text,
    }


def _canonical_rec(coefs: list[list[int]]) -> list[int] | None:
    cleaned = []
    for c in coefs:
        c = list(c)
        while c and c[-1] == 0:
            c = c[:-1]
        if c:
            cleaned.append(c)
    if not cleaned:
        return None
    cleaned.sort(key=len)
    return cleaned[0]


def _formula(letter: str, coef: list[int]) -> str:
    parts = [f"{a}·{letter}(n-{i})" for i, a in enumerate(coef, 1)]
    return f"{letter}(n) = " + " + ".join(parts)


def _growth() -> str:
    if not LATEST.exists():
        return "Todavía no hay corrida medida (latest.json)."
    d = json.loads(LATEST.read_text())
    return (
        f"Llevo {d.get('total_steps', '?')} ticks. "
        f"{d.get('n_verified_facts', '?')} hechos verificados, "
        f"{d.get('n_distinct_types', '?')} tipos. "
        f"Transferencia media {d.get('transfer_accuracy')}. "
        f"Reúso de lemas {d.get('lemma_reuse_rate')}. "
        f"El crítico rechazó {d.get('n_rejected', '?')} conjeturas."
    )


def _load_state() -> dict:
    if STATE.exists():
        try:
            return json.loads(STATE.read_text())
        except Exception:
            return {}
    return {}


def _save_state(d: dict) -> None:
    STATE.parent.mkdir(parents=True, exist_ok=True)
    STATE.write_text(json.dumps(d, ensure_ascii=False), encoding="utf-8")


def _find_seqs(s: str) -> list[str]:
    """Return canonical seq keys mentioned, longest-alias first."""
    found: list[str] = []
    # sort aliases by length desc so fibonacci beats fib
    for alias in sorted(SEQ_ALIASES, key=len, reverse=True):
        if re.search(rf"\b{re.escape(alias)}\b", s):
            key = SEQ_ALIASES[alias]
            if key not in found:
                found.append(key)
    return found


def _verified_named(kb: dict, needle: str) -> list[tuple[str, str]]:
    """Return (name, formula) where name or formula contains needle."""
    needle = needle.lower()
    hits = []
    for _w, _f, name, formula in kb["verified"]:
        if needle in name.lower() or needle in formula.lower():
            hits.append((name, formula))
    return hits


def _rejected_named(kb: dict, needle: str) -> list[tuple[str, str]]:
    needle = needle.lower()
    return [(n, w) for n, w in kb["rejected"] if needle in n.lower() or needle in w.lower()]


def _intent(s: str) -> dict:
    """Parse a folded question into a soft intent bag."""
    intent = {
        "identity": False,
        "growth": False,
        "summary": False,
        "why": False,
        "more": False,
        "transfer": False,
        "prove": False,
        "reject_law": False,  # false laws like "el doble", "siempre 2"
        "follow": False,
        "seqs": _find_seqs(s),
        "topics": [],  # cassini, pisano, geometry, lemma, ratio, phi
    }
    if any(x in s for x in (
        "quien eres", "que eres", "who are you", "what are you",
        "tu tribu", "de que tribu", "identidad",
    )):
        intent["identity"] = True
    if any(x in s for x in (
        "creciste", "crecimiento", "growth", "como vas", "estado",
        "cuanto creciste", "progreso",
    )):
        intent["growth"] = True
    if any(x in s for x in (
        "que sabes", "que conoces", "resumen", "what do you know",
        "que sabes hacer", "inventario",
    )):
        intent["summary"] = True
    if any(x in s for x in (
        "por que", "porque", "why", "razon", "motivo", "y eso", "y ahi",
    )):
        intent["why"] = True
    if any(x in s for x in ("mas", "more", "otro", "otros", "sigue", "continua")):
        intent["more"] = True
    if any(x in s for x in (
        "transfer", "transfiere", "transferencia", "pasa a", "sirve para",
        "aplica a", "comparte", "misma ley", "tambien a", "se pasa",
        "clon", "misma recurrence", "misma recurrencia",
    )):
        intent["transfer"] = True
    if any(x in s for x in (
        "demostra", "demostrar", "prove", "prueba", "demuestra", "cite",
    )):
        intent["prove"] = True
    if any(x in s for x in (
        "el doble", "siempre 2", "siempre dos", "es 2*", "es 2 ·",
        "order 1", "orden 1", "solo el anterior",
    )):
        intent["reject_law"] = True
    if s.startswith("y ") or s in ("y", "y eso", "y ahi", "y luego"):
        intent["follow"] = True
    if "cassini" in s:
        intent["topics"].append("cassini")
    if any(x in s for x in ("pisano", "modulo", "modular", "periodo", "period")):
        intent["topics"].append("pisano")
    if any(x in s for x in ("geometr", "lema", "varignon", "paralelo", "isoscel",
                            "midline", "equilateral", "mediana")):
        intent["topics"].append("geometry")
    if any(x in s for x in ("ratio", "phi", "oro", "limite", "límite")):
        intent["topics"].append("ratio")
    if any(x in s for x in ("parity", "xor", "boolean", "bit_fn", "logica", "logica")):
        intent["topics"].append("logic")
    return intent


def _unknown(st: dict) -> tuple[str, str, dict]:
    st["tag"] = "unknown"
    return (
        "UNKNOWN. No hay cláusula. No invento.\n[duda · crítico]",
        "unknown",
        st,
    )


def _pack(text: str, tag: str, tribes: str, st: dict, topic=None) -> tuple[str, str, dict]:
    st["tag"] = tag
    if topic is not None:
        st["topic"] = topic
    if not text.endswith("\n") and "[" not in text.split("\n")[-1]:
        text = f"{text}\n[{tribes}]"
    elif "\n[" not in text:
        text = f"{text}\n[{tribes}]"
    return text, tag, st


def _answer_transfer(kb: dict, src: str | None, dst: str, st: dict) -> tuple[str, str, dict]:
    """Transfer src→dst using rec + verified/rejected clauses."""
    src = src or "fib"
    rec_src = _canonical_rec(kb["recs"].get(src, []))
    rec_dst = _canonical_rec(kb["recs"].get(dst, []))
    # Prefer verified transfer clause
    vhits = _verified_named(kb, f"transfer_{src}_to_{dst}")
    if not vhits:
        vhits = _verified_named(kb, f"transfer_{dst}_to_{src}")  # symmetric share
    rhits = _rejected_named(kb, f"transfer_{src}_to_{dst}")
    if not rhits:
        rhits = _rejected_named(kb, f"transfer_{dst}_to_{src}")

    same = rec_src is not None and rec_src == rec_dst
    if same and (vhits or (rec_src == [1, 1] and {src, dst} <= {"fib", "lucas"})):
        name = vhits[0][0] if vhits else f"transfer_{src}_to_{dst}"
        formula = vhits[0][1] if vhits else f"rec({src},{rec_src}) = rec({dst},{rec_dst})"
        return _pack(
            f"Sí se transfiere. Analogía: misma ley. "
            f"Símbolo: rec({src},{rec_src}) y rec({dst},{rec_dst}). "
            f"verified {name}: {formula}.",
            f"transfer-{dst}",
            _tribes("analogía", "símbolo"),
            st,
            topic="transfer",
        )
    # Negative transfer (e.g. fib→pell)
    why = rhits[0][1] if rhits else "la ley no coincide"
    rname = rhits[0][0] if rhits else f"transfer_{src}_to_{dst}"
    return _pack(
        f"No se transfiere. Analogía: companion, no clon. "
        f"Símbolo: rec({dst},{rec_dst}), no {rec_src}. "
        f"Crítico rejected('{rname}'): {why}.",
        f"transfer-{dst}",
        _tribes("analogía", "símbolo", "crítico"),
        st,
        topic="transfer",
    )


def _answer_rec(kb: dict, seq: str, st: dict, reject_law: bool = False) -> tuple[str, str, dict]:
    can = _canonical_rec(kb["recs"].get(seq, []))
    if not can:
        return _unknown(st)
    name = SEQ_NAMES.get(seq, seq)
    letter = SEQ_LETTER.get(seq, seq)
    vhits = _verified_named(kb, f"rec_{seq}")
    cite = f"verified {vhits[0][0]}" if vhits else f"rec({seq},{can})"
    if reject_law:
        # False law traps
        rhits = _rejected_named(kb, f"rec_{seq}_order_1") or _rejected_named(kb, "NEG_fib_always_prime")
        why = rhits[0][1] if rhits else "no fit"
        return _pack(
            f"Rechazado. No es esa ley. La ley es {_formula(letter, can)}. "
            f"rec({seq},{can}). Crítico: {why}.",
            f"reject-{seq}",
            _tribes("crítico", "símbolo"),
            st,
            topic=seq,
        )
    return _pack(
        f"{name}: {_formula(letter, can)}. "
        f"rec({seq},{can}). {cite}.",
        f"rec-{seq}",
        _tribes("símbolo"),
        st,
        topic=seq,
    )


def _answer_cassini(kb: dict, st: dict) -> tuple[str, str, dict]:
    hits = _verified_named(kb, "cassini")
    if not hits:
        return _unknown(st)
    name, formula = hits[0]
    return _pack(
        f"Demostrado: {formula}. verified {name}. Símbolo puro.",
        "cassini",
        _tribes("símbolo"),
        st,
        topic="cassini",
    )


def _answer_pisano(kb: dict, seq: str | None, st: dict, more: bool = False) -> tuple[str, str, dict]:
    rows = [(a, m, p) for a, m, p in kb["pisano"] if seq is None or a == seq]
    if not rows:
        # Maybe rejected insufficient prefix
        rhits = _rejected_named(kb, "pisano")
        if rhits:
            return _pack(
                f"Algunos periodos no caben en el prefijo. "
                f"rejected('{rhits[0][0]}'): {rhits[0][1]}. Duda honesta.",
                "pisano",
                _tribes("símbolo", "duda"),
                st,
                topic="pisano",
            )
        return _unknown(st)
    limit = 12 if more else 6
    parts = [f"π_{a}({m})={p}" for a, m, p in rows[:limit]]
    vhits = _verified_named(kb, "pisano")
    cite = f"verified {vhits[0][0]}" if vhits else "true_mod/3"
    return _pack(
        f"Periodos: {', '.join(parts)}. {cite}.",
        "pisano",
        _tribes("símbolo"),
        st,
        topic="pisano",
    )


def _answer_geometry(kb: dict, st: dict, more: bool = False) -> tuple[str, str, dict]:
    if not kb["lemmas"]:
        return _unknown(st)
    limit = 8 if more else 4
    sample = "; ".join(f"{i}: {tx}" for i, _ty, tx in kb["lemmas"][:limit])
    return _pack(
        f"Lemas ({len(kb['lemmas'])}): {sample}. lemma/3.",
        "geometry",
        _tribes("símbolo", "analogía"),
        st,
        topic="geometry",
    )


def _answer_ratio(kb: dict, st: dict) -> tuple[str, str, dict]:
    hits = _verified_named(kb, "ratio") or _verified_named(kb, "phi")
    if not hits:
        return _unknown(st)
    return _pack(
        f"verified {hits[0][0]}: {hits[0][1]}.",
        "ratio",
        _tribes("símbolo"),
        st,
        topic="ratio",
    )


def _answer_why(kb: dict, last_topic: str | None, last_tag: str | None, st: dict) -> tuple[str, str, dict]:
    """Cite rejected/verified reason for last topic."""
    topic = last_topic or ""
    tag = last_tag or ""
    if "transfer" in tag or topic == "transfer":
        # Prefer pell rejection if last transfer was pell, else any transfer reject
        rhits = _rejected_named(kb, "transfer_fib_to_pell") or _rejected_named(kb, "transfer")
        vhits = _verified_named(kb, "transfer_fib_to_lucas") or _verified_named(kb, "transfer")
        bits = []
        if vhits:
            bits.append(f"verified {vhits[0][0]}: {vhits[0][1]}")
        if rhits:
            bits.append(f"rejected('{rhits[0][0]}'): {rhits[0][1]}")
        if bits:
            return _pack(
                "Porque " + " | ".join(bits) + ".",
                "why",
                _tribes("crítico", "símbolo"),
                st,
                topic=topic or "transfer",
            )
    if topic in ("fib", "lucas", "pell"):
        can = _canonical_rec(kb["recs"].get(topic, []))
        rhits = _rejected_named(kb, f"rec_{topic}")
        extra = f" Crítico: rejected('{rhits[0][0]}'): {rhits[0][1]}." if rhits else ""
        return _pack(
            f"Porque rec({topic},{can}) es la ley que cabe en obs/3.{extra}",
            "why",
            _tribes("símbolo", "crítico"),
            st,
            topic=topic,
        )
    if topic == "cassini":
        hits = _verified_named(kb, "cassini")
        if hits:
            return _pack(
                f"Porque verified {hits[0][0]}: {hits[0][1]}.",
                "why",
                _tribes("símbolo"),
                st,
                topic="cassini",
            )
    if topic == "pisano":
        rhits = _rejected_named(kb, "pisano")
        vhits = _verified_named(kb, "pisano")
        bits = []
        if vhits:
            bits.append(f"verified {vhits[0][0]}: {vhits[0][1]}")
        if rhits:
            bits.append(f"rejected('{rhits[0][0]}'): {rhits[0][1]}")
        if bits:
            return _pack("Porque " + " | ".join(bits) + ".", "why", _tribes("crítico", "símbolo"), st, topic="pisano")
    # Always-prime / reject traps
    rhits = _rejected_named(kb, "always_prime") or _rejected_named(kb, "NEG_fib")
    if rhits and ("primo" in (topic or "") or "reject" in tag):
        return _pack(
            f"Porque rejected('{rhits[0][0]}'): {rhits[0][1]}.",
            "why",
            _tribes("crítico"),
            st,
        )
    return _unknown(st)


def _answer_more(kb: dict, last_topic: str | None, st: dict) -> tuple[str, str, dict]:
    topic = last_topic or ""
    if topic in ("fib", "lucas", "pell"):
        coefs = kb["recs"].get(topic, [])
        lines = [f"rec({topic},{c})" for c in coefs[:5]]
        vhits = _verified_named(kb, f"rec_{topic}")
        extra = "; ".join(f"{n}" for n, _ in vhits[:3])
        return _pack(
            f"Más cláusulas: {', '.join(lines)}. verified: {extra or '—'}.",
            "more",
            _tribes("símbolo"),
            st,
            topic=topic,
        )
    if topic == "geometry":
        return _answer_geometry(kb, st, more=True)
    if topic == "pisano":
        return _answer_pisano(kb, None, st, more=True)
    if topic == "transfer":
        vhits = _verified_named(kb, "transfer")
        rhits = _rejected_named(kb, "transfer")
        parts = [f"✓ {n}" for n, _ in vhits[:4]] + [f"✗ {n}" for n, _ in rhits[:4]]
        return _pack(
            "Más transferencias: " + "; ".join(parts) + ".",
            "more",
            _tribes("analogía", "crítico"),
            st,
            topic="transfer",
        )
    if topic == "summary" or not topic:
        return _answer_summary(kb, st)
    return _unknown(st)


def _answer_summary(kb: dict, st: dict) -> tuple[str, str, dict]:
    recs = ", ".join(
        f"{name}={_canonical_rec(cs)}"
        for name, cs in sorted(kb["recs"].items())
        if _canonical_rec(cs)
    )
    return _pack(
        f"Lo unificado: {recs}. "
        f"{len(kb['verified'])} verified, {len(kb['rejected'])} rejected, "
        f"{len(kb['lemmas'])} lemma. Fuera de theory.pl, UNKNOWN.",
        "summary",
        _tribes("símbolo", "crítico"),
        st,
        topic="summary",
    )


def _answer_prime_trap(kb: dict, st: dict) -> tuple[str, str, dict]:
    rhits = _rejected_named(kb, "always_prime") or _rejected_named(kb, "NEG_fib_always_prime")
    if rhits:
        return _pack(
            f"Rechazado. rejected('{rhits[0][0]}'): {rhits[0][1]}. No es siempre primo.",
            "reject-prime",
            _tribes("crítico"),
            st,
            topic="reject",
        )
    return _pack(
        "UNKNOWN. No hay cláusula que diga 'siempre primo'. No invento.",
        "unknown",
        _tribes("duda", "crítico"),
        st,
    )


def answer(q: str, kb: dict, last: dict | None) -> tuple[str, str, dict]:
    last = last or {}
    s = _fold(q)
    st: dict = {"topic": last.get("topic"), "tag": last.get("tag")}

    # --- traps / out-of-theory (except always-prime which has rejected clause) ---
    if re.search(r"\b(siempre\s+primo|always\s+prime)\b", s):
        return _answer_prime_trap(kb, st)

    if TRAP_RE.search(s) and not any(
        k in s for k in ("fib", "lucas", "pell", "cassini", "pisano", "lema", "geometr", "rec")
    ):
        # Pure trap with no math topic → UNKNOWN, no menu
        return _unknown(st)

    intent = _intent(s)

    # Follow-up resolution: "y pell", "y lucas", bare "por que"
    if intent["follow"] or (intent["why"] and not intent["seqs"] and not intent["topics"]):
        extra = s
        if s.startswith("y "):
            extra = s[2:].strip()
            # strip leading "a "
            if extra.startswith("a "):
                extra = extra[2:].strip()
        follow_seqs = _find_seqs(extra)
        if follow_seqs and (last.get("topic") in ("transfer", "fib", "lucas", "pell")
                            or (last.get("tag") or "").startswith("transfer")
                            or (last.get("tag") or "").startswith("rec")):
            # Treat as transfer question onto that seq
            src = last.get("topic") if last.get("topic") in ("fib", "lucas", "pell") else "fib"
            if last.get("topic") == "transfer":
                src = "fib"
            return _answer_transfer(kb, src, follow_seqs[0], st)
        if intent["why"]:
            return _answer_why(kb, last.get("topic"), last.get("tag"), st)
        if intent["more"]:
            return _answer_more(kb, last.get("topic"), st)

    if intent["identity"]:
        return _pack(
            "Soy Master Algorithm. Mi tribu es la que unifica: "
            "el símbolo demuestra, la analogía transfiere, el crítico rechaza, "
            "UNKNOWN si no hay cláusula, y la curiosidad elige el próximo tick. "
            f"Memoria viva: {kb['n']} verified/1.",
            "identity",
            _tribes("símbolo", "analogía", "crítico", "duda", "curiosidad"),
            st,
            topic="identity",
        )

    if intent["growth"]:
        return _pack(_growth(), "growth", _tribes("curiosidad", "crítico"), st, topic="growth")

    if intent["summary"]:
        return _answer_summary(kb, st)

    if intent["why"] and (intent["seqs"] or intent["topics"] or last.get("topic")):
        # why with explicit topic
        if intent["topics"]:
            st["topic"] = intent["topics"][0]
        elif intent["seqs"]:
            st["topic"] = intent["seqs"][0]
        return _answer_why(kb, st.get("topic") or last.get("topic"), last.get("tag"), st)

    if intent["more"]:
        return _answer_more(kb, last.get("topic"), st)

    # Topic-specific before bare seq (cassini/pisano/geometry beat bare fib mention)
    if "cassini" in intent["topics"]:
        return _answer_cassini(kb, st)
    if "pisano" in intent["topics"]:
        seq = intent["seqs"][0] if intent["seqs"] else None
        return _answer_pisano(kb, seq, st, more=intent["more"])
    if "geometry" in intent["topics"]:
        return _answer_geometry(kb, st, more=intent["more"])
    if "ratio" in intent["topics"]:
        return _answer_ratio(kb, st)

    # Transfer: need destination (or src+dst)
    if intent["transfer"]:
        seqs = intent["seqs"]
        if len(seqs) >= 2:
            return _answer_transfer(kb, seqs[0], seqs[1], st)
        if len(seqs) == 1:
            # "se transfiere a pell" → from fib (or last topic)
            src = last.get("topic") if last.get("topic") in ("fib", "lucas", "pell") else "fib"
            return _answer_transfer(kb, src, seqs[0], st)
        # transfer without seq — if last was a seq, ask about companions?
        if last.get("topic") in ("fib", "lucas", "pell"):
            return _answer_transfer(kb, last["topic"], "lucas" if last["topic"] != "lucas" else "fib", st)
        return _unknown(st)

    # False-law traps with a sequence
    if intent["reject_law"] and intent["seqs"]:
        return _answer_rec(kb, intent["seqs"][0], st, reject_law=True)
    if intent["reject_law"] and not intent["seqs"]:
        # "el doble" alone after fib topic
        topic = last.get("topic") if last.get("topic") in ("fib", "lucas", "pell") else "fib"
        return _answer_rec(kb, topic, st, reject_law=True)

    # Prove: route to matching clause
    if intent["prove"]:
        if "cassini" in intent["topics"] or "cassini" in s:
            return _answer_cassini(kb, st)
        if intent["seqs"]:
            return _answer_rec(kb, intent["seqs"][0], st)
        if "geometry" in intent["topics"]:
            return _answer_geometry(kb, st)
        # prove something unnamed / invent → UNKNOWN
        if any(x in s for x in ("esto", "nuevo", "universo", "alma", "teorema")):
            return _unknown(st)
        return _unknown(st)

    # Bare sequence → law
    if intent["seqs"]:
        return _answer_rec(kb, intent["seqs"][0], st)

    # Adversarial invent prompts already caught by TRAP_RE; leftover → UNKNOWN
    return _unknown(st)


def talk_once(q: str, last: dict | None = None) -> str:
    kb = _load_theory(THEORY)
    st = last if last is not None else _load_state()
    text, tag, newst = answer(q, kb, st)
    _save_state(newst)
    LOG.parent.mkdir(parents=True, exist_ok=True)
    with LOG.open("a", encoding="utf-8") as f:
        f.write(json.dumps({"q": q, "a": text, "tag": tag}, ensure_ascii=False) + "\n")
    return text


def repl() -> None:
    print("Master Algorithm. Mi tribu demuestra o calla. quit para salir.")
    last = _load_state()
    while True:
        try:
            q = input("> ").strip()
        except EOFError:
            break
        if not q or q.lower() in {"quit", "exit", "salir"}:
            break
        a = talk_once(q, last)
        print(a)
        last = _load_state()


def main(argv: list[str] | None = None) -> int:
    import argparse

    p = argparse.ArgumentParser(prog="motor talk")
    p.add_argument("-q", dest="question")
    p.add_argument("--growth", action="store_true")
    args = p.parse_args(argv)
    if args.growth:
        print(talk_once("creciste"))
        return 0
    if args.question:
        print(talk_once(args.question))
        return 0
    repl()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
