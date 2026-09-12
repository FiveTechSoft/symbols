"""sample_mem.py — primeras N lineas memfact + busqueda por verbo."""
import sys

p = sys.argv[1]
n = int(sys.argv[2]) if len(sys.argv) > 2 else 40
q = sys.argv[3] if len(sys.argv) > 3 else None
i = 0
for l in open(p, encoding='utf-8'):
    s = l.strip()
    if not s.startswith('memfact('):
        continue
    if q and (', %s,' % q) not in s:
        continue
    print(s[:150])
    i += 1
    if i >= n:
        break
