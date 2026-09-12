"""spot_tasks.py — muestra 2 tareas por bloque para revision manual."""
import random

random.seed(7)
rows = [l.rstrip('\n').split('\t') for l in open('bookbrain/tasks_raw.tsv', encoding='utf-8')]
by = {}
for r in rows:
    by.setdefault(r[1], []).append(r)
for b in ['COMP', 'QA', 'REAS', 'MEM', 'GEN']:
    print('=== %s ===' % b)
    for r in random.sample(by[b], 2):
        print('  %s | %s | exp=%s | %s | %s' % (r[0], r[2], r[3], r[4], r[5]))
