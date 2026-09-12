"""diff_e56.py — diff por tipo M/P/C y contexto minimo."""
import sys

n = sys.argv[1] if len(sys.argv) > 1 else '500'
la = open('e56_eager_%s.txt' % n, encoding='utf-8').read().splitlines()
lb = open('e56_deferred_%s.txt' % n, encoding='utf-8').read().splitlines()
sa, sb = set(la), set(lb)
print('N=%s eager=%d deferred=%d' % (n, len(la), len(lb)))
for t in ['M ', 'P ', 'C ']:
    a = [l for l in sa - sb if l.startswith(t)]
    b = [l for l in sb - sa if l.startswith(t)]
    print('%s solo-eager=%d solo-deferred=%d' % (t.strip(), len(a), len(b)))
    for l in sorted(a)[:4]:
        print('  E>', l[:150])
    for l in sorted(b)[:4]:
        print('  D>', l[:150])
