"""
Full Bible Knowledge Extractor — Fetches all 66 books, extracts SVO triples with spaCy.
Uses Bible API (free, no auth) + spaCy dependency parsing.
"""
import requests
import time
import json
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from extract_triples_spacy import extract_batch

# All 66 Bible books (Spanish names for RVR1960)
BIBLE_BOOKS = [
    # Old Testament (39)
    ("Génesis", 50), ("Éxodo", 40), ("Levítico", 27), ("Números", 36),
    ("Deuteronomio", 34), ("Josué", 24), ("Jueces", 21), ("Rut", 4),
    ("1 Samuel", 31), ("2 Samuel", 24), ("1 Reyes", 22), ("2 Reyes", 25),
    ("1 Crónicas", 29), ("2 Crónicas", 36), ("Esdras", 10), ("Nehemías", 13),
    ("Ester", 10), ("Job", 42), ("Salmos", 150), ("Proverbios", 31),
    ("Eclesiastés", 12), ("Cantares", 8), ("Isaías", 66), ("Jeremías", 52),
    ("Lamentaciones", 5), ("Ezequiel", 48), ("Daniel", 12), ("Oseas", 14),
    ("Joel", 3), ("Amós", 9), ("Abdías", 1), ("Jonás", 4),
    ("Miqueas", 7), ("Nahúm", 3), ("Habacuc", 3), ("Sofonías", 3),
    ("Hageo", 2), ("Zacarías", 14), ("Malaquías", 4),
    # New Testament (27)
    ("Mateo", 28), ("Marcos", 16), ("Lucas", 24), ("Juan", 21),
    ("Hechos", 28), ("Romanos", 16), ("1 Corintios", 16), ("2 Corintios", 13),
    ("Gálatas", 6), ("Efesios", 6), ("Filipenses", 4), ("Colosenses", 4),
    ("1 Tesalonicenses", 5), ("2 Tesalonicenses", 3), ("1 Timoteo", 6),
    ("2 Timoteo", 4), ("Tito", 3), ("Filemón", 1), ("Hebreos", 13),
    ("Santiago", 5), ("1 Pedro", 5), ("2 Pedro", 3), ("1 Juan", 5),
    ("2 Juan", 1), ("3 Juan", 1), ("Judas", 1), ("Apocalipsis", 22),
]

# Map book names to short codes for file naming
BOOK_CODES = {
    "Génesis": "GEN", "Éxodo": "EXO", "Levítico": "LEV", "Números": "NUM",
    "Deuteronomio": "DEU", "Josué": "JOS", "Jueces": "JUE", "Rut": "RUT",
    "1 Samuel": "1SA", "2 Samuel": "2SA", "1 Reyes": "1RE", "2 Reyes": "2RE",
    "1 Crónicas": "1CR", "2 Crónicas": "2CR", "Esdras": "ESD", "Nehemías": "NEH",
    "Ester": "EST", "Job": "JOB", "Salmos": "SAL", "Proverbios": "PRO",
    "Eclesiastés": "ECC", "Cantares": "CAN", "Isaías": "ISA", "Jeremías": "JER",
    "Lamentaciones": "LAM", "Ezequiel": "EZE", "Daniel": "DAN", "Oseas": "OSA",
    "Joel": "JOL", "Amós": "AMO", "Abdías": "ABD", "Jonás": "JON",
    "Miqueas": "MIQ", "Nahúm": "NAH", "Habacuc": "HAB", "Sofonías": "SOF",
    "Hageo": "HAG", "Zacarías": "ZAC", "Malaquías": "MAL",
    "Mateo": "MAT", "Marcos": "MAR", "Lucas": "LUC", "Juan": "JUA",
    "Hechos": "HEC", "Romanos": "ROM", "1 Corintios": "1CO", "2 Corintios": "2CO",
    "Gálatas": "GAL", "Efesios": "EFE", "Filipenses": "FIL", "Colosenses": "COL",
    "1 Tesalonicenses": "1TE", "2 Tesalonicenses": "2TE", "1 Timoteo": "1TI",
    "2 Timoteo": "2TI", "Tito": "TIT", "Filemón": "FILM", "Hebreos": "HEB",
    "Santiago": "SAC", "1 Pedro": "1PE", "2 Pedro": "2PE", "1 Juan": "1JU",
    "2 Juan": "2JU", "3 Juan": "3JU", "Judas": "JUD", "Apocalipsis": "APO",
}


def fetch_chapter(book_name, chapter, max_retries=3):
    """Fetch a single chapter from Bible API."""
    api_name = book_name.replace(" ", "+")
    url = f"https://bible-api.com/{api_name}+{chapter}?translation=rvr1960"

    for attempt in range(max_retries):
        try:
            r = requests.get(url, timeout=20,
                           headers={'User-Agent': 'SymbolicLLM/1.0 (research)'})
            if r.status_code == 200:
                data = r.json()
                text = data.get('text', '')
                # Clean: remove verse numbers
                import re
                text = re.sub(r'\d+', '', text)
                text = re.sub(r'\s+', ' ', text).strip()
                return text
            elif r.status_code == 429:
                time.sleep(5 * (attempt + 1))
            else:
                time.sleep(1)
        except Exception as e:
            time.sleep(2)

    return None


