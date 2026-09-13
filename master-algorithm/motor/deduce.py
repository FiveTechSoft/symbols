"""Child-like deduction over theory.pl atoms.

The brain is theory.pl. Vocabulary is a byproduct of growth (obs → rec/verified
→ named clauses). A new word binds only by stem/overlap with atoms already
grown, or by unifying a formula already proved. No adult synonym tables.
"""
from __future__ import annotations

import re
import unicodedata
from typing import Any

MIN_ATOM = 3
FORMULA_THRESHOLD = 0.62


def fold(s: str) -> str:
    """Lowercase, strip accents, keep letters/digits and formula punctuation."""
    s = (s or "").strip().lower()
    s = "".join(
        c for c in unicodedata.normalize("NFD", s)
        if unicodedata.category(c) != "Mn"
    )
    for ch in "¿?¡!":
        s = s.replace(ch, "")
    s = s.replace("→", " a ").replace("->", " a ").replace("⇒", " a ")
    s = s.replace("·", "*").replace("×", "*")
    s = re.sub(r"\s+", " ", s).strip()
    return s


def tokenize(s: str) -> list[str]:
    """Generic tokens: words and formula-ish chunks. No domain word lists."""
    s = fold(s)
    # Keep = + - ^ ( ) / for formula pieces; split the rest
    raw = re.findall(r"[a-z0-9_]+|[=\+\-\^/\(\)\*]+", s)
    return [t for t in raw if t and not t.isspace()]


def _parts(name: str) -> list[str]:
    """Underscore / camel crumbs from a clause name — still theory-born."""
    name = name.replace("::", "_").replace("-", "_")
    bits = re.split(r"[_\W]+", name.lower())
    out = []
    for b in bits:
        if len(b) >= MIN_ATOM:
            out.append(b)
        # also peel trailing digits: period_fib_m11 → already split
    return out


def lexicon_from_kb(kb: dict) -> set[str]:
    """Atoms the child already has: rec heads, clause names/parts, predicates."""
    atoms: set[str] = set()
    # Predicate heads present in the loaded theory
    for pred in ("rec", "verified", "lemma", "rejected", "companion", "true_mod", "schema", "obs"):
        atoms.add(pred)

    for seq in (kb.get("recs") or {}):
        atoms.add(seq.lower())
        atoms.update(_parts(seq))

    for _w, _f, name, formula in kb.get("verified") or []:
        atoms.add(name.lower())
        atoms.update(_parts(name))
        # formula tokens that look like seq heads already in theory are enough;
        # do not invent human aliases from formula text beyond its own tokens
        for tok in re.findall(r"[a-z_][a-z0-9_]*", fold(formula)):
            if len(tok) >= MIN_ATOM:
                atoms.add(tok)

    for name, _ty, text in kb.get("lemmas") or []:
        atoms.add(name.lower())
        atoms.update(_parts(name))
        for tok in re.findall(r"[a-z_][a-z0-9_]*", fold(text)):
            if len(tok) >= 2:  # BC, MN from lemma captions
                atoms.add(tok)

    for name, _why in kb.get("rejected") or []:
        atoms.add(name.lower())
        atoms.update(_parts(name))

    for a, b in kb.get("companions") or []:
        atoms.add(a.lower())
        atoms.add(b.lower())

    for seq, _m, _p in kb.get("periods") or []:
        atoms.add(seq.lower())
        atoms.add("true_mod")

    for name, _st in kb.get("schemas") or []:
        atoms.add(name.lower())
        atoms.update(_parts(name))

    return atoms


