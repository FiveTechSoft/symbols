#!/usr/bin/env python3
"""make_corpus_natural2.py — Corpus Natural v0.2 (EXP43).

Tres estructuras simultaneas compartiendo personas, sin fusionarse:
  A: visits/in -> reaches      (llega de EXP42)
  B: works_at/located -> based (nueva)
  C: owns/belongs_to           (evidencia + retrieval)
Split held-out previo de CONCLUSIONES por skill. Gold solo puntua.
"""
import os, random

random.seed(7)
OUT = "corpus_natural2"

PERSONS = ["alba", "bruno", "carla", "diego", "elena", "fabio",
           "greta", "hugo", "irene", "jorge", "kira", "leo",
           "marta", "nils", "olga", "pablo"]
TRANSFER = ["quinn", "rosa", "sven", "tara", "ugo", "vera"]
CITIES = ["madrid", "oslo", "paris", "roma", "quito", "lima",
          "dublin", "bern", "lisboa", "atenea"]
COUNTRY = {"madrid": "spain", "oslo": "norway", "paris": "france",
           "roma": "italy", "quito": "ecuador", "lima": "peru",
           "dublin": "ireland", "bern": "switzerland",
           "lisboa": "portugal", "atenea": "grecia"}
OBJECTS = ["book", "tablet", "violin", "camera", "compass", "lantern",
           "globe", "kettle", "mirror", "paddle"]
ORGS = ["acme", "borealis", "civica", "danubio", "electra", "futura",
        "granito", "helvetia"]
FEM = {"alba", "carla", "elena", "greta", "irene", "kira", "marta",
       "olga", "rosa", "tara", "vera"}
FRONT_ADV = ["Yesterday", "Today", "Last week,"]
BACK_ADV = ["frequently.", "every summer.", "last year."]
NOISE = ["It rains frequently.", "The weather is pleasant today.",
         "Snow covers the mountains.", "The wind blows strongly.",
         "Time passes quickly.", "Nobody answered the phone.",
         "The train arrives late.", "Birds sing every morning.",
         "Coffee tastes better today.", "The river flows calmly.",
         "Children play outside.", "Music fills the streets.",
         "The moon shines brightly.", "Traffic moves slowly today.",
         "Tea cools on the table.", "The city never sleeps.",
         "Waves crash against the rocks.", "The garden blooms beautifully.",
         "Thunder rumbles far away.", "Stars appear gradually."]

def build_ground(persons, ci, oi, gi):
    g = {}
    for i, p in enumerate(persons):
        g[p] = {"visits": [CITIES[(ci + i + 1) % len(CITIES)],
                           CITIES[(ci + i + 3) % len(CITIES)],
                           CITIES[(ci + i + 5) % len(CITIES)]],
                "owns": [OBJECTS[(oi + i) % len(OBJECTS)],
                         OBJECTS[(oi + i + 4) % len(OBJECTS)]],
                "org": ORGS[(gi + i) % len(ORGS)]}
    return g

CORE = build_ground(PERSONS, 0, 0, 0)
TRA = build_ground(TRANSFER, 5, 6, 3)
ORG_CITY = {o: CITIES[(i * 3 + 1) % len(CITIES)] for i, o in enumerate(ORGS)}

# held-out previo de conclusiones por skill
HELD_REACH, HELD_BASED = set(), set()
for i in range(10):
    p = PERSONS[i]
    HELD_REACH.add((p, COUNTRY[CORE[p]["visits"][i % 3]]))
for i in range(3):
    p = TRANSFER[i]
    HELD_REACH.add((p, COUNTRY[TRA[p]["visits"][0]]))
for i in range(6):
    p = PERSONS[i + 2]
    HELD_BASED.add((p, COUNTRY[ORG_CITY[CORE[p]["org"]]]))
for i in range(2):
    p = TRANSFER[i + 3]
    HELD_BASED.add((p, COUNTRY[ORG_CITY[TRA[p]["org"]]]))

sentences, gold = [], []

def emit(text, triplet):
    sentences.append(text)
    gold.append(triplet)

def cap(w):
    return w[0].upper() + w[1:]

def pro(p):
    return "She" if p in FEM else "He"

def visits_v(p, c, k):
    v = [f"{cap(p)} visited {cap(c)}.",
         f"{cap(p)} visits {cap(c)} {BACK_ADV[(len(p) + len(c)) % 3]}",
         f"{FRONT_ADV[len(p) % 3]} {p} visited {cap(c)}.",
         f"{cap(p)} later returned to {cap(c)}.",
         f"{cap(p)} arrived in {cap(c)} {BACK_ADV[len(c) % 3]}"]
    return v[k % 5]

def owns_v(p, o, k):
    v = [f"{cap(p)} owns a {o}.",
         f"{cap(p)} has a {o}.",
         f"{FRONT_ADV[len(p) % 3]} {p} owns a {o}."]
    return v[k % 3]

def works_v(p, g2, k):
    v = [f"{cap(p)} works at {cap(g2)}.",
         f"{FRONT_ADV[(len(p) + 1) % 3]} {p} works at {cap(g2)}.",
         f"{cap(p)} works at {cap(g2)} {BACK_ADV[len(g2) % 3]}"]
    return v[k % 3]

