"""slice_genesis.py — piloto: frases de Genesis a corpus_pilot/corpus.txt"""
import os

os.makedirs('corpus_pilot', exist_ok=True)
prov = open('corpus_biblia/prov.tsv', encoding='utf-8').read().splitlines()
corp = open('corpus_biblia/corpus.txt', encoding='utf-8').read().splitlines()
assert len(prov) == len(corp), (len(prov), len(corp))

gen = [(p, c) for p, c in zip(prov, corp)
       if p.split('\t')[1].startswith('The First Book of Moses')]
print('frases genesis:', len(gen))
with open('corpus_pilot/corpus.txt', 'w', encoding='utf-8') as fh:
    for _, c in gen:
        fh.write(c + '\n')
with open('corpus_pilot/prov.tsv', 'w', encoding='utf-8') as fh:
    for p, _ in gen:
        fh.write(p + '\n')
print('ejemplo:', gen[0][1][:120])
print('ejemplo:', gen[5][1][:120])
