import csv
from collections import defaultdict

rows = list(csv.reader(open('data/bible/bible_relations.tsv',
                             encoding='utf-8'), delimiter='\t'))
tax = defaultdict(set)
for s, r, o, _ in rows:
    if r == 'HIJO_DE':
        tax[s].add(o)
multi = {s: sorted(o) for s, o in tax.items() if len(o) > 1}
print('subjects HIJO_DE:', len(tax), 'multi-parent:', len(multi))
for s in ('JAMES', 'SAUL', 'DAVID', 'JESSE', 'SOLOMON'):
    if s in tax:
        print(s, '->', sorted(tax[s]))
print('--- top multi-parent (up to 10) ---')
for s, objs in sorted(multi.items(), key=lambda kv: -len(kv[1]))[:10]:
    print(s, '->', objs)
