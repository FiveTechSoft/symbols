import csv, json

battery = json.load(open("battery.json", encoding="utf-8"))
layers = {q["id"].lower(): q["layer"] for q in battery["questions"]}
rows = {}
for row in csv.reader(open("bins_prolog.txt")):
    rows[(row[1], "prolog" if row[0] == "p" else "tinyllama")] = row[2]
agg = {}
for (qid, sysname), b in rows.items():
    layer = layers[qid]
    agg.setdefault((layer, sysname), {"correct": 0, "partial": 0, "wrong": 0, "unknown": 0})[b] += 1
order = ["retrieval", "comprension", "composicion", "desconocido", "mundo_abierto", "social", "generacion", "meta"]
hdr = f"{'layer':<14} {'sys':<10} C  P  W  U   n"
print(hdr)
for layer in order:
    for sysname in ("prolog", "tinyllama"):
        d = agg.get((layer, sysname))
        if d:
            n = sum(d.values())
            print(f"{layer:<14} {sysname:<10} {d['correct']:>2} {d['partial']:>2} {d['wrong']:>2} {d['unknown']:>2}  {n}")