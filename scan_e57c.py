"""scan_e57c.py — inventario de lineas del log curva (sale a fichero)."""
from collections import Counter

raw = open('exp57c.log', 'rb').read()
outs = ['bytes=%d head=%r' % (len(raw), raw[:60])]
cands = []
for enc in ('utf-8-sig', 'utf-16', 'cp1252'):
    try:
        t = raw.decode(enc)
        cands.append((enc, t))
    except Exception as e:
        outs.append('%s FAIL %s' % (enc, e))
lines = None
for enc, t in cands:
    ls = t.replace(chr(0), '').splitlines()
    outs.append('%s -> %d lineas' % (enc, len(ls)))
    if lines is None and len(ls) > 100:
        lines = ls
if lines is None:
    lines = cands[0][1].replace(chr(0), '').splitlines()
c = Counter()
for l in lines:
    s = l.strip()
    if s.startswith('E57'):
        c['E57*'] += 1
    elif s.startswith('CONFLICT'):
        c['CONFLICT'] += 1
    elif s.startswith('BOOK'):
        c['BOOK'] += 1
    elif s.startswith('DX-'):
        c['DX-'] += 1
    elif s.startswith('kjv'):
        c['kjv'] += 1
    elif s == '':
        c['blank'] += 1
    else:
        c['other'] += 1
for k, v in c.most_common(10):
    outs.append('%8d %s' % (v, k))
open('scan_out.txt', 'w', encoding='utf-8').write('\n'.join(outs))
