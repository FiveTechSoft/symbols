import json, re, sys

battery = json.load(open("battery.json", encoding="utf-8"))
IDK = battery["meta"]["idk_tokens"]

def answers(path):
    rows = []
    for line in open(path, encoding="utf-8"):
        m = re.match(r"(Q\d+|[AB]\d\d) \|\|\| (.*?) \|\|\| (.*)", line.rstrip("\n"))
        if m:
            tag = m.group(1)
            if tag.startswith("Q"):
                n = int(tag[1:])
                tag = f"A{n:02d}" if n <= 20 else f"B{n-20:02d}"
            rows.append((tag, m.group(3).strip()))
    return dict(rows)

def words(t):
    return re.findall(r"[a-z0-9]+", t.lower())

def bin_for(qid, ans, qs):
    spec = next(x for x in battery["questions"] if x["id"] == qid)
    ws = set(words(ans))
    groups = spec["key"]
    minc = spec.get("min_count")
    if groups:
        g_res = []
        for g in groups:
            if any(w in ws for w in g):
                g_res.append(True)
            elif minc is not None:
                c = sum(1 for w in ws if w in g)
                g_res.append(c > 0)
            else:
                g_res.append(False)
        if minc is not None and len(groups) == 1:
            c = sum(1 for w in ws if w in groups[0])
            if c >= minc: return "correct"
            if c > 0: return "partial"
        elif all(g_res):
            return "correct"
        elif any(g_res) and len(groups) > 1:
            return "partial"
    if any(w in ws for w in IDK):
        return "unknown"
    if not groups:
        return "wrong"
    return "wrong"

def main():
    pro = answers("gap_prolog_raw.txt")
    tin = answers("gap_tiny_raw.txt")
    tally = {"prolog": {}, "tinyllama": {}}
    detail = []
    for spec in battery["questions"]:
        qid = spec["id"]
        for sys_, src in (("prolog", pro), ("tinyllama", tin)):
            b = bin_for(qid, src[qid], spec)
            idk_ok = spec.get("idk_correct", False)
            credit = 1 if b == "correct" else (0.5 if b == "partial" else (1 if (b == "unknown" and idk_ok) else 0))
            tally[sys_][b] = tally[sys_].get(b, 0) + 1
            tally[sys_]["_score"] = tally[sys_].get("_score", 0.0) + credit
            detail.append(f"{qid},{sys_},{b},{credit}")
    for sys_ in ("prolog", "tinyllama"):
        t = tally[sys_]
        print(f"{sys_}: correct={t.get('correct',0)} partial={t.get('partial',0)} wrong={t.get('wrong',0)} unknown={t.get('unknown',0)} score={t['_score']:.1f}/40")
    open("bins_python.txt", "w").write("\n".join(detail))
    for tier, ids in (("A", [f"A{i:02d}" for i in range(1, 21)]), ("B", [f"B{i:02d}" for i in range(1, 21)])):
        for sys_ in ("prolog", "tinyllama"):
            d = {r.split(",")[0]: (r.split(",")[2], r.split(",")[3]) for r in detail if r.split(",")[1] == sys_}
            s = sum(float(v[1]) for k, v in d.items() if k in ids)
            print(f"  tier{tier} {sys_}: {s:.1f}/20")
main()