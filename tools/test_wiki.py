import requests

session = requests.Session()
session.headers.update({'User-Agent': 'SymbolicLLM/1.0 (research; contact@example.com)'})

# Test basic connectivity
api = 'https://en.wikipedia.org/w/api.php'
params = {
    'action': 'query',
    'titles': 'Cat',
    'prop': 'extracts',
    'exintro': True,
    'explaintext': True,
    'format': 'json',
}

try:
    resp = session.get(api, params=params, timeout=30)
    print(f"Status: {resp.status_code}")
    print(f"Content-Type: {resp.headers.get('content-type', '?')}")
    print(f"Content length: {len(resp.text)}")
    print(f"First 500 chars: {resp.text[:500]}")
except Exception as e:
    print(f"Error: {e}")
