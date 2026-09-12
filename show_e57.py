"""show_e57.py — lineas E57* del log."""
import sys

raw = open('exp57.log', 'rb').read()
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
    if s.startswith('E57') or s.startswith('ERROR'):
        print(s[:200])
