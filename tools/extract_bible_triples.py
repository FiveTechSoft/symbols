#!/usr/bin/env python3
"""
Run spaCy extraction on all Bible verses.
Produces data/bible/bible_knowledge.tsv
"""
import os
import sys
import time

sys.path.insert(0, "tools")
from extract_triples_spacy import extract_triples_spacy

INPUT = "data/bible/all_chapters.tsv"
OUTPUT = "data/bible/bible_knowledge.tsv"

def main():
    with open(INPUT, "r", encoding="utf-8") as f:
        verses = [l.strip() for l in f if l.strip()]

    print(f"Processing {len(verses)} verses...")
    t0 = time.time()
    all_triples = []
    errors = 0

    for i, verse in enumerate(verses):
        # verse format: "GEN 1:1\tIn the beginning..."
        parts = verse.split("\t", 1)
        if len(parts) < 2:
            continue
        ref = parts[0]  # e.g. "GEN 1:1"
        text = parts[1]

        try:
            triples = extract_triples_spacy(text)
            for t in triples:
                # t is a tuple (subject, predicate, object)
                # Add reference as source
                all_triples.append((ref, t[0], t[1], t[2]))
        except Exception:
            errors += 1

        if (i + 1) % 5000 == 0:
            elapsed = time.time() - t0
            print(f"  {i+1}/{len(verses)} ({(i+1)*100//len(verses)}%) - {elapsed:.0f}s - {len(all_triples)} triples")

    elapsed = time.time() - t0
    print(f"\nExtracted {len(all_triples)} triples from {len(verses)} verses in {elapsed:.0f}s ({errors} errors)")

    # Write output (reference, subject, predicate, object)
    with open(OUTPUT, "w", encoding="utf-8") as f:
        for ref, subj, pred, obj in all_triples:
            f.write(f"{subj}\t{pred}\t{obj}\t{ref}\n")

    print(f"Saved to {OUTPUT} ({os.path.getsize(OUTPUT)//1024}KB)")


if __name__ == "__main__":
    main()
