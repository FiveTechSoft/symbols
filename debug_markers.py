import re

t = open('corpus_biblia/kjv.txt', encoding='utf-8', errors='replace').read()
lines = t.splitlines()

start = 0
mid_examples = []
n_mid = 0
for l in lines:
    s = l.strip()
    m0 = re.match(r'^(\d{1,3}:\d{1,3})\b', s)
    rest = s[m0.end():] if m0 else s
    if m0:
        start += 1
    for m in re.finditer(r'\b(\d{1,3}:\d{1,3})\b', rest):
        n_mid += 1
        if len(mid_examples) < 15:
            i = max(0, m.start() - 60)
            mid_examples.append(s[i:m.end() + 60])
print('inicio-linea:', start, 'mid-linea:', n_mid, 'total:', start + n_mid)
print('--- ejemplos mid-linea ---')
for e in mid_examples:
    print(repr(e))
