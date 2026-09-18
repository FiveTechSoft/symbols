import json
import subprocess
import time
import urllib.request

srv = subprocess.Popen(
    ['build-gcc/symbols-server.exe', '8099'],
    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(1.0)
try:
    with urllib.request.urlopen('http://127.0.0.1:8099/v1/models',
                                timeout=10) as r:
        print('MODELS:', r.status, r.read().decode('utf-8')[:160])
    for q in ['Hello. Who is the father of David?',
              'Who is the father of Babylonia?',
              'Who is the mother of David?',
              'Quien es el padre de David y de Salomon?']:
        body = json.dumps({'model': 'symbols', 'messages': [
            {'role': 'user', 'content': q}]}).encode('utf-8')
        req = urllib.request.Request(
            'http://127.0.0.1:8099/v1/chat/completions', data=body,
            headers={'Content-Type': 'application/json'})
        with urllib.request.urlopen(req, timeout=10) as r:
            d = json.loads(r.read().decode('utf-8'))
        print('Q:', repr(q[:45]))
        print('A:', repr(d['choices'][0]['message']['content'][:130]))
finally:
    srv.terminate()
    srv.wait()
print('DONE')
