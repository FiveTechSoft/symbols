"""count_pilot.py — cuenta REJECTED vs hechos en el log del piloto."""
import sys

log = open(sys.argv[1], encoding='utf-8', errors='replace').read().splitlines()
rej = sum(1 for l in log if l.startswith('REJECTED'))
print('lineas log:', len(log), '| REJECTED:', rej, '| aceptadas~:', 2798 - rej)
for l in log:
    s = l.strip()
    if s.startswith('PILOT') or 'AFTER-INGEST' in s or s.startswith('stored') or 'ERROR' in s or 'Warning' in s:
        print(s[:200])
print('--- muestra (primeras 12 tripletas) ---')
n = 0
for l in log:
    if '--' in l and '>' in l and not l.startswith('REJECTED'):
        print(l[:160])
        n += 1
        if n >= 12:
            break
