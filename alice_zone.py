"""alice_zone.py — muestra lineas de books/alice.txt."""
import sys

a, b = int(sys.argv[1]), int(sys.argv[2])
lines = open('books/alice.txt', encoding='utf-8', errors='replace').read().splitlines()
for i in range(a - 1, b):
    print(i + 1, repr(lines[i][:130]))
