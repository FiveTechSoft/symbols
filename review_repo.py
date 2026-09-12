"""review_repo.py — vuelca a review_out/ lo esencial de origin/master."""
import os
import subprocess

OUT = 'review_out'
os.makedirs(OUT, exist_ok=True)

def show(ref_path, out_name, max_lines=120):
    r = subprocess.run(['git', 'show', 'origin/master:' + ref_path],
                       capture_output=True, cwd='C:/temp')
    if r.returncode != 0:
        open(os.path.join(OUT, out_name), 'w').write('SHOW-FAILED ' + ref_path)
        return
    lines = r.stdout.decode('utf-8', errors='replace').splitlines()
    head = '\n'.join(lines[:max_lines])
    open(os.path.join(OUT, out_name), 'w', encoding='utf-8').write(head)
    print('%-40s lineas=%d' % (ref_path, len(lines)))

for f in ['prolog/README.md', 'AGENTS.md', 'README.md', 'roadmap.md',
          'prolog/experiment49.pl', 'prolog/experiment50.pl',
          'prolog/experiment51.pl', 'prolog/experiment52.pl',
          'prolog/experiment53.pl']:
    show(f, f.replace('/', '__') + '.head.txt')

# tamano del resto de docs raiz
r = subprocess.run(['git', 'show', 'origin/master:symbols.md'],
                   capture_output=True, cwd='C:/temp')
print('symbols.md lineas=%d' % len(r.stdout.decode('utf-8', errors='replace').splitlines()))
