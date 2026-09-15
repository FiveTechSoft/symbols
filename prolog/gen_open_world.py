"""EXP-25 open-generalization world generator.

Worlds are DATA (single dicts below): persons, objects (+color/material),
places, institutions, types, links. Ground truth is COMPUTED from the world
model; only (Text, Query, Expected) triples are emitted. The world model
itself never enters symbols/*.

Usage: python gen_open_world.py  ->  open_world_data.pl
Layout: test_data(N, Group, Episode, Text, Query, Expected).
  Group in {vocab,attr,type,multihop,compositional,adversarial}.
  Expected is a lowercase atom or the atom unknown.
"""

# ---------------------------------------------------------------- worlds
# Anchors honored from the EXP-25 spec: Nora/bronze lantern/Prague/museum,
# Victor/wooden chest/Kyoto, Sara/violet sculpture/Lima, horse/map,
# artifact/container/artwork/object chains, institution, NOT-a-container,
# fronted/passive probes. World-B second storyline (compass/Seoul/archive)
# keeps cross-episode tests well-defined; all of it is data, one edit away.
WORLDS = {
    "A": {
        "persons": ["nora", "liam"],
        "restore": [("nora", "lantern", "bronze", "prague"),
                    ("liam", "kettle", "copper", "cairo")],
        "belongs": [("lantern", "museum")],
        "places": {"lantern": "prague", "kettle": "cairo"},
        "types": [("lantern", "artifact"), ("artifact", "object"),
                  ("museum", "institution"), ("kettle", None)],
        "materials": [],
        "extra": ["Liam built a stone wall."],
        "extra_material": [("wall", "stone")],
    },
    "B": {
        "persons": ["victor", "mia"],
        "carry": [("victor", "chest", "wooden", "kyoto"),
                  ("mia", "compass", "indigo", "seoul")],
        "belongs": [("chest", "archive")],
        "contains": [("chest", "map")],
        "places": {"chest": "kyoto", "compass": "seoul"},
        "types": [("chest", "container"), ("container", "object"),
                  ("archive", "institution"),
                  ("compass", "instrument"), ("instrument", "object")],
        "materials": [("chest", "wooden")],
        "extra": [],
        "extra_material": [],
    },
    "C": {
        "persons": ["sara", "omar"],
        "paint": [("sara", "sculpture", "violet", "lima"),
                  ("omar", "fence", "beige", "roma")],
        "depicts": [("sculpture", "horse")],
        "places": {"sculpture": "lima", "fence": "roma"},
        "types": [("sculpture", "artwork"), ("artwork", "object")],
        "materials": [],
        "extra": ["A lantern is NOT a container."],
        "extra_material": [],
    },
}

CAP = str.capitalize


def s_restore(p, o, c, pl):
    return f"{CAP(p)} restored a {c} {o} in {CAP(pl)}."


def s_carry(p, o, c, pl):
    return f"{CAP(p)} carried a {c} {o} in {CAP(pl)}."


def s_paint(p, o, c, pl):
    return f"{CAP(p)} painted a {c} {o} in {CAP(pl)}."


