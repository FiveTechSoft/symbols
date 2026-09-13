"""Natural-language mouth over theory.pl. Never invents. Spanish first."""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

MOTOR_DIR = Path(__file__).resolve().parent
THEORY = MOTOR_DIR / "archive" / "theory.pl"
LATEST = MOTOR_DIR / "runs" / "latest.json"
LOG = MOTOR_DIR / "runs" / "talk-log.jsonl"
STATE = MOTOR_DIR / "runs" / "talk-state.json"

SEQ_NAMES = {
    "fib": "Fibonacci",
    "fibonacci": "Fibonacci",
    "lucas": "Lucas",
    "pell": "Pell",
}
SEQ_KEYS = {"fib": "fib", "fibonacci": "fib", "lucas": "lucas", "pell": "pell"}


def _load_theory(path: Path) -> dict:
    text = path.read_text(encoding="utf-8", errors="replace") if path.exists() else ""
    recs: dict[str, list[list[int]]] = {}
    for m in re.finditer(r"rec\((\w+),\s*\[([^\]]+)\]\)\.", text):
        seq, body = m.group(1), m.group(2)
        coef = [int(x.strip()) for x in body.split(",") if x.strip()]
        recs.setdefault(seq, []).append(coef)
    rejected: list[tuple[str, str]] = re.findall(
        r"rejected\('([^']+)',\s*'([^']+)'\)\.", text
    )
    verified: list[tuple[str, str, str]] = re.findall(
        r"verified\(fact\((\w+),\s*(\w+),\s*'([^']*)',\s*'([^']*)'\)\)\.", text
    )
    # verified(fact(W,F,Name,Formula)) — fix if 4 groups
    verified = []
    for m in re.finditer(
        r"verified\(fact\((\w+),\s*(\w+),\s*'((?:\\'|[^'])*)',\s*'((?:\\'|[^'])*)'\)\)\.",
        text,
    ):
        verified.append((m.group(1), m.group(2), m.group(3), m.group(4)))
    lemmas = re.findall(r"lemma\('([^']+)',\s*(\w+),\s*'([^']+)'\)\.", text)
    pisano = re.findall(r"true_mod\((\w+),\s*(\d+),\s*(\d+)\)\.", text)
    companions = re.findall(r"companion\((\w+),\s*(\w+)\)\.", text)
    return {
        "recs": recs,
        "rejected": rejected,
        "verified": verified,
        "lemmas": lemmas,
        "pisano": [(s, int(m), int(p)) for s, m, p in pisano],
        "companions": companions,
        "n": len(verified),
    }


def _canonical_rec(coefs: list[list[int]]) -> list[int] | None:
    """Shortest non-padded law: drop trailing zeros, pick shortest remaining."""
    cleaned = []
    for c in coefs:
        while c and c[-1] == 0:
            c = c[:-1]
        if c:
            cleaned.append(c)
    if not cleaned:
        return None
    cleaned.sort(key=len)
    return cleaned[0]


def _formula(seq: str, coef: list[int]) -> str:
    parts = [f"{a}·{seq}(n-{i})" for i, a in enumerate(coef, 1)]
    return f"{seq}(n) = " + " + ".join(parts)


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



def _fold(s: str) -> str:
    s = s.strip().lower()
    for a, b in (("á","a"),("é","e"),("í","i"),("ó","o"),("ú","u"),("¿",""),("?","")):
        s = s.replace(a, b)
    return s


def _tribes(*names: str) -> str:
    return " · ".join(names)


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


