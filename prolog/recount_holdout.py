"""Independent recount of holdout_1000_results.txt (dual-method verification)."""
from collections import Counter

total = Counter()
by_lang = Counter()
by_type = Counter()
bad_examples = {}
n = 0
with open('holdout_1000_results.txt', encoding='utf-8') as f:
    for line in f:
        parts = line.strip().split('|')
        assert len(parts) == 6, line
        i, lang, qt, exp, got, st = parts
        n += 1
        total[st] += 1
        if st == 'pass':
            by_lang[lang] += 1
            by_type[qt] += 1
        else:
            bad_examples.setdefault((lang, qt), Counter())[(exp, got)] += 1

print('TOTAL lines:', n)
print('PASS:', total['pass'], 'FAIL:', total['fail'], 'ERROR:', total['error'])
print('by_lang pass:', dict(by_lang))
print('by_type pass:', dict(by_type))
# correlate failures with numeric-time texts
import re
times = {}
for line in open('holdout_1000_data.pl', encoding='utf-8'):
    m = re.match(r'^test_data\((\d+), (\w+), (\w+), "(.*)", ".*", (\w+)\)\.\s*$', line)
    if m:
        times[int(m.group(1))] = m.group(4)
year_fails = year_total = 0
for line in open('holdout_1000_results.txt', encoding='utf-8'):
    i, lang, qt, exp, got, st = line.strip().split('|')
    has_year = bool(re.search(r'\b(202[6-8]|203[012])\b', times[int(i)]))
    year_total += has_year
    year_fails += (has_year and st != 'pass')
print('year-text lines:', year_total, 'fails among them:', year_fails)
print('non-year fails:', (total['fail'] + total['error']) - year_fails)
