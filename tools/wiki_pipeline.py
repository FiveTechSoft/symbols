"""
Wikipedia -> Symbolic LLM Pipeline v4 (Aggressive Extraction)
Uses sentence-level subject-verb-object detection without strict patterns.
"""

import re
import sys
import json
import time
import requests
from collections import Counter
from requests.adapters import HTTPAdapter
from urllib3.util.retry import Retry

# Verbs that indicate relationships
VERBS = {
    'es', 'era', 'fue', 'ser', 'son', 'estaba', 'esta', 'estan',
    'tiene', 'posee', 'tenia', 'cuenta',
    'fue fundado', 'fue creado', 'fue establecido', 'fue inaugurado',
    'fue fundada', 'fue creada',
    'vive', 'habita', 'reside', 'se encuentra', 'esta ubicado',
    'produce', 'genera', 'fabrica', 'exporta', 'importa',
    'necesita', 'requiere', 'usa', 'utiliza', 'emplea',
    'pertenece', 'forma parte', 'depende',
    'incluye', 'contiene', 'comprende', 'abarca',
    'se llama', 'se conoce', 'se denomina', 'se le conoce',
    'coviene', 'colinda', 'limita',
    'causa', 'provoca', 'genera', 'produce',
    'sufre', 'padisce', 'experimenta',
    'existio', 'existia', 'existio',
    'funciona', 'opera', 'trabaja',
}

def clean_text(text):
    text = re.sub(r'\{\{[^}]*\}\}', '', text)
    text = re.sub(r'\[\[(?:[^|\]]*\|)?([^\]]+)\]\]', r'\1', text)
    text = re.sub(r'<ref[^>]*>.*?</ref>', '', text, flags=re.DOTALL)
    text = re.sub(r'<ref[^>]*/?>', '', text)
    text = re.sub(r'<[^>]+>', '', text)
    text = re.sub(r'\'{2,}', '', text)
    text = re.sub(r'\s+', ' ', text)
    return text.strip()


def split_sentences(text):
    sentences = re.split(r'(?<=[.!?])\s+(?=[A-Z])', text)
    return [s.strip() for s in sentences if len(s.strip()) > 25]


def extract_triples_aggressive(sentence):
    """Aggressive extraction: find capitalized phrases near key verbs."""
    triples = []
    words = sentence.split()

    for i, word in enumerate(words):
        w_lower = word.lower().rstrip('.,;:')

        for verb in VERBS:
            if w_lower == verb or sentence.lower().find(verb) != -1:
                # Look for capitalized subject before verb
                subj_words = []
                for j in range(max(0, i-5), i):
                    if words[j][0:1].isupper() or words[j].lower() in {'de', 'del', 'la', 'el', 'los', 'las', 'en', 'por', 'para'}:
                        subj_words.append(words[j])
                    else:
                        subj_words = []

                # Look for capitalized/meaningful object after verb
                verb_pos = sentence.lower().find(verb)
                if verb_pos == -1:
                    continue
                after_verb = sentence[verb_pos + len(verb):].strip()
                obj_words = []
                for ow in after_verb.split()[:8]:
                    ow_clean = ow.rstrip('.,;:')
                    if ow_clean and len(ow_clean) > 1:
                        obj_words.append(ow_clean)
                    if len(obj_words) >= 4:
                        break

                if len(subj_words) >= 1 and len(obj_words) >= 1:
                    subj = ' '.join(subj_words).upper()
                    obj = ' '.join(obj_words)

                    # Skip very short or very long
                    if len(subj) < 2 or len(obj) < 2:
                        continue
                    if len(subj) > 50 or len(obj) > 60:
                        continue

                    # Skip common false positives
                    skip_subj = {'EL', 'LA', 'LOS', 'LAS', 'UN', 'UNA', 'EN', 'DE', 'DEL', 'POR', 'PARA', 'CON', 'QUE', 'SE', 'NO', 'AL', 'LO'}
                    if subj in skip_subj:
                        continue

                    # Map verb to predicate
                    v = verb.split()[0] if ' ' in verb else verb
                    pred_map = {
                        'es': 'ES', 'era': 'ES', 'fue': 'ES', 'son': 'ES',
                        'tiene': 'TIENE', 'posee': 'TIENE', 'tenia': 'TIENE', 'cuenta': 'TIENE',
                        'vive': 'VIVE_EN', 'habita': 'VIVE_EN', 'reside': 'VIVE_EN',
                        'encuentra': 'UBICADO_EN', 'ubicado': 'UBICADO_EN',
                        'produce': 'PRODUCE', 'genera': 'PRODUCE', 'fabrica': 'PRODUCE',
                        'necesita': 'NECESITA', 'requiere': 'NECESITA',
                        'usa': 'USA', 'utiliza': 'USA', 'emplea': 'USA',
                        'pertenece': 'PERTENECE_A', 'forma': 'PARTE_DE',
                        'incluye': 'INCLUYE', 'contiene': 'INCLUYE',
                        'llama': 'SE_LLAMA', 'conoce': 'SE_LLAMA', 'denomina': 'SE_LLAMA',
                        'causa': 'CAUSA', 'provoca': 'CAUSA',
                        'sufre': 'SUFRE',
                        'funciona': 'FUNCIONA', 'opera': 'FUNCIONA',
                    }
                    pred = pred_map.get(v, 'ES')

                    triples.append((subj, pred, obj))
                break

    return triples


