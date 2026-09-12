"""find_where.py — triples where/when reales para probar el chat."""
import re

mem = {}
for l in open('bookbrain/alice.knowledge.pl', encoding='utf-8'):
    l = l.strip()
    if l.startswith('memfact('):
        p = l[len('memfact('):-2].split(',')
        mem[(p[0], p[1], p[2])] = True
n = 0
for (s, r, o) in mem:
    if r == 'actor':
        e = o
        for (s2, r2, o2) in mem:
            if s2 == e and r2 == 'action':
                v = o2
                for (s3, r3, o3) in mem:
                    if s3 == e and r3 in ('location', 'time') and o3 not in (s, e):
                        if re.match(r'^[a-z_]+$', s) and re.match(r'^[a-z_]+$', o3):
                            print('%s %s %s=%s' % (s, v, r3, o3))
                            n += 1
                            if n >= 12:
                                raise SystemExit
