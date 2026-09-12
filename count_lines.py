"""count_lines.py — diagnostico del log exp56."""
raw = open('exp56.log', 'rb').read()
print('bytes:', len(raw))
t = None
for enc in ('utf-16', 'utf-8'):
    try:
        t = raw.decode(enc)
        break
    except Exception:
        continue
t = t.replace(chr(0), '')
lines = t.splitlines()
print('lineas:', len(lines))
for tag in ['E56-EAGER', 'E56-DEFERRED', 'E56-DONE', 'E56 total', 'DX-INDEX', 'DX-SNAPSHOT', 'ERROR']:
    n = sum(1 for l in lines if tag in l)
    print('%-12s %d' % (tag, n))
print('--- ultimas 5 ---')
for l in lines[-5:]:
    print(l[:160])
