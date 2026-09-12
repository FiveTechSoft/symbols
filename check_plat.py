"""check_plat.py — verifica encoding y guiones de plataforma.txt."""
raw = open('books/plataforma.txt', 'rb').read()
print('bytes:', len(raw))
print('tiene o-utf8 (c3 b3):', raw.count(b'\xc3\xb3'))
print('tiene 0xF3 suelto:', raw.count(b'\xf3'))
print('tiene soft hyphen:', raw.count('­'.encode('utf-8')))
import re
t = raw.decode('utf-8', errors='replace')
print('guion+salto restantes:', len(re.findall(r'-\n', t)))
print('replacement chars:', t.count('�'))
i = t.find('Castej')
print(repr(t[i - 20:i + 60]))
