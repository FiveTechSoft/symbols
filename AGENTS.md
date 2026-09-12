# Reglas del proyecto (entrada por evidencia)

- Tras editar un script Python del pipeline (lint, extractores, progress), borra su `__pycache__` y exige idempotencia (segunda pasada sin cambios) antes de dar una limpieza por buena. Motivó: `--clean` con bytecode rancio dejó `SE__Resume` sin arreglar (2026-09-05).

### Prolog aggregation rule

Use `findall/3` followed by `sort/2` or `msort/2` for aggregation.

Do not use `setof/3` or `bagof/3` with existential quantification (`^`) combined
with conjunctions in aggregation queries.

In SWI-Prolog 10.1.14 this pattern can produce incorrect grouping/overlapping
results. See `probe57.pl` for the minimal reproducible case.

All aggregation queries must be verified against an equivalent `findall + sort`
form before being used in experiments.
