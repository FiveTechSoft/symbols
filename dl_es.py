"""dl_es.py — descarga libros en español (Gutenberg)."""
import urllib.request
import os

CANDS = {
    374: 'lazarillo.txt',
    2000: 'quijote.txt',
    15115: 'leyendas.txt',
    47629: 'desconocido.txt',
}
os.makedirs('books', exist_ok=True)
for i, name in CANDS.items():
    dest = 'books/' + name
    if os.path.exists(dest) and os.path.getsize(dest) > 50000:
        print(i, 'ya existe', os.path.getsize(dest))
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
        except Exception as ex:
            print(i, 'fallo', pat.split('/')[-1], str(ex)[:60])
            continue
    if data and len(data) > 50000:
        open(dest, 'wb').write(data)
        print(i, 'OK', len(data))
    else:
        print(i, 'SIN DATOS')
