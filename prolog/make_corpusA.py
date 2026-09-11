#!/usr/bin/env python3
"""Corpus A (~234 frases, lenguaje natural controlado, tiempo presente).
3 familias composicionales (24 cadenas c/u, 2 conclusiones ocultas c/u):
  FA lives_in/belongs_to/comes_from (personas/ciudades/paises)
  FB works_in/produces/provides     (personas/ciudades/bienes)
  FC cooks/needs/uses               (personas/platos/ingredientes)
Ruido owns/likes (24 hechos, sin conclusiones).
Bloques a1/a2/a3 (~78 c/u). Secreto aparte (NO se ingiere al aprender).
Determinista (semilla fija).
"""
import random

random.seed(20260914)

PERSONS = ["ana", "leo", "mia", "juan", "pedro", "lucia", "marco",
           "elena", "david", "sara", "jorge", "laura", "diego", "ines",
           "raul", "nadia", "ivan", "vera", "hugo", "clara", "pablo",
           "carmen", "luis", "alba"]
CITIES = ["madrid", "paris", "roma", "oslo", "quito", "lima",
          "dublin", "bern", "lisboa", "atenea", "napoles", "turin",
          "oporto", "niza", "gante", "brujas", "sevilla", "granada",
          "cordoba", "bilbao", "valencia", "zaragoza", "malaga",
          "santander"]
COUNTRIES = ["spain", "france", "italy", "norway",
             "ecuador", "peru", "ireland", "switzerland"]
GOODS = ["wine", "cheese", "steel", "glass",
         "paper", "silk", "coffee", "honey"]
DISHES = ["paella", "gazpacho", "tortilla", "fabada",
          "cocido", "migas", "pisto", "ajoblanco", "empanada", "flan",
          "churros", "salmorejo", "cachopo", "pulpo", "merluza",
          "bacalao", "alcachofa", "cardos", "zarzuela", "suquet",
          "escalivada", "fideua", "arroz", "caldero"]
INGS = ["rice", "tomato", "egg", "beans",
        "pork", "bread", "pepper", "garlic"]
NOUNS = ["bike", "house", "boat", "garden", "car", "dog", "book",
         "guitar"]

FA_P = PERSONS[0:24]
FA_C = CITIES[0:24]
# IMPORTANT: rotated pairings per family, or lives_in/works_in become
# perfectly aliased (same person->same city) and discovery rightly ties.
FB_P = PERSONS[0:24]
FB_C = [CITIES[(i + 7) % 24] for i in range(24)]
FC_P = PERSONS[0:24]
FC_D = [DISHES[(i + 5) % 24] for i in range(24)]


def chain_block(n, ppool, mpool, epool, t1, t2, t3, hidden_idx):
    ss, hidden, dist = [], [], []
    for i in range(n):
        p = ppool[i % len(ppool)]
        m = mpool[i % len(mpool)]
        e = epool[i % len(epool)]
        ss.append(t1.format(p=p, m=m))
        ss.append(t2.format(m=m, e=e))
        if i in hidden_idx:
            hidden.append((p, e))
        else:
            ss.append(t3.format(p=p, e=e))
    for i in sorted(hidden_idx):
        p = ppool[i % len(ppool)]
        others = [e for j, e in enumerate(epool) if e != epool[i % len(epool)]]
        dist.append((p, others[i % len(others)]))
    return ss, hidden, dist


def main():
    HID = {5, 17}
    a1, h1, d1 = chain_block(
        24, FA_P, FA_C, COUNTRIES,
        "{p} lives in {m}.", "{m} belongs to {e}.", "{p} comes from {e}.",
        HID)
    a2, h2, d2 = chain_block(
        24, FB_P, FB_C, GOODS,
        "{p} works in {m}.", "{m} produces {e}.", "{p} provides {e}.",
        HID)
    a3, h3, d3 = chain_block(
        24, FC_P, FC_D, INGS,
        "{p} cooks {m}.", "{m} needs {e}.", "{p} uses {e}.",
        HID)
    noise = []
    for i in range(12):
        p = PERSONS[(i * 7) % len(PERSONS)]
        o = NOUNS[i % len(NOUNS)]
        noise.append(f"{p} owns {o}.")
    for i in range(12):
        p = PERSONS[(i * 11 + 3) % len(PERSONS)]
        o = NOUNS[(i * 5 + 1) % len(NOUNS)]
        noise.append(f"{p} likes {o}.")
    # secreto: personas Y ciudades frescas (nadie las vio); objetos ancla
    # conocidos (spain/steel/pork) para probar ligadura cruzada.
    secret = [
        ("zara", "avila", "spain"),
        ("yago", "cuenca", "steel"),
        ("teo", "jaen", "pork"),
    ]
    sec_sents = []
    sec_sents.append("zara lives in avila.")
    sec_sents.append("avila belongs to spain.")
    sec_sents.append("yago works in cuenca.")
    sec_sents.append("cuenca produces steel.")
    sec_sents.append("teo cooks jaen.")
    sec_sents.append("jaen needs pork.")
    with open("corpusA/a1.txt", "w", encoding="utf-8") as fh:
        fh.write("\n".join(a1 + noise[0:8]) + "\n")
    with open("corpusA/a2.txt", "w", encoding="utf-8") as fh:
        fh.write("\n".join(a2 + noise[8:16]) + "\n")
    with open("corpusA/a3.txt", "w", encoding="utf-8") as fh:
        fh.write("\n".join(a3 + noise[16:24]) + "\n")
    with open("secret/secret_text.txt", "w", encoding="utf-8") as fh:
        fh.write("\n".join(sec_sents) + "\n")
    with open("secret/secret_expected.pl", "w", encoding="utf-8") as fh:
        fh.write("secret_expects(zara, spain, comes_from).\n")
        fh.write("secret_expects(yago, steel, provides).\n")
        fh.write("secret_expects(teo, pork, uses).\n")
        fh.write("secret_unknown(zorin).\n")
        fh.write("secret_no(ana, paris).\n")
    with open("heldoutA.pl", "w", encoding="utf-8") as fh:
        for p, e in h1:
            fh.write(f"heldout({p}, {e}, comes_from).\n")
        for p, e in h2:
            fh.write(f"heldout({p}, {e}, provides).\n")
        for p, e in h3:
            fh.write(f"heldout({p}, {e}, uses).\n")
    # distractores PROBADOS falsos: pools 1:1 por cadena + offset 3 mod 8
    # (nunca igual al verdadero; sin ruta alternativa posible).
    with open("distractorA.pl", "w", encoding="utf-8") as fh:
        for p, e in h1:
            fh.write(f"distractor({p}, {COUNTRIES[(COUNTRIES.index(e) + 3) % 8]}, comes_from).\n")
        for p, e in h2:
            fh.write(f"distractor({p}, {GOODS[(GOODS.index(e) + 3) % 8]}, provides).\n")
        for p, e in h3:
            fh.write(f"distractor({p}, {INGS[(INGS.index(e) + 3) % 8]}, uses).\n")
    n = len(a1) + len(a2) + len(a3) + 24
    print(f"blocks={[len(a1) + 8, len(a2) + 8, len(a3) + 8]} "
          f"total={n} hidden={len(h1) + len(h2) + len(h3)} "
          f"distractors={len(d1) + len(d2) + len(d3)}")


if __name__ == "__main__":
    main()
