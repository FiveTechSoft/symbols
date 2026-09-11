# Corpus Natural v0.1 (EXP42)

Primer corpus no diseñado para el motor: frases normales, variadas y
redundantes, con ruido y contradicciones. 656 frases (seed 42).

## Ficheros

- `corpus.txt` — el texto. Lo único que se ingiere.
- `gold.pl` — `gold_fact(Linea, S, R, O)` / `gold_none(Linea)`. Solo
  puntúa extracción; jamás se ingiere.
- `heldout_natural.pl` — conclusiones `reaches` ocultadas ANTES de
  generar el texto (ninguna frase las enuncia).
- `expected.pl` — queries con zona (`core`/`tra`) y modo
  (`retrieved`/`reasoned`/`unknown_person`).
- `distractor_natural.pl` — pares falsos verificados.

## Regenerar

```sh
python3 make_corpus_natural.py   # determinista (seed 42)
swipl -s experiment42.pl -g experiment42 -t halt
```

## Controles honestos

- Parser (`natural_parse.pl`) sin listas de entidades; pronombres por
  recencia de rol (el corpus lo garantiza por construcción en bloques).
- `reaches` observado denso (precedente EXP28); oculto = conclusión.
- Límites: nombres de un token; `returned/arrived` normalizados a
  `visits`; transferencia = entidades nuevas (el cruce de nombres de
  relación quedó probado simbólicamente en EXP39).