def bind_tokens(tokens: list[str], atoms: set[str], short_roots: set[str] | None = None) -> set[str]:
    """Bind question tokens to theory atoms by exact / stem / prefix.

    Never by a human alias dict.
    Short atoms (len==3) only prefix-expand if they are short_roots (rec heads).
    """
    bound: set[str] = set()
    atoms_l = {a.lower() for a in atoms}
    short_roots = {x.lower() for x in (short_roots or set())}

    def forms(tok: str) -> list[str]:
        out = [tok]
        if len(tok) > 4 and tok.endswith("s") and not tok.endswith("ss"):
            out.append(tok[:-1])
        return out

    def one_indel(a: str, b: str) -> bool:
        """True if shorter is the longer with exactly one char deleted."""
        if abs(len(a) - len(b)) != 1:
            return False
        short, long = (a, b) if len(a) < len(b) else (b, a)
        for i in range(len(long)):
            if short == long[:i] + long[i + 1 :]:
                return True
        return False

    def consider(t: str) -> None:
        if t in atoms_l:
            bound.add(t)
            return
        if len(t) < 2:
            return
        for atom in atoms_l:
            if len(atom) < MIN_ATOM:
                continue
            if atom.startswith(t) and len(t) >= MIN_ATOM:
                bound.add(atom)
                continue
            if t.startswith(atom):
                if len(atom) >= 4 or atom in short_roots:
                    bound.add(atom)
                    continue
            # no fuzzy shared-prefix: startswith / one-indel / one-edit only
            # (transformada must not bind transfer_*)
            # one-indel near-stem (lema↔lemma) — both ≥4
            if len(t) >= 4 and len(atom) >= 4 and one_indel(t, atom):
                bound.add(atom)
                continue
            # one-edit equal-length rec heads (lukas↔lucas); require same first char
            # so bell↛pell
            if (
                atom in short_roots
                and len(t) == len(atom)
                and len(t) >= 4
                and t[0] == atom[0]
                and sum(x != y for x, y in zip(t, atom)) == 1
            ):
                bound.add(atom)
                continue
            if len(t) >= 5 and t in atom and "_" in atom:
                bound.add(atom)

    for tok in tokens:
        raw = tok.lower()
        if not raw or raw in "()+-*/^=":
            continue
        for t in forms(raw):
            consider(t)
    return bound




def _norm_formula(s: str, rec_heads: set[str] | None = None) -> str:
    """Normalize and punch holes: rec heads and single-letter seq(n) become •."""
    s = fold(s)
    s = s.replace(" ", "")
    s = s.replace("**", "^")
    heads = sorted({h.lower() for h in (rec_heads or set())}, key=len, reverse=True)
    for h in heads:
        s = re.sub(rf"{re.escape(h)}(?=\()", "•", s)
    # A lone letter immediately before (n is a hole (F(n), 2F(n-1), …)
    s = re.sub(r"(?<![a-z0-9_•])[a-z](?=\(n)", "•", s)
    s = re.sub(r"(?<=\d)[a-z](?=\(n)", "•", s)
    return s


def formula_overlap(a: str, b: str, rec_heads: set[str] | None = None) -> float:
    """Normalized char-bigram overlap in [0,1]."""
    x, y = _norm_formula(a, rec_heads), _norm_formula(b, rec_heads)
    if not x or not y:
        return 0.0
    if x == y:
        return 1.0
    # strip punctuation noise for set compare but keep structure chars
    def grams(s: str) -> set[str]:
        if len(s) < 2:
            return {s}
        return {s[i : i + 2] for i in range(len(s) - 1)}

    gx, gy = grams(x), grams(y)
    inter = len(gx & gy)
    union = len(gx | gy) or 1
    jacc = inter / union
    # also reward containment of the shorter in the longer
    shorter, longer = (x, y) if len(x) <= len(y) else (y, x)
    contain = 1.0 if shorter in longer else 0.0
    # char coverage of shorter
    cov = sum(1 for c in shorter if c in longer) / max(len(shorter), 1)
    return max(jacc, 0.7 * contain + 0.3 * cov, 0.5 * jacc + 0.5 * cov)


