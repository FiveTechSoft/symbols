# Reglas del proyecto (entrada por evidencia)

- Tras editar un script Python del pipeline (lint, extractores, progress), borra su `__pycache__` y exige idempotencia (segunda pasada sin cambios) antes de dar una limpieza por buena. Motivó: `--clean` con bytecode rancio dejó `SE__Resume` sin arreglar (2026-09-05).
