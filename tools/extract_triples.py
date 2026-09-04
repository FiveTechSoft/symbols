#!/usr/bin/env python3
"""
extract_triples.py - OpenIE Triple Extraction from Plaintext

Extracts (subject, predicate, object) triples from raw text using spaCy
dependency parsing and outputs tab-separated values.

Usage:
    python extract_triples.py input.txt > triples.tsv
    python extract_triples.py input.txt -o triples.tsv
    cat raw_text.txt | python extract_triples.py -

Requires: pip install spacy
          python -m spacy download es_core_news_sm  (Spanish)
          python -m spacy download en_core_web_sm    (English)
"""

import sys
import argparse
import spacy


def extract_triples_from_doc(doc):
    """Extract SVO triples from a spaCy Doc using dependency parsing."""
    triples = []

    for sent in doc.sents:
        for token in sent:
            # Find ROOT verb
            if token.dep_ == "ROOT" and token.pos_ == "VERB":
                subject = None
                obj = None

                for child in token.children:
                    # Subject: nsubj, nsubjpass
                    if child.dep_ in ("nsubj", "nsubjpass"):
                        subject = get_full_noun_phrase(child)

                    # Object: dobj, pobj, attr
                    if child.dep_ in ("dobj", "pobj", "attr"):
                        obj = get_full_noun_phrase(child)

                    # Prepositional complements: "programa EN Harbour"
                    if child.dep_ == "prep":
                        for pobj in child.children:
                            if pobj.dep_ == "pobj":
                                # Merge prep + pobj into predicate
                                prep_text = child.text.upper()
                                obj_text = get_full_noun_phrase(pobj)
                                if subject and obj_text:
                                    pred = token.text.upper() + "_" + prep_text
                                    triples.append((
                                        subject,
                                        pred,
                                        obj_text
                                    ))

                # Direct SVO triple
                if subject and obj and token.pos_ == "VERB":
                    triples.append((
                        subject,
                        token.text.upper(),
                        obj
                    ))

    return triples


def get_full_noun_phrase(token):
    """Get the full noun phrase including determiners and modifiers."""
    # If token is part of a compound or has det, get the head noun phrase
    if hasattr(token, 'noun_chunks'):
        for chunk in token.doc.noun_chunks:
            if token in chunk:
                return chunk.text.upper()

    # Fallback: return the token text
    return token.text.upper()


def process_file(input_path, nlp, verbose=False):
    """Process a text file and extract triples."""
    if input_path == "-":
        text = sys.stdin.read()
        doc = nlp(text)
    else:
        doc = nlp.pipe(
            (line for line in open(input_path, "r", encoding="utf-8")),
            batch_size=1000,
            n_process=1
        )
        # Collect all triples from the generator
        all_triples = []
        for d in doc:
            all_triples.extend(extract_triples_from_doc(d))
        return all_triples

    return extract_triples_from_doc(doc)


def main():
    parser = argparse.ArgumentParser(
        description="Extract SVO triples from plaintext using spaCy")
    parser.add_argument("input", help="Input text file (- for stdin)")
    parser.add_argument("-o", "--output", help="Output TSV file (default: stdout)")
    parser.add_argument("-l", "--lang", default="es",
                        choices=["es", "en"],
                        help="Language model (default: es)")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="Print progress to stderr")
    args = parser.parse_args()

    # Load spaCy model
    model_map = {"es": "es_core_news_sm", "en": "en_core_web_sm"}
    if args.verbose:
        print(f"Loading spaCy model: {model_map[args.lang]}", file=sys.stderr)

    try:
        nlp = spacy.load(model_map[args.lang])
    except OSError:
        print(f"Error: Model '{model_map[args.lang]}' not found.", file=sys.stderr)
        print(f"Run: python -m spacy download {model_map[args.lang]}", file=sys.stderr)
        sys.exit(1)

    # Output handle
    out = open(args.output, "w", encoding="utf-8") if args.output else sys.stdout

    try:
        if args.input == "-":
            text = sys.stdin.read()
            doc = nlp(text)
            triples = extract_triples_from_doc(doc)
        else:
            triples = []
            with open(args.input, "r", encoding="utf-8") as f:
                for i, line in enumerate(f):
                    if args.verbose and i % 10000 == 0:
                        print(f"  Processed {i} lines...", file=sys.stderr)
                    doc = nlp(line.strip())
                    triples.extend(extract_triples_from_doc(doc))

        # Write TSV
        seen = set()
        count = 0
        for subj, pred, obj in triples:
            if not subj or not pred or not obj:
                continue
            key = (subj, pred, obj)
            if key in seen:
                continue
            seen.add(key)
            out.write(f"{subj}\t{pred}\t{obj}\n")
            count += 1

        if args.verbose:
            print(f"  Extracted {count} unique triples", file=sys.stderr)

    finally:
        if args.output:
            out.close()


if __name__ == "__main__":
    main()