def looks_like_equation(q: str) -> bool:
    s = fold(q)
    if "=" in s:
        return True
    # product of seq(n±k) style terms without needing a human name
    if re.search(r"[a-z]{2,}\s*\(\s*n\s*[+\-]\s*\d+\s*\)", s) and (
        "^" in s or "**" in s or ")^2" in s.replace(" ", "") or ")2" in s.replace(" ", "")
    ):
        return True
    if re.search(r"[a-z]{2,}\(n[+\-]\d+\)[a-z]{2,}\(n", s.replace(" ", "")):
        return True
    return False


def prove_formula(q: str, kb: dict) -> dict | None:
    """If q looks like an equation, score verified formulas by char overlap."""
    fq = fold(q)
    if not looks_like_equation(q):
        # allow close formula shapes without '=': seq(n±k) products
        if not re.search(r"[a-z]{2,}\(n[+\-]?\d*\)", fq.replace(" ", "")):
            return None
        if not any(c in fq for c in ("(n+", "(n-", "^2", "(-1)", ")(")):
            return None
    rec_heads = {k.lower() for k in (kb.get("recs") or {})}
    best = None
    best_score = 0.0
    for _w, _f, name, formula in kb.get("verified") or []:
        sc = formula_overlap(q, formula, rec_heads)
        # slight boost if query also mentions a name atom already in formula/name
        if sc > best_score:
            best_score = sc
            best = {
                "kind": "verified",
                "name": name,
                "formula": formula,
                "score": sc,
                "atoms": set(_parts(name)) | {name.lower()},
            }
    if best and best["score"] >= FORMULA_THRESHOLD:
        return best
    return None


def _score_name_hit(bound: set[str], name: str, extra: str = "") -> float:
    nl = name.lower()
    parts = set(_parts(name)) | {nl}
    blob = fold(name + " " + (extra or ""))
    score = 0.0
    for a in bound:
        if a == nl or a in parts:
            score += 3.0
        elif a in blob:
            score += 1.5
        elif nl.startswith(a) or a.startswith(nl[: max(MIN_ATOM, len(a))]):
            score += 2.0
    return score


