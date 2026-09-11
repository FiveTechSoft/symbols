#!/usr/bin/env python3
"""Corpus 2 (~345 sentences, 4 blocks): guided incremental learning.
b1: visits/in/reaches + eats (30 chains, 5 held-out).
b2: travels/in/arrives (30 chains, 5 held-out; 'in' shared).
b3: reads/in/borrows (30 chains, 5 held-out).
b4: noise owns/likes (60 facts, NO conclusions -> no new rules expected).
Deterministic (seed fixed). Writes corpus2_b{1..4}.txt + heldout2.pl.
"""
import random

random.seed(20260912)

COUNTRIES = ["italy", "france", "spain", "norway",
             "ecuador", "peru", "ireland", "switzerland"]
FOODS = ["bread", "cheese", "rice", "apple",
         "fish", "meat", "soup", "salad"]

SYL1 = ["za", "tu", "se", "bri", "ka", "du", "fe", "gi", "hel", "ir",
        "jo", "kle", "lu", "ma", "ne", "ot", "pa", "qui", "ra", "sel",
        "ta", "ul", "va", "wo", "xe", "ya", "ze", "bar", "cor", "dor"]
SYL2 = ["rin", "vio", "sel", "dor", "kan", "lix", "mor", "tan", "vil",
        "xor", "yan", "zel", "mar", "nis", "for", "gus", "han", "jos",
        "kil", "lan", "mir", "nor", "pos", "quil", "ros", "san", "tor",
        "um", "ver", "win"]


def invented(n, taken):
    out = []
    i = 0
    while len(out) < n:
        # producto cartesiano real (900 combos), no secuencia acoplada
        w = SYL1[i % len(SYL1)] + SYL2[(i // len(SYL1)) % len(SYL2)]
        i += 1
        if w not in taken:
            taken.add(w)
            out.append(w)
    return out


TAKEN = set(["lina", "mario", "sofia", "anna", "paul", "elena", "ruth",
             "ivan", "roma", "paris", "madrid", "oslo", "quito", "lima",
             "dublin", "bern"] + COUNTRIES + FOODS)

P1 = (["lina", "mario", "sofia", "anna", "paul", "elena", "ruth",
       "ivan"] + invented(22, TAKEN))
C1 = (["roma", "paris", "madrid", "oslo", "quito", "lima", "dublin",
       "bern"] + invented(22, TAKEN))
P2 = invented(30, TAKEN)
C2 = invented(30, TAKEN)
P3 = invented(30, TAKEN)
BOOKS = invented(30, TAKEN)
LIBS = invented(8, TAKEN)

VISITS_T = ["{p} visits {c}.", "{p} visited {c}.",
            "yesterday {p} visited {c}.", "today {p} visits {c}."]
IN_T = ["{c} is in {k}.", "{c} lies in {k}.", "{c} was in {k}."]
REACHES_T = ["{p} reaches {k}.", "{p} has reached {k}."]
EATS_T = ["{p} eats {f}.", "{p} ate {f}."]
TRAVELS_T = ["{p} travels through {c}.", "{p} travelled through {c}."]
ARRIVES_T = ["{p} arrives in {k}.", "{p} arrived in {k}."]
READS_T = ["{p} reads {b}.", "{p} read {b}."]
BORROWS_T = ["{p} borrows {b}.", "{p} borrowed {b}."]
OWNS_T = ["{p} owns {o}.", "{p} owned {o}."]
LIKES_T = ["{p} likes {o}.", "{p} liked {o}."]

HELDOUT_IDX = {3, 11, 19, 23, 27}


def main():
    # b1: persons x cities->countries + eats
    b1, h1 = [], []
    for i in range(30):
        p, c = P1[i], C1[i]
        k = COUNTRIES[i % len(COUNTRIES)]
        f = FOODS[i % len(FOODS)]
        b1.append(VISITS_T[i % len(VISITS_T)].format(p=p, c=c))
        b1.append(IN_T[i % len(IN_T)].format(c=c, k=k))
        if i in HELDOUT_IDX:
            h1.append((p, k, "reaches"))
        else:
            b1.append(REACHES_T[i % len(REACHES_T)].format(p=p, k=k))
        b1.append(EATS_T[i % len(EATS_T)].format(p=p, f=f))
    # b2: travels/in/arrives (new cities, same countries)
    b2, h2 = [], []
    for i in range(30):
        p, c = P2[i], C2[i]
        k = COUNTRIES[i % len(COUNTRIES)]
        b2.append(TRAVELS_T[i % len(TRAVELS_T)].format(p=p, c=c))
        b2.append(IN_T[i % len(IN_T)].format(c=c, k=k))
        if i in HELDOUT_IDX:
            h2.append((p, k, "arrives"))
        else:
            b2.append(ARRIVES_T[i % len(ARRIVES_T)].format(p=p, k=k))
    # b3: reads/in/borrows (books + libraries; borrows->library para que
    # 'in' sea load-bearing y [reads] solo no empate)
    b3, h3 = [], []
    for i in range(30):
        p, b = P3[i], BOOKS[i]
        li = LIBS[i % len(LIBS)]
        b3.append(READS_T[i % len(READS_T)].format(p=p, b=b))
        b3.append(IN_T[i % len(IN_T)].format(c=b, k=li))
        if i in HELDOUT_IDX:
            h3.append((p, li, "borrows"))
        else:
            b3.append(BORROWS_T[i % len(BORROWS_T)].format(p=p, b=li))
    # b4: noise owns/likes (no conclusions anywhere)
    b4 = []
    pool_p = P1 + P2 + P3
    pool_o = C1 + C2 + BOOKS + FOODS
    for i in range(30):
        p = pool_p[(i * 11) % len(pool_p)]
        o = pool_o[(i * 17) % len(pool_o)]
        b4.append(OWNS_T[i % len(OWNS_T)].format(p=p, o=o))
    for i in range(30):
        p = pool_p[(i * 13 + 5) % len(pool_p)]
        o = pool_o[(i * 19 + 3) % len(pool_o)]
        b4.append(LIKES_T[i % len(LIKES_T)].format(p=p, o=o))
    blocks = {"corpus2/b1.txt": b1, "corpus2/b2.txt": b2,
              "corpus2/b3.txt": b3, "corpus2/b4.txt": b4}
    total = 0
    for name, ss in blocks.items():
        with open(name, "w", encoding="utf-8") as fh:
            fh.write("\n".join(ss) + "\n")
        total += len(ss)
        print(f"{name}: {len(ss)}")
    with open("heldout2.pl", "w", encoding="utf-8") as fh:
        for p, x, rel in h1 + h2 + h3:
            fh.write(f"heldout({p}, {x}, {rel}).\n")
    # distractores verificados-falsos: mismo tipo, distinto objetivo real.
    # (El siguiente-ciclico ingenuo falla cuando dos heldout comparten
    # objetivo: el par "distractor" seria verdadero.)
    held = h1 + h2 + h3
    with open("distractor2.pl", "w", encoding="utf-8") as fh:
        for i, (p, x, rel) in enumerate(held):
            j = (i + 1) % len(held)
            while held[j][1] == x or held[j][2] != rel:
                j = (j + 1) % len(held)
            fh.write(f"distractor({p}, {held[j][1]}, {rel}).\n")
    print(f"total={total} heldout={len(h1) + len(h2) + len(h3)}")


if __name__ == "__main__":
    main()
