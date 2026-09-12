# TINYLLAMA BENCHMARK — SymbolicBrain vs TinyLlama

Objetivo: decidir con datos si nuestro motor simbólico alcanza funcionalidad
comparable a TinyLlama (1.1B) y dónde lo supera. No se compara arquitectura
ni parámetros: se comparan **capacidades observables + coste**.

Regla del benchmark: cada mejora del motor debe subir (o no bajar) la
puntuación aquí. Lo que no mueve esto, se aparca.

## Corpus

`books/alice.txt` (147 KB, dominio público). Todo el benchmark corre sobre
`bookbrain/alice.knowledge.pl` (2.901 hechos) salvo indicación contraria.
Nada del benchmark puede depender de conocimiento fuera del corpus.

## Tareas: 100 (tasks.pl)

| Bloque | N | Qué mide | Forma |
|---|---|---|---|
| COMP | 20 | comprensión: recuperar quién-hizo-qué-a-quién con paráfrasis del verbo | who/what/did |
| QA | 20 | pregunta directa + UNKNOWN honesto (10 respondibles, 10 sin evidencia) | who/what/did/why |
| REAS | 20 | razonamiento a 2 saltos sobre el grafo (A→B→C con prueba) | why/multi-hop |
| MEM | 20 | memoria: entidades persistentes entre capítulos + coreferencia | who/what cross-ch |
| GEN | 20 | generación/instrucciones: formato de respuesta, listar, negar, resumir entidad | formato |

Cada tarea: `task(Id, Bloque, Pregunta, Esperado, TipoEsperado)` donde
TipoEsperado ∈ {exact, proof, unknown}. `unknown` puntúa 1 solo si el
sistema responde UNKNOWN (responder algo = 0 y cuenta como hallucination).

## Métricas por tarea

- `accuracy`: 1 si respuesta == esperado (conjuntos como sets), 0 si no.
- `hallucination`: 1 si responde con contenido cuando lo esperado es UNKNOWN.
- `proof`: 1 si aporta prueba con fuente existente en provenance.
- `latency_ms`: tiempo de la consulta.
- Globales: `RAM_peak_MB` (polling), `disk_KB` (.knowledge), `learn_ms`
  (ingesta), `hallucination_rate`, `proof_rate`.

## Puntuación

Por bloque: media de accuracy + proof_rate + (1 - hallucination_rate).
Global: media de bloques (no hay número único que oculte debilidades).

## Columna TinyLlama

Protocolo: mismas 100 preguntas en texto plano (`tasks_text.txt`),
temperatura 0, sin contexto extra salvo el mismo libro cuando el bloque
lo exija (MEM: libro completo en contexto; resto: sin contexto).
`tinyllama_results.tsv` registra (Id, Respuesta, Prueba?) y el mismo
scorer puntúa. Estado actual: PENDIENTE (sin modelo local disponible);
el runner ya acepta el fichero cuando exista.

## Estado baseline SymbolicBrain (v1, 2026-09-12)

| Bloque | acc | proof | hall |
|---|---|---|---|
| COMP | — | — | — |
| QA | — | — | — |
| REAS | — | — | — |
| MEM | — | — | — |
| GEN | — | — | — |

(Se rellena al correr `bench_tiny`. Hipótesis: COMP/QA alto, REAS≈0
(sin reasoner multi-hop), GEN parcial, hall≈0.)

## Ficheros

- `tasks.pl` — las 100 tareas verificadas (ver abajo verificación).
- `tasks_text.txt` — preguntas en texto plano (para TinyLlama).
- `bench_tiny.pl` — runner: carga knowledge, ejecuta, puntúa, temporiza.
- `tinyllama_results.tsv` — resultados TinyLlama (pendiente).
- `baseline.tsv` — última corrida nuestra (generado).

## Verificación de tareas (anti-circularidad)

Las respuestas esperadas NO salen del sistema: cada tarea de retrieval
se verifica contra el texto (proximidad entidades+verbo) + spot-checks
manuales contados; las `unknown` se verifican por ausencia en texto
(rg) y en memoria. `gen_tasks.py` deja constancia del método por tarea.
