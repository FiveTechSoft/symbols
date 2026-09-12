"""show_e56b.py — solo lineas E56-EAGER/DEFERRED/DONE."""
import sys

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
    if s.startswith('E56-EAGER') or s.startswith('E56-DEFERRED') or s == 'E56-DONE':
        print(s[:200])