def emit_person(p, g):
    k = 0
    for ci, c in enumerate(g["visits"]):
        emit(visits_v(p, c, k), (p, "visits", c)); k += 1
        emit(visits_v(p, c, k + 2), (p, "visits", c)); k += 1
        emit(visits_v(p, c, k + 4), (p, "visits", c)); k += 1
        if ci == 0 and len(p) % 2 == 0:
            nxt = g["visits"][(ci + 1) % 3]
            emit(f"{pro(p)} visited {cap(nxt)}.", (p, "visits", nxt))
    for oi, o in enumerate(g["owns"]):
        emit(owns_v(p, o, k), (p, "owns", o)); k += 1
        emit(owns_v(p, o, k + 1), (p, "owns", o)); k += 1
        emit(f"The {o} belongs to {p}.", (o, "belongs_to", p))
        if oi == 0 and len(p) % 2 == 1:
            emit(f"{pro(p)} has a {g['owns'][1]}.", (p, "owns", g["owns"][1]))
    emit(works_v(p, g["org"], k), (p, "works_at", g["org"])); k += 1
    emit(works_v(p, g["org"], k + 1), (p, "works_at", g["org"])); k += 1

os.makedirs(OUT, exist_ok=True)
blocks = []
order = PERSONS + TRANSFER
random.shuffle(order)
for p in order:
    sentences, gold = [], []
    emit_person(p, CORE.get(p, TRA.get(p)))
    blocks.append(list(zip(sentences, gold)))
inblock = []
for c, k in COUNTRY.items():
    inblock.append((f"{cap(c)} is in {cap(k)}.", (c, "in", k)))
    inblock.append((f"{cap(c)} lies in {cap(k)}.", (c, "in", k)))
locblock = []
for o, c in ORG_CITY.items():
    locblock.append((f"{cap(o)} is located in {cap(c)}.",
                     (o, "located", c)))
    locblock.append((f"{cap(o)} was located in {cap(c)}.",
                     (o, "located", c)))
blocks.append(inblock)
blocks.append(locblock)
for n in NOISE:
    blocks.append([(n, None)])
# reaches/based densos (no held)
reached, basedd = set(), set()
reach_blocks = []
for p in PERSONS + TRANSFER:
    g = CORE.get(p, TRA.get(p))
    for c in g["visits"]:
        k = COUNTRY[c]
        if (p, k) not in HELD_REACH and (p, k) not in reached:
            reached.add((p, k))
            t = f"{cap(p)} reaches {cap(k)}." if len(reached) % 2 else f"{cap(p)} reached {cap(k)} last year."
            reach_blocks.append([(t, (p, "reaches", k))])
    k = COUNTRY[ORG_CITY[g["org"]]]
    if (p, k) not in HELD_BASED and (p, k) not in basedd:
        basedd.add((p, k))
        t = f"{cap(p)} is based in {cap(k)}." if len(basedd) % 2 else f"{cap(p)} was based in {cap(k)} last year."
        reach_blocks.append([(t, (p, "based", k))])
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

exp, dist = [], []
qid = [0]
def qadd(zone, kind, *a):
    qid[0] += 1
    exp.append((qid[0], zone, kind) + a)

for p, k in sorted(HELD_REACH):
    qadd("tra" if p in TRANSFER else "core", "reach", p, k, "reasoned")
for p, k in sorted(HELD_BASED):
    qadd("tra" if p in TRANSFER else "core", "based", p, k, "reasoned")
n = 0
for p in PERSONS:
    c = CORE[p]["visits"][0]
    qadd("core", "visits", p, c, "retrieved")
    n += 1
    if n >= 6:
        break
for p in TRANSFER[:3]:
    qadd("tra", "visits", p, TRA[p]["visits"][1], "retrieved")
n = 0
for p in PERSONS:
    qadd("core", "owns", p, CORE[p]["owns"][0], "retrieved")
    n += 1
    if n >= 4:
        break
for p in TRANSFER[:2]:
    qadd("tra", "owns", p, TRA[p]["owns"][0], "retrieved")
Ks = sorted(set(COUNTRY.values()))
# verificacion contra ground truth (falsos reales, no solo no-enunciados)
G = {**CORE, **TRA}
def entails(p, x, r):
    g = G[p]
    if r == "reach":
        return any(COUNTRY[c] == x for c in g["visits"])
    if r == "based":
        return COUNTRY[ORG_CITY[g["org"]]] == x
    if r == "visits":
        return x in g["visits"]
    if r == "owns":
        return x in g["owns"]
    return False

def pick_false(p, r, pool):
    for x in pool:
        if not entails(p, x, r):
            return (p, x, r)
    raise AssertionError(f"sin falso para {(p, r)}")

Ks = sorted(set(COUNTRY.values()))
dist = [pick_false("quinn", "reach", Ks),
        pick_false("alba", "reach", Ks),
        pick_false("marta", "based", Ks),
        pick_false("sven", "based", Ks),
        pick_false("alba", "visits", CITIES),
        pick_false("ugo", "owns", OBJECTS + ["liverpool"])]
for p, x, r in dist:
    assert not entails(p, x, r), f"distractor verdadero: {(p, x, r)}"
for z in ["zorin", "velara"]:
    qadd("core", "unknown_person", z, "-")

with open(f"{OUT}/expected.pl", "w", encoding="utf-8") as fh:
    for e in exp:
        fh.write(f"expected({', '.join(map(str, e))}).\n")
with open(f"{OUT}/distractor_natural2.pl", "w", encoding="utf-8") as fh:
    for p, x, r in dist:
        fh.write(f"distractor({p}, {x}, {r}).\n")
with open(f"{OUT}/heldout_natural2.pl", "w", encoding="utf-8") as fh:
    for p, k in sorted(HELD_REACH):
        fh.write(f"heldout({p}, {k}, reaches).\n")
    for p, k in sorted(HELD_BASED):
        fh.write(f"heldout({p}, {k}, based).\n")

print(f"sentences={len(sentences)} gold_facts={sum(1 for t in gold if t)} "
      f"held_reach={len(HELD_REACH)} held_based={len(HELD_BASED)} "
      f"queries={len(exp)} distractors={len(dist)}")
