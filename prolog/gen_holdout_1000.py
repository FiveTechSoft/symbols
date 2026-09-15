"""Generate 1000 NL hold-out queries (novel entities, disjoint from existing tests).

Existing test vocab (MUST avoid for hold-out):
  persons: juan maria pedro ana john mary peter anne
  objects: coche casa libro empresa manzana bicicleta car house book phone
  places: madrid barcelona malaga marbella london paris berlin
  colors: rojo azul red blue green yellow

Design: 200 texts (100 ES + 100 EN) x 5 queries (what/who/where/when/color) = 1000.
Text = purchase sentence (with time) + lives sentence (mirrors test_trajectory format).
Emits: holdout_1000_data.pl with test_data(N, Lang, QType, Text, Query, Expected).
"""
import itertools

ES_PERSONS = ["carlos", "lucia", "miguel", "sara", "diego",
              "elena", "pablo", "carmen", "jorge", "rosa"]
# (object, article, adjective) with correct ES agreement (parser doesn't check, but keep clean)
ES_OBJECTS = [("reloj", "un", "negro"), ("mesa", "una", "blanca"),
              ("silla", "una", "gris"), ("perro", "un", "negro"),
              ("gato", "un", "blanco"), ("caballo", "un", "gris"),
              ("barco", "un", "blanco"), ("tren", "un", "negro"),
              ("radio", "una", "gris"), ("espejo", "un", "blanco")]
ES_BUY_PLACES = ["sevilla", "valencia", "bilbao", "granada", "cordoba",
                 "toledo", "zaragoza", "murcia", "leon", "oviedo"]
ES_LIVE_PLACES = ["oviedo", "leon", "murcia", "zaragoza", "toledo",
                  "cordoba", "granada", "bilbao", "valencia", "sevilla"]
ES_TIMES = ["ayer", "2026", "2030", "ayer", "2027",
            "2031", "ayer", "2028", "2032", "ayer"]

EN_PERSONS = ["robert", "lisa", "david", "emma", "james",
              "olivia", "thomas", "sophia", "daniel", "nora"]
EN_OBJECTS = [("watch", "black"), ("table", "white"), ("chair", "gray"),
              ("dog", "black"), ("cat", "white"), ("horse", "gray"),
              ("boat", "white"), ("train", "black"), ("radio", "gray"),
              ("mirror", "white")]
EN_BUY_PLACES = ["rome", "oslo", "dublin", "lisbon", "bern",
                 "vienna", "prague", "athens", "cairo", "lima"]
EN_LIVE_PLACES = ["lima", "cairo", "athens", "prague", "vienna",
                  "bern", "lisbon", "dublin", "oslo", "rome"]
EN_TIMES = ["yesterday", "2026", "2030", "yesterday", "2027",
            "2031", "yesterday", "2028", "2032", "yesterday"]


def esc(s):
    return s.replace('"', '""')


def main():
    facts = []
    n = 0
    # ES: 10 persons x 10 objects = 100 texts
    for i, person in enumerate(ES_PERSONS):
        for j, (obj, art, adj) in enumerate(ES_OBJECTS):
            buy_place = ES_BUY_PLACES[(i + j) % 10]
            live_place = ES_LIVE_PLACES[(i + j) % 10]
            # ensure live != buy
            if live_place == buy_place:
                live_place = ES_LIVE_PLACES[(i + j + 5) % 10]
            time = ES_TIMES[(i + j) % 10]
            text = (f"{person.capitalize()} compro {art} {obj} {adj} "
                    f"en {buy_place.capitalize()} {time}. "
                    f"{person.capitalize()} vive en {live_place.capitalize()}.")
            queries = [
                ("what", f"que compro {person}", obj),
                ("who", f"quien compro el {obj}", person),
                ("where", f"donde compro {person} el {obj}", buy_place),
                ("when", f"cuando compro {person} el {obj}", time),
                ("color", f"de que color es el {obj}", adj),
            ]
            for qtype, q, exp in queries:
                n += 1
                facts.append((n, "es", qtype, text, q, exp))
    # EN: 10 persons x 10 objects = 100 texts
    for i, person in enumerate(EN_PERSONS):
        for j, (obj, adj) in enumerate(EN_OBJECTS):
            buy_place = EN_BUY_PLACES[(i + j) % 10]
            live_place = EN_LIVE_PLACES[(i + j) % 10]
            if live_place == buy_place:
                live_place = EN_LIVE_PLACES[(i + j + 5) % 10]
            time = EN_TIMES[(i + j) % 10]
            text = (f"{person.capitalize()} bought a {adj} {obj} "
                    f"in {buy_place.capitalize()} {time}. "
                    f"{person.capitalize()} lives in {live_place.capitalize()}.")
            queries = [
                ("what", f"what did {person} buy", obj),
                ("who", f"who bought the {obj}", person),
                ("where", f"where did {person} buy the {obj}", buy_place),
                ("when", f"when did {person} buy the {obj}", time),
                ("color", f"what color is the {obj}", adj),
            ]
            for qtype, q, exp in queries:
                n += 1
                facts.append((n, "en", qtype, text, q, exp))
    assert n == 1000, f"expected 1000, got {n}"
    # sanity: all atoms lowercase, no accents, no function-word collisions
    func = {"que", "quien", "donde", "cuando", "como", "no", "para",
            "who", "what", "where", "when", "how", "not"}
    for (i, lang, qt, text, q, exp) in facts:
        assert exp == exp.lower(), i
        assert exp not in func, (i, exp)
    with open("holdout_1000_data.pl", "w", encoding="utf-8") as f:
        f.write("% Auto-generated by gen_holdout_1000.py — 1000 hold-out queries.\n")
        f.write("% Novel entities, disjoint from test_trajectory/eng_test_full/stress_test vocab.\n")
        for (i, lang, qt, text, q, exp) in facts:
            f.write(f'test_data({i}, {lang}, {qt}, "{esc(text)}", "{esc(q)}", {exp}).\n')
    print(f"wrote {n} facts")


if __name__ == "__main__":
    main()
