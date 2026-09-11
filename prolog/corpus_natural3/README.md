# Corpus Natural v0.3 (EXP44-A)

Un mundo, DOS familias de superficie disjuntas (entidades y
relaciones). La familia B no enuncia ninguna conclusión:

- A: `owns` / `belongs_to` / `visits` → `reaches`;
  `cooks` / `needs` → `serves` (observadas densas)
- B: `keeps` / `held_by` / `tours` → `stored` (oculto);
  `prepares` / `requires` → `offers` (oculto)

495 frases (seed 11). El parser produce relaciones superficiales
SIN normalizar entre familias; el motor induce el mapa por roles
estructurales (EXP39 llevado a corpus).

## Ficheros

- `corpus.txt`, `gold.pl` (solo puntúa), `expected.pl` (con zona),
  `heldout_natural3.pl`, `distractor_natural3.pl` (falsedad
  verificada en el generador).

## Regenerar

```sh
python3 make_corpus_natural3.py   # determinista (seed 11)
swipl -s experiment44.pl -g experiment44 -t halt
```
