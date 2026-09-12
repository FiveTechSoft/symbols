"""extract_pdf.py — PDF -> texto plano (pypdf). Uso: extract_pdf.py IN.pdf OUT.txt"""
import re
import sys

from pypdf import PdfReader

src, dst = sys.argv[1], sys.argv[2]
rd = PdfReader(src)
print('paginas:', len(rd.pages))
parts = []
for i, pg in enumerate(rd.pages):
    try:
        parts.append(pg.extract_text() or '')
    except Exception as ex:
        print('pagina %d: %s' % (i, ex))
t = '\n'.join(parts)
t = t.replace('­', '')            # soft hyphen U+00AD
t = re.sub(r'-\n', '', t)           # guion de fin de linea -> une palabra
t = re.sub(r'[ \t]+\n', '\n', t)
t = re.sub(r'\n{3,}', '\n\n', t)
open(dst, 'w', encoding='utf-8').write(t)
print('bytes:', len(t.encode('utf-8')), '->', dst)
print('--- head ---')
print(t[:400])
