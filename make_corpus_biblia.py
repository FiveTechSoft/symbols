"""make_corpus_biblia.py — KJV (Gutenberg wrap) -> versiculos -> frases.

Salidas en corpus_biblia/:
  verses.tsv  ref \t book \t ch \t v \t text
  corpus.txt  una frase por linea (particion por . ! ? ; :)
  prov.tsv    nlinea \t ref   (1:1 con corpus.txt)
  books.tsv   nb \t titulo
Proveniencia por linea para auditar cada tripleta extraida.
"""
import re
import os

SRC = 'corpus_biblia/kjv.txt'
OUT = 'corpus_biblia'
MARK = re.compile(r'\b(\d{1,3}:\d{1,3})\b')

TESTAMENTS = ('The Old Testament of the King James Version of the Bible',
                'The New Testament of the King James Bible')

def clean_header(pre):
    h = pre.strip().strip('*').strip()
    for t in TESTAMENTS:
        if h.startswith(t):
            rest = h[len(t):].strip().strip('*').strip()
            if rest:
                h = rest
    # coletillas del verso anterior ("Amen.") pegadas a la cabecera
    parts = [p.strip() for p in h.split('. ') if p.strip()]
    while parts and parts[0].rstrip('.') in ('Amen', ''):
        parts = parts[1:]
    h = '. '.join(parts)
    return h.strip().strip('*').strip()

lines = open(SRC, encoding='utf-8', errors='replace').read().splitlines()

# Fase A: el cuerpo empieza en la primera linea con marcador de versiculo.
# Todo el preambulo (boilerplate Gutenberg + indice) se ignora, SALVO la
# ultima linea no vacia: es la cabecera del primer libro (Genesis).
body0 = next(i for i, l in enumerate(lines) if MARK.search(l))
preamble = [l.strip() for l in lines[:body0] if l.strip()]
first_book = clean_header(preamble[-1]) if preamble else '?'
lines = lines[body0:]
print('cuerpo desde linea:', body0 + 1, '| primer libro:', first_book)

verses = []          # (book_title, ch, v, text)
headers_seen = []    # titulos de libro en orden de aparicion
book = first_book
headers_seen.append(first_book)
cur = None           # [ch, v, texto]
orphan = []          # texto sin marcador (titulos de salmo...) -> proximo verso

for l in lines:
    s = l.strip()
    if s == '':
        if cur:
            verses.append((book, cur[0], cur[1], cur[2].strip()))
            cur = None
        continue
    parts = MARK.split(s)   # [texto, marca, texto, marca, texto, ...]
    if len(parts) == 1:
        # sin marcador: cabecera de libro o continuacion/titulo huerfano
        if cur:
            cur[2] += ' ' + s
        else:
            if s not in headers_seen and not s.startswith('The Project'):
                # posible cabecera: se confirma si luego viene 1:1
                orphan.append(s)
        continue
    # hay al menos un marcador
    head = parts[0].strip()
    if head:
        if cur:
            cur[2] += ' ' + head
        else:
            orphan.append(head)
    for k in range(1, len(parts), 2):
        mark = parts[k]
        txt = parts[k + 1].strip() if k + 1 < len(parts) else ''
        if cur:
            verses.append((book, cur[0], cur[1], cur[2].strip()))
        ch, v = mark.split(':')
        pre = ' '.join(orphan).strip()
        orphan_last = orphan[-1].strip() if orphan else ''
        orphan = []
        # las ultimas huérfanas antes de un 1:1 suelen ser la cabecera
        if ch == '1' and v == '1' and pre:
            # la cabecera puede ocupar varias lineas (Eclesiastes:
            # 'Ecclesiastes'/'or'/'The Preacher'): unir y limpiar.
            book = clean_header(pre)
            if book and book not in headers_seen:
                headers_seen.append(book)
            pre = ''
        cur = [ch, v, (pre + ' ' + txt).strip()]
if cur:
    verses.append((book, cur[0], cur[1], cur[2].strip()))

print('versiculos:', len(verses))
print('libros detectados:', len(headers_seen))
for h in headers_seen:
    print('  ', repr(h[:80]))

# --- partir en frases ---
SPLIT = re.compile(r'[.!?;:]')
sents = []   # (ref, frase)
for b, ch, v, txt in verses:
    ref = '%s %s:%s' % (b if b else '?', ch, v)
    for p in SPLIT.split(txt):
        p = ' '.join(p.split())
        if p:
            sents.append((ref, p))
print('frases:', len(sents))

os.makedirs(OUT, exist_ok=True)
with open(os.path.join(OUT, 'verses.tsv'), 'w', encoding='utf-8') as fh:
    for b, ch, v, txt in verses:
        fh.write('%s\t%s\t%s\t%s\n' % ('%s %s:%s' % (b, ch, v), b, ch + ':' + v,
                                          ' '.join(txt.split())))
with open(os.path.join(OUT, 'corpus.txt'), 'w', encoding='utf-8') as fh:
    for _, p in sents:
        fh.write(p + '\n')
with open(os.path.join(OUT, 'prov.tsv'), 'w', encoding='utf-8') as fh:
    for i, (ref, _) in enumerate(sents, 1):
        fh.write('%d\t%s\n' % (i, ref))
with open(os.path.join(OUT, 'books.tsv'), 'w', encoding='utf-8') as fh:
    for i, h in enumerate(headers_seen, 1):
        fh.write('%d\t%s\n' % (i, h))

# chequeo: versiculos por libro (Genesis debe dar 1533)
from collections import Counter
cb = Counter(b for b, _, _, _ in verses)
print('versiculos Genesis-like (libro 1):', cb[headers_seen[0]] if headers_seen else '?')
print('versiculos Salmos:', [n for h, n in cb.items() if 'Psalm' in str(h)])
