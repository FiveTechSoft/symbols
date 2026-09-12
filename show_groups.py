"""show_groups.py — lineas GROUP del log."""
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
for l in t.splitlines():
    s = l.strip()
    if s.startswith('GROUP'):
        print(s[:160])
