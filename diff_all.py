"""diff_all.py — diff M/P/C para los 5 N."""
for n in ['500', '1000', '1500', '2000', '2900']:
    la = open('e56_eager_%s.txt' % n, encoding='utf-8').read().splitlines()
    lb = open('e56_deferred_%s.txt' % n, encoding='utf-8').read().splitlines()
    sa, sb = set(la), set(lb)
    only_a = len(sa - sb)
    only_b = len(sb - sa)
    print('N=%s lineas=%d diff=%s' % (n, len(la), 'IDENTICOS' if only_a == 0 and only_b == 0 else 'DIFIEREN %d/%d' % (only_a, only_b)))
