"""check_plat2.py — identifica el caracter danado y guiones unicode."""
from collections import Counter

t = open('books/plataforma.txt', encoding='utf-8').read()
c = Counter(t)
print('top no-ascii:')
for ch, n in c.most_common(40):
    if ord(ch) > 127:
        print('  U+%04X %d %r' % (ord(ch), n, ch))
i = t.find('Castej')
print(repr(t[i:i + 12]), [hex(ord(x)) for x in t[i:i + 12]])
