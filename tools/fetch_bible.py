import requests
import os

os.makedirs('data/bible', exist_ok=True)

urls = [
    'https://raw.githubusercontent.com/nicholasgasior/gnt-bible-json/master/bible-rvr1960.json',
    'https://raw.githubusercontent.com/nicholasgasior/gnt-bible-json/master/bible.json',
    'https://raw.githubusercontent.com/sealdice/sealdice-bible/main/bible_rvr1960.json',
]

for url in urls:
    try:
        r = requests.get(url, timeout=30, headers={'User-Agent': 'SymbolicLLM/1.0'})
        name = url.split('/')[-1]
        print(f'{r.status_code} {len(r.content)//1024}KB {name}')
        if r.status_code == 200 and len(r.content) > 10000:
            with open('data/bible/bible.json', 'wb') as f:
                f.write(r.content)
            print('SAVED!')
            break
    except Exception as e:
        print(f'ERR: {e}')
