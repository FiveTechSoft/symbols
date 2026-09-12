"""show_e56.py — lineas E56/E56-*, DX-* y diff de snapshots por N."""
import sys
import subprocess

raw = open('exp56.log', 'rb').read()
t = None
for enc in ('utf-16', 'utf-8'):
    try:
        t = raw.decode(enc)
        break
    except Exception:
        continue
t = t.replace(chr(0), '')
for l in t.splitlines():
    s = l.strip()
    if s.startswith(('E56', 'DX-')) and 'CONFLICT' not in s:
        print(s[:160])
print('=== diffs eager vs deferred ===')
for n in [500, 1000, 1500, 2000, 2900]:
    a = 'e56_eager_%d.txt' % n
    b = 'e56_deferred_%d.txt' % n
    try:
        la = open(a, encoding='utf-8').read().splitlines()
        lb = open(b, encoding='utf-8').read().splitlines()
        if la == lb:
            print('N=%d IDENTICOS (%d lineas)' % (n, len(la)))
        else:
            sa, sb = set(la), set(lb)
            print('N=%d DIFIEREN solo-eager=%d solo-deferred=%d' % (n, len(sa - sb), len(sb - sa)))
            for l in sorted(sa - sb)[:5]:
                print('  E>', l[:150])
            for l in sorted(sb - sa)[:5]:
                print('  D>', l[:150])
    except FileNotFoundError as e:
        print('N=%d falta fichero: %s' % (n, e))
