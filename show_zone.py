import sys

lines = open('corpus_biblia/kjv.txt', encoding='utf-8', errors='replace').read().splitlines()
a, b = int(sys.argv[1]), int(sys.argv[2])
for i in range(a - 1, b):
    print(i + 1, repr(lines[i][:120]))
