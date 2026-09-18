import json
from collections import Counter

rows = [json.loads(ln) for ln in
        open('observations.jsonl', encoding='utf-8') if ln.strip()]
print('ROWS', len(rows))
print('by status:', dict(Counter(d.get('status') for d in rows)))
print('by intent:', dict(Counter(d.get('intent') for d in rows)))
print('top queries:')
for q, n in Counter(d.get('query') for d in rows).most_common(8):
    print(' ', n, repr((q or '')[:70]))
lats = [d.get('latency_ms', 0) for d in rows]
print('latency ms: min', min(lats), 'max', max(lats),
      'avg', round(sum(lats) / len(lats), 1))
unk = [d for d in rows if d.get('status') in ('UNKNOWN', 'ABSTAIN')]
print('unknown/abstain distinct queries:')
for q in sorted(set(d.get('query') for d in unk))[:15]:
    print('  ', repr((q or '')[:80]))
