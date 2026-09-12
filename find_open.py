"""find_open.py — frases con opened + contexto de s2064."""
import re

raw = open('probe_open.log', 'rb').read()
t = None
for enc in ('utf-8-sig', 'utf-8', 'cp1252'):
    try:
        t = raw.decode(enc)
        break
    except Exception:
        continue
t = t.replace(chr(0), '')
outs = []
for l in t.splitlines():
    m = re.match(r's(\d+): (.*)', l.strip())
    if m:
        n = int(m.group(1))
        outs.append((n, m.group(2)[:200]))
outs.sort()
print('total:', len(outs))
for n, s in outs:
    mark = ' <<<' if abs(n - 2064) < 3 else ''
    print('s%d: %s%s' % (n, s[:150], mark))
