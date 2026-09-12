"""prox.py — Footman antes de opened (ventana 15 palabras)."""
import re

t = open('books/alice.txt', encoding='utf-8', errors='replace').read()
toks = re.findall(r"[A-Za-z']+", t.lower())
for i, w in enumerate(toks):
    if w == 'opened':
        win = toks[max(0, i - 15):i + 8]
        if 'footman' in win or 'footmen' in win:
            print(' '.join(win))
            print('---')
