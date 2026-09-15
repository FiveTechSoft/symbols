import re

old = set('juan maria pedro ana john mary peter anne coche casa libro '
          'empresa manzana bicicleta car house book phone madrid barcelona '
          'malaga marbella london paris berlin rojo azul red blue green '
          'yellow'.split())
stop = set('compro tiene vive en ayer bought has have lives in yesterday '
           'what did who bought the where when does color is de que es el '
           'la las los una un a an the and 2026 2027 2028 2030 2031 '
           '2032'.split())
rx = re.compile(r'^test_data\(\d+, \w+, \w+, "(.*)", ".*", (\w+)\)\.\s*$')
novel = set()
for line in open('holdout_1000_data.pl', encoding='utf-8'):
    m = rx.match(line)
    if m:
        novel.update(m.group(1).lower().replace('.', '').split())
        novel.add(m.group(2))
leak = (novel - stop) & old
print('leaked vocab:', sorted(leak) if leak else 'NONE - hold-out is disjoint')
