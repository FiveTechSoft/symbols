"""
Wikipedia -> Symbolic LLM Pipeline v5 (Pattern-Based Extraction)
Uses regex patterns to extract clean subject-predicate-object triples.
"""

import re
import sys
import json
import time
import requests
from collections import Counter
from requests.adapters import HTTPAdapter
from urllib3.util.retry import Retry

ARTICLES = {'el', 'la', 'los', 'las', 'un', 'una', 'unos', 'unas'}
PREPOSITIONS = {'de', 'del', 'en', 'por', 'para', 'con', 'sin', 'sobre',
                'entre', 'hasta', 'desde', 'hacia', 'como', 'según'}
CONNECTORS = {'y', 'o', 'e', 'ni', 'pero', 'sino', 'que'}

PREDICATE_MAP = {
    'es': 'ES', 'era': 'ES', 'fue': 'ES', 'son': 'ES', 'estaba': 'ES',
    'esta': 'ES', 'estan': 'ES', 'ser': 'ES',
    'tiene': 'TIENE', 'posee': 'TIENE', 'tenia': 'TIENE', 'cuenta': 'TIENE',
    'vive': 'VIVE_EN', 'habita': 'VIVE_EN', 'reside': 'VIVE_EN',
    'produce': 'PRODUCE', 'genera': 'PRODUCE', 'fabrica': 'PRODUCE',
    'necesita': 'NECESITA', 'requiere': 'NECESITA',
    'usa': 'USA', 'utiliza': 'USA', 'emplea': 'USA',
    'pertenece': 'PERTENECE_A', 'forma': 'PARTE_DE',
    'incluye': 'INCLUYE', 'contiene': 'INCLUYE',
    'llama': 'SE_LLAMA', 'conoce': 'SE_LLAMA', 'denomina': 'SE_LLAMA',
    'causa': 'CAUSA', 'provoca': 'CAUSA',
    'sufre': 'SUFRE', 'padece': 'SUFRE',
    'funciona': 'FUNCIONA', 'opera': 'FUNCIONA',
}

STOP_SUBJ = {'EL', 'LA', 'LOS', 'LAS', 'UN', 'UNA', 'EN', 'DE', 'DEL',
             'POR', 'PARA', 'CON', 'QUE', 'SE', 'NO', 'AL', 'LO',
             'SU', 'LES', 'LH', 'D', 'L'}

def clean_text(text):
    text = re.sub(r'\u200b', '', text)
    text = re.sub(r'\{\{[^}]*\}\}', '', text)
    text = re.sub(r'\[\[(?:[^|\]]*\|)?([^\]]+)\]\]', r'\1', text)
    text = re.sub(r'<ref[^>]*>.*?</ref>', '', text, flags=re.DOTALL)
    text = re.sub(r'<ref[^>]*/?>', '', text)
    text = re.sub(r'<[^>]+>', '', text)
    text = re.sub(r'\'{2,}', '', text)
    text = re.sub(r'\([^)]*\)', '', text)
    text = re.sub(r'\s+', ' ', text)
    return text.strip()


def split_sentences(text):
    sentences = re.split(r'(?<=[.!?;])\s+(?=[A-ZÁÉÍÓÚÑ])', text)
    return [s.strip() for s in sentences if len(s.strip()) > 30]


def is_valid_noun(phrase):
    """Check if a phrase looks like a valid noun (not just articles/prepositions)."""
    words = phrase.upper().split()
    content_words = [w for w in words if w not in ARTICLES and w not in PREPOSITIONS and w not in CONNECTORS]
    return len(content_words) >= 1 and len(content_words[0]) >= 3


def extract_subject_before(sentence, verb_start):
    """Extract the noun phrase that is the subject (before the verb)."""
    before = sentence[:verb_start].strip().rstrip('.,;: ')

    # Find the main noun phrase: look for the last comma or start
    # Subject is typically: "El/La [noun phrase]"
    # Try to find a clean noun phrase
    words = before.split()
    if not words:
        return None

    # Walk backwards from the end to find the subject boundary
    # Skip trailing prepositions
    end = len(words)
    while end > 0 and words[end-1].lower().rstrip('.,;:') in PREPOSITIONS | CONNECTORS | {'que', 'quien', 'cual', 'donde', 'cuando'}:
        end -= 1

    # Now find the start of the subject
    # Subject typically starts with article or capital letter
    start = end - 1
    while start > 0:
        w = words[start].lower().rstrip('.,;:')
        if w in PREPOSITIONS or w in CONNECTORS or w in {'que', 'quien', 'cual'}:
            start += 1
            break
        start -= 1

    if start >= end:
        return None

    subj_words = words[start:end]
    subj = ' '.join(subj_words).strip('.,;: ')

    # Clean up: remove leading articles if they make the noun vague
    sw = subj.split()
    if len(sw) > 1 and sw[0].upper() in ARTICLES:
        # Keep article if it's part of a proper noun (e.g., "El Salvador")
        if len(sw) > 2 or sw[0].upper() not in {'EL', 'LA', 'LOS', 'LAS'}:
            pass  # Keep as-is for proper nouns
        elif sw[0].upper() in ARTICLES and len(sw) <= 2:
            # "El Salvador" -> keep, "El río" -> skip article
            pass

    return subj.strip() if subj.strip() else None


