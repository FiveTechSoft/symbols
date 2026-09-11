#!/usr/bin/env python3
"""Corpus 1 (pilot, ~150 sentences): visits/in/reaches + eats, EN.
Deterministic (seed fixed). 40 chains; reaches HELD-OUT for 10 persons
(every 4th, indices 3,7,...,39). eats for all 40 (concept uniformity).
Writes corpus1.txt (one sentence/line) + heldout1.pl + prints counts.
"""
import random

random.seed(20260911)

KNOWN_PERSONS = ["lina", "mario", "sofia", "anna",
                 "paul", "elena", "ruth", "ivan"]
EXTRA_PERSONS = ["zorin", "kadir", "wex", "nuru", "tavio", "sella",
                 "brizio", "kama", "dulio", "femia", "giras", "helmo",
                 "irsa", "jovito", "klen", "luria", "mavro", "nesli",
                 "otman", "pavia", "quilo", "ravia", "selmo", "taria",
                 "ulixes", "varna", "woldo", "xenia", "yarimo", "zelda",
                 "barto", "corin"]
KNOWN_CITIES = ["roma", "paris", "madrid", "oslo",
                "quito", "lima", "dublin", "bern"]
EXTRA_CITIES = ["velara", "quis", "tormo", "sarina", "belgrado", "caspia",
                "dornas", "elmira", "faro", "gandia", "halvar", "ibiza",
                "jaca", "karlov", "leiria", "mostar", "narva", "ourense",
                "palamos", "quero", "ribera", "soria", "teruel", "ubeda",
                "vilalba", "xativa", "yecla", "zambrana", "alaro",
                "benissa", "calella", "denia"]
COUNTRIES = ["italy", "france", "spain", "norway",
             "ecuador", "peru", "ireland", "switzerland"]
FOODS = ["bread", "cheese", "rice", "apple",
         "fish", "meat", "soup", "salad"]

PERSONS = KNOWN_PERSONS + EXTRA_PERSONS
CITIES = KNOWN_CITIES + EXTRA_CITIES
assert len(PERSONS) == 40 and len(CITIES) == 40

HELDOUT = set(range(3, 40, 4))  # 10 persons: 3,7,...,39

VISITS_T = ["{p} visits {c}.",
            "{p} visited {c}.",
            "yesterday {p} visited {c}.",
            "today {p} visits {c}."]
IN_T = ["{c} is in {k}.",
        "{c} lies in {k}.",
        "{c} was in {k}."]
REACHES_T = ["{p} reaches {k}.",
             "{p} has reached {k}."]
EATS_T = ["{p} eats {f}.",
          "{p} ate {f}."]


def main():
    sentences = []
    heldout = []
    for i, (p, c) in enumerate(zip(PERSONS, CITIES)):
        k = COUNTRIES[i % len(COUNTRIES)]
        f = FOODS[i % len(FOODS)]
        sentences.append(VISITS_T[i % len(VISITS_T)].format(p=p, c=c))
        sentences.append(IN_T[i % len(IN_T)].format(c=c, k=k))
        if i in HELDOUT:
            heldout.append((p, k))
        else:
            sentences.append(REACHES_T[i % len(REACHES_T)].format(p=p, k=k))
        sentences.append(EATS_T[i % len(EATS_T)].format(p=p, f=f))
    with open("corpus1/corpus1.txt", "w", encoding="utf-8") as fh:
        fh.write("\n".join(sentences) + "\n")
    with open("corpus1/heldout1.pl", "w", encoding="utf-8") as fh:
        for p, k in heldout:
            fh.write(f"heldout({p}, {k}).\n")
    print(f"sentences={len(sentences)} heldout={len(heldout)} "
          f"persons={len(PERSONS)}")
    print(f"expected facts={len(sentences)} "
          f"(visits={40} in={40} reaches={40 - len(heldout)} eats={40})")


if __name__ == "__main__":
    main()
