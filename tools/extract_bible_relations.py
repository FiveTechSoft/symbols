#!/usr/bin/env python3
"""Extractor determinista de parentesco/realeza biblica (sin ML).

all_chapters.tsv esta en INGLES: en vez de parsear con un modelo
espanol (origen de la basura THE/SONS OF), casamos patrones cerrados
de alto valor: begat/son/father/king/wife/brother of + nombre propio.

Cada candidato pasa lint_corpus.check_triple (la puerta de CI).
Salida: data/bible/bible_relations.tsv
Uso: python3 tools/extract_bible_relations.py
"""
import re
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from lint_corpus import check_triple

ROOT = Path(__file__).resolve().parent.parent
INPUT = ROOT / "data" / "bible" / "all_chapters.tsv"
OUTPUT = ROOT / "data" / "bible" / "bible_relations.tsv"

NAME = r"([A-Z][a-z]+)"
PATTERNS = [
    (re.compile(rf"\b{NAME} begat {NAME}\b"), "PADRE_DE", (1, 2)),
    (re.compile(rf"\b{NAME},? the son of {NAME}\b"), "HIJO_DE", (1, 2)),
    (re.compile(rf"\b{NAME},? the father of {NAME}\b"), "PADRE_DE", (1, 2)),
    (re.compile(rf"\b{NAME},? king of ([A-Z][A-Za-z]*)\b"), "REY_DE", (1, 2)),
    (re.compile(rf"\b{NAME},? (?:the )?wife of {NAME}\b"), "ESPOSA_DE", (1, 2)),
    (re.compile(rf"\b{NAME},? (?:the )?brother of {NAME}\b"), "HERMANO_DE", (1, 2)),
]

# Palabras que parecen nombre pero son ruido استخراج (lugares comunes
# valen como objeto; aqui solo filtramos pronombres/stop claros)
STOP = {"The", "And", "For", "His", "Her", "Their", "Its", "This",
        "That", "With", "From", "When", "Then", "There", "They"}


def main():
    verses = INPUT.read_text(encoding="utf-8").splitlines()
    found = []
    for verse in verses:
        if "\t" in verse:
            ref, text = verse.split("\t", 1)
        else:
            ref, text = "", verse
        for rx, pred, (a, b) in PATTERNS:
            for m in rx.finditer(text):
                s = m.group(a).upper()
                o = m.group(b).upper()
                if m.group(a) in STOP:
                    continue
                action, s2, p2, o2, _, _ = check_triple(s, pred, o)
                if action != "drop":
                    # 4a columna = procedencia (versiculo); el ingest la
                    # guarda en RELATION.source, el resto la ignora
                    found.append((s2, p2, o2, ref.strip()))

    print(f"matches: {len(found)}")
    unique = sorted(set(found))
    print(f"unique: {len(unique)}")
    by_pred = Counter(p for _, p, _, _ in unique)
    for p, c in by_pred.most_common():
        print(f"  {p:15s} {c:5d}")

    # CRLF como el resto de corpus
    with open(OUTPUT, "w", encoding="utf-8") as f:
        for s, p, o, ref in unique:
            f.write(f"{s}\t{p}\t{o}\t{ref}\n")
    print(f"-> {OUTPUT} ({len(unique)} triples con procedencia)")


if __name__ == "__main__":
    main()
