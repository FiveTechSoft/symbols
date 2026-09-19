"""
Wikipedia -> Symbolic LLM Text Corpus
Downloads random Wikipedia articles and saves raw sentences as .txt
Uso: python tools/wiki_to_txt.py --articles 500 --lang en --output data/texts/wiki_sample.txt
"""
import re
import sys
import time
import argparse
import requests
from requests.adapters import HTTPAdapter
from urllib3.util.retry import Retry


def create_session():
    session = requests.Session()
    session.headers.update({
        'User-Agent': 'SymbolicLLM/1.0 (research; contact@fivetechsoft.com)'
    })
    retries = Retry(total=3, backoff_factor=1, status_forcelist=[429, 500, 502, 503, 504])
    session.mount('https://', HTTPAdapter(max_retries=retries))
    return session


def fetch_with_retry(session, url, params, max_retries=3):
    for attempt in range(max_retries):
        try:
            resp = session.get(url, params=params, timeout=30)
            if resp.status_code == 200:
                return resp.json()
            elif resp.status_code == 429:
                wait = 2 ** (attempt + 1)
                print(f"   Rate limited, waiting {wait}s...")
                time.sleep(wait)
            else:
                return None
        except Exception as e:
            if attempt < max_retries - 1:
                time.sleep(2)
            else:
                print(f"   Error: {e}")
                return None
    return None


def fetch_articles_random(lang, count, session):
    """Fetch random article titles from Wikipedia."""
    api_url = f"https://{lang}.wikipedia.org/w/api.php"
    titles = []
    batch_size = 50

    while len(titles) < count:
        data = fetch_with_retry(session, api_url, {
            'action': 'query',
            'list': 'random',
            'rnnamespace': 0,
            'rnlimit': min(batch_size, count - len(titles)),
            'format': 'json'
        })
        if data and 'query' in data:
            for item in data['query']['random']:
                titles.append(item['title'])
        else:
            break
        time.sleep(0.1)

    return titles[:count]


def fetch_article_text(title, lang, session):
    """Fetch full article text from Wikipedia."""
    api_url = f"https://{lang}.wikipedia.org/w/api.php"
    data = fetch_with_retry(session, api_url, {
        'action': 'query',
        'titles': title,
        'prop': 'extracts',
        'exintro': False,
        'explaintext': True,
        'format': 'json'
    })
    if data:
        for pid, page in data.get('query', {}).get('pages', {}).items():
            if 'extract' in page:
                return page['extract']
    return None


def clean_text(text):
    """Remove wiki markup, HTML, and normalize whitespace."""
    text = re.sub(r'\u200b', '', text)
    text = re.sub(r'\{\{[^}]*\}\}', '', text)
    text = re.sub(r'\[\[(?:[^|\]]*\|)?([^\]]+)\]\]', r'\1', text)
    text = re.sub(r'<ref[^>]*>.*?</ref>', '', text, flags=re.DOTALL)
    text = re.sub(r'<ref[^>]*/?>', '', text)
    text = re.sub(r'<[^>]+>', '', text)
    text = re.sub(r'\'{2,}', '', text)
    text = re.sub(r'\s+', ' ', text)
    return text.strip()


def split_sentences(text):
    """Split text into sentences."""
    sentences = re.split(r'(?<=[.!?;])\s+(?=[A-ZÁÉÍÓÚÑ])', text)
    return [s.strip() for s in sentences if len(s.strip()) > 20]


def main():
    parser = argparse.ArgumentParser(description='Wikipedia -> Text corpus for Symbolic LLM')
    parser.add_argument('--articles', type=int, default=500,
                        help='Number of articles to fetch (default: 500)')
    parser.add_argument('--lang', type=str, default='en',
                        help='Wikipedia language (default: en)')
    parser.add_argument('--output', type=str, default='data/texts/wiki_sample.txt',
                        help='Output file path')
    args = parser.parse_args()

    print("=" * 60)
    print("  WIKIPEDIA -> TEXT CORPUS (Symbolic LLM)")
    print("=" * 60)
    print(f"  Language : {args.lang}")
    print(f"  Articles : {args.articles}")
    print(f"  Output   : {args.output}")
    print("=" * 60)
    print()

    session = create_session()

    # Step 1: Fetch random article titles
    print(f"1. Fetching {args.articles} random article titles...")
    t0 = time.time()
    titles = fetch_articles_random(args.lang, args.articles, session)
    t1 = time.time()
    print(f"   Got {len(titles)} titles in {t1-t0:.1f}s\n")

    # Step 2: Fetch and process articles
    print("2. Fetching articles and extracting sentences...")
    t0 = time.time()
    all_sentences = []
    ok = 0
    skipped = 0

    for i, title in enumerate(titles):
        text = fetch_article_text(title, args.lang, session)
        if text is None or len(text) < 100:
            skipped += 1
            continue

        text = clean_text(text)
        sentences = split_sentences(text)
        all_sentences.extend(sentences)
        ok += 1

        if (i + 1) % 50 == 0 or i == len(titles) - 1:
            t1 = time.time()
            print(f"   [{i+1}/{len(titles)}] ok={ok} skipped={skipped} "
                  f"sentences={len(all_sentences)} {t1-t0:.1f}s")

        time.sleep(0.2)  # Rate limiting

    t1 = time.time()
    print(f"\n   {ok} articles, {len(all_sentences)} sentences, {t1-t0:.1f}s\n")

    # Step 3: Write output
    print(f"3. Writing {args.output}...")
    with open(args.output, 'w', encoding='utf-8') as f:
        for sent in all_sentences:
            f.write(sent + '\n')

    # Stats
    file_size = len(open(args.output, 'r', encoding='utf-8').read())
    print(f"   Written: {len(all_sentences)} sentences")
    print(f"   File size: {file_size / 1024:.1f} KB")
    print()
    print("=" * 60)
    print(f"  DONE: {args.output}")
    print(f"  {len(all_sentences)} sentences from {ok} Wikipedia articles")
    print("=" * 60)


if __name__ == '__main__':
    main()
