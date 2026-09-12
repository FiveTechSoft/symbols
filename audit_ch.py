"""audit_ch.py — replica el split de parrafos de doc_corpus (literal)."""
lines = open('books/alice.txt', encoding='utf-8', errors='replace').read().splitlines()
# cuerpo desde CHAPTER I (linea 15, indice 14)
body = lines[14:]
paras, cur = [], []
for l in body:
    if l == '':
        if cur:
            paras.append(' '.join(cur))
            cur = []
    else:
        cur.append(l.strip())
if cur:
    paras.append(' '.join(cur))
print('parrafos (split literal):', len(paras))
n = 0
for p in paras:
    w = p.lower().split(' ', 1)
    if w and w[0] == 'chapter':
        n += 1
        if n <= 26:
            print('  ', repr(p[:80]))
print('total chapter-paras:', n)
