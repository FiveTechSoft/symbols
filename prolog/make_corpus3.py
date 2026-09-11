#!/usr/bin/env python3
"""Corpus 3 (~1000 sentences): real transfer test.
Train families (all observed, fresh vocabularies):
  FA provides:[works,supplies]   100 chains (persons/places/materials)
  FB imports:[buys,in]           100 chains (persons/items + shared 'in')
  FC uses:[cooks,needs]          100 chains (persons/dishes/ingredients)
  FD admires:[praises,honors,exalts] loop, 10 chains (near-miss exclusion)
Transfer family (fresh everything): plants/yields/harvests, 12 chains,
  2 observed + 10 hidden. Distractors: minimal pairs, verified false
  (1:1 chains => any other same-slot target is unreachable).
Deterministic (seed fixed). Writes corpus3.txt + heldout3.pl +
distractor3.pl.
"""
import random

random.seed(20260913)

SYL1 = ["za", "tu", "se", "bri", "ka", "du", "fe", "gi", "hel", "ir",
        "jo", "kle", "lu", "ma", "ne", "ot", "pa", "qui", "ra", "sel",
        "ta", "ul", "va", "wo", "xe", "ya", "ze", "bar", "cor", "dor",
        "fil", "gor", "hal", "iv", "jel", "kor"]
SYL2 = ["rin", "vio", "sel", "dor", "kan", "lix", "mor", "tan", "vil",
        "xor", "yan", "zel", "mar", "nis", "for", "gus", "han", "jos",
        "kil", "lan", "mir", "nor", "pos", "quil", "ros", "san", "tor",
        "um", "ver", "win", "xor2", "yap", "zim", "ash", "bel", "cor2"]

TAKEN = set(["lina", "mario", "roma", "italy", "bread", "person", "city",
             "country", "food", "he", "she", "him", "her", "is", "a",
             "in", "the", "yesterday", "today"])


def invented(n, taken):
    out = []
    i = 0
    while len(out) < n:
        w = SYL1[i % len(SYL1)] + SYL2[(i // len(SYL1)) % len(SYL2)]
        i += 1
        if w not in taken:
            taken.add(w)
            out.append(w)
    assert len(out) == n, "pool exhausted"
    return out


PA = invented(100, TAKEN)
WA = invented(100, TAKEN)
MA = invented(8, TAKEN)
PB = invented(100, TAKEN)
IB = invented(100, TAKEN)
MB = invented(8, TAKEN)
PC = invented(100, TAKEN)
DC = invented(100, TAKEN)
IC = invented(8, TAKEN)
PD = invented(10, TAKEN)
HD = invented(10, TAKEN)
OD = invented(10, TAKEN)
PD2 = invented(10, TAKEN)
HD2 = invented(10, TAKEN)
OD2 = invented(10, TAKEN)
PT = invented(12, TAKEN)
ST = invented(12, TAKEN)
FT = invented(4, TAKEN)

WORKS_T = ["{p} works {w}.", "{p} worked {w}.", "yesterday {p} worked {w}."]
SUPPLIES_T = ["{w} supplies {m}.", "{w} supplied {m}."]
PROVIDES_T = ["{p} provides {m}.", "{p} provided {m}."]
BUYS_T = ["{p} buys {i}.", "{p} bought {i}.", "yesterday {p} bought {i}."]
IN_T = ["{c} is in {k}.", "{c} lies in {k}.", "{c} was in {k}."]
IMPORTS_T = ["{p} imports {m}.", "{p} imported {m}."]
COOKS_T = ["{p} cooks {d}.", "{p} cooked {d}.", "today {p} cooks {d}."]
NEEDS_T = ["{d} needs {g}.", "{d} needed {g}."]
USES_T = ["{p} uses {g}.", "{p} used {g}."]
PRAISES_T = ["{p} praises {h}."]
HONORS_T = ["{h} honors {h}."]
EXALTS_T = ["{h} exalts {o}."]
ADMIRES_T = ["{p} admires {o}."]
PLANTS_T = ["{p} plants {s}.", "{p} planted {s}."]
YIELDS_T = ["{s} yields {f}.", "{s} yielded {f}."]
HARVESTS_T = ["{p} harvests {f}.", "{p} harvested {f}."]

OBS_T = 2  # observed transfer examples (rest hidden)


def main():
    ss, held, dist = [], [], []
    for i in range(100):
        p, w, m = PA[i], WA[i], MA[i % len(MA)]
        ss.append(WORKS_T[i % len(WORKS_T)].format(p=p, w=w))
        ss.append(SUPPLIES_T[i % len(SUPPLIES_T)].format(w=w, m=m))
        ss.append(PROVIDES_T[i % len(PROVIDES_T)].format(p=p, m=m))
    for i in range(100):
        p, it, m = PB[i], IB[i], MB[i % len(MB)]
        ss.append(BUYS_T[i % len(BUYS_T)].format(p=p, i=it))
        ss.append(IN_T[i % len(IN_T)].format(c=it, k=m))
        ss.append(IMPORTS_T[i % len(IMPORTS_T)].format(p=p, m=m))
    for i in range(100):
        p, d, g = PC[i], DC[i], IC[i % len(IC)]
        ss.append(COOKS_T[i % len(COOKS_T)].format(p=p, d=d))
        ss.append(NEEDS_T[i % len(NEEDS_T)].format(d=d, g=g))
        ss.append(USES_T[i % len(USES_T)].format(p=p, g=g))
    for i in range(10):
        p, h, o = PD[i], HD[i], OD[i]
        ss.append(PRAISES_T[0].format(p=p, h=h))
        ss.append(HONORS_T[0].format(h=h))
        ss.append(EXALTS_T[0].format(h=h, o=o))
        ss.append(ADMIRES_T[0].format(p=p, o=o))
    # decoys sin loop: [praises,exalts] empataria sin ellos (ver EXP10)
    for i in range(10):
        p, h, o = PD2[i], HD2[i], OD2[i]
        ss.append(PRAISES_T[0].format(p=p, h=h))
        ss.append(EXALTS_T[0].format(h=h, o=o))
    for i in range(12):
        p, s, f = PT[i], ST[i], FT[i % len(FT)]
        ss.append(PLANTS_T[i % len(PLANTS_T)].format(p=p, s=s))
        ss.append(YIELDS_T[i % len(YIELDS_T)].format(s=s, f=f))
        if i < OBS_T:
            ss.append(HARVESTS_T[i % len(HARVESTS_T)].format(p=p, f=f))
        else:
            held.append((p, f))
    # distractores verificados: otro objetivo del mismo slot (1:1 => falso)
    for i, (p, f) in enumerate(held):
        f2 = FT[(FT.index(f) + 1) % len(FT)]
        dist.append((p, f2, "harvests"))
        m2 = MA[i % len(MA)]
        dist.append((p, m2, "harvests"))
    with open("corpus3/corpus3.txt", "w", encoding="utf-8") as fh:
        fh.write("\n".join(ss) + "\n")
    with open("corpus3/heldout3.pl", "w", encoding="utf-8") as fh:
        for p, f in held:
            fh.write(f"heldout({p}, {f}, harvests).\n")
    with open("corpus3/distractor3.pl", "w", encoding="utf-8") as fh:
        for p, x, rel in dist:
            fh.write(f"distractor({p}, {x}, {rel}).\n")
    print(f"sentences={len(ss)} heldout={len(held)} distractors={len(dist)}")


if __name__ == "__main__":
    main()
