#!/usr/bin/env python3
"""make_corpus_natural3.py — Corpus Natural v0.3 (EXP44-A).

Un mundo, DOS familias de superficie disjuntas (entidades y relaciones):
  A: owns/belongs_to/visits -> stocked ; cooks/needs -> serves
  B: keeps/held_by/tours   -> stored   ; prepares/requires -> offers
B NO enuncia ninguna conclusion: stored/offers solo existen como
gold oculto (queries). El motor debe mapear por roles estructurales.
"""
import os, random

random.seed(11)
OUT = "corpus_natural3"

PA = ["alba", "bruno", "carla", "diego", "elena", "fabio",
      "greta", "hugo", "irene", "jorge", "kira", "leo",
      "nora", "otto"]
PB = ["moss", "nila", "ovar", "pela", "quim", "rufa", "silo", "tove",
      "urre", "xana"]
CITIES = ["madrid", "oslo", "paris", "roma", "quito", "lima",
          "dublin", "bern", "lisboa", "atenea"]
COUNTRY = {"madrid": "spain", "oslo": "norway", "paris": "france",
           "roma": "italy", "quito": "ecuador", "lima": "peru",
           "dublin": "ireland", "bern": "switzerland",
           "lisboa": "portugal", "atenea": "grecia"}
OBJS_A = ["book", "tablet", "violin", "camera", "compass", "lantern"]
DISH_A = ["paella", "tortilla", "gazpacho", "fabada", "cocido", "migas"]
ING_A = ["rice", "egg", "garlic", "pork", "pepper", "onion"]
OBJS_B = ["drum", "kettle", "mirror", "paddle", "radio", "sextant"]
DISH_B = ["stew", "broth", "curry", "goulash", "chowder", "tajine"]
ING_B = ["beans", "lentils", "cumin", "thyme", "barley", "sage"]
FEM_A = {"alba", "carla", "elena", "greta", "irene", "kira", "nora"}
FEM_B = {"nila", "pela", "rufa", "tove", "xana"}
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
         "Dogs bark loudly.", "The bakery smells wonderful.",
         "Stars appear gradually.", "Thunder rumbles far away."]

def build(persons, ci, oi, di, ii):
    g = {}
    for i, p in enumerate(persons):
        g[p] = {"visits": [CITIES[(ci + i + 1) % len(CITIES)],
                           CITIES[(ci + i + 3) % len(CITIES)],
                           CITIES[(ci + i + 5) % len(CITIES)]],
                "owns": [OBJS_A[(oi + i) % len(OBJS_A)],
                         OBJS_A[(oi + i + 3) % len(OBJS_A)]] if p in PA else [OBJS_B[(oi + i) % len(OBJS_B)],
                         OBJS_B[(oi + i + 3) % len(OBJS_B)]],
                "dish": [DISH_A[(di + i) % len(DISH_A)] if p in PA else
                         DISH_B[(di + i) % len(DISH_B)]],
                "ing": [ING_A[(ii + i) % len(ING_A)] if p in PA else
                        ING_B[(ii + i) % len(ING_B)]]}
    return g

GA = build(PA, 0, 0, 0, 0)
GB = build(PB, 4, 2, 1, 3)

sentences, gold = [], []

def emit(text, triplet):
    sentences.append(text)
    gold.append(triplet)

def cap(w):
    return w[0].upper() + w[1:]

def pro(p):
    fem = FEM_A if p in PA else FEM_B
    return "She" if p in fem else "He"

def vv(p, c, k):
    v = [f"{cap(p)} visited {cap(c)}.",
         f"{cap(p)} visits {cap(c)} {BACK_ADV[(len(p) + len(c)) % 3]}",
         f"{FRONT_ADV[len(p) % 3]} {p} visited {cap(c)}.",
         f"{cap(p)} later returned to {cap(c)}."]
    return v[k % 4]

def emit_a(p, g):
    k = 0
    for ci, c in enumerate(g["visits"]):
        emit(vv(p, c, k), (p, "visits", c)); k += 1
        emit(vv(p, c, k + 2), (p, "visits", c)); k += 1
        if ci == 0 and len(p) % 2 == 0:
            emit(f"{pro(p)} visited {cap(g['visits'][1])}.",
                 (p, "visits", g["visits"][1]))
    for o in g["owns"]:
        emit(f"{cap(p)} owns a {o}.", (p, "owns", o)); k += 1
        emit(f"{cap(p)} has a {o}.", (p, "owns", o)); k += 1
        emit(f"The {o} belongs to {p}.", (o, "belongs_to", p))
    d, ing = g["dish"][0], g["ing"][0]
    emit(f"{cap(p)} cooks {d}.", (p, "cooks", d)); k += 1
    emit(f"{cap(p)} cooked {d} {BACK_ADV[k % 3]}", (p, "cooks", d)); k += 1
    emit(f"The {d} needs {ing}.", (d, "needs", ing))
    emit(f"The {d} needed {ing} {BACK_ADV[k % 3]}", (d, "needs", ing)); k += 1

