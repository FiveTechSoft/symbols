"""show_e57c.py — E57 del log curva + err, salida ASCII segura."""
import sys

raw = open('exp57c.log', 'rb').read()
t = None
for enc in ('utf-16', 'utf-8'):
    try:
        t = raw.decode(enc)
        break
    except Exception:
        continue
t = t.replace(chr(0), '')
out = []
for l in t.splitlines():
    s = l.strip()
    if s.startswith('E57-COLLECT') or s.startswith('E57-TOTAL') or s.startswith('E57-EQUIV'):
        out.append(s[:200])
    elif s.startswith('E57-INSERT') or s.startswith('E57-REPLAY') or s.startswith('ERROR'):
        out.append(s[:200])
for l in out:
    print(l.encode('ascii', errors='replace').decode('ascii'))
print('--- err ---')
try:
    e = open('exp57c.err', encoding='utf-8', errors='replace').read()
    print(e[:800].encode('ascii', errors='replace').decode('ascii'))
except Exception as ex:
    print('no err: %s' % ex)
