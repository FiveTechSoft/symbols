# Corpus Natural v0.2 (EXP43)

Tres estructuras simultáneas compartiendo personas, sin fusionarse:

- A: `visits` / `in` → `reaches` (llega de EXP42)
- B: `works_at` / `located` → `based` (nueva, 3-hop)
- C: `owns` / `belongs_to` (evidencia + retrieval)

519 frases (seed 7). Held-out previo de CONCLUSIONES por skill
(premisas intactas). Gold solo puntúa.

## Ficheros

- `corpus.txt`, `gold.pl`, `expected.pl` (con zona `core`/`tra`),
  `heldout_natural2.pl`, `distractor_natural2.pl` (falsedad
  verificada contra ground truth en el generador).

## Regenerar

```sh
python3 make_corpus_natural2.py   # determinista (seed 7)
swipl -s experiment43.pl -g experiment43 -t halt
```

## Nota de motor

`discover_composition/2` deja solo la última regla (retractall
global); el runner conserva cada hallazgo en `found_rule/3` local.
El motor no se modifica.