def retrieve(q: str, kb: dict, last: dict | None = None) -> list[dict]:
    """Ranked hits from theory atoms only. Empty → child does not have it yet."""
    last = last or {}
    tokens = tokenize(q)
    atoms = lexicon_from_kb(kb)
    short_roots = {k.lower() for k in (kb.get("recs") or {})}
    bound = bind_tokens(tokens, atoms, short_roots=short_roots)

    hits: list[dict] = []

    # Formula unification first-class
    pf = prove_formula(q, kb)
    if pf:
        hits.append(pf)

    # rec/2 heads
    for seq, coefs in (kb.get("recs") or {}).items():
        if seq.lower() in bound:
            hits.append({
                "kind": "rec",
                "name": seq,
                "formula": coefs,
                "score": 8.0 + (2.0 if seq.lower() in {t.lower() for t in tokens} else 0.0),
                "atoms": {seq.lower()},
            })

    # verified — name/part bind only; formula unify is prove_formula's job
    for _w, _f, name, formula in kb.get("verified") or []:
        sc = _score_name_hit(bound, name, formula)
        if sc <= 0:
            continue
        hits.append({
            "kind": "verified",
            "name": name,
            "formula": formula,
            "score": sc,
            "atoms": bound & (set(_parts(name)) | {name.lower()}),
        })

    # lemmas — name bind OR text unification (child recognizes a drawn figure fact)
    for name, ty, text in kb.get("lemmas") or []:
        sc = _score_name_hit(bound, name, text)
        if "lemma" in bound and sc <= 0:
            sc = 3.0  # predicate head alone → list lemmas
        fo = formula_overlap(q, text, {k.lower() for k in (kb.get('recs') or {})})
        if fo >= FORMULA_THRESHOLD:
            sc = max(sc, 5.0 + fo)
        # short caption tokens (BC, MN) against lemma text
        if sc <= 0 and bound:
            blob = fold(text)
            if all(len(a) <= 3 or a in blob or a in name.lower() for a in bound):
                if any(a in blob for a in bound):
                    sc = 2.0 + 0.5 * sum(1 for a in bound if a in blob)
        if sc <= 0:
            continue
        hits.append({
            "kind": "lemma",
            "name": name,
            "formula": text,
            "payload": ty,
            "score": sc + (0.5 if "lemma" in bound else 0.0),
            "atoms": bound & (set(_parts(name)) | {name.lower()}),
        })

    # rejected
    for name, why in kb.get("rejected") or []:
        sc = _score_name_hit(bound, name, why)
        if sc <= 0:
            continue
        hits.append({
            "kind": "rejected",
            "name": name,
            "formula": why,
            "score": sc,
            "atoms": bound & (set(_parts(name)) | {name.lower()}),
        })

    # true_mod / periods — only with true_mod atom or seq+modulus digit (not bare "periodo")
    rows = kb.get("periods") or kb.get("pisano") or []
    seqs_p = [s for s in (kb.get("recs") or {}) if s.lower() in bound]
    digits = {tok for tok in tokens if tok.isdigit()}
    mod_hit = bool(rows) and bool(seqs_p) and any(
        str(m) in digits for _a, m, _p in rows if not seqs_p or _a in seqs_p
    )
    # explicit predicate / clause crumb true_mod — not every period_* stem flood
    want_period = ("true_mod" in bound) or mod_hit
    if want_period and rows:
        picked = [(a, m, p) for a, m, p in rows if not seqs_p or a in seqs_p]
        if mod_hit:
            picked = [(a, m, p) for a, m, p in picked if str(m) in digits] or picked
        if picked:
            hits.append({
                "kind": "period",
                "name": "true_mod",
                "formula": picked[:12],
                "score": 8.0 if mod_hit else (7.0 if seqs_p else 5.0),
                "atoms": ({"true_mod"} | {s.lower() for s in seqs_p}),
            })

    # companion
    if "companion" in bound or (
        len([s for s in (kb.get("recs") or {}) if s.lower() in bound]) >= 2
        and "companion" in atoms
    ):
        comps = kb.get("companions") or []
        seqs = [s for s in (kb.get("recs") or {}) if s.lower() in bound]
        if comps and (not seqs or any(a in seqs and b in seqs for a, b in comps)
                      or any(a in seqs or b in seqs for a, b in comps)):
            hits.append({
                "kind": "companion",
                "name": "companion",
                "formula": [(a, b) for a, b in comps
                            if not seqs or a in seqs or b in seqs][:6],
                "score": 5.0,
                "atoms": {"companion"} | {s.lower() for s in seqs},
            })

    # Child with no bound atoms and no formula hit: empty (UNKNOWN upstream)
    has_formula = any(h.get("kind") == "verified" and h.get("score", 0) >= FORMULA_THRESHOLD
                      and looks_like_equation(q) for h in hits)
    # keep prove_formula hits even without bound
    pf_names = {h["name"] for h in hits if h.get("score", 0) >= 0.9 and looks_like_equation(q)}
    if not bound and not pf_names:
        # allow only explicit prove_formula-quality hits already inserted at top
        hits = [h for h in hits if h.get("score", 0) >= 0.9 and looks_like_equation(q)]
        if not hits:
            return []

    # period-when-modulus-digit: period_* verified/rejected only if exact name
    # token or the modulus digit in the question (not bare "periodo"/"period")
    tokset = {t.lower() for t in tokens}
    digits = {t for t in tokens if t.isdigit()}
    pruned: list[dict] = []
    for h in hits:
        if h.get("kind") in ("verified", "rejected"):
            nl = str(h.get("name") or "").lower()
            if nl.startswith("period_"):
                if nl in tokset:
                    pruned.append(h)
                    continue
                mm = re.search(r"_m(\d+)$", nl)
                if mm and mm.group(1) in digits and (
                    "true_mod" in bound
                    or any(s.lower() in bound for s in (kb.get("recs") or {}))
                    or any(a.startswith("period") for a in bound)
                ):
                    # digit alone is not enough without a period/seq cue
                    pruned.append(h)
                    continue
                continue  # drop stem-only period_* hits
        pruned.append(h)
    hits = pruned

    # Deduplicate by (kind, name), keep best score
    best_map: dict[tuple, dict] = {}
    for h in hits:
        key = (h["kind"], h.get("name"))
        if key not in best_map or h["score"] > best_map[key]["score"]:
            best_map[key] = h
    ranked = sorted(best_map.values(), key=lambda h: -h["score"])
    for h in ranked:
        h["bound"] = bound
    return ranked



