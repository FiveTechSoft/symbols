"""check_saved.py — verifica kjv_memory.pl y linea SAVED del log."""
import os
import re

raw = open('biblia_save.log', 'rb').read()
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
    if s.startswith('SAVED') or s.startswith('BIBLIA '):
        print(s[:200])

p = 'kjv_memory.pl'
print('existe:', os.path.exists(p), 'bytes:', os.path.getsize(p) if os.path.exists(p) else 0)
n_mem = n_prov = 0
with open(p, encoding='utf-8') as fh:
    for l in fh:
        if l.startswith('memfact('):
            n_mem += 1
        elif l.startswith('provfact('):
            n_prov += 1
print('py  memfact:', n_mem, 'provfact:', n_prov)
