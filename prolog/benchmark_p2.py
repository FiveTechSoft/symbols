#!/usr/bin/env python3
"""benchmark_p2.py — Measure P2 canonicalization on real Alice text."""
import subprocess
import re

SWIPL = r"C:\Program Files\swipl\bin\swipl.exe"
WORKDIR = r"C:\symbols\prolog"

QUESTIONS = [
    ("who is alice?", "alice"),
    ("what is alice?", "book"),
    ("who wrote alice?", "carroll"),
    ("what did alice follow?", "rabbit"),
    ("who did alice meet?", "hatter"),
    ("what did alice eat?", "cake"),
    ("what did alice drink?", "bottle"),
    ("who smoked hookah?", "caterpillar"),
    ("who has a grin?", "cheshire"),
    ("who hosted tea party?", "hatter"),
    ("who plays croquet?", "queen"),
    ("who threatens alice?", "queen"),
    ("who judges knave?", "king"),
    ("what did gardeners paint?", "roses"),
    ("who sings quadrille?", "mock_turtle"),
    ("who led alice?", "gryphon"),
    ("who attended trial?", "hatter"),
    ("who woke alice?", "sister"),
    ("who reads book?", "sister"),
    ("who carries watch?", "rabbit"),
    ("who wears waistcoat?", "rabbit"),
    ("who dropped fan?", "rabbit"),
    ("who reads accusation?", "rabbit"),
    ("who advised alice?", "caterpillar"),
    ("who turned pig?", "baby"),
    ("who nurses baby?", "duchess"),
    ("who offered wine?", "hare"),
    ("who attends tea party?", "dormouse"),
    ("who told story?", "dormouse"),
    ("who waved fan?", "alice"),
    ("who found key?", "alice"),
    ("who entered garden?", "alice"),
    ("who defied court?", "alice"),
    ("who played croquet?", "alice"),
    ("who held flamingo?", "alice"),
    ("who recited poem?", "alice"),
    ("who swam pool?", "alice"),
    ("who peeped book?", "alice"),
    ("who has sister?", "alice"),
    ("is alice a character?", "yes"),
    ("is the hatter mad?", "yes"),
    ("does the cat grin?", "yes"),
    ("is there a queen?", "yes"),
    ("what book is this?", "alice"),
    ("where is wonderland?", "down"),
    ("who painted roses?", "gardeners"),
    ("what did alice find?", "key"),
    ("who is humpty dumpty?", "No"),
    ("who is the author?", "carroll"),
    ("what did alice enter?", "garden"),
]

def run_prolog(script):
    proc = subprocess.run(
        [SWIPL, "-g", script, "-t", "halt"],
        cwd=WORKDIR, timeout=120,
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT
    )
    return proc.stdout.decode('latin-1', errors='replace')

def main():
    print("=" * 60)
    print("P2 BENCHMARK: Alice in Wonderland (Gutenberg)")
    print("=" * 60)

    # Step 1: Process Alice
    print("\n--- Processing Alice through bookbrain ---")
    out = run_prolog("""
consult('bookbrain.pl'), consult('positional.pl'),
consult('open_vocab.pl'), consult('chat.pl'), consult('corpus.pl'),
bookbrain('alice_gutenberg.txt', alice_auto).
""")
    m = re.search(r'facts=(\d+)', out)
    n_facts = int(m.group(1)) if m else 0
    print(f"  Facts extracted: {n_facts}")

    # Step 2: Test questions one by one
    print("\n--- Running 50 questions ---")
    passes = 0
    fails = 0
    results = []

    for q, expected in QUESTIONS:
        out = run_prolog(f"""
consult('chat.pl'), consult('bookbrain.pl'),
consult('positional.pl'), consult('open_vocab.pl'), consult('corpus.pl'),
bb_load('alice_auto.knowledge.pl'),
chat_line('{q}').
""")
        # Clean output - keep only answer lines
        lines = []
        for l in out.split('\n'):
            l = l.strip()
            if not l: continue
            if any(skip in l for skip in ['Warning:', 'GAP', 'Loaded', 'BOOKBRAIN',
                                           'CONFLICT', 'link_grammar', 'Singleton',
                                           'Redefined', 'Unknown procedure', ' chatte']):
                continue
            lines.append(l)
        answer = ' '.join(lines).strip()

        if expected == "No":
            ok = "No." in answer or answer.strip() == "No"
        elif expected == "UNKNOWN":
            ok = "No." in answer or "No " in answer or "know" in answer.lower()
        else:
            ok = expected.lower() in answer.lower()

        if ok:
            passes += 1
            status = "PASS"
        else:
            fails += 1
            status = "FAIL"

        results.append((q, expected, answer[:60], status))
        print(f"  [{status}] {q} -> expected '{expected}' got '{answer[:45]}'")

    # Summary
    total = passes + fails
    acc = passes / total * 100 if total > 0 else 0

    print(f"\n{'=' * 60}")
    print(f"SUMMARY")
    print(f"{'=' * 60}")
    print(f"  Facts extracted: {n_facts}")
    print(f"  Questions: {total}")
    print(f"  Correct: {passes}/{total} ({acc:.1f}%)")
    print(f"  Wrong: {fails}")
    print(f"{'=' * 60}")

    with open("benchmark_p2_results.txt", "w") as f:
        f.write(f"P2 BENCHMARK RESULTS\n")
        f.write(f"Facts: {n_facts}\n")
        f.write(f"Accuracy: {passes}/{total} ({acc:.1f}%)\n\n")
        for q, exp, ans, st in results:
            f.write(f"[{st}] {q} -> expected '{exp}' got '{ans}'\n")
    print(f"\nResults saved to benchmark_p2_results.txt")

if __name__ == "__main__":
    main()
