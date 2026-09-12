"""show_exp54.py — resume exp54_1.log (utf-16/utf-8). argv: log + secciones."""
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
want = sys.argv[2:] or ['REL', 'SUBJ', 'OBJ', 'PAIR', 'TRI', 'FUNCTIONAL', 'EXP54', 'ERROR']
for l in t.splitlines():
    s = l.strip()
    if not s or 'Warning' in s or 'CategoryInfo' in s or 'FullyQualified' in s:
        continue
    if s.startswith('swipl.exe'):
        continue
    for w in want:
        if s.startswith(w):
            print(s[:200])
            break