def emit_b(p, g):
    k = 0
    for ci, c in enumerate(g["visits"]):
        emit(kv(p, c, k), (p, "tours", c)); k += 1
        emit(kv(p, c, k + 2), (p, "tours", c)); k += 1
        if ci == 0 and len(p) % 2 == 0:
            emit(f"{pro(p)} toured {cap(g['visits'][1])}.",
                 (p, "tours", g["visits"][1]))
    for o in g["owns"]:
        emit(f"{cap(p)} keeps a {o}.", (p, "keeps", o)); k += 1
        emit(f"{cap(p)} kept a {o} {BACK_ADV[k % 3]}", (p, "keeps", o)); k += 1
        emit(f"The {o} is held by {p}.", (o, "held_by", p))
    d, ing = g["dish"][0], g["ing"][0]
    emit(f"{cap(p)} prepares {d}.", (p, "prepares", d)); k += 1
    emit(f"{cap(p)} prepared {d} {BACK_ADV[k % 3]}", (p, "prepares", d)); k += 1
    emit(f"The {d} requires {ing}.", (d, "requires", ing))
    emit(f"The {d} required {ing} {BACK_ADV[k % 3]}", (d, "requires", ing)); k += 1

def kv(p, c, k):
    v = [f"{cap(p)} toured {cap(c)}.",
         f"{cap(p)} tours {cap(c)} {BACK_ADV[(len(p) + len(c)) % 3]}",
         f"{FRONT_ADV[len(p) % 3]} {p} toured {cap(c)}.",
         f"{cap(p)} later came back to {cap(c)}."]
    return v[k % 4]

os.makedirs(OUT, exist_ok=True)
blocks = []
order = PA + PB
random.shuffle(order)
for p in order:
    sentences, gold = [], []
    if p in PA:
        emit_a(p, GA[p])
    else:
        emit_b(p, GB[p])
    blocks.append(list(zip(sentences, gold)))
inblock = []
for c, k in COUNTRY.items():
    inblock.append((f"{cap(c)} is in {cap(k)}.", (c, "in", k)))
    inblock.append((f"{cap(c)} lies in {cap(k)}.", (c, "in", k)))
blocks.append(inblock)
for n in NOISE:
    blocks.append([(n, None)])
# conclusiones densas SOLO en A (B las oculta todas)
# conclusiones densas SOLO en A (descubrimiento nativo; B lo oculta todo
# y lo resuelve por mapa). reaches(P,K) + serves(P,I) observados.
aconcl = []
for p in PA:
    seen = set()
    for c in GA[p]["visits"]:
        k = COUNTRY[c]
        if k not in seen:
            seen.add(k)
            t = f"{cap(p)} reaches {cap(k)}." if len(seen) % 2 else f"{cap(p)} reached {cap(k)} last year."
            aconcl.append([(t, (p, "reaches", k))])
    ing = GA[p]["ing"][0]
    aconcl.append([(f"{cap(p)} serves {ing}.", (p, "serves", ing))])
for b in aconcl:
    blocks.append(b)
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

# ---- queries: B oculto (stored/offers), A cordura, retrieval, unknown ----
exp = []
qid = [0]
def qadd(zone, kind, *a):
    qid[0] += 1
    exp.append((qid[0], zone, kind) + a)

# B: stored(O,C) para cada estructura (oculto: jamas enunciado)
for p in PB[:6]:
    c = GB[p]["visits"][0]
    qadd("tra", "stored", GB[p]["owns"][0], c, "reasoned")
for p in PB[:4]:
    qadd("tra", "offers", p, GB[p]["ing"][0], "reasoned")
# A: retrieval de conclusiones observadas + retrieval base
for p in PA[:3]:
    c = GA[p]["visits"][0]
    qadd("core", "reaches", p, COUNTRY[c], "retrieved")
for p in PA[3:5]:
    qadd("core", "serves", p, GA[p]["ing"][0], "retrieved")
for p in PA[:3]:
    qadd("core", "visits", p, GA[p]["visits"][1], "retrieved")
for p in PB[:3]:
    qadd("tra", "tours", p, GB[p]["visits"][1], "retrieved")

G = {**GA, **GB}
def entails(p, x, r):
    g = G[p]
    if r in ("stored", "reaches"):
        return any(COUNTRY[c] == x for c in g["visits"])
    if r in ("offers", "serves"):
        return x in g["ing"]
    if r in ("tours", "visits"):
        return x in g["visits"]
    if r in ("keeps", "owns"):
        return x in g["owns"]
    return False

def pick_false(p, r, pool):
    for x in pool:
        if not entails(p, x, r):
            return (p, x, r)
    raise AssertionError(f"sin falso para {(p, r)}")

Ks = sorted(set(COUNTRY.values()))
dist = [pick_false("moss", "stored", CITIES),
        pick_false("nila", "stored", CITIES),
        pick_false("ovar", "offers", ING_B),
        pick_false("alba", "reaches", Ks),
        pick_false("moss", "tours", CITIES),
        pick_false("alba", "visits", CITIES)]
for z in ["zorin", "velara"]:
    qadd("core", "unknown_person", z, "-", "unknown")

with open(f"{OUT}/expected.pl", "w", encoding="utf-8") as fh:
    for e in exp:
        fh.write(f"expected({', '.join(map(str, e))}).\n")
with open(f"{OUT}/distractor_natural3.pl", "w", encoding="utf-8") as fh:
    for p, x, r in dist:
        fh.write(f"distractor({p}, {x}, {r}).\n")
with open(f"{OUT}/heldout_natural3.pl", "w", encoding="utf-8") as fh:
    for e in exp:
        if e[2] in ("stored", "offers"):
            fh.write(f"heldout({e[3]}, {e[4]}, {e[2]}).\n")

print(f"sentences={len(sentences)} gold_facts={sum(1 for t in gold if t)} "
      f"queries={len(exp)} distractors={len(dist)}")
