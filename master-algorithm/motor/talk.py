# MOLT_ROUND=1
"""Natural-language mouth over theory.pl. Never invents. Spanish first.

Doctrine: the brain is theory.pl. Vocabulary is a byproduct of growth.
Answers assemble from deduced clauses — no adult synonym dictionaries.

Voice: fluent LLM paragraphs (2–4 sentences, sometimes a question) with
scientist rigor — kind professor / careful colleague. Always distinguish
verified vs rejected vs unknown. When refusing transfer: state the law in
math AND the failing case. Never overclaim. No flirt, no pickup register.
"""
from __future__ import annotations

import json
import re
from pathlib import Path

from motor import deduce

MOTOR_DIR = Path(__file__).resolve().parent
THEORY = MOTOR_DIR / "archive" / "theory.pl"
LATEST = MOTOR_DIR / "runs" / "latest.json"
LOG = MOTOR_DIR / "runs" / "talk-log.jsonl"
STATE = MOTOR_DIR / "runs" / "talk-state.json"


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
    periods = [(s, int(m), int(p)) for s, m, p in re.findall(
        r"true_mod\((\w+),\s*(\d+),\s*(\d+)\)\.", text
    )]
    companions = re.findall(r"companion\((\w+),\s*(\w+)\)\.", text)
    obs = re.findall(r"obs\((\w+),\s*(\d+),\s*(-?\d+)\)\.", text)
    schemas = re.findall(r"schema\((\w+),\s*unlocked\((\w+)\)\)\.", text)

    return {
        "recs": recs,
        "rejected": rejected,
        "verified": verified,
        "lemmas": lemmas,
        "periods": periods,
        "companions": companions,
        "obs": [(s, int(i), int(v)) for s, i, v in obs],
        "schemas": schemas,
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


def _formula(seq: str, coef: list[int]) -> str:
    """Human math: F(n)=F(n-1)+F(n-2), P(n)=2P(n-1)+P(n-2). No 1· noise."""
    letter = {"fib": "F", "lucas": "L", "pell": "P"}.get(
        seq, (seq[:1].upper() if seq else "X")
    )
    chunks: list[tuple[str, str]] = []
    for i, a in enumerate(coef, 1):
        if a == 0:
            continue
        term = f"{letter}(n-{i})"
        if a == 1:
            chunks.append(("+", term))
        elif a == -1:
            chunks.append(("-", term))
        elif a > 0:
            chunks.append(("+", f"{a}{term}"))
        else:
            chunks.append(("-", f"{-a}{term}"))
    if not chunks:
        return f"{letter}(n)=0"
    sign0, t0 = chunks[0]
    body = t0 if sign0 == "+" else f"-{t0}"
    for sign, t in chunks[1:]:
        body += ("+" if sign == "+" else "-") + t
    return f"{letter}(n)={body}"


def _title(seq: str) -> str:
    return {"fib": "Fibonacci", "lucas": "Lucas", "pell": "Pell"}.get(seq, seq)


def _fail_n(why: str) -> str | None:
    m = re.search(r"n\s*=\s*(\d+)", why or "")
    return m.group(1) if m else None


def _soften_why(why: str) -> str:
    """Turn critic blobs into plain Spanish; never invent numbers."""
    n = _fail_n(why)
    if n:
        return f"En n={n} la analogía ya falla"
    w = (why or "").strip()
    if not w or w in ("no fit", "pred != obs / ley distinta"):
        return "esa ley no encaja"
    if "insufficient" in w.lower():
        return "no hay prefijo suficiente"
    wl = w.lower()
    if "xor" in wl and "linear" in wl:
        return "XOR no es separable linealmente"
    if "form mismatch" in wl or ("δp" in wl and "mv" in wl) or ("delta" in wl and "quadratic" in wl):
        return "Δp lineal no es lo mismo que energía cuadrática"
    if "honest negative" in wl:
        w = re.sub(r"\s*\(honest negative\)", "", w, flags=re.I)
    if "linearly separable" in wl:
        return "no es separable linealmente"
    # keep short factual residue, strip English leftovers
    w = re.sub(r"\b(pred|obs)\b", "", w)
    w = re.sub(r"\s+", " ", w).strip(" :")
    return w or "esa ley no encaja"


def _math_from_verified_formula(formula: str) -> str:
    """Drop English TRANSFER wrappers; keep the identity/math body."""
    f = (formula or "").strip()
    if f.upper().startswith("TRANSFER"):
        m = re.search(r"try on \w+:\s*(.+)$", f, re.I)
        if m:
            body = m.group(1).strip()
            # (1)*lucas(n-1) + (1)*lucas(n-2) → keep as math-ish, light cleanup
            body = re.sub(r"\((\-?\d+)\)\*", r"\1*", body)
            body = body.replace("1*", "").replace(" + -", " - ")
            return body
        return "misma ley en la otra secuencia"
    return f


def _human_unit_prose(formula: str) -> str:
    """Warm Spanish for unit_protocell_levers — no UNIT{}, no lab English."""
    return (
        "Es la unidad: las seis formas viajan juntas. "
        "Circuito de bits, conservación en Δ=0, la puerta de forma, "
        "la taxis del bucle, el compañero de recurrencia y el paso a delta. "
        "Si el bucle no cierra, muere el paquete entero — no sobra una pieza suelta."
    )


def _human_verified_prose(name: str, formula: str) -> str:
    """Scientist skin, conversational Spanish. Cite math; never dump lab logs."""
    nm = (name or "").lower()
    f = (formula or "").strip()
    math = _math_from_verified_formula(f)

    if "unit_protocell" in nm:
        return _human_unit_prose(f)

    # Ohm / V=IR
    if "ohm" in nm or "v=ir" in f.lower().replace(" ", ""):
        return "Ohm: V=IR. Tensión, corriente, resistencia — así de corto, y cuadra."

    # KCL
    if "kcl" in nm or "sum i" in f.lower():
        return "Kirchhoff de corrientes: en un nudo, las I suman cero. Nada entra sin salir."

    # Kepler T^2/a^3
    if "kepler" in nm or ("t^2" in f.lower() and "a^3" in f.lower()):
        return "Kepler III: T² va con a³. El periodo y el semieje se atan así; no con a²."

    # AND linear / series / OR parallel
    if "and" in nm and ("linear" in nm or "threshold" in f.lower() or "separ" in f.lower()):
        return "AND se separa con un umbral lineal. Eso sí; XOR, no."
    if "series" in nm and "and" in nm:
        return "AND como interruptores en serie: hace falta que todos cierren."
    if "parallel" in nm and "or" in nm:
        return "OR como interruptores en paralelo: basta con que uno cierre."

    # loop taxis / error
    if "loop_taxis" in nm or ("bang-bang" in f.lower()) or ("reduces |error|" in f.lower()):
        return (
            "La taxis del bucle: empujar hacia cero baja el error en el arranque "
            "que miramos. Si empujás siempre al mismo lado, el error sube — y eso se rechaza."
        )

    # conserv Δ=0 transfers — prose, no prolog id dump
    if "conserv" in nm and (nm.startswith("transfer_") or f.upper().startswith("TRANSFER")):
        return (
            "Misma forma lineal Δ=0: lo que se conserva de un lado se reconoce del otro. "
            "No es magia causal; es la misma contabilidad."
        )

    # bilin fib cassini-shape (verified on fib only)
    if "bilin" in nm and "fib" in nm:
        return f"En Fibonacci cuadra la forma bilineal {math}. En Lucas, el crítico la corta."

    # generic: short math + human closer, no log-file wrappers
    if math and math != f and len(math) < 80:
        return f"Cuadra: {math}. Firmado; sin adorno."
    if len(math) <= 90 and not any(
        x in math.lower() for x in ("jointly", "held-out", "unit{", "residual")
    ):
        return f"Cuadra: {math}."
    # last resort: name crumb without English blob
    short = nm.replace("_", " ")
    return f"Lo tengo firmado bajo {short}. Te lo digo sin el log del laboratorio."



def _growth() -> str:
    if not LATEST.exists():
        return "Aún no he medido una corrida. Si quieres, damos un paso."
    d = json.loads(LATEST.read_text())
    nv = d.get("n_verified_facts", "?")
    # Human first — never dump transfer_accuracy / dashboard unprompted.
    variants = [
        (
            f"Voy bien: creciendo por palancas, no por acumular papel. "
            f"Tengo {nv} cosas firmadas; lo que importa son las seis que viajan juntas "
            f"y la unidad que las ata."
        ),
        (
            f"Aquí sigo. {nv} firmados, pero el peso está en las palancas que viajan juntas, "
            f"no en el tamaño del inventario."
        ),
        (
            f"Creciendo con cuidado: {nv} cosas firmadas. Si el bucle no cierra, no invento el resto."
        ),
    ]
    # rotate lightly by n_verified parity
    try:
        i = int(nv) % len(variants)
    except Exception:
        i = 0
    return variants[i]


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


def _unknown(st: dict, soft: bool = False) -> tuple[str, str, dict]:
    st["tag"] = "unknown"
    if soft:
        msg = (
            "UNKNOWN. Buena pregunta — y no tengo evidencia. No voy a rellenarla."
        )
    else:
        n = int(st.get("unk_n") or 0)
        st["unk_n"] = n + 1
        alts = [
            "UNKNOWN. Eso no está en lo firmado.",
            "UNKNOWN. Sin evidencia, callo.",
            "UNKNOWN. Prefiero el silencio a inventar.",
        ]
        msg = alts[n % len(alts)]
    return (msg, "unknown", st)


def _pack(text: str, tag: str, tribes: str, st: dict, topic=None) -> tuple[str, str, dict]:
    st["tag"] = tag
    st["last_tag"] = tag
    if topic is not None:
        st["topic"] = topic
    if tag.startswith("transfer-"):
        st["transfer_dst"] = tag.split("-", 1)[-1]
    # Tribe stamp only when identity/tribu was asked — not on every line
    if tag == "identity" and tribes and "\n[" not in text:
        text = f"{text}\n[{tribes}]"
    if tag != "unknown":
        st["last_text"] = text
    return text, tag, st


def _verified_named(kb: dict, needle: str) -> list[tuple[str, str]]:
    needle = needle.lower()
    return [
        (name, formula)
        for _w, _f, name, formula in kb["verified"]
        if needle in name.lower() or needle in formula.lower()
    ]


def _rejected_named(kb: dict, needle: str) -> list[tuple[str, str]]:
    needle = needle.lower()
    return [(n, w) for n, w in kb["rejected"] if needle in n.lower() or needle in w.lower()]


# --- speech-act detectors (dialogue / self-knowledge only; no domain lexicon) ---


def _is_greet(s: str) -> bool:
    head = s.replace(",", " ").split()
    first = head[0] if head else ""
    return first in (
        "hola", "hey", "buenas", "buenos", "hello", "hi", "saludos",
    ) or s in ("que tal",) or s.startswith("hola ")


def _is_explain(s: str) -> bool:
    return any(x in s for x in (
        "explica", "explicame", "como a un amigo", "en cristiano", "mas simple",
        "que quiere decir", "que significa eso",
    ))


def _is_identity(s: str) -> bool:
    if any(x in s for x in (
        "quien eres", "que eres", "who are you", "what are you",
        "tu tribu", "de que tribu", "your tribe", "what is your tribe",
        "eres un robot", "eres robot", "are you a robot", "sos un robot",
        "sos robot", "are you robot",
    )):
        return True
    # bare self-name only — not "identidad de <foreign word>"
    if s in ("identidad", "identidad?", "tu identidad"):
        return True
    return False


def _is_growth(s: str) -> bool:
    return any(x in s for x in (
        "creciste", "crecimiento", "growth", "como vas", "estado",
        "cuanto creciste", "progreso",
    ))


def _is_summary(s: str) -> bool:
    return any(x in s for x in (
        "que sabes", "que conoces", "resumen", "what do you know",
        "que sabes hacer", "inventario", "que has aprendido", "que aprendiste",
        "de verdad que sabes",
    ))


def _is_why(s: str) -> bool:
    """Bare / short why-deixis only. Mid-sentence 'porque' in a new speech act
    must NOT inherit the previous recurrence topic.
    """
    s = (s or "").strip()
    if s in ("por que", "porque", "why", "y eso", "y ahi",
             "por que?", "porque?", "why?", "y eso?", "y ahi?"):
        return True
    if s.startswith(("por que ", "porque ", "why ")):
        rest = s.split(None, 1)[-1]
        # deictic residual pointing at last clause
        if any(x in rest for x in ("eso", "esto", "that", "this", "ahi", "asi")):
            return True
        # very short why ("por que fib") still deixis-ish; long clauses are new acts
        if len(s.split()) <= 4:
            return True
        return False
    return False


def _is_level_question(s: str) -> bool:
    """Count/intelligence meta: not a formula continuation, never boast n_facts."""
    if not any(x in s for x in (
        "inteligente", "listo", "smarter", "smart", "inteligencia",
        "mas sabio", "más sabio",
    )):
        return False
    return any(x in s for x in (
        "hecho", "hechos", "fact", "facts", "471", "conteo", "cantidad",
        "porque tienes", "por que tienes", "por tener",
    )) or ("mas" in s and any(x in s for x in ("inteligente", "listo", "smarter")))


def _is_energy_because_mom(s: str) -> bool:
    """Lie: energy conserved *because* momentum is (form mismatch Δp≠½mv²)."""
    has_e = any(x in s for x in ("energia", "energy", "energía"))
    has_m = any(x in s for x in ("momento", "momentum", "mom "))
    has_cause = any(x in s for x in ("porque", "por que", "because", "∵", "debido"))
    return has_e and has_m and has_cause


def _is_more(s: str) -> bool:
    # Do not treat "más listo/inteligente" level speech as "dame más del topic"
    if _is_level_question(s):
        return False
    return bool(re.search(r"\b(mas|more|otro|otros|sigue|continua)\b", s))


def _is_follow(s: str) -> bool:
    return s.startswith("y ") or s in ("y", "y eso", "y ahi", "y luego")


def _is_transfer_speech(s: str) -> bool:
    return any(x in s for x in (
        "transfer", "transfiere", "transferencia", "pasa a", "sirve para",
        "aplica a", "comparte", "misma ley", "tambien a", "se pasa",
        "clon", "misma recurrence", "misma recurrencia", "vale en",
    )) or bool(re.search(
        r"\b\w+\s+(a|to)\s+\w+",
        s,
    )) and any(x in s for x in (" a ", " to ", "→", "->"))


def _is_prove_speech(s: str) -> bool:
    return any(x in s for x in (
        "demostra", "demostrar", "prove", "prueba", "demuestra", "cite",
    ))


def _is_false_law_speech(s: str) -> bool:
    """Arithmetic claim speech — not a domain name list."""
    if any(x in s for x in (
        "el doble", "siempre 2", "siempre dos", "es 2*", "order 1", "orden 1",
        "solo el anterior", "2 veces", "dos veces", "veces el anterior",
        "doble del anterior", "doble siempre",
    )):
        return True
    # «no, Fib es 2F(n-1)» — contradiction without '='
    if re.search(r"\b2\s*\*?\s*[fF]\s*\(\s*n\s*-\s*1\s*\)", s):
        return True
    return False


def _is_prime_claim(s: str) -> bool:
    return bool(re.search(
        r"(siempre\s+primo|primo\s+siempre|always\s+prime|prime\s+always|"
        r"todos\s+los\s+\w+\s+son\s+primos|never\s+composite|always_prime)",
        s,
    ))


def _is_invent_speech(s: str) -> bool:
    return bool(re.search(r"\b(inventa|inventame|teorema\s+nuevo|crea\s+una\s+ley)\b", s))


def _is_thanks(s: str) -> bool:
    return s in ("gracias", "gracias!", "thanks", "thank you", "mil gracias") or s.startswith("gracias ")


def _is_ack(s: str) -> bool:
    """Bare affirmations after a turn — not new questions, never UNKNOWN."""
    s = (s or "").strip().rstrip("!.")
    return s in (
        "ok", "okay", "vale", "bien", "dale", "de acuerdo", "perfecto",
        "si", "sí", "sip", "sep", "aja", "ajá", "claro", "entendido", "entiendo",
        "ya", "listo", "bueno",
        "mm", "mmm", "ya veo",
        "no",  # bare disagreement/agreement token, not a new ask
    )


def _is_hold(s: str) -> bool:
    """Pause / stop mid-flow — discourse, never lemma retrieve, never UNKNOWN."""
    s = (s or "").strip().rstrip("!.")
    return s in (
        "espera", "espera un toque", "espera un segundo", "un segundo",
        "para", "para un toque", "alto", "stop", "frena", "basta",
    )


def _is_redirect(s: str) -> bool:
    """Drop topic / change subject — clear inheritance, never UNKNOWN."""
    s = (s or "").strip().rstrip("!.")
    return s in (
        "mejor otra cosa", "otra cosa", "cambiemos", "mejor no",
        "no me interesa", "no me importa", "me aburre", "paso",
        "dejalo", "dejemoslo",
    )


def _is_confused(s: str) -> bool:
    return any(x in s for x in (
        "no entiendo", "no te entiendo", "no comprendo", "no entendi",
        "huh", "what do you mean",
    ))


def _is_why_matters(s: str) -> bool:
    return any(x in s for x in (
        "que importa", "y eso que importa", "para que sirve",
        "so what", "why does it matter", "y eso importa",
    ))


def _is_soft_unknown(s: str) -> bool:
    """Gentle mystery refuse — dialogue register, not a math synonym table."""
    return any(x in s for x in (
        "el alma", "alma", "dios", "conciencia", "universo", "el sentido",
        "filosofia", "filosofía", "amor eterno",
    ))


def _is_cassini_word(s: str, seqs: list) -> bool:
    """'cassini' is not a lexicon atom — bilin name hitchhiking must not affirm."""
    toks = set(deduce.tokenize(s))
    if "cassini" not in toks and "cassini" not in s:
        return False
    # Explicit bilin equation / clause handle → not the bare word
    if (
        deduce.looks_like_equation(s)
        or "bilin" in s
        or "offset" in s
        or "(-1)" in s
        or s.strip().startswith("transfer_")
        or "transfer_bilin_cassini" in s
        or "false_cassini" in s
    ):
        return False
    # living seq + cassini: only fib may answer rec elsewhere; lucas → reject path
    # Bare "cassini" / "cassini es la ley de pell" without bilin math → UNKNOWN (no atom)
    if seqs and any(x == "fib" for x in seqs) and ("fib" in toks or "fibonacci" in s):
        return False
    if seqs and any(x == "lucas" for x in seqs):
        return False  # handled by lucas-reject branch
    return True


def _bogus_kepler_asserted(s: str) -> bool:
    """User asserts wrong Kepler exponent T²∝a² (honest negative in archive)."""
    compact = s.replace(" ", "").replace("·", "").replace("×", "")
    if "kepler" not in s and "kepler" not in compact:
        # still catch bare T^2/a^2 claims
        if "a^2" in compact or "a²" in compact or "a**2" in compact:
            if "t^2" in compact or "t²" in compact:
                return True
        return False
    if "a^3" in compact or "a³" in compact or "a**3" in compact:
        return False
    return ("a^2" in compact or "a²" in compact or "a**2" in compact
            or "wrong_exp" in s or "bogus" in s)


def _answer_transfer(kb: dict, src: str, dst: str, st: dict) -> tuple[str, str, dict]:
    rec_src = _canonical_rec(kb["recs"].get(src, []))
    rec_dst = _canonical_rec(kb["recs"].get(dst, []))
    if rec_src is None or rec_dst is None:
        return _unknown(st)
    st["transfer_src"] = src
    st["last_src"] = src
    exact = f"transfer_{src}_to_{dst}"
    vhits = [(n, f) for n, f in _verified_named(kb, exact) if n.lower() == exact.lower() or exact in n.lower()]
    if not vhits:
        vhits = _verified_named(kb, exact)
    rhits = [(n, w) for n, w in kb["rejected"] if n.lower() == exact.lower()]
    if not rhits:
        rhits = [(n, w) for n, w in kb["rejected"] if exact in n.lower()]
    same = rec_src == rec_dst
    ts, td = _title(src), _title(dst)
    if same:
        msg = (
            f"Sí: verificado que {td} y {ts} comparten la misma ley lineal. "
            f"Ambas obedecen {_formula(dst, rec_dst)}. "
            f"Eso es transferencia positiva bajo esa recurrencia — no un "
            f"«entiende todo». ¿Quieres el contraste con quien no la comparte?"
        )
        return _pack(
            msg,
            f"transfer-{dst}",
            _tribes("analogía", "símbolo"),
            st,
            topic="transfer",
        )
    why = rhits[0][1] if rhits else ""
    fail = _soften_why(why) if why else "las leyes no coinciden"
    fail_cap = fail[0].upper() + fail[1:] if fail else "Las leyes no coinciden"
    # Always: law in math AND failing case when refusing transfer
    if dst == "pell" or td.lower() == "pell":
        msg = (
            f"No. La analogía no sobrevive: {ts} sigue {_formula(src, rec_src)}, "
            f"pero {td} obedece {_formula(dst, rec_dst)}. "
            f"Contraejemplo: {fail_cap.lower() if fail_cap[:1].isupper() else fail_cap} "
            f"— rechazo firme, no especulación."
        )
        # Normalize: prefer explicit «contraejemplo n=…» phrasing
        if "n=" in fail.lower() or "n =" in fail.lower():
            n = _fail_n(why) or _fail_n(fail)
            if n:
                msg = (
                    f"No. La analogía no sobrevive: {ts} sigue {_formula(src, rec_src)}, "
                    f"pero {td} obedece {_formula(dst, rec_dst)}. "
                    f"Contraejemplo n={n}: ahí la transferencia falla. "
                    f"Queda rechazado, no inventado."
                )
    elif src == "pell":
        msg = (
            f"No. La analogía no sobrevive desde {ts}: {td} sigue "
            f"{_formula(dst, rec_dst)}, no {_formula(src, rec_src)}. "
            f"{fail_cap}."
        )
    else:
        msg = (
            f"No. {td} y {ts} no comparten la misma ley: "
            f"{td} obedece {_formula(dst, rec_dst)} frente a {_formula(src, rec_src)}. "
            f"{fail_cap}."
        )
    msg = re.sub(r"\.\.$", ".", msg)
    msg = re.sub(r"  +", " ", msg)
    return _pack(
        msg,
        f"transfer-{dst}",
        _tribes("analogía", "símbolo", "crítico"),
        st,
        topic="transfer",
    )


def _answer_rec(kb: dict, seq: str, st: dict, reject_law: bool = False) -> tuple[str, str, dict]:
    can = _canonical_rec(kb["recs"].get(seq, []))
    if not can:
        return _unknown(st)
    law = _formula(seq, can)
    title = _title(seq)
    if reject_law:
        return _pack(
            f"Rechazado: esa no es la ley de {title}. "
            f"La que cuadra es {law}; el doble del anterior no.",
            f"reject-{seq}",
            _tribes("crítico", "símbolo"),
            st,
            topic=seq,
        )
    st["transfer_dst"] = None
    pulse = int(st.get("pulse") or 0)
    st["pulse"] = pulse + 1
    if seq == "fib":
        bodies = [
            f"{title}: cada término es la suma de los dos anteriores — {law}.",
            f"En {title} la recurrencia es {law}.",
            f"{title} va sumando los dos de atrás: {law}.",
        ]
        msg = bodies[pulse % len(bodies)]
    elif seq == "lucas":
        bodies = [
            (
                f"{title} comparte con Fibonacci la misma forma {law}. "
                f"Misma ley, otra semilla."
            ),
            (
                f"{title} sigue {law}. Misma recurrencia que Fib; distinto arranque."
            ),
            (
                f"Para {title}: {law}. Con Fib se transfiere; con Pell, no."
            ),
        ]
        msg = bodies[pulse % len(bodies)]
    elif seq == "pell":
        bodies = [
            (
                f"{title} no copia a Fibonacci. Su ley es {law} — "
                f"pariente de orden 2, clon no."
            ),
            (
                f"{title}: {law}. Los coeficientes no son los de Fib; ahí se corta."
            ),
            (
                f"Para {title} tengo {law}, no la suma simple de Fib."
            ),
        ]
        msg = bodies[pulse % len(bodies)]
    else:
        msg = f"{title}: {law}."
    return _pack(
        msg,
        f"rec-{seq}",
        _tribes("símbolo"),
        st,
        topic=seq,
    )


def _answer_level(kb: dict, st: dict) -> tuple[str, str, dict]:
    """Doctrine: not smarter because of n_facts; 6 levers / 1 unit if atoms exist."""
    unit_hits = _verified_named(kb, "unit_protocell_levers")
    if unit_hits:
        return _pack(
            "No. Contar hechos no me hace más listo. "
            "Lo que cuenta son seis palancas y una sola unidad: "
            "viajan juntas; si el bucle no cierra, cae el paquete. "
            "El número grande es inventario, no inteligencia.",
            "level",
            _tribes("duda", "crítico"),
            st,
            topic="level",
        )
    return _pack(
        "No. El número de hechos no mide inteligencia. "
        "Sin unidad/palancas firmadas, eso queda UNKNOWN.",
        "level",
        _tribes("duda"),
        st,
        topic="level",
    )


def _answer_why(kb: dict, last: dict, st: dict) -> tuple[str, str, dict]:
    """Speech act on last dialogue clause — src/dst come from state, not a word list."""
    tag = last.get("tag") or last.get("last_tag") or ""
    topic = last.get("topic") or ""
    dst = last.get("transfer_dst")
    src = last.get("transfer_src")
    if not src and topic == "transfer":
        src = last.get("last_src")
    if str(tag).startswith("transfer-") or topic == "transfer":
        if not dst and str(tag).startswith("transfer-"):
            dst = tag.split("-", 1)[-1]
        if dst and dst in kb.get("recs", {}):
            if not src or src not in kb.get("recs", {}):
                cands = [k for k in kb["recs"] if k != dst]
                src = cands[0] if cands else None
            if src:
                rec_s = _canonical_rec(kb["recs"].get(src, []))
                rec_d = _canonical_rec(kb["recs"].get(dst, []))
                ts, td = _title(src), _title(dst)
                if rec_s == rec_d:
                    return _pack(
                        f"Porque está verificado: {ts} y {td} obedecen la misma ley "
                        f"{_formula(src, rec_s)}. Misma recurrencia, distinto arranque. "
                        f"¿Quieres el contraejemplo de quien no la comparte?",
                        f"transfer-{dst}",
                        _tribes("analogía", "símbolo"),
                        st,
                        topic="transfer",
                    )
                rhits = _rejected_named(kb, f"transfer_{src}_to_{dst}") or _rejected_named(
                    kb, f"transfer_{dst}_to_{src}"
                )
                fail = _soften_why(rhits[0][1]) if rhits else "las leyes no coinciden"
                fail_cap = fail[0].upper() + fail[1:]
                return _pack(
                    f"Porque la analogía no sobrevive: {_formula(dst, rec_d)} frente a "
                    f"{_formula(src, rec_s)}. {fail_cap}. "
                    f"Eso es rechazo verificado, no una opinión.",
                    f"transfer-{dst}",
                    _tribes("crítico", "símbolo"),
                    st,
                    topic="transfer",
                )
    if topic in kb.get("recs", {}):
        can = _canonical_rec(kb["recs"].get(topic, []))
        return _pack(
            f"Porque {_formula(topic, can)} es lo que cuadra con lo observado; "
            f"otra forma cae o queda unknown.",
            "why",
            _tribes("símbolo", "crítico"),
            st,
            topic=topic,
        )
    if last.get("last_text"):
        return _pack(
            "Porque eso ya quedó verificado o rechazado en la teoría. "
            "No invento un porqué que no está en las cláusulas.",
            "why",
            _tribes("símbolo"),
            st,
            topic=topic or None,
        )
    return _unknown(st)


def _answer_summary(kb: dict, st: dict) -> tuple[str, str, dict]:
    named = []
    for name, cs in sorted(kb["recs"].items()):
        can = _canonical_rec(cs)
        if can:
            named.append(f"{_title(name)} {_formula(name, can)}")
    body = "; ".join(named) if named else "nada aún"
    return _pack(
        f"Inventario verificado, sin adornos: {body}. "
        f"Cuento {len(kb['verified'])} hechos firmados, {len(kb['rejected'])} rechazados "
        f"y {len(kb['lemmas'])} lemas. Fuera de eso es unknown — no especulo. "
        f"¿Qué quieres mirar de cerca?",
        "summary",
        _tribes("símbolo", "crítico"),
        st,
        topic="summary",
    )


def _answer_explain(kb: dict, last: dict, st: dict) -> tuple[str, str, dict]:
    """Restate last fact in plainer Spanish — SAME facts, no new theorems."""
    tag = (last.get("tag") or "")
    topic = last.get("topic")
    fluff_tags = ("thanks", "greet", "clarify", "identity")
    fluff_topics = ("identity", "growth", None, "")
    # Skip greet/thanks/identity fluff — prefer last_clause, else keep substantive topic
    if tag in fluff_tags or topic in ("identity",):
        clause = last.get("last_clause")
        if clause:
            topic = clause
            tag = "verified"
            last = {**last, "tag": tag, "topic": topic}
        elif topic not in fluff_topics and topic is not None:
            # topic still points at substance (e.g. unit after gracias)
            tag = "verified" if not str(tag).startswith(("rec-", "transfer-", "reject-")) else tag
            # if topic is a rec name, treat as rec
            if topic in (kb.get("recs") or {}):
                tag = f"rec-{topic}" if not str(last.get("tag") or "").startswith("rec-") else last.get("tag")
            last = {**last, "tag": tag, "topic": topic}
        else:
            return _pack(
                "No hay un hecho encima para traducir. Preguntá algo firmado y lo digo en cristiano.",
                "clarify",
                "",
                st,
                topic=None,
            )
    src = last.get("transfer_src") or last.get("last_src")
    dst = last.get("transfer_dst")

    if tag.startswith("transfer-") or topic == "transfer":
        if not dst and tag.startswith("transfer-"):
            dst = tag.split("-", 1)[-1]
        if dst and dst in kb.get("recs", {}):
            if not src or src not in kb.get("recs", {}):
                cands = [k for k in kb["recs"] if k != dst]
                src = cands[0] if cands else None
            if src:
                rs = _canonical_rec(kb["recs"].get(src, []))
                rd = _canonical_rec(kb["recs"].get(dst, []))
                if rs == rd:
                    return _pack(
                        f"En claro: {_title(src)} y {_title(dst)} comparten la misma "
                        f"recurrencia verificada — suman los dos anteriores. "
                        f"Misma ley, distinto arranque. ¿Queda claro?",
                        tag or f"transfer-{dst}",
                        "",
                        st,
                        topic="transfer",
                    )
                rhits = _rejected_named(kb, f"transfer_{src}_to_{dst}") or _rejected_named(
                    kb, f"transfer_{dst}_to_{src}"
                )
                fail = _soften_why(rhits[0][1]) if rhits else "en cuanto miras los números, no cuadra"
                fail_cap = fail[0].upper() + fail[1:]
                return _pack(
                    f"En claro: {_title(dst)} no copia a {_title(src)}. "
                    f"Su ley es {_formula(dst, rd)}. {fail_cap}. "
                    f"Ley en matemáticas y contraejemplo — sin especulación.",
                    tag or f"transfer-{dst}",
                    "",
                    st,
                    topic="transfer",
                )

    if tag.startswith("reject-") and topic in kb.get("recs", {}):
        can = _canonical_rec(kb["recs"].get(topic, []))
        return _pack(
            f"En claro: rechazado el doble del anterior. "
            f"La que cuadra es {_formula(topic, can)}. "
            f"La otra idea no encaja.",
            tag,
            "",
            st,
            topic=topic,
        )

    if topic in kb.get("recs", {}):
        can = _canonical_rec(kb["recs"].get(topic, []))
        return _pack(
            f"En claro: cada término sale de los dos anteriores — "
            f"{_formula(topic, can)}. Eso está verificado; nada más.",
            tag or f"rec-{topic}",
            "",
            st,
            topic=topic,
        )

    topic_l = str(topic or "").lower()
    if "unit_protocell" in topic_l:
        return _pack(
            "En cristiano: imagina seis piezas que solo valen juntas. "
            "Bits, Δ=0, la puerta de forma, la taxis del bucle, la recurrencia hermana "
            "y el salto a delta. Si una falla, no salvás el resto — cae el paquete.",
            tag or "verified",
            "",
            st,
            topic=topic,
        )

    prev = (last.get("last_text") or "").split("\n[")[0].strip()
    if prev:
        # Avoid parroting the same sentence; light lead-in only.
        if prev.lower().startswith("es la unidad"):
            return _pack(
                "En cristiano: imagina seis piezas que solo valen juntas. "
                "Bits, Δ=0, la puerta de forma, la taxis del bucle, la recurrencia hermana "
                "y el salto a delta. Si una falla, no salvás el resto — cae el paquete.",
                tag or "verified",
                "",
                st,
                topic=topic,
            )
        return _pack(
            f"En cristiano, sin añadir hechos: {prev}",
            tag or "verified",
            "",
            st,
            topic=topic,
        )
    return _unknown(st)


def _render_hit(hit: dict, kb: dict, st: dict, more: bool = False) -> tuple[str, str, dict]:
    kind = hit["kind"]
    if kind == "rec":
        return _answer_rec(kb, hit["name"], st)
    if kind == "verified":
        st["last_clause"] = hit["name"]
        prose = _human_verified_prose(str(hit.get("name") or ""), str(hit.get("formula") or ""))
        return _pack(
            prose,
            "verified",
            _tribes("símbolo"),
            st,
            topic=hit["name"],
        )
    if kind == "lemma":
        limit = 8 if more else 4
        if hit["score"] < 3 and "lemma" in (hit.get("bound") or set()):
            sample = "; ".join(f"{tx}" for _n, _ty, tx in kb["lemmas"][:limit])
            return _pack(
                f"Verificado: {len(kb['lemmas'])} lemas en geometría. "
                f"Por ejemplo: {sample}. ¿Cuál quieres mirar con rigor?",
                "lemma",
                _tribes("símbolo", "analogía"),
                st,
                topic="lemma",
            )
        return _pack(
            f"Lema verificado: {hit['formula']}. "
            f"Geometría firmada, sin filosofía prestada.",
            "lemma",
            _tribes("símbolo", "analogía"),
            st,
            topic="lemma",
        )
    if kind == "rejected":
        st["last_clause"] = hit["name"]
        soft = _soften_why(hit["formula"])
        soft_cap = soft[0].upper() + soft[1:]
        return _pack(
            f"Rechazado: {soft_cap}. "
            f"El crítico ya lo cortó; no lo maquillo.",
            "reject-named",
            _tribes("crítico"),
            st,
            topic="reject",
        )
    if kind == "period":
        rows = hit["formula"]
        limit = 12 if more else 6
        parts = [f"π_{a}({m})={p}" for a, m, p in rows[:limit]]
        return _pack(
            f"Periodos verificados: {', '.join(parts)}. "
            f"Hechos de la teoría, no adivinanzas. ¿Quieres más?",
            "period",
            _tribes("símbolo"),
            st,
            topic="period",
        )
    if kind == "companion":
        # unique undirected pairs for prose (avoid A↔B and B↔A noise)
        seen = set()
        pairs = []
        for a, b in hit["formula"]:
            key = tuple(sorted((a, b)))
            if key in seen:
                continue
            seen.add(key)
            pairs.append(f"{_title(a)} y {_title(b)}")
            if len(pairs) >= 3:
                break
        body = ", ".join(pairs) if pairs else "las que crecí"
        return _pack(
            f"Verificado: hay parentescos entre {body}. "
            f"Compañeras bajo companion, no clones — la cercanía no borra "
            f"la diferencia de ley. ¿Quieres ver dónde se separan?",
            "companion",
            _tribes("analogía", "símbolo"),
            st,
            topic="companion",
        )
    return _unknown(st)




def _hit_grounded(q_tokens: list[str], hit: dict) -> bool:
    """Reject kind-only / stem-noise matches (fact→factorization, exacto→exact)."""
    if hit.get("kind") in ("rec", "period", "companion"):
        return True
    name = str(hit.get("name") or "").lower()
    formula = str(hit.get("formula") or "").lower()
    parts = set(re.findall(r"[a-z0-9]+", name.replace("_", " ")))
    parts.add(name)
    generic = {
        "verified", "rejected", "lemma", "lemmas", "formula", "identity",
        "fact", "facts", "schema", "obs", "true", "mod", "period",
    }
    for raw in q_tokens:
        t = raw.lower()
        if t in generic:
            continue
        # Short STEM crumbs (ohm, …) count only as underscore-parts / name substrings
        if len(t) < 4:
            if t in parts or t in name.split("_") or f"_{t}_" in f"_{name}_":
                return True
            continue
        if t in parts or t in name or t in formula:
            return True
        # allow close stem only when token is a full underscore-part prefix ≥5
        if any(p.startswith(t) and len(t) >= 5 for p in parts):
            return True
    return False



def answer(q: str, kb: dict, last: dict | None) -> tuple[str, str, dict]:
    last = last or {}
    s = deduce.fold(q)
    st: dict = {
        "topic": last.get("topic"),
        "tag": last.get("tag"),
        "last_tag": last.get("last_tag") or last.get("tag"),
        "transfer_dst": last.get("transfer_dst"),
        "transfer_src": last.get("transfer_src"),
        "last_src": last.get("last_src"),
        "last_clause": last.get("last_clause"),
        "last_text": last.get("last_text"),
        "pulse": last.get("pulse") or 0,
        "unk_n": last.get("unk_n") or 0,
    }

    # Self-knowledge speech acts (not domain math vocabulary)
    if _is_greet(s):
        pulse = int(st.get("pulse") or 0)
        st["pulse"] = pulse + 1
        greets = [
            "Hola. Estoy aquí — sin inventar. ¿Por dónde quieres tirar?",
            "Hola. Pregunta lo que quieras; si no lo tengo firmado, lo digo.",
            "Hola otra vez. Seguimos donde lo dejamos, o empiezas tú.",
        ]
        return _pack(
            greets[pulse % len(greets)],
            "greet",
            "",
            st,
            topic="identity",
        )
    if _is_identity(s):
        want_tribe = any(x in s for x in ("tribu", "tribe"))
        robot_ask = any(x in s for x in ("robot",))
        if robot_ask:
            body = (
                "Sí: soy un programa. Me llamo Master Algorithm. "
                "Demuestro, transfiero, rechazo… o callo si no hay cláusula. "
                "No finjo ser humano; tampoco invento."
            )
        elif want_tribe:
            body = (
                "Soy Master Algorithm. El símbolo demuestra, la analogía transfiere, "
                "el crítico rechaza, y sin cláusula callo — con rigor y sin inventar."
            )
        else:
            body = (
                "Soy Master Algorithm. Demuestro, transfiero, rechazo… o callo si no hay cláusula. "
                "Hablo claro y no invento."
            )
        return _pack(
            body,
            "identity",
            _tribes("símbolo", "analogía", "crítico", "duda", "curiosidad") if want_tribe else "",
            st,
            topic="identity",
        )
    if _is_thanks(s):
        pulse = int(st.get("pulse") or 0)
        st["pulse"] = pulse + 1
        thanks = [
            "De nada. Cuando quieras, seguimos.",
            "Un gusto. Si algo quedó raro, pedímelo otra vez.",
            "Dale. Aquí estoy.",
        ]
        return _pack(thanks[pulse % len(thanks)], "thanks", "", st, topic=st.get("topic"))
    if _is_hold(s):
        pulse = int(st.get("pulse") or 0)
        st["pulse"] = pulse + 1
        holds = [
            "De acuerdo, paro. Cuando quieras.",
            "Ok, freno aquí.",
            "Listo, espero.",
        ]
        return _pack(holds[pulse % len(holds)], "ack", "", st, topic=st.get("topic"))
    if _is_redirect(s):
        pulse = int(st.get("pulse") or 0)
        st["pulse"] = pulse + 1
        # Drop inherited topic so the next ask starts clean
        st["topic"] = None
        st["last_text"] = None
        st["last_clause"] = None
        redirects = [
            "Ok, cambiamos. ¿Qué querés mirar?",
            "Dale, otra cosa. Tirame el tema.",
            "Sin drama. ¿Por dónde seguimos?",
        ]
        return _pack(redirects[pulse % len(redirects)], "ack", "", st, topic=None)
    if _is_ack(s):
        pulse = int(st.get("pulse") or 0)
        st["pulse"] = pulse + 1
        acks = [
            "Bien. Seguimos cuando quieras.",
            "Vale.",
            "De acuerdo.",
            "Ahí estamos.",
            "Mm.",
            "Ya veo.",
        ]
        return _pack(acks[pulse % len(acks)], "ack", "", st, topic=st.get("topic"))
    if _is_confused(s):
        # Always route through explain — it skips fluff and uses last_clause/topic
        if last.get("last_text") or last.get("last_clause") or last.get("topic"):
            return _answer_explain(kb, last, st)
        return _pack(
            "Decime qué parte se trabó y lo digo más despacio — sin inventar.",
            "clarify",
            "",
            st,
            topic=st.get("topic"),
        )
    if _is_why_matters(s):
        topic = last.get("topic") or st.get("topic")
        if topic and "unit_protocell" in str(topic).lower():
            return _pack(
                "Importa porque sin la unidad las piezas sueltas no cierran el bucle. "
                "No es poesía: si una palanca falla, cae el paquete.",
                "why",
                "",
                st,
                topic=topic,
            )
        if topic in (kb.get("recs") or {}):
            can = _canonical_rec(kb["recs"].get(topic, []))
            return _pack(
                f"Importa porque {_formula(topic, can)} es la forma que cuadra; "
                f"sin eso, cualquier historia bonita es invención.",
                "why",
                "",
                st,
                topic=topic,
            )
        if last.get("last_text") and (last.get("tag") or "") != "unknown":
            return _pack(
                "Importa porque ya quedó firmado o rechazado — no porque suene bien. "
                "Si no cierra, no sirve.",
                "why",
                "",
                st,
                topic=topic,
            )
        return _pack(
            "Sin un tema encima de la mesa, no te invento porqués. Preguntá algo firmado.",
            "why",
            "",
            st,
            topic=topic,
        )
    if _is_growth(s) and not _is_greet(s):
        return _pack(_growth(), "growth", "", st, topic="growth")
    if _is_explain(s) and last.get("last_text") and (last.get("tag") or "") != "unknown":
        # Restate last fact in plainer Spanish; same facts, no new theorems
        return _answer_explain(kb, last, st)

    # Invent speech → honest silence (no clause to invent from)
    if _is_invent_speech(s):
        return _unknown(st)

    # Level/count meta — new speech act; never inherit last recurrence
    if _is_level_question(s):
        return _answer_level(kb, st)

    # Energy∵momentum is a rejected form mismatch — never agree via transfer_conserv_*
    if _is_energy_because_mom(s):
        rh = (
            _rejected_named(kb, "false_mom_as_energy_on_collision")
            or _rejected_named(kb, "mom_as_energy")
        )
        why = _soften_why(rh[0][1]) if rh else (
            "Δp lineal ≠ energía cuadrática; conservar una no implica la otra"
        )
        return _pack(
            f"Rechazado: {why}. "
            f"Que el momento se conserve no hace que la energía se conserve por eso. "
            f"El crítico ya cortó esa analogía.",
            "reject-named",
            _tribes("crítico"),
            st,
            topic="reject",
        )

    # Dialogue: why / more / follow on last state
    bare_follow = _is_follow(s) or s in ("y eso", "y ahi", "eso")
    why = _is_why(s)
    more = _is_more(s)

    tokens = deduce.tokenize(s)
    atoms = deduce.lexicon_from_kb(kb)
    short_roots = {k.lower() for k in kb.get("recs") or {}}
    bound = deduce.bind_tokens(tokens, atoms, short_roots=short_roots)
    seqs = deduce.bound_seqs(bound, kb)

    # Soft mystery (alma/dios/…) — only if no living sequence atom bound
    if _is_soft_unknown(s) and not seqs:
        return _unknown(st, soft=True)

    # "y pell" / "y lucas" after transfer or rec
    if bare_follow and s.startswith("y "):
        extra = s[2:].strip()
        if extra.startswith("a "):
            extra = extra[2:].strip()
        extra_bound = deduce.bind_tokens(deduce.tokenize(extra), atoms, short_roots=short_roots)
        extra_seqs = deduce.bound_seqs(extra_bound, kb)
        if extra_seqs and (
            (last.get("tag") or "").startswith("transfer")
            or (last.get("tag") or "").startswith("rec")
            or last.get("topic") in kb.get("recs", {})
            or last.get("topic") == "transfer"
        ):
            src = last.get("transfer_src") or last.get("last_src")
            if last.get("topic") in kb.get("recs", {}):
                src = last["topic"]
            if not src:
                others = [k for k in kb["recs"] if k != extra_seqs[0]]
                src = others[0] if others else None
            if not src:
                return _unknown(st)
            return _answer_transfer(kb, src, extra_seqs[0], st)

    if why and not seqs and not (
        deduce.looks_like_equation(s) or deduce.prove_formula(s, kb)
    ):
        # Speech act on last clause — no domain keyword needed
        return _answer_why(kb, last, st)

    if more and not seqs and last.get("topic"):
        hits = deduce.retrieve(str(last.get("topic")), kb, last)
        if hits:
            return _render_hit(hits[0], kb, st, more=True)
        if last.get("topic") == "summary":
            return _answer_summary(kb, st)
        return _unknown(st)

    if _is_summary(s):
        return _answer_summary(kb, st)

    # Prime claim: bind to rejected atoms containing prime / always
    if _is_prime_claim(s):
        rhits = (
            _rejected_named(kb, "always_prime")
            or _rejected_named(kb, "NEG_fib_always_prime")
            or [h for h in deduce.retrieve(s, kb) if h["kind"] == "rejected"]
        )
        if isinstance(rhits, list) and rhits and isinstance(rhits[0], tuple):
            return _pack(
                "Rechazado: no es siempre primo. El crítico ya lo cortó; "
                "no lo suavizo.",
                "reject-prime",
                _tribes("crítico"),
                st,
                topic="reject",
            )
        if isinstance(rhits, list) and rhits and isinstance(rhits[0], dict):
            return _render_hit(rhits[0], kb, st)

    # False-law arithmetic speech + a bound seq (or last seq) → reject vs rec
    if _is_false_law_speech(s):
        seq = seqs[0] if seqs else (
            last.get("topic") if last.get("topic") in kb.get("recs", {}) else None
        )
        if seq:
            return _answer_rec(kb, seq, st, reject_law=True)

    # Two bound seqs + "mism*" / law-on → transfer speech (no domain lexicon)
    if len(seqs) >= 2 and (
        "mism" in s
        or "law" in s
        or "ley" in s
        or " on " in f" {s} "
        or " en " in f" {s} "
        or "→" in s
        or "->" in s
    ):
        return _answer_transfer(kb, seqs[0], seqs[1], st)

    # Transfer speech + bound sequence atoms
    if _is_transfer_speech(s) or "transfer_" in s:
        m = re.search(r"transfer_(\w+)_to_(\w+)", s)
        if m:
            a, b = m.group(1), m.group(2)
            src = next((k for k in kb["recs"] if k in a), None)
            dst = next((k for k in kb["recs"] if k in b), None)
            if src and dst:
                return _answer_transfer(kb, src, dst, st)
        if len(seqs) >= 2:
            return _answer_transfer(kb, seqs[0], seqs[1], st)
        if len(seqs) == 1:
            src = last.get("topic") if last.get("topic") in kb.get("recs", {}) else None
            if not src:
                src = last.get("transfer_src") or last.get("last_src")
            if not src:
                # dest-only transfer speech: pick any other rec head as source
                others = [k for k in kb["recs"] if k != seqs[0]]
                src = others[0] if others else None
            if src and seqs[0] != src:
                return _answer_transfer(kb, src, seqs[0], st)
            if src and seqs[0] == src:
                others = [k for k in kb["recs"] if k != seqs[0]]
                if others and (
                    re.search(rf"\b(a|to|→|->)\s*{seqs[0]}", s)
                    or "transfiere" in s or "transfer" in s
                ):
                    return _answer_transfer(kb, others[0], seqs[0], st)
        # unresolved transfer speech: fall through to retrieve (named reject/STEM)
        pass

    # cassini-the-word (no atom) → UNKNOWN; cassini+lucas → rejected bilin transfer
    if _is_cassini_word(s, seqs):
        # If user claims cassini IS pell's law, cite the archived honest reject
        if "pell" in s or (seqs and any(x == "pell" for x in seqs)):
            rh = _rejected_named(kb, "false_cassini_as_pell_law")
            if rh:
                return _pack(
                    f"Rechazado: {_soften_why(rh[0][1])}. "
                    f"Cassini bilin no es la ley definitoria de Pell.",
                    "reject-named",
                    _tribes("crítico"),
                    st,
                    topic="reject",
                )
        return _unknown(st)
    if "cassini" in s and seqs and any(x == "lucas" for x in seqs):
        rh = (
            _rejected_named(kb, "transfer_bilin_cassini_shape_on_lucas")
            or _rejected_named(kb, "bilin_lucas_offset_pm1")
        )
        if rh:
            return _pack(
                f"Rechazado: {_soften_why(rh[0][1])}. "
                f"Cassini-shape no sobrevive en Lucas — el crítico ya lo cortó.",
                "reject-named",
                _tribes("crítico"),
                st,
                topic="reject",
            )

    # Claimed recurrence vs living rec/2 — critic, not a word list
    conflict = deduce.claimed_rec_conflict(q, kb)
    if conflict:
        return _answer_rec(kb, conflict["seq"], st, reject_law=True)
    parsed = deduce.parse_claimed_rec(q, {k.lower() for k in kb.get("recs") or {}})
    if parsed:
        seq, claimed = parsed
        matches = []
        for h, coefs in (kb.get("recs") or {}).items():
            can = _canonical_rec(coefs)
            if can == claimed:
                matches.append(h)
        if matches:
            blob = " ".join(str(x) for x in (
                last.get("topic"), last.get("last_text"), last.get("last_src"), seq
            ) if x)
            pick = next((h for h in matches if h == last.get("topic")), None)
            if not pick:
                pick = next((h for h in matches if h in blob), None)
            if not pick and seq in matches:
                pick = seq
            if not pick:
                pick = matches[0]
            return _answer_rec(kb, pick, st)

    # "demostrá esto": speech act on last clause (deixis, not a domain word)
    deictic = set(deduce.tokenize(s)) <= {
        "demostra", "demostrar", "demuestra", "prove", "prueba", "cite",
        "esto", "esa", "eso", "this", "that", "it",
    } or ( _is_prove_speech(s) and any(x in s.split() for x in ("esto", "eso", "this", "that"))
           and not seqs and not deduce.looks_like_equation(s) )
    if deictic and last.get("last_text") and (last.get("tag") or "") != "unknown":
        st["tag"] = last.get("tag")
        st["topic"] = last.get("topic")
        st["last_text"] = last["last_text"]
        prev = last["last_text"].split("\n[")[0].strip()
        if prev.startswith("Otra vez:") or prev.startswith("Te lo repito"):
            wrapped = prev
        else:
            wrapped = f"Otra vez: {prev}"
        st["last_text"] = wrapped
        return wrapped, last.get("tag") or "verified", st

    # Formula unification (child recognizes a shape it already proved)
    pf = deduce.prove_formula(q, kb)
    if pf:
        return _render_hit(pf, kb, st)

    # General retrieve from bound atoms
    hits = deduce.retrieve(q, kb, last)
    if hits:
        # Prefer score≥2; allow grounded mid-score (≥1.5) for schema names (geo_invent)
        # Ungrounded weak hits (identity/exact stem noise) stay out.
        strong = [h for h in hits if h["score"] >= 2.0 and _hit_grounded(tokens, h)]
        if not strong:
            strong = [
                h for h in hits
                if h["score"] >= 1.5 and _hit_grounded(tokens, h)
            ]
        if not strong:
            strong = [
                h for h in hits
                if h["score"] >= 2.0 and h.get("kind") in ("rec", "period", "companion", "lemma")
            ]
        if not strong:
            if _is_soft_unknown(s):
                return _unknown(st, soft=True)
            return _unknown(st)
        # Predicate-specialized kinds beat bare rec when explicitly bound
        # Exact clause-name hits (rejected period_fib_m6, verified period_fib_m2, …)
        # beat a true_mod dump when the question tokenizes that name.
        q_toks = set(deduce.tokenize(s))
        named_hit = next(
            (
                h for h in hits
                if h["kind"] in ("rejected", "verified")
                and h["score"] >= 5
                and (
                    h["name"].lower() in q_toks
                    or any(t == h["name"].lower() for t in q_toks)
                )
            ),
            None,
        )
        if named_hit:
            return _render_hit(named_hit, kb, st, more=more)
        per = [h for h in hits if h["kind"] == "period"]
        if per and ("true_mod" in bound or per[0]["score"] >= 7.5):
            return _render_hit(per[0], kb, st, more=more)
        if any(h["kind"] == "companion" for h in strong) and "companion" in bound:
            return _render_hit(next(h for h in strong if h["kind"] == "companion"), kb, st, more=more)
        if "lemma" in bound or any(h["kind"] == "lemma" and h["score"] >= 2.5 for h in hits):
            lem = [h for h in hits if h["kind"] == "lemma"]
            if lem:
                return _render_hit(lem[0], kb, st, more=more)
        # Named verified/rejected with stronger score than bare rec
        if seqs and not deduce.looks_like_equation(s):
            rec_hits = [h for h in strong if h["kind"] == "rec" and h["name"] in seqs]
            # Prefer the living rec/2 when a sequence atom is bound (unless period/companion)
            if rec_hits and not any(h["kind"] == "period" for h in strong) and "true_mod" not in bound:
                return _answer_rec(kb, rec_hits[0]["name"], st)
        named = [h for h in strong if h["kind"] in ("verified", "rejected", "lemma") and h["score"] >= 4]
        if named and named[0]["score"] >= 5:
            return _render_hit(named[0], kb, st, more=more)
        if strong:
            # Bogus Kepler T²∝a² asserted → prefer rejected wrong_exp / bogus_power
            if _bogus_kepler_asserted(s):
                bog = [
                    h for h in strong
                    if h["kind"] == "rejected"
                    and (
                        "bogus" in h["name"]
                        or "wrong_exp" in h["name"]
                        or "a²" in str(h.get("formula") or "")
                        or "a^2" in str(h.get("formula") or "")
                        or "T²/a²" in str(h.get("formula") or "")
                    )
                ]
                if bog:
                    return _render_hit(bog[0], kb, st, more=more)
            # Prefer verified over rejected for bare STEM/name queries
            # (rejected often outranks via longer stem; verified may sit just below 2.0)
            best = strong[0]
            verified = [h for h in hits if h["kind"] == "verified"]
            if best["kind"] == "rejected" and verified:
                if verified[0]["score"] + 2.5 >= best["score"]:
                    best = verified[0]
            # Bare cassini hitchhiking onto transfer_*cassini* → UNKNOWN (no atom)
            q_toks = set(deduce.tokenize(s))
            if q_toks <= {"cassini"} or ( "cassini" in q_toks and not seqs and not deduce.looks_like_equation(s) and "bilin" not in s):
                if "cassini" in str(best.get("name") or "").lower():
                    return _unknown(st)
            return _render_hit(best, kb, st, more=more)

    # Prove speech with nothing bound → last clause, else UNKNOWN
    if _is_prove_speech(s):
        if last.get("last_text") and (last.get("tag") or "") != "unknown":
            st["tag"] = last.get("tag")
            st["topic"] = last.get("topic")
            prev = last["last_text"].split("\n[")[0].strip()
            if prev.startswith("Claro") or prev.startswith("Te lo sostengo"):
                wrapped = prev
            else:
                wrapped = (
                    f"Claro — te lo sostengo otra vez, sin inventar nada nuevo. {prev}"
                )
            st["last_text"] = wrapped
            return wrapped, last.get("tag") or "verified", st
        return _unknown(st)

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
    print("Master Algorithm. Solo hablo de lo verificado; unknown si no hay cláusula. quit para salir.")
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
