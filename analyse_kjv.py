import re
from collections import Counter

t = open('corpus_biblia/kjv.txt', encoding='utf-8', errors='replace').read()
lines = t.splitlines()
verses = []
cur = None
for l in lines:
    s = l.strip()
    m = re.match(r'^(\d+:\d+)\s?(.*)$', s)
    if m:
        # puede haber varios marcadores en la misma linea: partir
        rest = m.group(2)
        inner = re.split(r'\b(\d{1,3}:\d{1,3})\b', rest)
        # inner[0] pertenece al marcador inicial
        if cur:
            verses.append(cur)
        cur = [m.group(1), inner[0].strip()]
        for k in range(1, len(inner), 2):
            verses.append(cur)
            nxt_txt = inner[k + 1].strip() if k + 1 < len(inner) else ''
            cur = [inner[k], nxt_txt]
    elif s == '':
        if cur:
            verses.append(cur)
            cur = None
    else:
        if cur:
            cur[1] += ' ' + s
if cur:
    verses.append(cur)
print('versiculos:', len(verses))

parts = []
for ref, txt in verses:
    for p in re.split(r'[.!?;:]', txt):
        p = p.strip()
        if p:
            parts.append(p)
print('frases aprox:', len(parts))

tok = re.compile("[a-z']+")
c = Counter()
for p in parts:
    c.update(tok.findall(p.lower()))
print('total tokens:', sum(c.values()))
print('--- top 80 ---')
for w, n in c.most_common(80):
    print(w, n)

# candidatos verbales: terminaciones arcaicas/morfologicas
cands = [(w, n) for w, n in c.items()
         if n >= 20 and (w.endswith('ed') or w.endswith('eth') or w.endswith('th')
                         or w in ('said', 'saw', 'heard', 'knew', 'went', 'came',
                                  'took', 'gave', 'made', 'slew', 'smote', 'spake',
                                  'dwelt', 'begat', 'arose', 'awoke', 'bore', 'born'))]
cands.sort(key=lambda x: -x[1])
print('--- candidatos verbales freq>=20 (' + str(len(cands)) + ') ---')
for w, n in cands[:120]:
    print(w, n)
