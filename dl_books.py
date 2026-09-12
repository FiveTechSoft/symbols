"""dl_books.py — descarga candidatos Gutenberg a books/g<ID>.txt."""
import os
import urllib.request

IDS = [1342, 84, 1661, 74, 76, 98, 1400, 43, 36, 345, 1260, 768, 174,
       2591, 844, 147, 158, 1259, 120, 45, 55, 62, 205, 1184, 2701,
       37106, 141, 219, 214, 215, 201, 216, 217, 218, 221, 222, 223,
       224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235,
       236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247]
# Nota: algunos IDs pueden no existir o no ser ingles; se registran fallos.

os.makedirs('books', exist_ok=True)
ok, fail = [], []
for i in IDS:
    dest = 'books/g%d.txt' % i
    if os.path.exists(dest) and os.path.getsize(dest) > 50000:
        ok.append(i)
        continue
    data = None
    for pat in ['https://www.gutenberg.org/files/%d/%d-0.txt',
                'https://www.gutenberg.org/files/%d/%d.txt',
                'https://www.gutenberg.org/files/%d/%d-8.txt']:
        try:
            req = urllib.request.Request(pat % (i, i), headers={'User-Agent': 'BookBrain/1.0'})
            with urllib.request.urlopen(req, timeout=30) as r:
                data = r.read()
            break
        except Exception:
            continue
    if data and len(data) > 50000:
        open(dest, 'wb').write(data)
        ok.append(i)
    else:
        fail.append(i)
print('OK:', len(ok), ok)
print('FAIL:', len(fail), fail)
with open('books/index.tsv', 'w', encoding='utf-8') as fh:
    for i in ok:
        t = ''
        try:
            for l in open('books/g%d.txt' % i, encoding='utf-8', errors='replace'):
                if l.startswith('Title:'):
                    t = l[6:].strip()
                    break
        except Exception:
            pass
        fh.write('%d\t%s\t%d\n' % (i, t, os.path.getsize('books/g%d.txt' % i)))
