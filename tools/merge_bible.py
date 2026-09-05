import os

books_dir = "data/bible/books"
files = sorted(os.listdir(books_dir))
total = 0
all_lines = []
for f in files:
    path = os.path.join(books_dir, f)
    with open(path, "r", encoding="utf-8") as fh:
        lines = [l.strip() for l in fh if l.strip()]
    total += len(lines)
    all_lines.extend(lines)

merged = "data/bible/all_chapters.tsv"
with open(merged, "w", encoding="utf-8") as fh:
    for l in all_lines:
        fh.write(l + "\n")

size = os.path.getsize(merged)
print(f"{len(files)} books, {total} verses, merged to {merged} ({size // 1024}KB)")
