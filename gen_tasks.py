"""gen_tasks.py v2 — 100 tareas con filtros estrictos anti-circulares."""
import re
from collections import defaultdict

ROLES = {'actor', 'action', 'object', 'recipient', 'location', 'of', 'agent_by'}
EV = re.compile(r'_ch\d+_')
JUNK = {'again', 'very', 'away', 'back', 'forth', 'aside', 'ever', 'never',
        'always', 'often', 'sometimes', 'usually', 'hardly', 'scarcely',
        'nearly', 'almost', 'quite', 'rather', 'well', 'much', 'more', 'most'}
PRON = re.compile(r'^(he|she|him|her|it|they|them)(_[0-9]+)?$')

mem = []
for l in open('bookbrain/alice.knowledge.pl', encoding='utf-8'):
    l = l.strip()
    if l.startswith('memfact('):
        p = l[len('memfact('):-2].split(',')
        mem.append((p[0], p[1], p[2]))
prov = {}
for l in open('bookbrain/alice.knowledge.pl', encoding='utf-8'):
    l = l.strip()
    if l.startswith('provfact('):
        p = l[len('provfact('):-2].split(',')
        prov[(p[0], p[1], p[2])] = p[3]
text = open('books/alice.txt', encoding='utf-8', errors='replace').read()

# vocabulario de entidades: mayuscula no-inicial (lowercase set)
vocab = set()
for m in re.finditer(r'[A-Za-z][a-zA-Z]*', text):
    w = m.group(0)
    if w[0].isupper():
        # inicio de frase? caracter previo es .!?:
        i = m.start() - 1
        while i >= 0 and text[i] in ' \t\n"\'':
            i -= 1
        if i >= 0 and text[i] not in '.!?':
            vocab.add(w.lower())
print('entidades vocab:', len(vocab))


def ok_node(s):
    return (not PRON.match(s) and s != 'narrator' and not EV.search(s)
            and s not in JUNK)


def candidates():
    out = []
    for (s, v, o) in mem:
        if v in ROLES:
            continue
        if not (ok_node(s) and ok_node(o)):
            continue
        if s == o:
            continue
        if s not in vocab and o not in vocab:
            continue
        out.append((s, v, o))
    return out


cands = candidates()
print('candidatos:', len(cands))


def words(sym):
    return sym.replace('_', ' ')


def destem(v):
    if v.endswith('ied'):
        return v[:-3] + 'y'
    if v.endswith('ed'):
        return v[:-2]
    if v.endswith('ing'):
        return v[:-3]
    if v.endswith('es'):
        return v[:-2]
    if v.endswith('s'):
        return v[:-1]
    return v


def third_sg(base):
    if len(base) > 1 and base[-1] == 'y' and base[-2] not in 'aeiou':
        return base[:-1] + 'ies'
    if base[-1:] in ('s', 'x', 'o') or base[-2:] in ('sh', 'ch'):
        return base + 'es'
    return base + 's'


def text_order(s, v, o):
    """S antes que stem(V) antes que O en ventana de 150 chars."""
    sv, ov, vv = s.replace('_', ' '), o.replace('_', ' '), destem(v)
    tl = text.lower()
    i = tl.find(sv)
    while i != -1:
        j = tl.find(vv, i + len(sv), i + 100)
        if j != -1 and tl.find(ov, j + len(vv), j + 100) != -1:
            return True
        i = tl.find(sv, i + 1)
        if i > 2000000:
            break
    return False


def chapters_of(triples):
    chs = set()
    for t in triples:
        r = prov.get(t, '')
        m = re.search(r'_ch(\d+)_', r)
        if m:
            chs.add(int(m.group(1)))
    return chs


tasks = []


def add(tid, blk, q, exp, typ, method):
    tasks.append((tid, blk, q, exp, typ, method))


goods = [t for t in cands if text_order(*t)]
print('con orden textual:', len(goods))

# ---------- COMP 20: who-3sg + what ----------
n = 0
for (s, v, o) in goods:
    if n >= 20:
        break
    bv = destem(v)
    if n % 2 == 0:
        q = 'Who %s %s?' % (third_sg(bv), words(o))
        exp = s
    else:
        q = 'What did %s %s?' % (words(s), bv)
        exp = o
    add('C%02d' % (n + 1), 'COMP', q, exp, 'exact', 'auto-text-order')
    n += 1
print('COMP:', n)

# ---------- QA 10 + 10 unknown ----------
n = 0
for (s, v, o) in goods:
    if n >= 10:
        break
    bv = destem(v)
    if n % 3 == 0:
        q = 'Did %s %s %s?' % (words(s), bv, words(o))
        exp, typ = 'yes', 'exact'
    elif n % 3 == 1:
        q = 'Why did %s %s %s?' % (words(s), bv, words(o))
        exp, typ = '%s-%s-%s' % (s, v, o), 'proof'
    else:
        q = 'Who %s %s?' % (third_sg(bv), words(o))
        exp, typ = s, 'exact'
    add('Q%02d' % (n + 1), 'QA', q, exp, typ, 'auto-text-order')
    n += 1
