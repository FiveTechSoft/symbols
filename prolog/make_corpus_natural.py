#!/usr/bin/env python3
"""make_corpus_natural.py — Corpus Natural v0.1 (EXP42).

Principio metodologico: el split held-out se decide ANTES de generar el
texto. Ninguna frase del corpus enuncia un par held-out. El gold
(gold.pl) solo se usa para PUNTUAR la extraccion, jamas se ingiere.
"""
import os, random

random.seed(42)
OUT = "corpus_natural"

PERSONS = ["maria", "pedro", "ana", "luis", "carmen", "jorge", "elena",
           "pablo", "lucia", "marcos", "sara", "diego", "alba", "hugo",
           "vera", "ivan", "nora", "leo", "mia", "teo"]
TRANSFER = ["sofia", "yago", "ines", "bruno", "gaia", "dario", "julia", "eric"]
CITIES = ["madrid", "barcelona", "oslo", "bergen", "paris", "lyon",
          "roma", "milan", "quito", "lima", "dublin", "bern"]
COUNTRY = {"madrid": "spain", "barcelona": "spain", "oslo": "norway",
           "bergen": "norway", "paris": "france", "lyon": "france",
           "roma": "italy", "milan": "italy", "quito": "ecuador",
           "lima": "peru", "dublin": "ireland", "bern": "switzerland"}
OBJECTS = ["book", "tablet", "pen", "guitar", "camera", "watch",
           "bicycle", "lamp", "radio", "chess", "globe", "drum"]
FRONT_ADV = ["Yesterday", "Today", "Last week,"]
BACK_ADV = ["frequently.", "every summer.", "last year."]
NOISE = ["It rains frequently.", "The weather is pleasant today.",
         "Snow covers the mountains.", "The wind blows strongly.",
         "Maria likes sunny days.", "Pedro likes quiet evenings.",
         "Time passes quickly.", "Nobody answered the phone.",
         "The train arrives late.", "Birds sing every morning.",
         "Coffee tastes better today.", "The river flows calmly.",
         "Children play outside.", "Music fills the streets.",
         "The moon shines brightly.", "Traffic moves slowly today.",
         "Ana enjoys long walks.", "Luis prefers black coffee.",
         "The market opens early.", "Rain falls softly tonight.",
         "Dogs bark loudly.", "The bakery smells wonderful.",
         "Stars appear gradually.", "Tea cools on the table.",
         "The city never sleeps.", "Waves crash against the rocks.",
         "Elena reads the newspaper.", "Pablo watches the sunset.",
         "The garden blooms beautifully.", "Thunder rumbles far away."]

# ---- ground truth ----
def build_ground(persons, ci, oi):
    g = {}
    for i, p in enumerate(persons):
        live = CITIES[(ci + i) % len(CITIES)]
        work = CITIES[(ci + i + 4) % len(CITIES)]
        visits = [CITIES[(ci + i + 1) % len(CITIES)],
                  CITIES[(ci + i + 3) % len(CITIES)],
                  CITIES[(ci + i + 5) % len(CITIES)]]
        owns = [OBJECTS[(oi + i) % len(OBJECTS)],
                OBJECTS[(oi + i + 5) % len(OBJECTS)]]
        g[p] = {"lives": live, "works": work, "visits": visits, "owns": owns}
    return g

CORE = build_ground(PERSONS, 0, 0)
TRA = build_ground(TRANSFER, 6, 7)

# ---- held-out previo: CONCLUSIONES (P,K) jamas enunciadas ----
# Las premisas (visitas) se enuncian todas: lo oculto es la conclusion
# compuesta, como en EXP30. Sin premisas no habria inferencia posible.
HELD_REACH = set()
for i in range(12):
    p = PERSONS[i]
    c = CORE[p]["visits"][i % 3]
    HELD_REACH.add((p, COUNTRY[c]))
for i in range(4):
    p = TRANSFER[i]
    c = TRA[p]["visits"][0]
    HELD_REACH.add((p, COUNTRY[c]))

# contradicciones: personas excluidas de queries lives
CONTRA = {"nora": "lyon", "leo": "oslo"}

