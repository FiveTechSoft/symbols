import collections, re

lines = open('data/samples/wikidata_infoboxes.tsv', 'r', encoding='utf-8').readlines()

KEEP = {'CAPITAL', 'IDIOMA', 'IDIOMA_OFICIAL', 'MONEDA', 'CONTINENTE', 'PAIS',
        'ESTADO', 'PROVINCIA', 'MUNICIPIO', 'GENTILICIO', 'GOBIERNO',
        'FUNDADO_EN', 'FUNDADO_POR', 'SEDE_EN', 'CREADO_POR', 'DIRIGIDO_POR',
        'PRODUCIDO_POR', 'ESCRITO_POR', 'NACIO_EN', 'FALLCIO_EN', 'NACIONALIDAD',
        'PROFESION', 'OCUPACION', 'CONYUGE_DE', 'HIJO_DE', 'PADRE_DE',
        'PREMIO', 'CONOCIDO_POR', 'FRONTERA_CON', 'MIEMBRO_DE', 'EDITORIAL',
        'GENERO', 'FORMATO', 'LANZADO_EN', 'REINO', 'ESPECIE', 'TIPO'}


def clean_val(v):
    v = re.sub(r'\{\{[^}]*\}\}', '', v)
    v = re.sub(r'\[\[(?:[^|\]]*\|)?([^\]]+)\]\]', r'\1', v)
    v = re.sub(r'<[^>]+>', '', v)
    v = re.sub(r"'{2,}", '', v)
    v = v.replace('&nbsp;', ' ')
    v = re.sub(r'\s+', ' ', v).strip()
    v = v.strip('.,;:')
    return v


clean = []
for l in lines:
    parts = l.strip().split('\t')
    if len(parts) >= 3:
        s, p, o = parts[0], parts[1], parts[2]
        if p in KEEP:
            o = clean_val(o)
            if len(o) > 1 and len(o) < 80 and '{{' not in o and '[' not in o:
                clean.append((s, p, o))

print(f'Clean triples: {len(clean)}')
unique = sorted(set(clean))
print(f'Unique: {len(unique)}')

with open('data/samples/wikidata_clean.tsv', 'w', encoding='utf-8') as f:
    for s, p, o in unique:
        f.write(f'{s}\t{p}\t{o}\n')

print()
pred_counts = collections.Counter()
for s, p, o in unique:
    pred_counts[p] += 1

print('Predicate distribution:')
for p, c in pred_counts.most_common(30):
    print(f'  {p:30s} {c:5d}')

print()
print('Sample triples:')
for s, p, o in unique[:20]:
    print(f'  {s:30s} {p:25s} {o}')
