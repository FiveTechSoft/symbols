"""curve_e57.py — filas E57-COLLECT/TOTAL/EQUIV por tag."""
import re

t = open('exp57c.log', encoding='utf-8', errors='replace').read().splitlines()
outs = []
for l in t:
    s = l.strip()
    if s.startswith('E57-COLLECT') or s.startswith('E57-TOTAL') or s.startswith('E57-EQUIV'):
        outs.append(s[:220])
    elif s.startswith('E57-INSERT') or s.startswith('E57-REPLAY'):
        outs.append(s[:160])
open('curve_out.txt', 'w', encoding='utf-8').write('\n'.join(outs))