sentences, gold = [], []

def emit(text, triplet):
    sentences.append(text)
    gold.append(triplet)  # (s,r,o) o None (ruido)

def cap(w):
    return w[0].upper() + w[1:]

def lives_variants(p, c, k):
    v = [f"{cap(p)} lives in {cap(c)}.",
         f"{cap(p)} lives in {cap(c)} {BACK_ADV[(len(p)) % 3]}",
         f"{FRONT_ADV[len(c) % 3]} {p} lives in {cap(c)}."]
    return v[k % 3]

def works_variants(p, c, k):
    v = [f"{cap(p)} works in {cap(c)}.",
         f"{FRONT_ADV[(len(p) + 1) % 3]} {p} works in {cap(c)}.",
         f"{cap(p)} works in {cap(c)} {BACK_ADV[(len(c)) % 3]}"]
    return v[k % 3]

def visits_variants(p, c, k):
    v = [f"{cap(p)} visited {cap(c)}.",
         f"{cap(p)} visits {cap(c)} {BACK_ADV[(len(p) + len(c)) % 3]}",
         f"{FRONT_ADV[(len(p)) % 3]} {p} visited {cap(c)}.",
         f"{cap(p)} later returned to {cap(c)}.",
         f"{cap(p)} arrived in {cap(c)} {BACK_ADV[len(c) % 3]}"]
    return v[k % 5]

def owns_variants(p, o, k):
    v = [f"{cap(p)} owns a {o}.",
         f"{cap(p)} has a {o}.",
         f"{FRONT_ADV[len(p) % 3]} {p} owns a {o}."]
    return v[k % 3]

def belongs_variant(o, p):
    return f"The {o} belongs to {p}."

def in_variants(c, k):
    v = [f"{cap(c)} is in {cap(k)}.",
         f"{cap(c)} lies in {cap(k)}.",
         f"{cap(c)} was in {cap(k)} {BACK_ADV[len(c) % 3]}"]
    return v

def pron_visit(p, c):
    pro = "She" if p in ("maria", "ana", "carmen", "elena", "lucia", "sara",
                          "alba", "vera", "nora", "mia", "sofia", "ines",
                          "gaia", "julia") else "He"
    return f"{pro} visited {cap(c)}."

def pron_owns(p, o):
    pro = "She" if p in ("maria", "ana", "carmen", "elena", "lucia", "sara",
                          "alba", "vera", "nora", "mia", "sofia", "ines",
                          "gaia", "julia") else "He"
    return f"{pro} has a {o}."

# ---- emision ----
def emit_person(p, g, tag):
    k = 0
    c = g["lives"]
    emit(lives_variants(p, c, k), (p, "lives_in", c)); k += 1
    emit(lives_variants(p, c, k + 1), (p, "lives_in", c)); k += 2
    if p in CONTRA:
        emit(lives_variants(p, CONTRA[p], k), (p, "lives_in", CONTRA[p])); k += 1
    c = g["works"]
    emit(works_variants(p, c, k), (p, "works_in", c)); k += 1
    emit(works_variants(p, c, k + 1), (p, "works_in", c)); k += 2
    for ci, c in enumerate(g["visits"]):
        emit(visits_variants(p, c, k), (p, "visits", c)); k += 1
        emit(visits_variants(p, c, k + 2), (p, "visits", c)); k += 1
        if ci == 0 and len(p) % 2 == 0:
            nxt = g["visits"][(ci + 1) % 3]
            emit(pron_visit(p, nxt), (p, "visits", nxt))
    for oi, o in enumerate(g["owns"]):
        emit(owns_variants(p, o, k), (p, "owns", o)); k += 1
        emit(owns_variants(p, o, k + 1), (p, "owns", o)); k += 1
        emit(owns_variants(p, o, k + 2), (p, "owns", o)); k += 1
        emit(belongs_variant(o, p), (o, "belongs_to", p))
        if oi == 0 and len(p) % 2 == 1:
            emit(pron_owns(p, g["owns"][1]), (p, "owns", g["owns"][1]))

