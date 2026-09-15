# Reglas del proyecto (entrada por evidencia)

- Tras editar un script Python del pipeline (lint, extractores, progress), borra su `__pycache__` y exige idempotencia (segunda pasada sin cambios) antes de dar una limpieza por buena. Motivó: `--clean` con bytecode rancio dejó `SE__Resume` sin arreglar (2026-09-05).
- Cadena NL congelada en baseline `fc64115` (parser_v2 + graph + semantic_field + BFS): no editar esos ficheros; toda mejora futura debe mantener holdout 1000/1000 + core 25/25 + composición 7/7, verificados por ejecución. Motivó: P4 hold-out 1000/1000 con vocabulario disjunto + EXP-22 14/14 con baseline intacto (2026-09-15).
- EXP-25.1 cerrado en `open_generalization.pl` (F-H eventos + type/kind endpoint/1-hop + FIX-3 yes/no + FIX-4 attr): batería 120/120 + m0 limpio; mantener además EXP-22 14/14 + EXP-23 19/19 + EXP-24 23/23 + direct 7/7, verificados por ejecución. Motivó: batería 105/120 → 120/120 sin tocar baselines congelados (2026-09-15).
