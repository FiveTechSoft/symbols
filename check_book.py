"""check_book.py — verifica libro descargado."""
import sys

p = sys.argv[1]
t = open(p, encoding='utf-8', errors='replace').read()
lines = t.splitlines()
print('lineas:', len(lines), 'bytes:', len(t))
print('--- head ---')
for l in lines[:8]:
    print(repr(l[:100]))
print('--- buscar inicio real ---')
for i, l in enumerate(lines[:120]):
    if 'CHAPTER' in l.upper() and len(l.strip()) < 60:
        print(i, repr(l[:100]))
        break
