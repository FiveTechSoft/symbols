"""biblia_stats.py — metricas del log de run_biblia (utf-16/utf-8)."""
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
    if (s.startswith('BIBLIA') or s.startswith('book ')
            or s.startswith('BOOK ') or s.startswith('stored triples')
            or 'ERROR' in s or 'verses ingested' in s):
        print(s[:220])
