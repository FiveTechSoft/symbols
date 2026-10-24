import argparse, csv, json, re, sys, urllib.request
from pathlib import Path

ap = argparse.ArgumentParser(description="Token-match diagnostic only, not functional correctness or OpenCode acceptance")
ap.add_argument("--endpoint", default="http://127.0.0.1:8100/v1/chat/completions")
ap.add_argument("--bank", type=Path, default=Path(__file__).with_name("bank.tsv"))
ap.add_argument("--output", type=Path, required=True)
args = ap.parse_args()

rows = list(csv.DictReader(args.bank.open(), delimiter='\t'))
results = []
for r in rows:
    body = json.dumps({"model":"symbols","messages":[{"role":"user","content":r['question']}]}).encode()
    req = urllib.request.Request(args.endpoint, data=body, headers={"Content-Type":"application/json"})
    try:
        with urllib.request.urlopen(req, timeout=20) as resp:
            d = json.load(resp)
        reply = d['choices'][0]['message']['content']
    except Exception as e:
        reply = f"<ERROR: {e}>"
    low = reply.lower()
    if r['mode'] == 'contains':
        toks = [t.lower() for t in r['pattern'].split('|') if t]
        ok = all(t in low for t in toks)
    else:  # abstain
        ok = bool(re.search(r"don'?t know|abstain|no consta|no lo s[ée]", low))
    results.append({"id": r['id'], "cat": r['cat'], "q": r['question'], "reply": reply, "pass": ok})

with args.output.open('w') as f:
    for x in results:
        f.write(json.dumps(x) + "\n")

from collections import Counter
tot = Counter(); ok = Counter()
for x in results:
    tot[x['cat']] += 1
    if x['pass']: ok[x['cat']] += 1
for c in sorted(tot):
    print(f"{c}: {ok[c]}/{tot[c]}")
print(f"TOTAL: {sum(ok.values())}/{sum(tot.values())}")
print("FAILS:", " ".join(x['id'] for x in results if not x['pass']))
