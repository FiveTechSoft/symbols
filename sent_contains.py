"""sent_contains.py — frases que contienen todas las palabras dadas."""
import re
import sys

words = [w.lower() for w in sys.argv[1:]]
t = open('books/alice.txt', encoding='utf-8', errors='replace').read()
sents = [s for s in re.split(r'[.!?;:]', t) if s.strip()]
for s in sents:
    low = s.lower()
    if all(w in low for w in words):
        print(repr(' '.join(s.split())[:260]))
        print('---')
