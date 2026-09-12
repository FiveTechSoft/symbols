"""check_es.py — titulo + idioma de los descargados."""
import re
from collections import Counter

for f in ['books/lazarillo.txt', 'books/quijote.txt', 'books/leyendas.txt', 'books/desconocido.txt']:
    t = open(f, encoding='utf-8', errors='replace').read()
    title = ''
    for l in t.splitlines()[:60]:
        if l.startswith('Title:'):
            title = l[6:].strip()
    toks = re.findall(r'[a-záéíóúñü]+', t.lower())
    c = Counter(toks)
    tot = sum(c.values())
    es = sum(c[w] for w in ['de', 'la', 'el', 'que', 'los', 'las', 'una', 'para', 'con', 'como']) / tot
    en = sum(c[w] for w in ['the', 'and', 'was', 'for', 'with', 'that', 'from']) / tot
    print('%s | %s | ES=%.3f EN=%.3f' % (f, title[:60], es, en))
