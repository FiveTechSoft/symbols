#!/usr/bin/env python3
"""Snapshot de progreso hacia el objetivo (QA en conocimiento ruidoso).

Mide los 4 numeros independientes y los anexa a tools/progress.csv:
  fecha, commit, suite_pass, suite_total, eval_pass, eval_total,
  hygiene_pass(1/0), lint_rows, lint_fixed, lint_dropped

Uso: python3 tools/progress.py   (desde la raiz del repo)
"""
import csv
import subprocess
import sys
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import lint_corpus


def run(cmd):
    p = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True,
                       encoding="utf-8", errors="replace")
    return p.returncode, (p.stdout or "") + (p.stderr or "")


def find_build():
    """Detecta el directorio de build y el sufijo de binario.
    Sonda en orden: build-gcc/*.exe (historico Windows), build/* (Linux/CI)."""
    for nombre, sufijo in (("build-gcc", ".exe"), ("build", "")):
        p = ROOT / nombre
        if (p / ("test_eval_qa" + sufijo)).is_file():
            return p, sufijo
    sys.exit("ERROR: no hay binarios de test. Compilar: cmake -S . -B build "
             "&& cmake --build build")


def binario(build, suf, name):
    return str(build / (name + suf))


def main():
    build, suf = find_build()
    # 1. suite
    import re as _re
    rc, out = run(["ctest", "--test-dir", str(build)])
    suite_pass = suite_total = 0
    m = _re.search(r"(\d+) tests failed out of (\d+)", out)
    if m:
        # "96% tests passed, 1 tests failed out of 26"
        suite_total = int(m.group(2))
        suite_pass = suite_total - int(m.group(1))
    # 2. eval + hygiene (binarios sueltos, no ctest: queremos el numero)
    rc, out = run([binario(build, suf, "test_eval_qa"),
                   "tests/qa_eval.tsv", "wiki_model.bin"])
    eval_pass = eval_total = 0
    for line in out.splitlines():
        if line.startswith("QA eval:"):
            # "QA eval: 99/99 = 100.0% (umbral 90%)"
            frac = line.split()[2].split("=")[0]
            eval_pass, eval_total = map(int, frac.split("/"))
    rc_h, _ = run([binario(build, suf, "test_qa_hygiene"),
                   "tests/qa_eval.tsv", "wiki_model.bin"])
    # 2b. hard aspiracional (sin umbral: el progreso es verlo subir).
    # El exit code sera != 0 mientras falle: solo se parsea el numero.
    hard_pass = hard_total = 0
    _, out_h = run([binario(build, suf, "test_eval_qa"),
                    "tests/qa_eval_hard.tsv", "wiki_model.bin"])
    for line in out_h.splitlines():
        if line.startswith("QA eval:"):
            frac = line.split()[2].split("=")[0]
            hard_pass, hard_total = map(int, frac.split("/"))
    # 3. lint (reusa lint_file: mismo codigo que el gate de CI)
    rows = fixed = dropped = 0
    for fn in lint_corpus.FILES:
        stats, _, _, _ = lint_corpus.lint_file(ROOT / fn)
        rows += stats["rows"]
        fixed += stats["fixed"]
        dropped += stats["dropped"]
    # 4. cobertura del modelo (P1): el binario corre el corpus en
    # memoria (salida a scratch bin, wiki_model.bin intacto desde
    # el aislamiento de 69f8d54); leer su TOTAL es seguro y sin efectos.
    import re as _re2
    model_rels = model_syms = 0
    rc, out = run([binario(build, suf, "test_wikidata_ingest")])
    m = _re2.search(r"TOTAL:\s*(\d+)\s+relations,\s*(\d+)\s+symbols", out)
    if m:
        model_rels, model_syms = int(m.group(1)), int(m.group(2))
    # 5. commit
    rc, commit = run(["git", "rev-parse", "--short", "HEAD"])
    commit = commit.strip() or "n/a"

    log = ROOT / "tools" / "progress.csv"
    is_new = not log.exists()
    with open(log, "a", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        if is_new:
            w.writerow(["fecha", "commit", "suite_pass", "suite_total",
                        "eval_pass", "eval_total", "hygiene_ok",
                        "lint_rows", "lint_fixed", "lint_dropped",
                        "model_relations", "model_symbols",
                        "hard_pass", "hard_total"])
        w.writerow([date.today().isoformat(), commit, suite_pass,
                    suite_total, eval_pass, eval_total,
                    1 if rc_h == 0 else 0, rows, fixed, dropped,
                    model_rels, model_syms, hard_pass, hard_total])
    print(f"{date.today().isoformat()} {commit} suite={suite_pass}/{suite_total} "
          f"eval={eval_pass}/{eval_total} hygiene={'OK' if rc_h == 0 else 'FAIL'} "
          f"lint=rows:{rows} fixed:{fixed} dropped:{dropped} "
          f"model={model_rels}rels/{model_syms}syms "
          f"hard={hard_pass}/{hard_total}")
    print(f"-> anexado a tools/progress.csv")


if __name__ == "__main__":
    sys.exit(main())