def parse_claimed_rec(q: str, rec_heads: set[str]) -> tuple[str | None, list[int]] | None:
    """Parse seq(n)=a*seq(n-1)+b*seq(n-2) after hole-normalization. None if not a rec claim."""
    if "=" not in (q or ""):
        return None
    heads = {h.lower() for h in rec_heads}
    norm = _norm_formula(q, heads)
    if "=" not in norm:
        return None
    lhs, rhs = norm.split("=", 1)
    if "•(n)" not in lhs.replace(" ", "") and not re.match(r"•\(n\)$", lhs):
        # only a recurrence claim if LHS is •(n)
        if lhs not in ("•(n)", "•n"):
            return None
    # collect a•(n-k) terms; allow (2)*•(n-1), 2*•(n-1), 2•(n-1), •(n-1)
    coeffs: dict[int, int] = {}
    for m in re.finditer(
        r"([+-]?)(?:\((\d+)\)|(\d+))?\*?•\(n-(\d+)\)",
        rhs,
    ):
        sign, paren_num, bare_num, k = m.group(1), m.group(2), m.group(3), int(m.group(4))
        num = paren_num or bare_num
        a = int(num) if num else 1
        if sign == "-":
            a = -a
        coeffs[k] = coeffs.get(k, 0) + a
    if not coeffs:
        return None
    order = max(coeffs)
    vec = [coeffs.get(i, 0) for i in range(1, order + 1)]
    # which rec head? first bound-looking token that is a head
    tokens = tokenize(q)
    seq = None
    for tok in tokens:
        tl = tok.lower()
        if tl in heads:
            seq = tl
            break
        for h in heads:
            if tl.startswith(h) and len(h) >= 3:
                seq = h
                break
        if seq:
            break
    return seq, vec


def claimed_rec_conflict(q: str, kb: dict) -> dict | None:
    """If the question claims a rec that does not match the shortest living rec/2."""
    heads = {k.lower() for k in (kb.get("recs") or {})}
    parsed = parse_claimed_rec(q, heads)
    if not parsed:
        return None
    seq, claimed = parsed
    recs = kb.get("recs") or {}
    if not seq or (seq not in recs and seq not in heads):
        # hole-only: conflict if the claimed vector matches no living rec/2
        mismatch = []
        for h, coefs in recs.items():
            cleaned = []
            for c in coefs:
                c = list(c)
                while c and c[-1] == 0:
                    c = c[:-1]
                if c:
                    cleaned.append(c)
            if not cleaned:
                continue
            cleaned.sort(key=len)
            can = cleaned[0]
            if claimed == can:
                return None  # it is someone's law
            mismatch.append((h, can))
        if mismatch:
            h, can = mismatch[0]
            return {"seq": h, "claimed": claimed, "canonical": can}
        return None
    # canonical shortest
    coefs = kb["recs"].get(seq) or kb["recs"].get(seq.lower())
    if not coefs:
        return None
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
    can = cleaned[0]
    if claimed == can:
        return None
    return {"seq": seq, "claimed": claimed, "canonical": can}


def bound_seqs(bound: set[str], kb: dict) -> list[str]:
    """Sequence atoms among bound, order stable by recs key order."""
    keys = list((kb.get("recs") or {}).keys())
    return [k for k in keys if k.lower() in bound]