def extract_object_after(sentence, verb_end):
    """Extract the noun phrase that is the object (after the verb)."""
    after = sentence[verb_end:].strip().lstrip('.,;: ')

    if not after:
        return None

    words = after.split()
    if not words:
        return None

    # Find the end of the object phrase
    # Stop at next verb-like word, period, or conjunction that starts a new clause
    end = min(len(words), 10)
    for i, w in enumerate(words[:end]):
        wl = w.lower().rstrip('.,;:')
        if wl in {'que', 'quien', 'cual', 'donde', 'cuando', 'porque', 'pues'}:
            end = i
            break
        if wl in PREPOSITIONS and i > 0:
            # Check if this preposition starts a new phrase
            # "en España" after "es un país" -> stop before "en"
            end = i
            break

    obj_words = words[:end]
    obj = ' '.join(obj_words).strip('.,;: ')

    if len(obj) < 2:
        return None

    return obj


def extract_triples_v5(sentence):
    """Split-at-verb triple extraction for Spanish Wikipedia text."""
    triples = []

    # Split sentence into clauses at semicolons and period
    clauses = re.split(r'[.;]', sentence)

    for clause in clauses:
        clause = clause.strip()
        if len(clause) < 20:
            continue

        # For each clause, try to find a verb and split around it
        for verb, pred in [
            ('fue fundado', 'FUE_CREADO'), ('fue creada', 'FUE_CREADO'),
            ('fue creado', 'FUE_CREADO'), ('fue establecido', 'FUE_CREADO'),
            ('se encuentra', 'UBICADO_EN'), ('está ubicado', 'UBICADO_EN'),
            ('se halla', 'UBICADO_EN'), ('se sitúa', 'UBICADO_EN'),
            ('pertenece a', 'PERTENECE_A'),
            ('forma parte de', 'PARTE_DE'),
            ('tiene', 'TIENE'), ('posee', 'TIENE'),
            ('produce', 'PRODUCE'), ('genera', 'PRODUCE'),
            ('utiliza', 'USA'), ('usa', 'USA'), ('requiere', 'USA'),
            ('incluye', 'INCLUYE'), ('contiene', 'INCLUYE'),
            ('causa', 'CAUSA'), ('provoca', 'CAUSA'),
            ('es', 'ES'), ('era', 'ES'), ('fue', 'ES'), ('son', 'ES'),
        ]:
            # Find the verb in the clause
            idx = clause.lower().find(' ' + verb + ' ')
            if idx == -1:
                idx = clause.lower().find(' ' + verb + ',')
            if idx == -1:
                idx = clause.lower().find(' ' + verb + ' ')
            if idx == -1:
                continue

            before = clause[:idx].strip()
            after = clause[idx + len(verb) + 1:].strip().lstrip('.,;: ')

            # Extract subject: last noun phrase before verb
            # Find the last comma that has meaningful text after it
            last_comma = before.rfind(',')
            while last_comma > 0:
                candidate = before[last_comma + 1:].strip()
                if len(candidate) >= 3:
                    before = candidate
                    break
                last_comma = before.rfind(',', 0, last_comma)

            subj_words = before.split()
            if not subj_words:
                continue

            # Trim trailing prepositions/connectors
            while subj_words and subj_words[-1].lower().rstrip('.,;:') in \
                    PREPOSITIONS | CONNECTORS | {'que', 'quien', 'cual', 'donde', 'cuando'}:
                subj_words.pop()

            # Find the start of the subject (skip leading articles if followed by lowercase)
            start = 0
            if len(subj_words) > 1 and subj_words[0].lower() in ARTICLES:
                if subj_words[1][0:1].islower():
                    start = 1

            subj = ' '.join(subj_words[start:]).strip('.,;: ')

            # Extract object: first meaningful phrase after verb
            # Clean up leading articles that got stuck to the verb
            after_clean = re.sub(r'^(?:\s*(?:es|un|una|el|la|los|las))+\s*', '', after)
            if not after_clean:
                after_clean = after

            obj_words = []
            for w in after_clean.split():
                wl = w.lower().rstrip('.,;:')
                # Stop at conjunctions that start new clauses
                if wl in {'que', 'quien', 'cual', 'donde', 'cuando', 'porque',
                          'pues', 'aunque', 'pero', 'sino', 'y', 'o'}:
                    break
                obj_words.append(w.rstrip('.,;:'))
                if len(obj_words) >= 6:
                    break

            obj = ' '.join(obj_words).strip()

            # Validate
            if len(subj) < 3 or len(obj) < 3:
                continue
            if not subj[0].isalpha():
                continue

            # Skip if subject is just articles/prepositions
            sw = subj.upper().split()
            content = [w for w in sw if w not in ARTICLES and w not in PREPOSITIONS]
            if len(content) == 0:
                continue

            triples.append((subj.upper(), pred, obj))
            break  # One triple per clause

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
            for s, p, o in extract_triples_v5(sent):
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
