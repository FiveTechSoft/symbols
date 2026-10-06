#!/usr/bin/env python3
"""Declared mechanical rewrites of a task text, for the wording-robustness probe.

The rewrites are fixed here, before any run, and are not tuned to the agent:
  R1  punctuation-light: lower the first letter of each sentence, drop the
      sentence-ending '.', '!' or '?' and collapse white space. Code-like
      tokens (HELLO_SHELL, run.sh) keep their case and their dots.
  R2  reverse the order of the sentences.
  R3  drop the first sentence (a one-sentence text is kept as it is, never empty).
  R4  keep only the last sentence.
  R5  replace words from the fixed table below, whole words only, keeping an
      initial capital.
A sentence ends at '.', '!' or '?' followed by white space. Nothing here reads
the agent, the bank answers or the check scripts.
"""
import re
import sys

SENT = re.compile(r"(?<=[.!?])\s+")

R5_TABLE = [
    ("must", "needs to"), ("should", "ought to"), ("fix", "repair"),
    ("add", "include"), ("currently", "at the moment"), ("update", "modify"),
    ("change", "alter"), ("replace", "swap"), ("remove", "delete"),
    ("rename", "relabel"), ("exactly", "precisely"), ("prints", "outputs"),
    ("print", "output"), ("because", "since"), ("but", "however"),
    ("make", "cause"), ("only", "just"), ("fails", "breaks"),
]


def sentences(text):
    return [s for s in SENT.split(text.strip()) if s]


def r1(text):
    out = []
    for s in sentences(text):
        s = s.rstrip()
        s = re.sub(r"[.!?]+$", "", s)
        if len(s) > 1 and s[0].isupper() and not s[1].isupper() and "_" not in s.split()[0]:
            s = s[0].lower() + s[1:]
        out.append(s)
    return re.sub(r"\s+", " ", " ".join(out)).strip()


def r2(text):
    return " ".join(reversed(sentences(text)))


def r3(text):
    s = sentences(text)
    return " ".join(s[1:]) if len(s) > 1 else text.strip()


def r4(text):
    s = sentences(text)
    return s[-1] if s else text.strip()


def r5(text):
    for a, b in R5_TABLE:
        def sub(m, b=b):
            w = m.group(0)
            return b[0].upper() + b[1:] if w[0].isupper() else b
        text = re.sub(r"(?<![\w`./-])" + a + r"(?![\w`/-])(?!\.\w)", sub, text, flags=re.I)
    return text


REWRITES = {"R1": r1, "R2": r2, "R3": r3, "R4": r4, "R5": r5}


def apply(name, text):
    return REWRITES[name](text)


def self_test():
    t = "Fix the loop. It must print HELLO_SHELL exactly. run.sh is the file!"
    assert sentences(t) == ["Fix the loop.", "It must print HELLO_SHELL exactly.", "run.sh is the file!"]
    assert r1(t) == "fix the loop it must print HELLO_SHELL exactly run.sh is the file", r1(t)
    assert r2(t) == "run.sh is the file! It must print HELLO_SHELL exactly. Fix the loop."
    assert r3(t) == "It must print HELLO_SHELL exactly. run.sh is the file!"
    assert r4(t) == "run.sh is the file!"
    assert r3("One sentence.") == "One sentence." and r4("One sentence.") == "One sentence."
    assert r5(t) == "Repair the loop. It needs to output HELLO_SHELL precisely. run.sh is the file!", r5(t)
    assert r5("Run `fix.sh` then fix it.") == "Run `fix.sh` then repair it."
    for n in REWRITES:
        assert apply(n, "") == ""
    print("wording_rewrites self-test ok")


if __name__ == "__main__":
    if sys.argv[1:] == ["--self-test"]:
        self_test()
    else:
        print("usage: wording_rewrites.py --self-test", file=sys.stderr)
        sys.exit(2)
