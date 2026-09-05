#!/usr/bin/env python3
"""
Extract love/amor related triples from the Bible verses.
"""
import os
import sys
import time

sys.path.insert(0, "tools")
from extract_triples_spacy import extract_triples_spacy

INPUT = "data/bible/all_chapters.tsv"
OUTPUT = "data/bible/love_knowledge.tsv"

# Love-related keywords
LOVE_KEYWORDS = [
    "love", "loved", "loveth", "loveth", "beloved", "lovest",
    "charity", "merciful", "mercy", "grace", "kindness",
    "affection", "compassion", "tender", "dear", "friend",
]

def main():
    with open(INPUT, "r", encoding="utf-8") as f:
        verses = [l.strip() for l in f if l.strip()]

    print(f"Scanning {len(verses)} verses for love-related content...")
    t0 = time.time()

    love_verses = []
    for verse in verses:
        parts = verse.split("\t", 1)
        if len(parts) < 2:
            continue
        ref = parts[0]
        text = parts[1].lower()
        for kw in LOVE_KEYWORDS:
            if kw in text:
                love_verses.append(verse)
                break

    print(f"Found {len(love_verses)} love-related verses in {time.time()-t0:.1f}s")

    # Extract triples from love verses
    all_triples = []
    for verse in love_verses:
        parts = verse.split("\t", 1)
        if len(parts) < 2:
            continue
        ref = parts[0]
        text = parts[1]
        try:
            triples = extract_triples_spacy(text)
            for t in triples:
                all_triples.append((ref, t[0], t[1], t[2]))
        except Exception:
            pass

    print(f"Extracted {len(all_triples)} raw triples")

    # Filter: keep only triples where love/mercy/grace appears
    love_predicates = {
        "LOVE", "LOVES", "LOVETH", "LOVEST", "LOVED",
        "BELoved", "BELOVED",
        "MERCY", "MERCIFUL", "GRACIOUS", "GRACE",
        "KINDNESS", "COMPASSION", "TENDERHEARTED",
        "CHARITY", "AFFECTION", "DEAR",
    }

    filtered = []
    for ref, subj, pred, obj in all_triples:
        # Keep if any component mentions love-related concepts
        all_text = f"{subj} {pred} {obj}".upper()
        for kw in ["LOVE", "MERCY", "GRACE", "KINDNESS", "COMPASSION", "CHARITY"]:
            if kw in all_text:
                filtered.append((ref, subj, pred, obj))
                break

    print(f"Filtered to {len(filtered)} love-related triples")

    # Show top 30
    print("\nTop 30 love triples:")
    seen = set()
    for ref, subj, pred, obj in filtered[:100]:
        key = (subj.upper(), pred.upper(), obj.upper())
        if key not in seen:
            seen.add(key)
            print(f"  {subj} --{pred}--> {obj}")
            if len(seen) >= 30:
                break

    # Write output
    with open(OUTPUT, "w", encoding="utf-8") as f:
        for ref, subj, pred, obj in filtered:
            f.write(f"{subj}\t{pred}\t{obj}\n")

    print(f"\nSaved {len(filtered)} triples to {OUTPUT} ({os.path.getsize(OUTPUT)//1024}KB)")


if __name__ == "__main__":
    main()
