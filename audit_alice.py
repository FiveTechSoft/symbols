"""audit_alice.py — verifica chapters y pronombres independientemente."""
import re

t = open('books/alice.txt', encoding='utf-8', errors='replace').read()
lines = t.splitlines()
paras, cur = [], []
for l in lines:
    if l.strip() == '':
        if cur:
            paras.append(' '.join(cur))
            cur = []
    else:
        cur.append(l.strip())
if cur:
    paras.append(' '.join(cur))
print('parrafos:', len(paras))
ch = [p for p in paras if p.split() and p.split()[0].lower() == 'chapter']
print('parrafos que empiezan por chapter:', len(ch))
for p in ch[:30]:
    print('  ', repr(p[:70]))

# pronombre + verbo-ed adyacentes
sents = re.split(r'[.!?;:,]', t.lower())
pron = {'he', 'she', 'him', 'her', 'it', 'they', 'them'}
n = 0
ex = []
for s in sents:
    toks = re.findall(r"[a-z']+", s)
    for i, w in enumerate(toks):
        if w in pron and i + 1 < len(toks):
            nxt = toks[i + 1]
            if len(nxt) > 4 and nxt.endswith('ed'):
                n += 1
                if len(ex) < 10:
                    ex.append((w, nxt))
print('pron + V-ed adyacente:', n)
for w, v in ex:
    print('  ', w, v)
