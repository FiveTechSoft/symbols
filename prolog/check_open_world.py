"""Dual-method verification of open_world_data.pl + hold-out disjointness."""
import re
from collections import Counter

rx = re.compile(r'^test_data\((\d+), (\w+), "([^"]+)", "(.*?)", "(.*?)", (\w+)\)\.\s*$')
ids, groups, texts = [], Counter(), []
for line in open('open_world_data.pl', encoding='utf-8'):
    m = rx.match(line)
    if m:
        ids.append(int(m.group(1)))
        groups[m.group(2)] += 1
        texts.append(m.group(4))
print('count:', len(ids), 'sequential:', ids == list(range(1, len(ids) + 1)))
print('groups:', dict(groups))

# banned vocab from prior experiments must not appear as entities
banned = set('elena carlos mary bicycle motorcycle vehicle machine repair '
             'rescue juan maria pedro ana john peter anne coche casa libro '
             'car house book phone madrid barcelona malaga marbella london '
             'paris berlin rojo azul red blue green yellow'.split())
stop = set('compro tiene vive en ayer bought has have lives in yesterday '
           'what did who bought the where when does color is de que es el '
           'la las los una un a an the and is are was were does do did of '
           'to for or object thing kind type material size belongs belong '
           'contains contain depicts depict has have had with was were been '
           'being are is it she he her his their its whose whom how not no '
           'unknown yes evidence restored carried painted restored built '
           'carved rescued belongs contains depicts'.split())
words = set()
for t in texts:
    words.update(t.lower().replace('.', '').split())
leak = (words - stop) & banned
print('banned leak:', sorted(leak) if leak else 'NONE')
