#!/usr/bin/env python3
"""
Serial Bible fetcher with checkpointing.
Uses bible-api.com (World English Bible), ~0.4s/chapter.
~8 min for all 1189 chapters. Resumes from checkpoint.
"""
import os
import sys
import json
import time
import requests

BOOKS = [
    ("GEN", "Genesis", 50), ("EXO", "Exodus", 40), ("LEV", "Leviticus", 27),
    ("NUM", "Numbers", 36), ("DEU", "Deuteronomy", 34), ("JOS", "Joshua", 24),
    ("JDG", "Judges", 21), ("RUT", "Ruth", 4), ("1SA", "1 Samuel", 31),
    ("2SA", "2 Samuel", 24), ("1KI", "1 Kings", 22), ("2KI", "2 Kings", 25),
    ("1CH", "1 Chronicles", 29), ("2CH", "2 Chronicles", 36), ("EZR", "Ezra", 10),
    ("NEH", "Nehemiah", 13), ("EST", "Esther", 10), ("JOB", "Job", 42),
    ("PSA", "Psalms", 150), ("PRO", "Proverbs", 31), ("ECC", "Ecclesiastes", 12),
    ("SOL", "Song of Solomon", 8), ("ISA", "Isaiah", 66), ("JER", "Jeremiah", 52),
    ("LAM", "Lamentations", 5), ("EZE", "Ezekiel", 48), ("DAN", "Daniel", 12),
    ("HOS", "Hosea", 14), ("JOE", "Joel", 3), ("AMO", "Amos", 9),
    ("OBA", "Obadiah", 1), ("JON", "Jonah", 4), ("MIC", "Micah", 7),
    ("NAH", "Nahum", 3), ("HAB", "Habakkuk", 3), ("ZEP", "Zephaniah", 3),
    ("HAG", "Haggai", 2), ("ZEC", "Zechariah", 14), ("MAL", "Malachi", 4),
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
CACHE_DIR = "data/bible"
BOOKS_DIR = os.path.join(CACHE_DIR, "books")


def fetch_chapter(book_id, chapter, retries=3):
    url = f"https://bible-api.com/{book_id}+{chapter}"
    for attempt in range(retries):
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
            elif r.status_code == 429:
                time.sleep(2 * (attempt + 1))
            else:
                return []
        except Exception:
            time.sleep(1)
    return []


def main():
    os.makedirs(BOOKS_DIR, exist_ok=True)

    # Load progress
    progress_file = os.path.join(CACHE_DIR, "progress.json")
    progress = {}
    if os.path.exists(progress_file):
        with open(progress_file, 'r') as f:
            progress = json.load(f)

    total = sum(ch for _, _, ch in BOOKS)
    done = 0
    total_verses = 0
    t0 = time.time()

    for book_id, book_name, chapters in BOOKS:
        book_file = os.path.join(BOOKS_DIR, f"{book_id}.tsv")
        key = f"{book_id}"

        if key in progress and progress[key] >= chapters:
            # Already fetched
            if os.path.exists(book_file):
                with open(book_file, 'r', encoding='utf-8') as f:
                    total_verses += sum(1 for _ in f)
                done += chapters
                continue

        print(f"Fetching {book_name} ({chapters} chapters)...", end=' ', flush=True)
        all_lines = []
        for ch in range(1, chapters + 1):
            lines = fetch_chapter(book_id, ch)
            all_lines.extend(lines)
            done += 1
            if done % 50 == 0:
                elapsed = time.time() - t0
                eta = elapsed / done * (total - done) if done else 0
                print(f"\n  [{done}/{total}] {elapsed:.0f}s elapsed, ~{eta:.0f}s remaining...", end=' ', flush=True)
            time.sleep(0.35)

        # Save book
        with open(book_file, 'w', encoding='utf-8') as f:
            for line in all_lines:
                f.write(line + '\n')

        total_verses += len(all_lines)
        progress[key] = chapters
        with open(progress_file, 'w') as f:
            json.dump(progress, f)

        print(f"{len(all_lines)} verses")

    # Merge all books
    print(f"\nMerging {len(BOOKS)} books...")
    book_order = {b[0]: i for i, b in enumerate(BOOKS)}
    all_lines = []
    for book_id, _, _ in BOOKS:
        book_file = os.path.join(BOOKS_DIR, f"{book_id}.tsv")
        if os.path.exists(book_file):
            with open(book_file, 'r', encoding='utf-8') as f:
                all_lines.extend(l.strip() for l in f if l.strip())

    merged_file = os.path.join(CACHE_DIR, "all_chapters.tsv")
    with open(merged_file, 'w', encoding='utf-8') as f:
        for line in all_lines:
            f.write(line + '\n')

    elapsed = time.time() - t0
    print(f"Done: {len(all_lines)} verses from {len(BOOKS)} books in {elapsed:.0f}s")
    print(f"Saved: {merged_file} ({os.path.getsize(merged_file)//1024}KB)")
    return merged_file


if __name__ == "__main__":
    main()
