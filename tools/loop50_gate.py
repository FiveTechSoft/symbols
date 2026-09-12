"""Puerta LOOP-50 (prioridad del proyecto): el loop completo con 50 hechos
reales no escritos para ningun test.

Fase 1: 50 primeras filas ASCII limpias de data/samples/wikidata_clean.tsv
(filtro mecanico, sin eleccion) -> frases "S R O." -> sesion chat limpia
-> save.
Fase 2: PROCESO NUEVO -> reload -> 20 preguntas mecanicas (filas 0-19,
formas who/what/did/why) -> cada respuesta con prueba citada ([ref]).
Puerta: 20/20 (todo lo ensenado esta almacenado; menos es un bug).
Limpia sus artefactos (el .knowledge.pl temporal de prolog/) siempre.

Uso: python tools/loop50_gate.py  (cwd = raiz del repo)
"""
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PROLOG = ROOT / "prolog"
DATA = ROOT / "data" / "samples" / "wikidata_clean.tsv"

SWIPL = shutil.which("swipl") or r"C:\Program Files\swipl\bin\swipl.exe"
SAVE_NAME = "loop50_gate"


def pick_rows():
    rows = []
    with open(DATA, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            parts = line.split("\t")
            if len(parts) != 3:
                continue
            s, r, o = (p.strip() for p in parts)
            if not (s and r and o):
                continue
            if not re.fullmatch(r"[ -~]+", f"{s} {r} {o}"):
                continue
            if " " in s or " " in r or " " in o:
                continue
            rows.append((s, r, o))
            if len(rows) == 50:
                break
    assert len(rows) == 50, f"solo {len(rows)} filas limpias"
    return rows


def run_chat(args, stdin_text, cwd):
    p = subprocess.run(
        [SWIPL] + args, input=stdin_text, capture_output=True,
        cwd=str(cwd), timeout=300,
        encoding="utf-8", errors="replace",
    )
    out = (p.stdout or "") + (p.stderr or "")
    return p.returncode, out


def main():
    rows = pick_rows()
    print(f"filas reales: {len(rows)} (primera={rows[0]}, ultima={rows[-1]})")
    forms = ["who", "what", "did", "why"]
    qa = []
    for i in range(20):
        s, r, o = rows[i]
        f = forms[i % 4]
        if f == "who":
            qa.append((f"Who {r} {o}?", s.lower()))
        elif f == "what":
            qa.append((f"What did {s} {r}?", o.lower()))
        elif f == "did":
            qa.append((f"Did {s} {r} {o}?", "Yes."))
        else:
            qa.append((f"Why did {s} {r} {o}?",
                       f"{s.lower()} --{r.lower()}--> {o.lower()}"))

    tmp = Path(tempfile.gettempdir()) / "opencode"
    tmp.mkdir(exist_ok=True)
    seed = tmp / "loop50_empty.knowledge.pl"
    seed.write_text("% Semilla vacia para la puerta loop-50.\n",
                    encoding="utf-8")
    saved_kb = PROLOG / f"{SAVE_NAME}.knowledge.pl"
    if saved_kb.exists():
        saved_kb.unlink()

    try:
        teach = "".join(f"{s} {r} {o}.\n" for s, r, o in rows)
        teach += f"save {SAVE_NAME}\nquit\n"
        rc, out1 = run_chat(
            ["-s", "chat.pl", "-g", f"chat('{seed.as_posix()}')",
             "-t", "halt"],
            teach, PROLOG,
        )
        learned = len(re.findall(r"Learned", out1))
        unknown = len(re.findall(r"I don't know", out1))
        saved_ok = f"Saved 50 facts -> {SAVE_NAME}.knowledge.pl" in out1
        print(f"ensenar: learned={learned}/50 unknown={unknown} "
              f"save={'OK' if saved_ok else 'FAIL'}")
        if learned != 50 or not saved_ok or not saved_kb.exists():
            print("LOOP50: FAIL en fase 1");
            return 1

        ask = "".join(q + "\n" for q, _ in qa) + "quit\n"
        rc, out2 = run_chat(
            ["-s", "chat.pl", "-g", f"chat('{SAVE_NAME}.knowledge.pl')",
             "-t", "halt"],
            ask, PROLOG,
        )
        m = re.search(r"Loaded (\d+) facts", out2)
        print(f"recargar: {m.group(0) if m else 'sin Loaded'}")
        pos, fails = 0, []
        for i, (q, exp) in enumerate(qa, 1):
            at = out2.find(exp, pos)
            if at < 0:
                fails.append((i, q, exp, "sin respuesta"))
                continue
            if "[" not in out2[at:at + 400]:
                fails.append((i, q, exp, "sin prueba citada"))
                continue
            pos = at + len(exp)
        print(f"preguntar: {len(qa) - len(fails)}/{len(qa)} con prueba")
        for i, q, exp, why in fails:
            print(f"  FAIL #{i} [{why}]: {q!r} -> {exp!r}")
        ok = not fails
        print(f"LOOP50: {'PASS 20/20' if ok else 'FAIL'}")
        return 0 if ok else 1
    finally:
        if saved_kb.exists():
            saved_kb.unlink()
        if seed.exists():
            seed.unlink()


if __name__ == "__main__":
    sys.exit(main())