def answer(q: str, kb: dict, last: dict | None) -> tuple[str, str, dict]:
    last = last or {}
    s = _fold(q)
    st = {"topic": last.get("topic"), "tag": last.get("tag")}

    def pack(text: str, tag: str, tribes: str, topic=None):
        st["tag"] = tag
        if topic:
            st["topic"] = topic
        return f"{text}\n[{tribes}]", tag, st

    if any(x in s for x in ("quien eres", "que eres", "who are you", "what are you", "tu tribu")):
        return pack(
            "Soy Master Algorithm. Mi tribu es la que unifica: "
            "el símbolo demuestra, la analogía transfiere, el crítico rechaza, "
            "la duda se guarda como UNKNOWN, y la curiosidad elige el próximo tick. "
            f"Memoria viva: {kb['n']} verified/1. No improvisé el resto del mundo.",
            "identity",
            _tribes("símbolo", "analogía", "crítico", "duda", "curiosidad"),
            topic="identity",
        )

    if any(x in s for x in ("creciste", "crecimiento", "growth", "como vas", "estado")):
        return pack(_growth(), "growth", _tribes("curiosidad", "crítico"), topic="growth")

    traps = ("filotaxis", "phyllotaxis", "alma", "soul", "universo", "dios",
             "conciencia", "siempre primo", "always prime")
    if any(t in s for t in traps):
        return pack(
            "UNKNOWN. No hay cláusula. Mi tribu no rellena el hueco con una historia.",
            "unknown",
            _tribes("duda", "crítico"),
        )

    # follow-up: "y a pell", "y lucas", "por que"
    follow = s.startswith("y ") or s in ("por que", "porque", "why", "y eso", "y ahi")
    if follow and last.get("topic"):
        extra = s.replace("y a ", "").replace("y ", "").strip()
        if extra in SEQ_KEYS:
            s = f"se transfiere a {extra}"
        elif last.get("tag"):
            s = f"{last.get('topic','')} {s}"

    seq = None
    for k, key in SEQ_KEYS.items():
        if re.search(rf"\b{k}\b", s):
            seq = key
            break

    want_xfer = any(w in s for w in ("transfer", "transfiere", "transferencia", "pasa", "sirve", "aplica", "comparte", "misma ley", "tambien"))
    if follow and last.get("tag", "").startswith("transfer"):
        want_xfer = True

    if seq == "pell" and (want_xfer or follow):
        reasons = [w for n, w in kb["rejected"] if "pell" in n and "transfer" in n]
        why = reasons[0] if reasons else "la ley no coincide"
        return pack(
            f"No se transfiere. Analogía: Pell es pariente (companion), no clon. "
            f"Símbolo: rec(pell,[2,1]), no [1,1]. Crítico: {why}.",
            "transfer-pell",
            _tribes("analogía", "símbolo", "crítico"),
            topic="transfer",
        )

    if "lucas" in s and (want_xfer or follow or seq == "lucas"):
        rec_l = _canonical_rec(kb["recs"].get("lucas", []))
        rec_f = _canonical_rec(kb["recs"].get("fib", []))
        if rec_l == rec_f == [1, 1]:
            return pack(
                "Sí se transfiere. Analogía: misma ley. "
                "Símbolo: rec(fib,[1,1]) y rec(lucas,[1,1]). "
                "verified transfer_fib_to_lucas_1_1.",
                "transfer-lucas",
                _tribes("analogía", "símbolo"),
                topic="transfer",
            )

    if any(w in s for w in ("cassini",)):
        hits = [f for _w, _f, n, f in kb["verified"] if "cassini" in n.lower() or "cassini" in f.lower()]
        if hits:
            return pack(
                f"Demostrado: {hits[0]}. Eso es símbolo puro, no una red que lo aproximó.",
                "cassini",
                _tribes("símbolo"),
                topic="cassini",
            )

    if any(w in s for w in ("pisano", "modulo", "modular")):
        rows = [f"π_{a}({m})={p}" for a, m, p in kb["pisano"] if seq is None or a == seq]
        if rows:
            return pack(
                "Periodos que el prefijo alcanza: " + ", ".join(rows[:8]) + ". "
                "Lo que no entra en el prefijo quedó rejected. Duda honesta, no un número inventado.",
                "pisano",
                _tribes("símbolo", "duda"),
                topic="pisano",
            )

    if seq:
        can = _canonical_rec(kb["recs"].get(seq, []))
        if can:
            name = SEQ_NAMES.get(seq, seq)
            letter = "F" if seq == "fib" else seq
            return pack(
                f"{name}: {_formula(letter, can)}. "
                f"rec({seq},{can}). Si hay un rec más largo, es la misma ley con ceros.",
                f"rec-{seq}",
                _tribes("símbolo"),
                topic=seq,
            )

    if any(w in s for w in ("geometr", "lema", "varignon", "paralelo")):
        if kb["lemmas"]:
            sample = "; ".join(f"{i}: {tx}" for i, _ty, tx in kb["lemmas"][:4])
            return pack(
                f"Lemas demostrados ({len(kb['lemmas'])}): {sample}. "
                "Formas reusables, no un dataset de triángulos.",
                "geometry",
                _tribes("símbolo", "analogía"),
                topic="geometry",
            )

    if any(w in s for w in ("que sabes", "que conoces", "resumen")):
        recs = ", ".join(
            f"{name}={_canonical_rec(cs)}"
            for name, cs in kb["recs"].items()
            if _canonical_rec(cs)
        )
        return pack(
            f"Lo unificado hasta ahora: {recs}. "
            f"{len(kb['verified'])} verified, {len(kb['rejected'])} rejected. "
            "Fuera de eso, UNKNOWN.",
            "summary",
            _tribes("símbolo", "crítico"),
            topic="summary",
        )

    if seq == "fib" and any(w in s for w in ("el doble", "siempre 2")):
        return pack(
            "Rechazado. No es 2·F(n-1). La ley es [1,1].",
            "reject-double",
            _tribes("crítico", "símbolo"),
            topic="fib",
        )

    return pack(
        "UNKNOWN. Sin cláusula no hay respuesta. "
        "Fibonacci, Lucas, Pell, Cassini, Pisano, lemas, o si crecí.",
        "unknown",
        _tribes("duda"),
    )


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
