"""rep_head.py — cabecera del BOOK-REPORT (primeras filas)."""
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
i = next(k for k, l in enumerate(lines) if l.strip().startswith('BOOK-REPORT'))
for l in lines[i:i + 20]:
    print(l[:160])
