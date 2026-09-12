"""read_log.py — lee logs swipl (utf-16 o utf-8) y filtra lineas clave."""
import sys

raw = open(sys.argv[1], 'rb').read()
t = None
for enc in ('utf-16', 'utf-8'):
    try:
        t = raw.decode(enc)
        break
    except Exception:
        continue
t = t.replace(chr(0), '')
lines = t.splitlines()
print('lineas:', len(lines))
rej = sum(1 for l in lines if l.startswith('REJECTED'))
print('REJECTED:', rej)
for l in lines:
    s = l.strip()
    if (s.startswith('PILOT') or 'AFTER-INGEST' in s or s.startswith('stored')
            or 'ERROR' in s or 'Warning' in s or 'passed' in s):
        print(s[:200])
print('--- primeras tripletas ---')
n = 0
for l in lines:
    if '--' in l and '>' in l and not l.startswith('REJECTED'):
        print(l[:160])
        n += 1
        if n >= 15:
            break
