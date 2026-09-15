import re
rx = re.compile(r'^test_data\((\d+), (\w+), "([^"]+)", "(.*?)", "(.*?)", (\w+)\)\.\s*$')
for line in open('open_world_data.pl', encoding='utf-8'):
    m = rx.match(line)
    if m:
        i, g, ep, text, q, exp = m.groups()
        if any(w in q for w in ('belong', 'contain', 'depict', 'horse', 'map ',
                                'museum', 'archive')):
            print(i, g, ep, '|', q, '=>', exp)
