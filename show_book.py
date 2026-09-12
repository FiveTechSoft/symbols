"""show_book.py — resume book_*.log."""
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
    if not s or 'Warning' in s or 'CategoryInfo' in s or 'FullyQualified' in s:
        continue
    if s.startswith('swipl.exe') or s.startswith('+'):
        continue
    if s.startswith(('DOC-', 'BOOK-', '  ', 'ERROR')):
        print(s[:160])
