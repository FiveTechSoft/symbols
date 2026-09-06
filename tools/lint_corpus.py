#!/usr/bin/env python3
"""Linter de corpus TSV para Symbolic LLM.

Revisa los 7 TSV que ingiere test_wikidata_ingest con las MISMAS
reglas de skip que ParseTSVLine (comentarios #, lineas sin 2 tabs)
y estas categorias:

  mojibake   doble-codificacion UTF-8 (patron estrecho). Intenta
             reparar via latin-1 -> utf-8; si no, DROP.
   wikimarkup [[...]] balanceado       -> quita segmento (FIX);
              markup sin balancear ({{, }}, [[, ]], <, >) = fragmento
              destrozado ({{NOWRAP, INGLÉS_}}) -> DROP (markup_fragment,
              nunca se lava a hecho limpio)
  asterisk   '*' residual de markdown   -> quita + colapsa _ (FIX)
  paren      parentetico final _(1921)  -> quita (FIX con perdida
             documentada); parentesis desbalanceados -> DROP
   anaphora   sujeto anaforico (SU/TU/THE/SHE/SONS OF...) -> DROP
   null       marcador de ausencia (NINGUNO/NO/N-A como valor
              completo: "sin valor" no es un hecho) -> DROP
  spaced_pred predicado con espacios    -> DROP (irrecuperable)
  phrase_obj  objeto >4 tokens o >48 chars con espacio -> DROP
  empty      campo vacio                -> DROP
  whitespace espacios multiples         -> colapsa (FIX)

Uso:
  python3 tools/lint_corpus.py --check   # informa, exit 1 si hay issues
  python3 tools/lint_corpus.py --clean   # reescribe in-place (git = backup),
                                         # preserva comentarios y fin de linea
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FILES = [
    "data/samples/wikidata_clean.tsv",
    "wiki_corpus.tsv",
    "data/samples/c_knowledge.tsv",
    "data/samples/psalms_knowledge.tsv",
    "data/bible/bible_relations.tsv",
    "data/samples/love_knowledge.tsv",
    "data/samples/geo_knowledge.tsv",
    "data/samples/math_knowledge.tsv",
    "data/samples/iconclass_trees.tsv",
    "data/samples/jung_symbols.tsv",
]

STOP_SUBJ = {
    "SU", "SUS", "TU", "TUS", "MI", "MIS", "YO", "EL", "ELLA", "ELLO",
    "ELLOS", "ELLAS", "NOSOTROS", "USTED", "ESTE", "ESTA", "ESTO",
    "ESE", "ESA", "ESO", "AQUEL", "QUE", "QUIEN", "CUAL", "CUALES",
    "DONDE", "CUANDO", "THE", "A", "AN", "SHE", "HE", "IT", "THEY",
    "WE", "YOU", "HIS", "HER", "THEIR", "ITS",
}
STOP_PREFIX = ("SU ", "SUS ", "TU ", "TUS ", "MI ", "MIS ", "THE ",
               "AN ", "SONS OF", "DAUGHTERS OF", "KINGS OF")

# Whole-value absence markers (same set as the infobox extractor):
# "no value" is not a fact, never a symbol. "-" excluded on purpose:
# MENOS SIMBOLO - is a real fact (the minus sign), not an absence.
NULL_VALUES = frozenset([
    "NINGUNO", "NINGUNA", "NINGUN", "NINGÚN",
    "NO", "N/A",
])

MOJI_RE = re.compile(r"Ã[©±²³­¯º-¼]|Â[¿¡]|â€")
WIKILINK_RE = re.compile(r"\[\[.*?\]\]")
TRAILING_PAREN_RE = re.compile(r"\s*_?\(.*?\)\s*$")
MULTISPACE_RE = re.compile(r" {2,}")


def detect_newline(raw: bytes) -> str:
    crlf = raw.count(b"\r\n")
    lf = raw.count(b"\n")
    if crlf and crlf == lf:
        return "\r\n"
    return "\n"


def split_row(line: str):
    """Mimica ParseTSVLine: 2 primeros tabs parten; 3er tab (opcional)
    separa objeto de procedencia. Devuelve (s, p, o, src)."""
    parts = line.split("\t")
    if len(parts) < 3:
        return None
    if len(parts) > 3:
        return parts[0], parts[1], parts[2], "\t".join(parts[3:])
    return parts[0], parts[1], parts[2], ""


def is_data_line(line: str) -> bool:
    s = line.lstrip(" \t")
    return bool(s) and not s.startswith("#") and split_row(line) is not None


def repair_mojibake(field: str):
    """Devuelve (campo, reparado: bool). Solo toca patron estrecho."""
    if not MOJI_RE.search(field):
        return field, False
    try:
        fixed = field.encode("latin-1").decode("utf-8")
    except (UnicodeError, ValueError):
        return field, False
    if MOJI_RE.search(fixed):
        return field, False
    return fixed, True


def clean_field(field: str):
    """Devuelve (campo_limpio, accion) con accion en
    {'ok','fixed','drop'} y motivo aparte."""
    # 1. wikimarkup: segmentos [[...]] balanceados fuera; colas sin
    # cerrar ([[ARCHIVO:... sin ]]) cortadas. Si queda markup sin
    # balancear ({{, }}, [[, ]], <, >), el campo es un fragmento
    # destrozado ({{NOWRAP, INGLÉS_}}): DROP con markup_fragment.
    # Lavar esos caracteres (INGLÉS_}} -> INGLÉS) fabricaria un hecho
    # limpio a partir de basura: prohibido.
    new = WIKILINK_RE.sub("", field)
    new = re.sub(r"\[\[.*$", "", new)
    new = re.sub(r"\{\{[^{}]*\}\}", "", new)
    new = re.sub(r"\{\{.*$", "", new)
    if re.search(r"[{}\[\]<>]", new):
        return new, "drop", "markup_fragment"
    # comillas angulares de lemas/citas: puro markup
    new = new.replace("«", "").replace("»", "")
    # 2. asteriscos markdown + colapso de guiones
    new = new.replace("*", "")
    new = re.sub(r"__+", "_", new)
    # 3. parentetico final _(1921) / (nota)
    new = TRAILING_PAREN_RE.sub("", new)
    # 4. colapsa espacios
    new = MULTISPACE_RE.sub(" ", new).strip(" \t_")
    fixed = new != field
    # 5. parentesis desbalanceados -> irrecuperable
    if new.count("(") != new.count(")"):
        return new, "drop", "paren_unbalanced"
    if not new:
        return new, "drop", "empty_after_clean"
    return new, ("fixed" if fixed else "ok"), ""


def check_triple(s: str, p: str, o: str, src: str = ""):
    """Un triple (crudo) -> (accion, s2, p2, o2, src2, regla).

    accion: 'keep' (intacto), 'fixed' (reparado), 'drop' (regla=motivo).
    src es la procedencia explicita (4a columna, p.ej. "GEN 1:1"):
    se valida leve (sin tabs, <128 chars) y se preserva tal cual.
    Misma funcion que usa lint_file y los extractores: una sola
    fuente de verdad para que lo extraido pase la puerta.
    """
    rule = ""
    src2 = src.strip()
    if "\t" in src2 or "\n" in src2 or len(src2) >= 128:
        return "drop", s, p, o, src2, "bad_source"
    if not s.strip() or not p.strip() or not o.strip():
        return "drop", s, p, o, src2, "empty"
    su = s.strip().upper()
    if su in STOP_SUBJ or su.startswith(STOP_PREFIX):
        return "drop", s, p, o, src2, "anaphora"
    if su in NULL_VALUES or o.strip().upper() in NULL_VALUES:
        return "drop", s, p, o, src2, "null_marker"
    if " " in p.strip():
        return "drop", s, p, o, src2, "spaced_pred"
    ou = o.strip().upper()
    if len(ou.split()) > 4 or (len(ou) > 48 and " " in ou):
        return "drop", s, p, o, src2, "phrase_obj"

    moji_fixed = False
    flds = []
    for fld in (s, p, o):
        if MOJI_RE.search(fld):
            fld, ok = repair_mojibake(fld)
            if MOJI_RE.search(fld):
                return "drop", s, p, o, src2, "mojibake"
            moji_fixed = moji_fixed or ok
        flds.append(fld)
    s, p, o = flds

    fixed_any = moji_fixed
    flds = []
    for fld in (s, p, o):
        fld, act, why = clean_field(fld)
        if act == "drop":
            return "drop", s, p, o, src2, why
        fixed_any = fixed_any or (act == "fixed")
        flds.append(fld)
    s, p, o = flds
    ou = o.strip().upper()
    if len(ou.split()) > 4 or (len(ou) > 48 and " " in ou):
        return "drop", s, p, o, src2, "phrase_obj"

    # Re-chequeo post-limpieza: valores que DEVIENEN nulos al limpiar
    # (Ninguno<ref>...</ref> -> NINGUNO) caen aqui, no se rescatan.
    if (s.strip().upper() in NULL_VALUES or
            o.strip().upper() in NULL_VALUES):
        return "drop", s, p, o, src2, "null_marker"

    # Sujetos/objetos con espacio: frase larga -> drop (simetrico a
    # phrase_obj); resto -> guion bajo (convencion del proyecto:
    # SAN_MARINO, no SAN MARINO). Los predicados con espacio ya
    # cayeron en spaced_pred (vocabulario cerrado, no se normaliza).
    for which in ("s", "o"):
        fld = s if which == "s" else o
        fu = fld.strip().upper()
        if " " in fu:
            if len(fu.split()) > 4 or len(fu) > 48:
                return "drop", s, p, o, src2, ("phrase_subj" if which == "s" else "phrase_obj")
            norm = re.sub(r" +", "_", fld.strip())
            if norm != fld:
                fixed_any = True
            fld = norm
            if which == "s":
                s = fld
            else:
                o = fld

    if fixed_any:
        return "fixed", s.strip(), p.strip(), o.strip(), src2, ""
    return "keep", s.strip(), p.strip(), o.strip(), src2, ""


def lint_file(path: Path):
    raw = path.read_bytes()
    newline = detect_newline(raw)
    text = raw.decode("utf-8", errors="replace")
    lines = text.split("\n")
    # quita el '' fantasma del split final (el newline real se repone al escribir)
    if lines and lines[-1] == "":
        lines.pop()

    stats = {"rows": 0, "fixed": 0, "dropped": 0, "by_rule": {}}
    out_lines = []
    dropped_examples = []

    def bump(rule):
        stats["by_rule"][rule] = stats["by_rule"].get(rule, 0) + 1

    for line in lines:
        stripped = line.rstrip("\r")
        if not is_data_line(stripped):
            out_lines.append(stripped)  # comentarios/blancos: intactos
            continue
        s, p, o, src = split_row(stripped)
        stats["rows"] += 1
        action, s2, p2, o2, src2, rule = check_triple(s, p, o, src)
        if action == "drop":
            stats["dropped"] += 1
            bump(rule)
            if len(dropped_examples) < 5:
                dropped_examples.append((rule, s2[:40], p2[:25], o2[:55]))
        else:
            if action == "fixed":
                stats["fixed"] += 1
            out_lines.append(f"{s2}\t{p2}\t{o2}" + (f"\t{src2}" if src2 else ""))

    return stats, dropped_examples, out_lines, newline


def main(argv):
    mode = "--check" if "--check" in argv else ("--clean" if "--clean" in argv else "")
    if not mode:
        print(__doc__)
        return 2
    total_rows = total_fixed = total_dropped = 0
    total_rules = {}
    failed = False
    for fn in FILES:
        path = ROOT / fn
        stats, examples, out_lines, newline = lint_file(path)
        total_rows += stats["rows"]
        total_fixed += stats["fixed"]
        total_dropped += stats["dropped"]
        for k, v in stats["by_rule"].items():
            total_rules[k] = total_rules.get(k, 0) + v
        issues = stats["fixed"] + stats["dropped"]
        if issues:
            failed = True
        print(f"{fn}: rows={stats['rows']} fixed={stats['fixed']} "
              f"dropped={stats['dropped']} {stats['by_rule']}")
        for rule, s, p, o in examples:
            print(f"    ej [{rule}] {s!r} | {p!r} | {o!r}")
        if mode == "--clean" and issues:
            raw_nl = newline.encode()
            data = newline.join(out_lines) + newline
            path.write_bytes(data.encode("utf-8"))
            print(f"    -> reescrito ({len(out_lines)} lineas, fin {newline!r})")
    print(f"TOTAL: rows={total_rows} fixed={total_fixed} "
          f"dropped={total_dropped} {total_rules}")
    if mode == "--check":
        return 1 if failed else 0
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