def fetch_full_bible(output_dir="data/bible"):
    """Fetch the entire Bible and save chapters as text files."""
    os.makedirs(output_dir, exist_ok=True)
    total_chapters = sum(chapters for _, chapters in BIBLE_BOOKS)
    fetched = 0
    failed = 0

    print(f"Fetching {total_chapters} chapters from {len(BIBLE_BOOKS)} books...\n")

    for book_name, num_chapters in BIBLE_BOOKS:
        book_code = BOOK_CODES.get(book_name, book_name[:3].upper())
        book_dir = os.path.join(output_dir, book_code)
        os.makedirs(book_dir, exist_ok=True)

        for ch in range(1, num_chapters + 1):
            ch_file = os.path.join(book_dir, f"{ch:03d}.txt")

            # Skip if already fetched
            if os.path.exists(ch_file) and os.path.getsize(ch_file) > 10:
                fetched += 1
                continue

            text = fetch_chapter(book_name, ch)
            if text and len(text) > 20:
                with open(ch_file, 'w', encoding='utf-8') as f:
                    f.write(text)
                fetched += 1
            else:
                failed += 1

            # Rate limit: ~3 requests/second
            time.sleep(0.35)

        print(f"  {book_name:20s} ({book_code}): {num_chapters} chapters done")

    print(f"\nFetched: {fetched}/{total_chapters}, Failed: {failed}")
    return fetched


def load_bible_text(data_dir="data/bible"):
    """Load all fetched Bible text as list of (book, chapter, text) tuples."""
    chapters = []
    for book_name, num_chapters in BIBLE_BOOKS:
        book_code = BOOK_CODES.get(book_name, book_name[:3].upper())
        book_dir = os.path.join(data_dir, book_code)

        for ch in range(1, num_chapters + 1):
            ch_file = os.path.join(book_dir, f"{ch:03d}.txt")
            if os.path.exists(ch_file):
                with open(ch_file, 'r', encoding='utf-8') as f:
                    text = f.read().strip()
                    if text:
                        chapters.append((book_name, ch, text))

    return chapters


def extract_bible_triples(chapters):
    """Extract SVO triples from all Bible chapters using spaCy."""
    all_triples = []
    seen = set()

    # Process in batches for spaCy efficiency
    batch_size = 50
    total = len(chapters)

    for i in range(0, total, batch_size):
        batch = chapters[i:i + batch_size]
        texts = [ch[2] for ch in batch]

        triples = extract_batch(texts)

        # Enrich with book context
        for t in triples:
            s, p, o = t
            # Clean up
            s = s.strip('.,;:!?\"\'()[]{}')
            o = o.strip('.,;:!?\"\'()[]{}')

            if len(s) < 2 or len(o) < 2:
                continue
            if s == o:
                continue
            if len(s) > 60 or len(o) > 60:
                continue

            if (s, p, o) not in seen:
                seen.add((s, p, o))
                all_triples.append((s, p, o))

        if (i + batch_size) % 500 == 0 or i + batch_size >= total:
            print(f"  Processed {min(i + batch_size, total)}/{total} chapters, "
                  f"{len(all_triples)} triples")

    return all_triples


def save_triples(triples, filepath):
    """Save triples to TSV."""
    with open(filepath, 'w', encoding='utf-8') as f:
        for s, p, o in sorted(triples):
            f.write(f'{s}\t{p}\t{o}\n')
    return len(triples)


if __name__ == "__main__":
    print("=" * 60)
    print("  FULL BIBLE KNOWLEDGE EXTRACTOR (spaCy)")
    print("=" * 60)
    print()

    # Step 1: Fetch Bible text
    print("STEP 1: Fetching Bible text...")
    bible_dir = "data/bible"

    # Check if already fetched
    total_files = 0
    for book, chapters in BIBLE_BOOKS:
        code = BOOK_CODES.get(book, book[:3].upper())
        book_dir = os.path.join(bible_dir, code)
        if os.path.exists(book_dir):
            total_files += len([f for f in os.listdir(book_dir) if f.endswith('.txt')])

    if total_files > 30000:
        print(f"  Already fetched {total_files} chapters, skipping download.\n")
    else:
        fetched = fetch_full_bible(bible_dir)
        print()

    # Step 2: Load text
    print("STEP 2: Loading Bible text...")
    chapters = load_bible_text(bible_dir)
    print(f"  Loaded {len(chapters)} chapters\n")

    # Step 3: Extract triples
    print("STEP 3: Extracting triples with spaCy...")
    triples = extract_bible_triples(chapters)
    print(f"  Extracted {len(triples)} unique triples\n")

    # Step 4: Save
    print("STEP 4: Saving...")
    os.makedirs('data/samples', exist_ok=True)
    outfile = 'data/samples/bible_knowledge.tsv'
    n = save_triples(triples, outfile)
    print(f"  Saved {n} triples to {outfile}\n")

    # Step 5: Stats
    from collections import Counter
    pred_counts = Counter()
    entity_counts = Counter()
    for s, p, o in triples:
        pred_counts[p] += 1
        entity_counts[s] += 1

    print("Top predicates:")
    for pred, count in pred_counts.most_common(20):
        print(f"  {pred:30s} {count:5d}")

    print(f"\nTop entities:")
    for ent, count in entity_counts.most_common(15):
        print(f"  {ent:30s} {count:5d}")

    print(f"\nTotal: {len(triples)} triples from {len(chapters)} chapters")
    print("=" * 60)
