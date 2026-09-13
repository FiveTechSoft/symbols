# MOLT_ROUND=1
"""Natural-language mouth over theory.pl. Never invents. Spanish first.

Doctrine: the brain is theory.pl. Vocabulary is a byproduct of growth.
Answers assemble from deduced clauses — no adult synonym dictionaries.
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
    letter = seq[:1].upper() if seq else "X"
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


def _unknown(st: dict) -> tuple[str, str, dict]:
    st["tag"] = "unknown"
    return (
        "UNKNOWN. No hay cláusula en theory.pl que unifique con eso.\n"
        "No invento.\n[duda · crítico]",
        "unknown",
        st,
    )


def _pack(text: str, tag: str, tribes: str, st: dict, topic=None) -> tuple[str, str, dict]:
    st["tag"] = tag
    st["last_tag"] = tag
    if topic is not None:
        st["topic"] = topic
    if tag.startswith("transfer-"):
        st["transfer_dst"] = tag.split("-", 1)[-1]
    if "\n[" not in text:
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

def _is_identity(s: str) -> bool:
    if any(x in s for x in (
        "quien eres", "que eres", "who are you", "what are you",
        "tu tribu", "de que tribu", "your tribe", "what is your tribe",
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
        "que sabes hacer", "inventario",
    ))


def _is_why(s: str) -> bool:
    return any(x in s for x in ("por que", "porque", "why", "y eso", "y ahi"))


def _is_more(s: str) -> bool:
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
    return any(x in s for x in (
        "el doble", "siempre 2", "siempre dos", "es 2*", "order 1", "orden 1",
        "solo el anterior", "2 veces", "dos veces", "veces el anterior",
        "doble del anterior", "doble siempre",
    ))


def _is_prime_claim(s: str) -> bool:
    return bool(re.search(
        r"(siempre\s+primo|primo\s+siempre|always\s+prime|prime\s+always|"
        r"todos\s+los\s+\w+\s+son\s+primos|never\s+composite|always_prime)",
        s,
    ))


def _is_invent_speech(s: str) -> bool:
    return bool(re.search(r"\b(inventa|inventame|teorema\s+nuevo|crea\s+una\s+ley)\b", s))


def _answer_transfer(kb: dict, src: str, dst: str, st: dict) -> tuple[str, str, dict]:
    rec_src = _canonical_rec(kb["recs"].get(src, []))
    rec_dst = _canonical_rec(kb["recs"].get(dst, []))
    if rec_src is None or rec_dst is None:
        return _unknown(st)
    st["transfer_src"] = src
    st["last_src"] = src
    vhits = _verified_named(kb, f"transfer_{src}_to_{dst}")
    rhits = _rejected_named(kb, f"transfer_{src}_to_{dst}")
    same = rec_src == rec_dst
    if same:
        if vhits:
            name, formula = vhits[0]
            return _pack(
                f"Sí se transfiere. Analogía: misma ley. "
                f"Símbolo: rec({src},{rec_src}) y rec({dst},{rec_dst}). "
                f"verified {name}: {formula}.",
                f"transfer-{dst}",
                _tribes("analogía", "símbolo"),
                st,
                topic="transfer",
            )
        return _pack(
            f"Sí se transfiere. Analogía: misma ley. "
            f"Símbolo: rec({src},{rec_src}) y rec({dst},{rec_dst}).",
            f"transfer-{dst}",
            _tribes("analogía", "símbolo"),
            st,
            topic="transfer",
        )
    why = rhits[0][1] if rhits else "pred != obs / ley distinta"
    rname = rhits[0][0] if rhits else None
    cite = f"rejected('{rname}'): {why}" if rname else f"rec({dst},{rec_dst}) vs rec({src},{rec_src})"
    return _pack(
        f"No se transfiere. Analogía: companion, no clon. "
        f"Símbolo: rec({dst},{rec_dst}) vs rec({src},{rec_src}). "
        f"Crítico {cite}.",
        f"transfer-{dst}",
        _tribes("analogía", "símbolo", "crítico"),
        st,
        topic="transfer",
    )


def _answer_rec(kb: dict, seq: str, st: dict, reject_law: bool = False) -> tuple[str, str, dict]:
    can = _canonical_rec(kb["recs"].get(seq, []))
    if not can:
        return _unknown(st)
    vhits = _verified_named(kb, f"rec_{seq}")
    cite = f"verified {vhits[0][0]}" if vhits else f"rec({seq},{can})"
    if reject_law:
        rhits = (
            _rejected_named(kb, f"rec_{seq}_o1")
            or _rejected_named(kb, f"rec_{seq}_order")
            or _rejected_named(kb, "o1_none")
        )
        why = rhits[0][1] if rhits else "no fit"
        rname = rhits[0][0] if rhits else f"rec_{seq}_o1"
        return _pack(
            f"Rechazado. No es esa ley. La ley es {_formula(seq, can)}. "
            f"rec({seq},{can}). Crítico rejected('{rname}'): {why}.",
            f"reject-{seq}",
            _tribes("crítico", "símbolo"),
            st,
            topic=seq,
        )
    st["transfer_dst"] = None
    return _pack(
        f"{seq}: {_formula(seq, can)}. "
        f"rec({seq},{can}). {cite}.",
        f"rec-{seq}",
        _tribes("símbolo"),
        st,
        topic=seq,
    )


def _answer_why(kb: dict, last: dict, st: dict) -> tuple[str, str, dict]:
    """Speech act on last dialogue clause — src/dst come from state, not a word list."""
    tag = last.get("tag") or last.get("last_tag") or ""
    topic = last.get("topic") or ""
    dst = last.get("transfer_dst")
    src = last.get("transfer_src")
    if not src and topic == "transfer":
        # recover src from tag like transfer-X or from last rec topic
        src = last.get("last_src")
    if str(tag).startswith("transfer-") or topic == "transfer":
        if not dst and str(tag).startswith("transfer-"):
            dst = tag.split("-", 1)[-1]
        if dst and dst in kb.get("recs", {}):
            # pick src: stored, or any other rec that has transfer clauses with dst
            if not src or src not in kb.get("recs", {}):
                # prefer a rec that differs from dst and appears in transfer_* names
                cands = [k for k in kb["recs"] if k != dst]
                src = cands[0] if cands else None
            if src:
                rec_s = _canonical_rec(kb["recs"].get(src, []))
                rec_d = _canonical_rec(kb["recs"].get(dst, []))
                if rec_s == rec_d:
                    vhits = _verified_named(kb, f"transfer_{src}_to_{dst}")
                    cite = (
                        f"verified {vhits[0][0]}: {vhits[0][1]}"
                        if vhits else f"rec({src},{rec_s}) = rec({dst},{rec_d})"
                    )
                    return _pack(
                        f"Porque comparten la ley: rec({src},{rec_s}) y rec({dst},{rec_d}). {cite}.",
                        f"transfer-{dst}",
                        _tribes("analogía", "símbolo"),
                        st,
                        topic="transfer",
                    )
                rhits = _rejected_named(kb, f"transfer_{src}_to_{dst}") or _rejected_named(
                    kb, f"transfer_{dst}_to_{src}"
                )
                if rhits:
                    return _pack(
                        f"Porque la ley no coincide: rec({dst},{rec_d}) vs rec({src},{rec_s}). "
                        f"Contraejemplo rejected('{rhits[0][0]}'): {rhits[0][1]}.",
                        f"transfer-{dst}",
                        _tribes("crítico", "símbolo"),
                        st,
                        topic="transfer",
                    )
                return _pack(
                    f"Porque la ley no coincide: rec({dst},{rec_d}) vs rec({src},{rec_s}).",
                    f"transfer-{dst}",
                    _tribes("crítico", "símbolo"),
                    st,
                    topic="transfer",
                )
    if topic in kb.get("recs", {}):
        can = _canonical_rec(kb["recs"].get(topic, []))
        return _pack(
            f"Porque rec({topic},{can}) es la ley que cabe en obs/3.",
            "why",
            _tribes("símbolo", "crítico"),
            st,
            topic=topic,
        )
    if last.get("last_clause"):
        return _pack(
            f"Porque verified/rejected {last['last_clause']}.",
            "why",
            _tribes("símbolo"),
            st,
            topic=topic or None,
        )
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
        f"{len(kb['lemmas'])} lemma. Fuera de theory.pl, silencio.",
        "summary",
        _tribes("símbolo", "crítico"),
        st,
        topic="summary",
    )


def _render_hit(hit: dict, kb: dict, st: dict, more: bool = False) -> tuple[str, str, dict]:
    kind = hit["kind"]
    if kind == "rec":
        return _answer_rec(kb, hit["name"], st)
    if kind == "verified":
        st["last_clause"] = hit["name"]
        return _pack(
            f"Demostrado: {hit['formula']}. verified {hit['name']}.",
            "verified",
            _tribes("símbolo"),
            st,
            topic=hit["name"],
        )
    if kind == "lemma":
        limit = 8 if more else 4
        # If weak predicate-only bind, list several lemmas
        if hit["score"] < 3 and "lemma" in (hit.get("bound") or set()):
            sample = "; ".join(f"{n}: {tx}" for n, _ty, tx in kb["lemmas"][:limit])
            return _pack(
                f"Lemas ({len(kb['lemmas'])}): {sample}. lemma/3.",
                "lemma",
                _tribes("símbolo", "analogía"),
                st,
                topic="lemma",
            )
        return _pack(
            f"Lema {hit['name']}: {hit['formula']}. lemma/3.",
            "lemma",
            _tribes("símbolo", "analogía"),
            st,
            topic="lemma",
        )
    if kind == "rejected":
        st["last_clause"] = hit["name"]
        return _pack(
            f"Rechazado. rejected('{hit['name']}'): {hit['formula']}.",
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
            f"Periodos: {', '.join(parts)}. true_mod/3.",
            "period",
            _tribes("símbolo"),
            st,
            topic="period",
        )
    if kind == "companion":
        pairs = ", ".join(f"companion({a},{b})" for a, b in hit["formula"][:6])
        return _pack(
            f"{pairs}. companion/2.",
            "companion",
            _tribes("analogía", "símbolo"),
            st,
            topic="companion",
        )
    return _unknown(st)


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
    }

    # Self-knowledge speech acts (not domain math vocabulary)
    if _is_identity(s):
        return _pack(
            "Soy Master Algorithm. Mi tribu es la que unifica: "
            "el símbolo demuestra, la analogía transfiere, el crítico rechaza, "
            "la duda guarda silencio sin cláusula, y la curiosidad elige el próximo tick. "
            f"Memoria viva: {kb['n']} verified/1.",
            "identity",
            _tribes("símbolo", "analogía", "crítico", "duda", "curiosidad"),
            st,
            topic="identity",
        )
    if _is_growth(s):
        return _pack(_growth(), "growth", _tribes("curiosidad", "crítico"), st, topic="growth")

    # Invent speech → honest silence (no clause to invent from)
    if _is_invent_speech(s):
        return _unknown(st)

    # Dialogue: why / more / follow on last state
    bare_follow = _is_follow(s) or s in ("y eso", "y ahi", "eso")
    why = _is_why(s)
    more = _is_more(s)

    tokens = deduce.tokenize(s)
    atoms = deduce.lexicon_from_kb(kb)
    short_roots = {k.lower() for k in kb.get("recs") or {}}
    bound = deduce.bind_tokens(tokens, atoms, short_roots=short_roots)
    seqs = deduce.bound_seqs(bound, kb)

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
                f"Rechazado. rejected('{rhits[0][0]}'): {rhits[0][1]}. No es siempre primo.",
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

    # Two bound seqs + "mism*" → transfer speech (no domain lexicon)
    if len(seqs) >= 2 and "mism" in s:
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
        return _unknown(st)

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
        return last["last_text"], last.get("tag") or "verified", st

    # Formula unification (child recognizes a shape it already proved)
    pf = deduce.prove_formula(q, kb)
    if pf:
        return _render_hit(pf, kb, st)

    # General retrieve from bound atoms
    hits = deduce.retrieve(q, kb, last)
    if hits:
        strong = [h for h in hits if h["score"] >= 2.0] or hits
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
            return _render_hit(strong[0], kb, st, more=more)

    # Prove speech with nothing bound → last clause, else UNKNOWN
    if _is_prove_speech(s):
        if last.get("last_text") and (last.get("tag") or "") != "unknown":
            st["tag"] = last.get("tag")
            st["topic"] = last.get("topic")
            return last["last_text"], last.get("tag") or "verified", st
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
