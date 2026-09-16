import csv

# Ruth: short (4 chapters, 85 verses), narrative prose, rich in SVO actions:
# names (ruth, naomi, boaz, orpah, mahlon, chilion, elimelech) + verbs (went, said, took, dwelt...).
# Better multi-fact candidate than Jonah (which is mostly prayer/dialog).
rows = [r for r in csv.reader(open(r'C:\symbols\prolog\corpus_biblia\verses.tsv', encoding='utf-8'), delimiter='\t') if r and r[1] == 'The Book of Ruth']
print('ruth verses:', len(rows))

out = r'C:\Users\Anto\AppData\Local\Temp\opencode\kjv_ruth_full.txt'
with open(out, 'w', encoding='utf-8') as f:
    for r in rows:
        vid = r[0].lower().replace('the book of ruth ', '').replace(':', '_')
        text = r[3].replace(';', ',').replace('"', '').replace('\'', '').replace('\u2019s', 's').replace('\u2019', '')
        f.write(vid + '\t' + text + '\n')
print('written', out)

# Also build Ruth+Jonah combined (2 books, KB with cross-entity interference)
out2 = r'C:\Users\Anto\AppData\Local\Temp\opencode\kjv_rj_full.txt'
with open(out2, 'w', encoding='utf-8') as f:
    for book, pref in (('The Book of Ruth', 'r'), ('Jonah', 'j')):
        rs = [r for r in csv.reader(open(r'C:\symbols\prolog\corpus_biblia\verses.tsv', encoding='utf-8'), delimiter='\t') if r and r[1] == book]
        for r in rs:
            vid = r[0].lower().replace(book.lower() + ' ', '').replace(':', '_')
            if book != 'Jonah':
                vid = vid.replace('the book of ruth ', '')
            text = r[3].replace(';', ',').replace('"', '').replace('\'', '').replace('\u2019s', 's').replace('\u2019', '')
            f.write(pref + '_' + vid + '\t' + text + '\n')
print('written', out2, 'combined')