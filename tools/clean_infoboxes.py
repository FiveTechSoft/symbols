"""Filtra wikidata_infoboxes.tsv -> wikidata_clean.tsv.

Solo predicados KEEP y solo triples que pasen la puerta del linter
(lint_corpus.check_triple: la misma funcion que CI ejecuta, asi lo
extraido nace limpio). Uso: python3 tools/clean_infoboxes.py
"""
import collections
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from lint_corpus import check_triple

KEEP = {'CAPITAL', 'IDIOMA', 'IDIOMA_OFICIAL', 'MONEDA', 'CONTINENTE', 'PAIS',
        'ESTADO', 'PROVINCIA', 'MUNICIPIO', 'GENTILICIO', 'GOBIERNO',
        'FUNDADO_EN', 'FUNDADO_POR', 'SEDE_EN', 'CREADO_POR', 'DIRIGIDO_POR',
        'PRODUCIDO_POR', 'ESCRITO_POR', 'NACIO_EN', 'FALLCIO_EN', 'NACIONALIDAD',
        'PROFESION', 'OCUPACION', 'CONYUGE_DE', 'HIJO_DE', 'PADRE_DE',
        'PREMIO', 'CONOCIDO_POR', 'FRONTERA_CON', 'MIEMBRO_DE', 'EDITORIAL',
        'GENERO', 'FORMATO', 'LANZADO_EN', 'REINO', 'ESPECIE', 'TIPO',
        # Hornada P1 (2026-09-05): valores verificados atomicos y cortos
        'CIUDAD_MÁS_POBLADA', 'LEMA_NACIONAL', 'CODIGO_ISO',
        'PREFIJO_RADIOFÓNICO'}

RAW = Path(__file__).resolve().parent.parent / "data" / "samples" / "wikidata_infoboxes.tsv"
OUT = Path(__file__).resolve().parent.parent / "data" / "samples" / "wikidata_clean.tsv"


def main():
    lines = RAW.read_text(encoding="utf-8").splitlines()
    clean = []
    dropped = collections.Counter()
    for line in lines:
        parts = line.split("\t")
        if len(parts) < 3:
            continue
        s, p, o = parts[0], parts[1], "\t".join(parts[2:])
        if p not in KEEP:
            continue
        action, s2, p2, o2, _, rule = check_triple(s, p, o)
        if action == "drop" or len(o2) >= 80:
            dropped[rule or "too_long"] += 1
            continue
        clean.append((s2, p2, o2))

    print(f"Raw con KEEP: {len(clean) + sum(dropped.values())}, "
          f"limpios: {len(clean)}, dropped: {dict(dropped)}")
    unique = sorted(set(clean))
    print(f"Unique: {len(unique)}")

    # Modo texto por defecto: en Windows traduce \n -> CRLF,
    # como el resto de corpus trackeados
    with open(OUT, "w", encoding="utf-8") as f:
        for s, p, o in unique:
            f.write(f"{s}\t{p}\t{o}\n")

    pred_counts = collections.Counter(p for _, p, _ in unique)
    print("Predicate distribution:")
    for p, c in pred_counts.most_common(30):
        print(f"  {p:30s} {c:5d}")


if __name__ == "__main__":
    main()
