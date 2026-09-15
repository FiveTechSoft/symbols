"""Independent recount of open_results.txt (dual-method verification)."""
from collections import Counter

total = Counter()
by_group = {}
isolation = Counter()
unknown = Counter()
probes = Counter()
n = 0
with open('open_results.txt', encoding='utf-8') as f:
    for line in f:
        parts = line.strip().split('|')
        assert len(parts) == 6, line
        i, group, ep, exp, got, mark = parts
        n += 1
        total[mark] += 1
        by_group.setdefault(group, Counter())[mark] += 1
        if ep == 'AB':
            isolation[mark] += 1
        if exp in ('unknown', 'no_evidence'):
            unknown[mark] += 1
        if ep in ('F', 'P'):
            probes[mark] += 1

print('TOTAL lines:', n)
print('PASS:', total['pass'], 'FAIL:', total['fail'])
for g, c in sorted(by_group.items()):
    print(f'  {g}: {c["pass"]}/{c["pass"] + c["fail"]}')
print('isolation(AB):', isolation['pass'], '/',
      isolation['pass'] + isolation['fail'])
print('unknown:', unknown['pass'], '/',
      unknown['pass'] + unknown['fail'])
print('probes(F/P):', probes['pass'], '/',
      probes['pass'] + probes['fail'])