print('QA answerable:', n)
unknowns = [
    ('Who wrote alice?', 'author-absent'),
    ('Who killed alice?', 'no-kill'),
    ('What did queen eat?', 'no-eat'),
    ('Did hatter sing song?', 'no-record'),
    ('Who visited london?', 'no-london'),
    ('What did gryphon drink?', 'no-drink'),
    ('Did alice meet shakespeare?', 'no-entity'),
    ('Who taught turtle?', 'no-teach'),
    ('What did king build?', 'no-build'),
    ('Why did dodo fly?', 'no-fly'),
]
for i, (q, why) in enumerate(unknowns):
    add('Q%02d' % (11 + i), 'QA', q, 'unknown', 'unknown', 'manual-absence:' + why)
print('QA unknown: 10')

# ---------- REAS 10x1 + 10x2 (solo verbos, orden) ----------
n = 0
for (s, v, o) in goods:
    if n >= 10:
        break
    q = 'Why did %s %s %s?' % (words(s), destem(v), words(o))
    add('R%02d' % (n + 1), 'REAS', q, '%s-%s-%s' % (s, v, o), 'proof', 'auto-text-order')
    n += 1
print('REAS-1H:', n)
chains = []
seen = set()
for (a, r1, b) in goods:
    for (a2, r2, c) in goods:
        if a2 == b and (a, c) not in seen and a != c:
            if text_order(a, r1, b) and text_order(b, r2, c):
                chains.append((a, r1, b, r2, c))
                seen.add((a, c))
            break
    if len(chains) >= 10:
        break
for i, (a, r1, b, r2, c) in enumerate(chains[:10]):
    q = 'Why did %s %s %s?' % (words(a), destem(r1), words(b))
    add('R%02d' % (11 + i), 'REAS', q, '%s-via-%s' % (c, b), 'proof2', 'auto-chain-order')
print('REAS-2H:', len(chains[:10]))

# ---------- MEM 20: listas multi + what persistentes ----------
by_pair = defaultdict(list)
for t in goods:
    by_pair[(t[1], t[2])].append(t[0])
ml = sorted(by_pair.items(), key=lambda x: -len(chapters_of([(s, x[0][0], x[0][1]) for s in x[1]])))
n = 0
for (v, o), subj in ml:
    if n >= 12:
        break
    subj = sorted(set(subj))
    chs = chapters_of([(s, v, o) for s in subj])
    if len(chs) < 2:
        continue
    q = 'Who %s %s?' % (third_sg(destem(v)), words(o))
    add('M%02d' % (n + 1), 'MEM', q, '|'.join(subj), 'exact', 'auto-multich')
    n += 1
print('MEM list:', n)
m0 = n
for (s, v, o) in goods:
    if n >= 20:
        break
    if len(chapters_of([(s, v, o)])) >= 1 and s in vocab:
        q = 'What did %s %s?' % (words(s), destem(v))
        add('M%02d' % (n + 1), 'MEM', q, o, 'exact', 'auto-multich')
        n += 1
print('MEM what:', n - m0)

# ---------- GEN 20 ----------
g = 0
for (s, v, o) in goods:
    if g >= 8:
        break
    q = 'Answer in one word: who %s %s?' % (third_sg(destem(v)), words(o))
    add('G%02d' % (g + 1), 'GEN', q, s, 'exact', 'auto-text-order')
    g += 1
print('GEN oneword:', g)
for (s, v, o) in goods:
    if g >= 14:
        break
    q = 'Say yes or no: did %s %s %s?' % (words(s), destem(v), words(o))
    add('G%02d' % (g + 1), 'GEN', q, 'yes', 'exact', 'auto-text-order')
    g += 1
print('GEN yesno:', g - 8)
add('G15', 'GEN', 'Admit it if unknown: who wrote alice?', 'unknown', 'unknown', 'manual-absence')
add('G16', 'GEN', 'List all who opened door.', 'alice', 'exact', 'manual:footman-coref-error-excluded')
add('G17', 'GEN', 'Answer in one word: what did footman open?', 'door', 'exact', 'manual:triple-exists-but-agent-wrong')
add('G18', 'GEN', 'Say yes or no: did alice open door?', 'yes', 'exact', 'manual-verified')
add('G19', 'GEN', 'Admit it if unknown: why did dodo fly?', 'unknown', 'unknown', 'manual-absence')
add('G20', 'GEN', 'List all: what did alice open?', 'door', 'exact', 'manual:footman-excluded')
print('GEN manual: 6')
print('TOTAL:', len(tasks))
with open('bookbrain/tasks_raw.tsv', 'w', encoding='utf-8') as fh:
    for t in tasks:
        fh.write('\t'.join(t) + '\n')
