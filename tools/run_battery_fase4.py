"""run_battery_fase4: pipe tests/battery_fase4.txt through chat_main and
record one output line per query as TSV (query \\t output).

Usage: python tools/run_battery_fase4.py [--out FILE] [--exe FILE]
Defaults: out=tools/battery_fase4_out.tsv, exe=build-gcc/chat_main.exe,
corpus=data/bible/bible_relations.tsv. Idempotent: same inputs always
rewrite the same bytes (UTF-8, LF).
"""
import csv
import subprocess
import sys

BATTERY = "tests/battery_fase4.txt"
CORPUS = "data/bible/bible_relations.tsv"


def main(argv):
    out_path = "tools/battery_fase4_out.tsv"
    exe = "build-gcc/chat_main.exe"
    battery = BATTERY
    for i, a in enumerate(argv):
        if a == "--out" and i + 1 < len(argv):
            out_path = argv[i + 1]
        if a == "--exe" and i + 1 < len(argv):
            exe = argv[i + 1]
        if a == "--battery" and i + 1 < len(argv):
            battery = argv[i + 1]
    with open(battery, encoding="utf-8") as f:
        queries = [ln.rstrip("\n") for ln in f if ln.strip() != ""]
    blob = ("\n".join(queries) + "\n").encode("utf-8")
    proc = subprocess.run(
        [exe, CORPUS], input=blob, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, timeout=120)
    text = proc.stdout.decode("utf-8", errors="replace")
    lines = text.splitlines()
    # chat_main prints 2 header lines ([chat] corpus..., Listo...),
    # then exactly one line per query (empty inputs are skipped and
    # the battery has none).
    answers = lines[2:]
    if len(answers) < len(queries):
        sys.stderr.write(
            "SHORT: %d answers for %d queries\n" % (len(answers),
                                                    len(queries)))
        sys.stderr.write(text[:2000])
        return 1
    answers = answers[:len(queries)]
    with open(out_path, "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f, delimiter="\t", lineterminator="\n")
        for q, a in zip(queries, answers):
            w.writerow([q, a])
    print("wrote %s (%d rows)" % (out_path, len(queries)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
