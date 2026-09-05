#!/usr/bin/env python3
"""
Fetch NT from etext archive (public domain KJV).
"""
import os
import time
import re
import requests

# NT books with KJV chapter counts (same as WEB)
NT_BOOKS = [
    ("MAT", "Matthew", 28), ("MAR", "Mark", 16), ("LUK", "Luke", 24),
    ("JOH", "John", 21), ("ACT", "Acts", 28), ("ROM", "Romans", 16),
    ("1CO", "1 Corinthians", 16), ("2CO", "2 Corinthians", 13),
    ("GAL", "Galatians", 6), ("EPH", "Ephesians", 6), ("PHP", "Philippians", 4),
    ("COL", "Colossians", 4), ("1TH", "1 Thessalonians", 5), ("2TH", "2 Thessalonians", 3),
    ("1TI", "1 Timothy", 6), ("2TI", "2 Timothy", 4), ("TIT", "Titus", 3),
    ("PHM", "Philemon", 1), ("HEB", "Hebrews", 13), ("JAS", "James", 5),
    ("1PE", "1 Peter", 5), ("2PE", "2 Peter", 3), ("1JO", "1 John", 5),
    ("2JO", "2 John", 1), ("3JO", "3 John", 1), ("JUD", "Jude", 1),
    ("REV", "Revelation", 22),
]

HEADERS = {"User-Agent": "SymbolicLLM/1.0"}
BOOKS_DIR = "data/bible/books"


def fetch_from_bible_api(book_id, chapter):
    """Try bible-api.com with a fresh user agent."""
    url = f"https://bible-api.com/{book_id}+{chapter}"
    try:
        r = requests.get(url, timeout=15, headers=HEADERS)
        if r.status_code == 200:
            data = r.json()
            verses = data.get("verses", [])
            lines = []
            for v in verses:
                text = v.get("text", "").strip()
                if text:
                    lines.append(f"{book_id} {chapter}:{v.get('verse', '?')}\t{text}")
            return lines
    except Exception:
        pass
    return []


def fetch_from_ourcodex(book_id, chapter):
    """Try ourcodex.com Bible API."""
    # Map abbreviations to full names
    name_map = {
        "MAT": "Matthew", "MAR": "Mark", "LUK": "Luke", "JOH": "John",
        "ACT": "Acts", "ROM": "Romans", "1CO": "1-Corinthians",
        "2CO": "2-Corinthians", "GAL": "Galatians", "EPH": "Ephesians",
        "PHP": "Philippians", "COL": "Colossians", "1TH": "1-Thessalonians",
        "2TH": "2-Thessalonians", "1TI": "1-Timothy", "2TI": "2-Timothy",
        "TIT": "Titus", "PHM": "Philemon", "HEB": "Hebrews", "JAS": "James",
        "1PE": "1-Peter", "2PE": "2-Peter", "1JO": "1-John", "2JO": "2-John",
        "3JO": "3-John", "JUD": "Jude", "REV": "Revelation",
    }
    name = name_map.get(book_id, book_id)
    url = f"https://www.ourcodex.com/api/bible/verses/{name}/{chapter}"
    try:
        r = requests.get(url, timeout=15, headers=HEADERS)
        if r.status_code == 200:
            data = r.json()
            lines = []
            for v in data.get("verses", []):
                text = v.get("text", "").strip()
                verse_num = v.get("verse", v.get("verse_number", "?"))
                if text:
                    lines.append(f"{book_id} {chapter}:{verse_num}\t{text}")
            return lines
    except Exception:
        pass
    return []


def try_alternative_sources(book_id, chapter):
    """Try multiple alternative sources."""
    # Source 1: crosswalk Bible data
    # Source 2: biblehub.com text
    url = f"https://biblehub.com/text/{book_id.lower()}/{chapter}-1.htm"
    try:
        r = requests.get(url, timeout=15, headers={
            "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"
        })
        if r.status_code == 200:
            # Parse verse numbers and text from HTML
            from html.parser import HTMLParser
            class VerseParser(HTMLParser):
                def __init__(self):
                    super().__init__()
                    self.verses = []
                    self.current_verse = ""
                    self.in_verse = False
                def handle_starttag(self, tag, attrs):
                    for name, val in attrs:
                        if name == "class" and "verse" in str(val):
                            self.in_verse = True
                def handle_data(self, data):
                    if self.in_verse:
                        self.current_verse += data
                def handle_endtag(self, tag):
                    if self.in_verse and self.current_verse.strip():
                        self.verses.append(self.current_verse.strip())
                        self.current_verse = ""
                        self.in_verse = False

            parser = VerseParser()
            parser.feed(r.text)
            lines = []
            for i, text in enumerate(parser.verses, 1):
                if text:
                    lines.append(f"{book_id} {chapter}:{i}\t{text}")
            if lines:
                return lines
    except Exception:
        pass
    return []


def main():
    os.makedirs(BOOKS_DIR, exist_ok=True)

    # Check which NT books we already have
    existing = set()
    for f in os.listdir(BOOKS_DIR):
        if f.endswith('.tsv'):
            existing.add(f.replace('.tsv', ''))

    total = sum(ch for _, _, ch in NT_BOOKS)
    total_verses = 0
    done = 0
    errors = 0
    t0 = time.time()

    for book_id, book_name, chapters in NT_BOOKS:
        book_file = os.path.join(BOOKS_DIR, f"{book_id}.tsv")

        # Skip if already fetched with verses
        if book_id in existing and os.path.exists(book_file):
            with open(book_file, 'r', encoding='utf-8') as f:
                count = sum(1 for _ in f)
            if count > 0:
                total_verses += count
                done += chapters
                continue

        print(f"Fetching {book_name} ({chapters} chapters)...", end=' ', flush=True)
        all_lines = []
        for ch in range(1, chapters + 1):
            lines = fetch_from_bible_api(book_id, ch)
            if not lines:
                lines = try_alternative_sources(book_id, ch)
            if not lines:
                errors += 1
            all_lines.extend(lines)
            done += 1
            time.sleep(0.5)

        with open(book_file, 'w', encoding='utf-8') as f:
            for line in all_lines:
                f.write(line + '\n')

        total_verses += len(all_lines)
        print(f"{len(all_lines)} verses")

    elapsed = time.time() - t0
    print(f"\nDone: {total_verses} NT verses in {elapsed:.0f}s ({errors} errors)")


if __name__ == "__main__":
    main()
