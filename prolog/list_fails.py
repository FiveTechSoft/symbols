fails = []
with open('open_results.txt', encoding='utf-8') as f:
    for line in f:
        parts = line.strip().split('|')
        if len(parts) == 6 and parts[5] == 'fail':
            fails.append(parts)
print('total fails:', len(fails))
for p in fails:
    print(f"#{p[0]} [{p[1]}/{p[2]}] Exp:{p[3]} Got:{p[4]}")
