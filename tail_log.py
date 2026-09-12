"""tail_log.py — ultimas lineas no vacias de un log swipl."""
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
lines = [l for l in t.splitlines() if l.strip()]
for l in lines[-25:]:
    print(l[:200])
