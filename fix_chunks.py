"""fix_chunks.py — renombra gen_chunk residuales a gen_chunk_core."""
lines = open('gen_parse.pl', encoding='utf-8').read().splitlines()
n = 0
for i, l in enumerate(lines):
    s = l.strip()
    if s.startswith('gen_chunk(C, T)') or s == 'gen_chunk(C, C).':
        lines[i] = l.replace('gen_chunk(', 'gen_chunk_core(', 1)
        n += 1
open('gen_parse.pl', 'w', encoding='utf-8').write('\n'.join(lines) + '\n')
print('renombradas:', n)
