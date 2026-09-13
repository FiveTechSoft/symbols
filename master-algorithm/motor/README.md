# Motor autodidáctico (híbrido Python + Prolog)

> Pedido de Anto: *«un pequeño motor que aprende y aprende por sí solo.
> Cada vez más inteligente, absorbiendo inteligencia de todo.»*

Esto **no es AGI**. Es un kernel de curiosidad (UCB) con varios *mundos*,
un crítico que solo archiva hechos verificados, y transferencia entre
secuencias hermanas. El catálogo de familias de hipótesis es **diseñado
por humanos** — el motor agenda *qué* probar, no inventa Cassini desde el vacío.

## Por qué Prolog + Python (híbrido)

| Capa | Lenguaje | Rol |
|------|----------|-----|
| Kernel | **Python** | Scheduler UCB, métricas, curvas, baselines, plugins de mundos |
| Archivo + crítico | **Prolog (Horn)** | Hechos/lemas como cláusulas; `verify` = query/resolución; rechazo = fallo finito + contraejemplo |

**Prolog** es el lenguaje natural de una base de conocimiento acumulativa:
`rec(fib,[1,1]).`, `verified(...)`, `lemma(...)`, `rejected(...)`.
La transferencia de inteligencia es literalmente reutilizar una cláusula
`rec/2` como prior en Lucas/Pell (`transfer_prior` / `companion/2`).

**Python** se queda con UCB, I/O, geometría numérico-simbólica ya escrita,
y las curvas — no reescribimos el bandit en Prolog.

Backend: **SWI-Prolog** (`swipl`, instalado en esta box). Si faltara,
hay un intérprete Horn mínimo en `motor/prolog/horn.py` (subconjunto
documentado) para no bloquear la corrida. No usamos pyswip.

## Cómo correr

Desde `/workspace/master-algorithm`:

```bash
# primera corrida (archivo limpio)
python -m motor tick --steps 40 --reset

# sigue aprendiendo (persiste en disco)
python -m motor tick --steps 20

# estado + preview de la teoría Horn
python -m motor status

# teoría completa
python -m motor theory
```

Salidas:

- `motor/archive/theory.pl` — teoría Horn que **crece**
- `motor/archive/meta.json` — brazos UCB, log, curvas
- `motor/runs/latest.json` — métricas de la última corrida

## Qué significa «más inteligente»

Curvas vs tick en `latest.json`:

| Métrica | Significado |
|---------|-------------|
| `n_verified_facts` | Hechos que pasaron el crítico (archivados) |
| `n_distinct_types` | Diversidad de *tipos* de relación/lema |
| `transfer_accuracy` | Prior `rec(fib,·)` predice términos en Lucas/Pell |
| `lemma_reuse_rate` | Fracción de cierres geométricos que citan lemas previos |
| `reject_rate` | Rechazos / (aceptados+rechazados) — curiosidad que falla |

Baselines en la misma corrida:

- **(A)** más datos, lenguaje congelado (Fib N=20→40): tipicamente **+0 tipos**
- **(B)** sin archivo / sin transferencia (prior incorrecto `[2,0]` en Lucas)

Si la transferencia **no** supera al baseline, eso **también es un resultado**
(p.ej. Fib→Pell debe fallar: leyes distintas).

## Mundos (plugins)

1. **sequences** — Fibonacci + Lucas + Pell. Aprender `rec/2` en una y
   probarla en las otras (absorber inteligencia = transferencia, no internet).
2. **logic** — paridad / conjunción / XOR desde tablas de ejemplos
   (`holds_bit_fn` en Prolog).
3. **geometry** — si existe `geometry/engine.py`, se enchufa; lemas → `lemma/3`.

Cada mundo expone: `observe / hypothesize / verify` y opcionalmente
`transfer_prior`. Familias *dead-end* obligatorias (la curiosidad puede
desperdiciar un tick).

## Cómo añadir un mundo

1. Crear `motor/worlds/mi_mundo.py` subclase de `WorldBase`.
2. Definir `families()` (al menos una productiva + una dead-end).
3. Implementar `hypothesize` → `Conjecture` y `verify` → `VerifiedFact`
   (idealmente vía `PrologCritic` o afirmando cláusulas en el archivo).
4. Registrar en `motor/worlds/__init__.py` → `build_worlds()`.

## Qué no puede hacer

- No llama a un LLM como aprendiz.
- No inventa el lenguaje de hipótesis (catálogo humano).
- No “absorbe internet”: solo reutiliza cláusulas del archivo propio.
- No es un theorem prover completo fuera del dominio cableado.
- Geometría ≠ AlphaGeometry; secuencias ≠ descubrimiento matemático abierto.

## Reutiliza

- `fibonacci/miner.py` — checks de recurrencia, φ, Cassini, Pisano, controles negativos
- `autodidact/loop.py` — ideas UCB + crítico + crecimiento de lenguaje
- `geometry/engine.py` — cierre axiomático + lemas citables
