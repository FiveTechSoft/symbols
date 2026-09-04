import csv
import random

random.seed(42)

subjects = [f"entity_{i}" for i in range(2000)]
predicates = [f"relates_to", "is_a", "part_of", "located_in", "has_property",
              "was_created_by", "uses", "depends_on", "generates", "contains",
              "precedes", "causes", "requires", "influences", "produces"]
objects = [f"concept_{i}" for i in range(2000)]

freq_subjects = subjects[:100]   # Zipf: 100 entities appear very often
freq_predicates = predicates[:5]
freq_objects = objects[:100]

def pick_weighted(freq_list, full_list, p_freq=0.9):
    if random.random() < p_freq:
        return random.choice(freq_list)
    return random.choice(full_list)

lines = []
for _ in range(500000):
    s = pick_weighted(freq_subjects, subjects)
    p = pick_weighted(freq_predicates, predicates)
    o = pick_weighted(freq_objects, objects)
    lines.append((s, p, o))

# Add 50k low-frequency "noise" relations (appear once each)
for i in range(50000):
    s = f"rare_noise_{i}"
    p = random.choice(predicates)
    o = random.choice(objects)
    lines.append((s, p, o))

random.shuffle(lines)

with open("C:/symbols/tools/test_corpus.tsv", "w", encoding="utf-8") as f:
    for s, p, o in lines:
        f.write(f"{s}\t{p}\t{o}\n")

print(f"Generated {len(lines)} triples")