os.makedirs(OUT, exist_ok=True)
# reaches observados densos (no held): el objetivo debe verse para que la
# regla latente sea descubrible (precedente EXP28); las conclusiones de
# pares held-out jamas se enuncian y son el test.
reached = set()
reach_blocks = []
for p in PERSONS + TRANSFER:
    g = CORE.get(p, TRA.get(p))
    for c in g["visits"]:
        k = COUNTRY[c]
        if (p, k) in HELD_REACH:
            continue
        if (p, k) not in reached:
            reached.add((p, k))
            t = f"{cap(p)} reaches {cap(k)}." if len(reached) % 2 else f"{cap(p)} reached {cap(k)} last year."
            reach_blocks.append([(t, (p, "reaches", k))])
# Emision por BLOQUES de persona: cada bloque conserva el orden interno
# (el pronombre sigue inmediatamente a su antecedente, mismo sujeto).
# Se mezclan bloques, nunca frases sueltas: la adyacencia pronombre->
# antecedente queda garantizada por construccion y el gold la verifica.
blocks = []
order = PERSONS + TRANSFER
random.shuffle(order)
for p in order:
    sentences, gold = [], []
    emit_person(p, CORE.get(p, TRA.get(p)), "x")
    blocks.append(list(zip(sentences, gold)))
inblock = []
for c, k in COUNTRY.items():
    vs = in_variants(c, k)
    for v in (vs[0], vs[1 + (len(c) % 2)]):
        inblock.append((v, (c, "in", k)))
blocks.append(inblock)
for n in NOISE:
    blocks.append([(n, None)])
for rb in reach_blocks:
    blocks.append(rb)
random.shuffle(blocks)
flat = [x for b in blocks for x in b]
sentences = [x[0] for x in flat]
gold = [x[1] for x in flat]
with open(f"{OUT}/corpus.txt", "w", encoding="utf-8") as fh:
    fh.write("\n".join(sentences) + "\n")
with open(f"{OUT}/gold.pl", "w", encoding="utf-8") as fh:
    for i, t in enumerate(gold, 1):
        if t is None:
            fh.write(f"gold_none({i}).\n")
        else:
            fh.write(f"gold_fact({i}, {t[0]}, {t[1]}, {t[2]}).\n")

# ---- expected: retrieval + reasoning + unknown + distractores ----
exp, dist = [], []
qid = [0]
def qadd(zone, kind, *a):
    qid[0] += 1
    exp.append((qid[0], zone, kind) + a)

for p, k in sorted(HELD_REACH):
    qadd("tra" if p in TRANSFER else "core", "reach", p, k, "reasoned")
# retrieval observado (no held, no contra)
n = 0
for p in PERSONS:
    if p in CONTRA:
        continue
    c = CORE[p]["visits"][0]
    qadd("core", "visits", p, c, "retrieved")
    n += 1
    if n >= 8:
        break
for p in TRANSFER[:4]:
    c = TRA[p]["visits"][1]
    qadd("tra", "visits", p, c, "retrieved")
# distractores falsos verificados (nunca enunciados ni implicados)
dist += [("ines", "norway", "reach"), ("teo", "peru", "reach"),
         ("maria", "quito", "visits"), ("leo", "liverpool", "visits")]
# unknowns
for z in ["zorin", "velara"]:
    qadd("core", "unknown_person", z, "-")

with open(f"{OUT}/expected.pl", "w", encoding="utf-8") as fh:
    for e in exp:
        fh.write(f"expected({', '.join(map(str, e))}).\n")
with open(f"{OUT}/distractor_natural.pl", "w", encoding="utf-8") as fh:
    for p, x, r in dist:
        fh.write(f"distractor({p}, {x}, {r}).\n")
with open(f"{OUT}/heldout_natural.pl", "w", encoding="utf-8") as fh:
    for p, k in sorted(HELD_REACH):
        fh.write(f"heldout({p}, {k}, reaches).\n")

print(f"sentences={len(sentences)} gold_facts={sum(1 for t in gold if t)} "
      f"held_reach={len(HELD_REACH)} "
      f"queries={len(exp)} distractors={len(dist)}")