def main():
    rows = []  # (group, episode, text, query, expected)

    def add(group, ep, text, query, exp):
        assert exp == exp.lower(), exp
        rows.append((group, ep, text, query, exp))

    # ---- episode texts (multi-sentence, restore/carry/paint sentence first
    #      so frozen first-main coreference resolves "it" to the object).
    TA = ("Nora restored a bronze lantern in Prague. "
          "Liam restored a copper kettle in Cairo. "
          "The lantern belongs to a museum. "
          "A museum is an institution. "
          "A lantern is an artifact. "
          "An artifact is an object. "
          "Liam built a stone wall.")
    TB = ("Victor carried a wooden chest in Kyoto. "
          "Mia carried an indigo compass in Seoul. "
          "The chest belongs to an archive. "
          "An archive is an institution. "
          "A chest is a container. "
          "A container is an object. "
          "A compass is an instrument. "
          "An instrument is an object. "
          "The chest contains a map.")
    TC = ("Sara painted a violet sculpture in Lima. "
          "Omar painted a beige fence in Roma. "
          "Tomas carved a turquoise flute in Oslo. "
          "The sculpture depicts a horse. "
          "A sculpture is an artwork. "
          "An artwork is an object. "
          "A lantern is NOT a container.")
    TF = "In Prague, Nora restored a bronze lantern."
    TP = "A bronze lantern was restored by Nora in Prague."

    # ---- VOCAB (novel words, known structures) ----
    add("vocab", "A", TA, "what did nora restore", "lantern")
    add("vocab", "A", TA, "what did liam restore", "kettle")
    add("vocab", "A", TA, "who restored the lantern", "nora")
    add("vocab", "A", TA, "who restored the kettle", "liam")
    add("vocab", "A", TA, "where did nora restore the lantern", "prague")
    add("vocab", "A", TA, "where did liam restore the kettle", "cairo")
    add("vocab", "A", TA, "what color is the kettle", "copper")
    add("vocab", "A", TA, "what material is the wall", "stone")
    add("vocab", "A", TA, "what did liam build", "wall")
    add("vocab", "A", TA, "what type of thing is the museum", "institution")
    add("vocab", "B", TB, "what did victor carry", "chest")
    add("vocab", "B", TB, "what did mia carry", "compass")
    add("vocab", "B", TB, "who carried the chest", "victor")
    add("vocab", "B", TB, "who carried the compass", "mia")
    add("vocab", "B", TB, "where did victor carry the chest", "kyoto")
    add("vocab", "B", TB, "where did mia carry the compass", "seoul")
    add("vocab", "B", TB, "what color is the compass", "indigo")
    add("vocab", "B", TB, "what does the chest contain", "map")
    add("vocab", "B", TB, "what type of thing is the archive", "institution")
    add("vocab", "C", TC, "what did sara paint", "sculpture")
    add("vocab", "C", TC, "what did omar paint", "fence")
    add("vocab", "C", TC, "what did tomas carve", "flute")
    add("vocab", "C", TC, "where did sara paint the sculpture", "lima")

    # ---- ATTR (color vs material slot discipline) ----
    add("attr", "A", TA, "what color is the lantern", "bronze")
    add("attr", "A", TA, "what color is the kettle", "copper")
    add("attr", "A", TA, "what material is the wall", "stone")
    add("attr", "A", TA, "what color is the wall", "unknown")
    add("attr", "A", TA, "what material is the lantern", "unknown")
    add("attr", "A", TA, "what material is the kettle", "unknown")
    add("attr", "B", TB, "what color is the compass", "indigo")
    add("attr", "B", TB, "what material is the chest", "wooden")
    add("attr", "B", TB, "what color is the chest", "unknown")
    add("attr", "B", TB, "what material is the compass", "unknown")
    add("attr", "B", TB, "what color is the map", "unknown")
    add("attr", "B", TB, "what color is the archive", "unknown")
    add("attr", "C", TC, "what color is the sculpture", "violet")
    add("attr", "C", TC, "what color is the fence", "beige")
    add("attr", "C", TC, "what material is the sculpture", "unknown")
    add("attr", "C", TC, "what color is the horse", "unknown")
    add("attr", "A", TA, "what color is the museum", "unknown")
    add("attr", "B", TB, "what material is the museum", "unknown")
    add("attr", "C", TC, "what material is the fence", "unknown")
    add("attr", "A", TA, "what color is the institution", "unknown")
    add("attr", "C", TC, "what color is the flute", "turquoise")

    # ---- TYPE (direct, chains, kind, negation) ----
    add("type", "A", TA, "what type of thing is the lantern", "artifact")
    add("type", "A", TA, "what type of thing is the museum", "institution")
    add("type", "A", TA, "what type of thing is the kettle", "unknown")
    add("type", "A", TA, "what type of thing is the wall", "unknown")
    add("type", "B", TB, "what type of thing is the chest", "container")
    add("type", "B", TB, "what type of thing is the archive", "institution")
    add("type", "B", TB, "what type of thing is the compass", "instrument")
    add("type", "B", TB, "what type of thing is the map", "unknown")
    add("type", "B", TB, "what type of thing is the horse", "unknown")
    add("type", "C", TC, "what type of thing is the sculpture", "artwork")
    add("type", "C", TC, "what kind of object is the sculpture", "object")
    add("type", "A", TA, "what kind of object is the lantern", "object")
    add("type", "B", TB, "what kind of object is the chest", "object")
    add("type", "C", TC, "is the lantern a container", "no_evidence")
    add("type", "C", TC, "is the lantern a vehicle", "no_evidence")
    add("type", "B", TB, "is the chest an object", "yes")
    add("type", "A", TA, "what is an artifact", "lantern")
    add("type", "A", TA, "what is an institution", "museum")
    add("type", "B", TB, "what is a container", "chest")
    add("type", "C", TC, "what is an artwork", "sculpture")
    add("type", "A", TA, "what is an institution", "museum")

    # ---- MULTIHOP (>=2 learned hops; kind/object chains) ----
    add("multihop", "A", TA, "what kind of object is the museum", "institution")
    add("multihop", "A", TA, "who restored the artifact", "nora")
    add("multihop", "B", TB, "what kind of object is the archive", "institution")
    add("multihop", "B", TB, "who carried the container", "victor")
    add("multihop", "C", TC, "what kind of object is the horse", "unknown")
    add("multihop", "A", TA, "what type of thing is the museum of the lantern", "institution")
    add("multihop", "B", TB, "what type of thing is the archive of the chest", "institution")
    add("multihop", "C", TC, "what type of thing is the horse of the sculpture", "unknown")
    add("multihop", "A", TA, "what color was the artifact nora restored", "bronze")
    add("multihop", "B", TB, "what color was the container victor carried", "unknown")
    add("multihop", "A", TA, "where is the museum", "unknown")
    add("multihop", "B", TB, "where is the archive", "unknown")
    add("multihop", "C", TC, "what kind of object is the fence", "unknown")
    add("multihop", "A", TA, "what kind of object is the kettle", "unknown")
    add("multihop", "B", TB, "what kind of object is the compass", "object")
    add("multihop", "C", TC, "what kind of object is the horse", "unknown")
    add("multihop", "A", TA, "what type of thing is the artifact", "object")
    add("multihop", "B", TB, "what type of thing is the container", "object")
    add("multihop", "C", TC, "what type of thing is the artwork", "object")
    add("multihop", "B", TB, "what type of thing is the instrument", "object")

    # ---- COMPOSITIONAL (episode bundles: relation+attr+type+location) ----
    add("compositional", "A", TA, "what did nora restore", "lantern")
    add("compositional", "A", TA, "what color was it", "bronze")
    add("compositional", "A", TA, "where did she restore it", "prague")
    add("compositional", "A", TA, "what type of thing is the lantern", "artifact")
    add("compositional", "A", TA, "what is an artifact", "lantern")
    add("compositional", "B", TB, "what did victor carry", "chest")
    add("compositional", "B", TB, "what material was it", "wooden")
    add("compositional", "B", TB, "where did he carry it", "kyoto")
    add("compositional", "B", TB, "what type of thing is the chest", "container")
    add("compositional", "B", TB, "what is a container", "chest")
    add("compositional", "C", TC, "what did sara paint", "sculpture")
    add("compositional", "C", TC, "what color was it", "violet")
    add("compositional", "C", TC, "where did she paint it", "lima")
    add("compositional", "C", TC, "what type of thing is the sculpture", "artwork")
    add("compositional", "C", TC, "what is an artwork", "sculpture")

    # ---- ADVERSARIAL (impossible queries, cross-episode, probes) ----
    add("adversarial", "AB", TA + " " + TB, "what did nora carry", "unknown")
    add("adversarial", "AB", TA + " " + TB, "what did mia restore", "unknown")
    add("adversarial", "AB", TA + " " + TB, "what color was victor's lantern", "unknown")
    add("adversarial", "AB", TA + " " + TB, "what color was nora's compass", "unknown")
    add("adversarial", "AB", TA + " " + TB, "who restored the compass", "unknown")
    add("adversarial", "AB", TA + " " + TB, "what type of thing is the kettle", "unknown")
    add("adversarial", "AB", TA + " " + TB, "where did victor restore the chest", "unknown")
    add("adversarial", "AB", TA + " " + TB, "what color was nora's lantern", "bronze")
    add("adversarial", "AB", TA + " " + TB, "what material was victor's chest", "wooden")
    add("adversarial", "AB", TA + " " + TB, "is the compass a container", "no_evidence")
    add("adversarial", "F", TF, "what did nora restore", "unknown")
    add("adversarial", "F", TF, "where did nora restore the lantern", "unknown")
    add("adversarial", "P", TP, "what did nora restore", "unknown")
    add("adversarial", "P", TP, "what color is the lantern", "unknown")
    add("adversarial", "C", TC, "what did sara restore", "unknown")
    add("adversarial", "C", TC, "what color was omar's sculpture", "unknown")
    add("adversarial", "B", TB, "what did victor restore", "unknown")
    add("adversarial", "A", TA, "what did liam carry", "unknown")
    add("adversarial", "B", TB, "what material is the map", "unknown")
    add("adversarial", "C", TC, "what type of thing is the fence", "unknown")

    # ---- sanity: counts per group ----
    from collections import Counter
    c = Counter(g for g, _, _, _, _ in rows)
    print("groups:", dict(c), "total:", len(rows))
    assert len(rows) >= 100, len(rows)

    def esc(s):
        return s.replace('"', '""')

    with open("open_world_data.pl", "w", encoding="utf-8") as f:
        f.write("% Auto-generated by gen_open_world.py — EXP-25 open generalization.\n")
        f.write("% Ground truth computed from the world model; the model itself is NOT loaded.\n")
        for i, (g, ep, text, q, exp) in enumerate(rows, 1):
            f.write(f'test_data({i}, {g}, "{ep}", "{esc(text)}", "{esc(q)}", {exp}).\n')
    print("wrote", len(rows), "items")


if __name__ == "__main__":
    main()