def create_session():
    session = requests.Session()
    retries = Retry(total=5, backoff_factor=2, status_forcelist=[429, 500, 502, 503, 504])
    adapter = HTTPAdapter(max_retries=retries)
    session.mount('https://', adapter)
    session.mount('http://', adapter)
    session.headers.update({'User-Agent': 'SymbolicLLM/1.0 (research; contact@example.com)'})
    return session


def fetch_with_retry(session, api_url, params, max_retries=5):
    for attempt in range(max_retries):
        try:
            resp = session.get(api_url, params=params, timeout=30)
            if resp.status_code == 429:
                wait = min(60, 2 ** (attempt + 2))
                time.sleep(wait)
                continue
            if resp.status_code != 200:
                time.sleep(2)
                continue
            return resp.json()
        except:
            time.sleep(3)
            continue
    return None


def fetch_articles_random(lang='es', limit=1000):
    api_url = f"https://{lang}.wikipedia.org/w/api.php"
    session = create_session()
    articles = []
    seen = set()

    while len(articles) < limit:
        batch = min(50, limit - len(articles))
        data = fetch_with_retry(session, api_url, {
            'action': 'query', 'list': 'random',
            'rnnamespace': '0', 'rnlimit': str(batch), 'format': 'json'
        })
        if not data:
            break
        for r in data.get('query', {}).get('random', []):
            t = r['title']
            if t not in seen:
                seen.add(t)
                articles.append(t)
        time.sleep(0.5)

    return articles[:limit]


def fetch_article_text(title, lang='es', session=None):
    if session is None:
        session = create_session()
    api_url = f"https://{lang}.wikipedia.org/w/api.php"
    data = fetch_with_retry(session, api_url, {
        'action': 'query', 'titles': title,
        'prop': 'extracts', 'exintro': False,
        'explaintext': True, 'format': 'json'
    })
    if data:
        for pid, page in data.get('query', {}).get('pages', {}).items():
            if 'extract' in page:
                return page['extract']
    return None


def main():
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--articles', type=int, default=1000)
    parser.add_argument('--lang', type=str, default='es')
    parser.add_argument('--output', type=str, default='wiki_corpus.tsv')
    parser.add_argument('--json', type=str, default=None)
    args = parser.parse_args()

    print("=" * 60)
    print("  WIKIPEDIA -> SYMBOLIC LLM (Aggressive v4)")
    print("=" * 60)
    print()

    print(f"1. Fetching {args.articles} titles...")
    t0 = time.time()
    titles = fetch_articles_random(args.lang, args.articles)
    t1 = time.time()
    print(f"   Got {len(titles)} in {t1-t0:.1f}s\n")

    print("2. Fetching and extracting...")
    t0 = time.time()
    session = create_session()
    all_triples = []
    stats_pred = Counter()
    ok = 0

    for i, title in enumerate(titles):
        text = fetch_article_text(title, args.lang, session)
        if text is None or len(text) < 100:
            continue

        text = clean_text(text)
        sentences = split_sentences(text)

        for sent in sentences:
            for s, p, o in extract_triples_aggressive(sent):
                all_triples.append((s, p, o))
                stats_pred[p] += 1

        ok += 1
        if (i + 1) % 50 == 0 or i == len(titles) - 1:
            t1 = time.time()
            print(f"   [{i+1}/{len(titles)}] ok={ok} triples={len(all_triples)} {t1-t0:.1f}s")
        time.sleep(0.3)

    t1 = time.time()
    print(f"\n   {ok} articles, {len(all_triples)} triples, {t1-t0:.1f}s\n")

    # Deduplicate
    counts = Counter()
    for s, p, o in all_triples:
        counts[(s, p, o)] += 1

    unique = sorted(counts.items(), key=lambda x: -x[1])
    print(f"3. Unique: {len(unique)} / {len(all_triples)}\n")

    # Write TSV
    print(f"4. Writing {args.output}...")
    with open(args.output, 'w', encoding='utf-8') as f:
        for (s, p, o), c in unique:
            for _ in range(c):
                f.write(f"{s}\t{p}\t{o}\n")

    if args.json:
        with open(args.json, 'w', encoding='utf-8') as f:
            json.dump({
                'articles': ok,
                'total_triples': len(all_triples),
                'unique_triples': len(unique),
                'top_predicates': dict(stats_pred.most_common(20)),
                'triples': [{'subject': s, 'predicate': p, 'object': o, 'count': c}
                            for (s, p, o), c in unique[:3000]]
            }, f, ensure_ascii=False, indent=2)

    print()
    print("=" * 60)
    print(f"  Articles   : {ok}")
    print(f"  Triples    : {len(all_triples)} total, {len(unique)} unique")
    print(f"  Output     : {args.output}")
    print()
    print("  Predicates:")
    for pred, count in stats_pred.most_common(20):
        print(f"    {pred:20s} {count:6d}")
    print()
    print("  Sample triples:")
    for (s, p, o), count in unique[:20]:
        print(f"    {s:30s} --{p:12s}--> {o:40s} x{count}")
    print("=" * 60)


if __name__ == '__main__':
    main()
