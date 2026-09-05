import requests
import json
import os

os.makedirs("data/bible/books", exist_ok=True)

urls = [
    ("https://raw.githubusercontent.com/aruljohn/Bible-kjv/master/Matthew.json", "Matthew"),
    ("https://raw.githubusercontent.com/aruljohn/Bible-kjv/master/Mark.json", "Mark"),
    ("https://raw.githubusercontent.com/aruljohn/Bible-kjv/master/Luke.json", "Luke"),
    ("https://raw.githubusercontent.com/aruljohn/Bible-kjv/master/John.json", "John"),
    ("https://raw.githubusercontent.com/aruljohn/Bible-kjv/master/Acts.json", "Acts"),
    ("https://raw.githubusercontent.com/aruljohn/Bible-kjv/master/Romans.json", "Romans"),
    ("https://raw.githubusercontent.com/aruljohn/Bible-kjv/master/1Corinthians.json", "1Corinthians"),
    ("https://raw.githubusercontent.com/aruljohn/Bible-kjv/master/2Corinthians.json", "2Corinthians"),
    ("https://raw.githubusercontent.com/aruljohn/Bible-kjv/master/Galatians.json", "Galatians"),
    ("https://raw.githubusercontent.com/aruljohn/Bible-kjv/master/Ephesians.json", "Ephesians"),
    ("https://raw.githubusercontent.com/aruljohn/Bible-kjv/master/Philippians.json", "Philippians"),
    ("https://raw.githubusercontent.com/aruljohn/Bible-kjv/master/Colossians.json", "Colossians"),
    ("https://raw.githubusercontent.com/aruljohn/Bible-kjv/master/1Thessalonians.json", "1Thessalonians"),
    ("https://raw.githubusercontent.com/aruljohn/Bible-kjv/master/2Thessalonians.json", "2Thessalonians"),
    ("https://raw.githubusercontent.com/aruljohn/Bible-kjv/master/1Timothy.json", "1Timothy"),
    ("https://raw.githubusercontent.com/aruljohn/Bible-kjv/master/2Timothy.json", "2Timothy"),
    ("https://raw.githubusercontent.com/aruljohn/Bible-kjv/master/Titus.json", "Titus"),
    ("https://raw.githubusercontent.com/aruljohn/Bible-kjv/master/Philemon.json", "Philemon"),
    ("https://raw.githubusercontent.com/aruljohn/Bible-kjv/master/Hebrews.json", "Hebrews"),
    ("https://raw.githubusercontent.com/aruljohn/Bible-kjv/master/James.json", "James"),
    ("https://raw.githubusercontent.com/aruljohn/Bible-kjv/master/1Peter.json", "1Peter"),
    ("https://raw.githubusercontent.com/aruljohn/Bible-kjv/master/2Peter.json", "2Peter"),
    ("https://raw.githubusercontent.com/aruljohn/Bible-kjv/master/1John.json", "1John"),
    ("https://raw.githubusercontent.com/aruljohn/Bible-kjv/master/2John.json", "2John"),
    ("https://raw.githubusercontent.com/aruljohn/Bible-kjv/master/3John.json", "3John"),
    ("https://raw.githubusercontent.com/aruljohn/Bible-kjv/master/Jude.json", "Jude"),
    ("https://raw.githubusercontent.com/aruljohn/Bible-kjv/master/Revelation.json", "Revelation"),
]

# Map to our book IDs
NAME_TO_ID = {
    "Matthew": "MAT", "Mark": "MAR", "Luke": "LUK", "John": "JOH",
    "Acts": "ACT", "Romans": "ROM", "1Corinthians": "1CO", "2Corinthians": "2CO",
    "Galatians": "GAL", "Ephesians": "EPH", "Philippians": "PHP",
    "Colossians": "COL", "1Thessalonians": "1TH", "2Thessalonians": "2TH",
    "1Timothy": "1TI", "2Timothy": "2TI", "Titus": "TIT", "Philemon": "PHM",
    "Hebrews": "HEB", "James": "JAS", "1Peter": "1PE", "2Peter": "2PE",
    "1John": "1JO", "2John": "2JO", "3John": "3JO", "Jude": "JUD",
    "Revelation": "REV",
}

total = 0
for url, name in urls:
    book_id = NAME_TO_ID[name]
    book_file = f"data/bible/books/{book_id}.tsv"

    # Skip if already has verses
    if os.path.exists(book_file):
        with open(book_file, 'r', encoding='utf-8') as f:
            count = sum(1 for _ in f)
        if count > 0:
            total += count
            print(f"SKIP {name}: {count} verses (cached)")
            continue

    try:
        r = requests.get(url, timeout=15, headers={"User-Agent": "SymbolicLLM/1.0"})
        if r.status_code != 200:
            print(f"FAIL {name}: HTTP {r.status_code}")
            continue

        data = r.json()
        # Format: {"book": "Name", "chapters": [{"chapter": "1", "verses": [{"verse": "1", "text": "..."}]}]}
        lines = []
        for ch_data in data.get("chapters", []):
            ch = int(ch_data["chapter"])
            for v_data in ch_data.get("verses", []):
                v = int(v_data["verse"])
                text = v_data.get("text", "").strip()
                if text:
                    lines.append(f"{book_id} {ch}:{v}\t{text}")

        with open(book_file, 'w', encoding='utf-8') as f:
            for line in lines:
                f.write(line + '\n')

        total += len(lines)
        print(f"OK {name}: {len(lines)} verses")

    except Exception as e:
        print(f"ERR {name}: {e}")

print(f"\nTotal NT: {total} verses")
