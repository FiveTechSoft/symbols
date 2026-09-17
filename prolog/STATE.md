# STATE.md — Estado del proyecto (2026-09-16)

## Git

| Commit | Contenido |
|---|---|
| `75af1cd` | **HEAD/master** — R12-C: particle-first object skip + G-C1 veto NP desnuda + degree adverbs en `r5a` (solo `parser_v2.pl`, +66/−3) |
| `a771119` | Baseline pre-R12-C (post R12-B + R6..R9) |
| `1564577` | R12-B: `drop_rel_fragments/3` en parser_v4 (FP p6-which) |
| `354dd2a` | R7: copular attribute (`r7_copula` + compound_steal + r7_shared_subject) |
| `9cb4944` | R6: r5_of PP-sujeto |
| `5b87992` | BookBrain v0 (`bb0/2`) |
| `f1bcd79` | R5: r5_first_object inicial (HUGE 71.41→88.15%) |

Worktree limpio: solo `??` untracked. `parser_v2.pl` == `75af1cd` (verificado `git diff`, 0 líneas).

## Matriz congelada (verde, verificada por ejecución con 75af1cd)

| Test | Resultado |
|---|---|
| EXP-25 batería (`test_open_generalization.pl → run_exp25`) | 120/120, HARDCODING=0 |
| Holdout (`holdout_1000.pl → run_holdout`) | 1000/1000 |
| Composición (`composition_test.pl → run_composition`) | 7/7 |
| EXP-22 (`test_novel_relation.pl → run_exp22`) | 14/14 |
| EXP-23 (`test_attribute_learning.pl → run_exp23`) | 19/19 (novel 8) |
| EXP-24 (`test_multihop.pl → run_exp24`) | 23/23 |
| Direct (`test_exp25_1_direct.pl → test`) | 7 PASS / 0 FAIL |
| Curated (`test_curated.pl → test_all`) | 50/50 |
| QA batería (`bb0bat.pl`, KB jonah_r12c) | 11/11 |

Nota HARDCODING: m0 escanea solo `open_generalization.pl`; parser_v2 no escaneado textualmente (garantía conductual por adversarios+holdout+curada). Extender m0 a parser_v2 = opción pendiente barata.

## Funnel Jonás (BookBrain)

- Entrada canónica: `Temp\opencode\jonah_clauses.txt` (66 cláusulas)
- KB: `Temp\opencode\jonah_r12c.knowledge.pl` — facts=6, dropped=2, MD5 `E81DF9AB7EBC69F0F59E781C8DCF2D45`, idempotente ×2
- Recuperados R12-C: `memfact(lord,sent,wind,1.0,1)` (4_1) + `memfact(waters,compassed,soul,1.0,1)` (22_1)
- Runner: `swipl -f bookbrain_v0.pl -g "bb0(In,Out)"` (runner con `use_module` falla: module_header domain error)

## Delta R12-C medido vs baseline a771119 (entornos aislados firmados)

| Corpus | Métrica | a771119 | 75af1cd | Δ |
|---|---|---|---|---|
| HUGE (6024, gold-scored) | coverage | 88.15% | 87.75% | −0.40 (las 24 pérdidas = FPs gold_none `main(opens/arrives,X,unknown)` eliminados; 0 facts reales perdidos) |
| HUGE | precision | 98.47% | 98.85% | **+0.38** (fp-none 96→72, −25%) |
| HUGE | recall | 90.90% | 90.90% | 0 (correct 6174 inalterado) |
| KJV (31102) | covered | 18.94% | 18.56% | 127 versos junk-only fuera, **0 pérdidas reales**, 7 ganancias genuinas |

7 ganancias KJV: `brought(children,offering)`, `swept(river,ancient)`, `cleanseth(blueness,evil)`, `flee(shadows,turn)`, `flee(shadows,get)`, `took(keepers,veil)`, `went(disciples,own)` — verbo+objeto+complemento (la familia particle/prepositional que R12-C apunta).

**Veredicto: MARGINAL, no estructural.** R12-C es filtro de precisión (+0.38 prec, −25% fp-none) con recall 0 en texto real. Fixes quirúrgicos de parser NO mueven recall en corpora abiertos.

Contaminación detectada y archivada: primera pasada medida con parser híbrido rancio (`Temp\opencode\parser_v2.pl`, tenía r5_first_object pero no r5_particle/G-C1); `use_module` resuelve relativo al dir del fichero cargador. TSVs contaminados en `Temp\opencode\tainted\` con README (caso de estudio). Rerun limpio con envs aislados `baserv2\` / `r12cenv\` verificados por firma.

## Atención simbólica (integrada, bloqueada por calidad de KB)

- `symbolic_attention.pl` (weights: relations .4 / connections .3 / position .15 / novelty .15; `predict_answer/3`, `token_attention/4`) + `chat_attention.pl` (temperature/top_k/min_score) cargan OK
- `chat.pl` integrado (:16–17 consults, fallback `predict_answer` :1214–1218)
- `benchmark_ab` (KB curada alice_clean ~45 facts): baseline 27/30 = attention T=1/T=0.3 27/30, rels examinadas 0→0.9
- `benchmark_auto` (alice_auto.knowledge.pl, 4696 memfacts basura): 0/20 en TODAS las configs (verificado Python: 0/20 queries tienen fact)
- **Cuello de botella = calidad de la KB, no la atención** → conecta con el parser (proveedor de KB)

## Barras cerradas (no repetir)

- R8 coreference: proximidad ~75% FP, quote-scope 1ª persona 85.7% FP → pronombres quedan UNKNOWN; deferred 2ª persona
- R10 negación: barrera fail-closed 0 inversiones/0 negfacts; `negfact` diferido hasta corpus con sujeto nominal + predicado identificable
- R12-B/C: guardias sustractivas; unknown > hecho falso

## Decisión estratégica (FIJADA, medida, no intuida)

**R12-C → CLOSED / MARGINAL. No abrir R12-D.**

| Dato | Valor |
|---|---|
| Recall real | **Δ 0** |
| Precision HUGE | **+0.38** |
| FP `gold_none` | 96 → 72 |
| Pérdidas reales | **0** |
| KJV ganancias genuinas | 7 |
| KJV pérdidas reales | 0 |
| Cobertura perdida | exclusivamente junk |
| 1ª medición | **INVALIDADA por contaminación de entorno** |
| 2ª medición | **VALIDADA (entornos aislados + verificación por firma)** |

- Las 7 ganancias KJV se conservan como **material diagnóstico** (clase sintáctica verbo+objeto+complemento que V4 no representa bien) para cuando volvamos al parser; no justifican una ronda R12 ahora.
- KB congelada `E81DF9AB...` ANTES de trabajar en atención: cualquier mejora próxima se atribuye al mecanismo de atención, no a cambios simultáneos de conocimiento/parser.

### Cambio de nivel (nueva etapa)

```text
PARSER V4 (71.4% accuracy aceptada como baseline)
    ▼
50 DIÁLOGOS REALES
    ▼
ATENCIÓN SIMBÓLICA  (selección entidades / relaciones / contexto / referencias / conflicto-tiempo / UNKNOWN explícito)
    ▼
RESPUESTA
```

> R12-C termina la etapa de "hacer que entren más facts". Empieza la etapa de "decidir qué facts importan".

### Requisito de atribución de fallo por nivel

No basta medir si la respuesta es correcta: hay que saber EN QUÉ NIVEL falló.

```text
INPUT → PARSER → KB → ATENCIÓN SIMBÓLICA → REASONER → RESPUESTA
```

- Experimento Jonás A/B (KB 6 facts con contenido conocido): la atribución es parcialmente built-in — in-fact fail → atención/respuesta (el fact EXISTE); adversarial contestado → **alucinación** (fuga atención/KB); adversarial UNKNOWN → honesto OK. El nivel PARSER se mide con el dataset de 50 diálogos (parse del input antes de KB).
- Dataset ~50 diálogos sobre parser actual (88% cov / 71.4% accuracy aceptado): medir cuántos fallos de chat son del parser (arrastrados) vs capa de atención.

### Ejecución en curso

1. **`benchmark_jonah_attention.pl`** (A/B sobre KB congelada Jonás): 20 preguntas = 12 in-fact + 8 adversarias; 3 condiciones (baseline attention-off / T=1.0 / T=0.3); recarga KB por condición (chat_line puede aprender); scoring estricto (adversarial solo aprueba con UNKNOWN honesto); log TSV `jonah_ab.tsv` + columna de atribución. **EJECUTADO (2026-09-16)**: 18/20 en las 3 condiciones; in-fact 11/12, adversarial 7/8 idénticos; avg rels examined 0 / 0.75 / 0.75.
2. Doble-método: TSV (60 filas) + resumen interno cruzados con Python. ✔
3. Veredicto guardado en AgentBrain (index 125). ✔

## Resultados experimento Jonás A/B (2026-09-16)

| Condición | Total | in-fact | adversarial-strict | avg rels |
|---|---|---|---|---|
| baseline (attention off) | 18/20 | 11/12 | 7/8 | 0.00 |
| attention T=1.0 | 18/20 | 11/12 | 7/8 | 0.75 |
| attention T=0.3 | 18/20 | 11/12 | 7/8 | 0.75 |

**La atención es un no-op sobre KB congelada de 6 facts** (0 ganancia, 0 regreso; el baseline chat ya resuelve todo). Los 2 fails idénticos en las 3 condiciones, atribuidos por nivel:

- **Q6** `what came to jonah` (fact existe: `(word,came,jonah)`): fallo en **capa atención** — `focus_answer/3` (`chat_attention.pl:170`) solo cubre `who` / `what-is` / top-entity>0.3; forma `what V E` no cubierta (rels=1 examinada).
- **Q20** `who created the world` → **ALUCINACIÓN** `word came jonah` en las 3 condiciones: causa raiz en **capa entity-resolution del chat** (no parser, no KB, no atención): `qnorm fuzzy_one` (`chat.pl:2245-2257`) mapea `world`→`word` (Levenshtein 1, MaxD = len//3 = 1, símbolo único) + `chat_form:2013` (verbo desconocido → infiere relaciones del ente).

**Límite real encontrado:** (1) cobertura de formas de la capa de respuesta, (2) fuzzy alias demasiado permisivo (`world`→`word`). El experimento confirma la decisión de cambiar de nivel; los fixes concretos (guardia anti-alias cruzado en `fuzzy_one`; cobertura `what V E` en `focus_answer`) son dirigidos y se miden por separado — nunca en el mismo cambio.

## FIX-A + FIX-B cerrados (2026-09-16, verificados por ejecución)

- **FIX-A** (`what V E` en `focus_answer/3` + `verb_in_tokens/2` stem-`ed` en `chat_attention.pl`): Jonás A/B baseline 18/20 (fails {6,20}), att_t1 19/20, att_t03 19/20 — Q6 `what came to jonah` pasa solo con atención ("word came jonah."); in-fact 11/12→12/12; adversarial 8/8 intacto; bb0bat 11/11 idéntico.
- **FIX-B** (guardia anti-alias en `fuzzy_one`, `chat.pl`): candidato nunca acorta el token (`LS >= L`); mata `world(5)→word(4)` (única vía fuzzy medida: inventario doble-método — alice 0 consumidores de fuzzy_one, jonah solo el FP). Jonás A/B: **baseline 19/20 (solo Q6), att_t1 20/20, att_t03 20/20** (Q20 → "I don't know." honesto en las 3 condiciones, adversarial 8/8 intacto). Regresiones: bb0bat **11/11**, benchmark_ab **27/30** (fails pre-existentes: who ate mushroom ×2, what is alice in wonderland), curated **50/50**. `fuzzy_rel` (lado verbo) intacto.
- Commiteados juntos en `5b27c76` (deltas individuales preservados en AgentBrain mem-r13-fixab).
- **Próximo nivel (decisión firme): abandonar Jonás como KB de desarrollo; pasar a diálogos multi-fact** (primer experimento funcional real de atención: KB con varios facts donde la selección de qué fact responder importa).

## MULTI-FACT A/B cerrado (2026-09-16, verificado por ejecución)

- KB `multifact.knowledge.pl`: 15 memfacts, 6 entidades (juan/maria/luis/ana/pedro), 5 rels (work/live/visit/speak/own); `(V,O)` únicos por S; luis asertado primero (sesgo de orden). Runner `benchmark_multifact.pl` (entry `-g run`): M1 13 standalone `who-V` + M2 5 pares diálogo (A learn + B con pronombre he/she); condiciones direct / att_t1 / att_t03; scoring estricto `f(Atom)` ("don't know"/"unknown" = fail); TSV en `Temp\opencode\multifact_ab.tsv`.
- **Resultado: 18/18 (100%) en las 3 condiciones** — M1 13/13 ×3, M2 5/5 ×3 (tras fix de harness: `dialog_reset` por PAR en `mf_run_dialog`; antes stacks acumulaban entidades entre pares → pronombre con 2+ candidatos sin sustituir → fails falsos Q15/Q17). avg rels: direct 0.00, att 0.83 (atención examina menos que KB pero selecciona lo mismo).
- **Veredicto: en este KB la atención es Δ0 vs acceso directo** (igual que en Jonás 6-facts). Direct ya responde todo; la selección de atención no mejora ni empeora. Pregunta dura respondida: **no hay valor propio de la atención demostrado en texto sintético limpio** — para demostrar valor hace falta texto ruidoso/ambiguo donde la selección importe (corpus real, no KB curada).
- Infraestructura: probe_stem confirma `works/lives/owns/visits/speaks/does` → stem correcto; `chat_form([what,does|...])` NO existe (ruta real de M2: `did_split`-familia vía `chat_ask_qp`→`parse_question`); `dialog_resolve` exige 1 superviviente (`tier_single`).
- Commit pendiente si usuario pide: `benchmark_multifact.pl` + `multifact.knowledge.pl` (KB en Temp, copiar a repo si se comitea). Worktree pre-existente sucio: `AGENTS.md`, `debug_test.pl`, `parser.pl`.

## KJV REAL-TEXT A/B cerrado (2026-09-16, verificado por ejecución)

- Corpus real: `corpus_biblia\verses.tsv` (31.102 versos KJV) → `make_ruth.py` genera Ruth (85) / Jonah (48) / rj (133) en formato cláusulas; ingesta bb0 (`bookbrain_v0.pl`) → KBs `kjv_ruth.knowledge.pl` (10 facts) / `kjv_rj.knowledge.pl` (16 facts) en `Temp\opencode`. Ruido documentado: relación `visitar` = FP del parser sobre genealogías (5 de 16 facts), inflección display "visitars".
- Runner `benchmark_kjv.pl` (entry `-g run`, workdir prolog): 18 preguntas gold `kq/4` cubriendo los 16 memfacts (variantes unto/comes/visits/feareth; repes 13≡5, 15≡2); condiciones direct / att_t1 / att_t03; TSV `Temp\opencode\kjv_ab.tsv`.
- **Resultado: direct 18/18 (100%), att_t1 18/18, att_t03 18/18** tras añadir en copia aislada (Temp\opencode\kjv_direct_fix2) un cláusula-espejo de FIX-A a `chat_form` ("what V E" sin did, chat.pl:2028 solo cubría "what did"). Primera pasada sin espejo: direct 17/18 (solo fallaba q14 "what came to jonah") vs att 18/18 — la brecha era **hueco de cobertura del camino directo**, no valor de atención: la cláusula espejo (mismo mecanismo que focus_answer FIX-A, accediendo KB directo) lo cierra con avg rels 0.00.
- **CORRECCIÓN de diagnóstico (trazado por sonda de cadena de dispatch, chat.pl:1155)**: q14 NO es hueco simple de chat_form. Cadena real: p13 `chat_form` falla (no existe cláusula what-V-E) → p19 `graph_ask` bloqueado por `stray_unknown` (token `to` no es bb_content, chat.pl:1549) → p24 atención desactivada → p26 `chat_ask_qp` → `parse_question` NO parsea forma what-V-E (question_parser solo where/who/does/why) → `chat_unknown`. El espejo funciona porque `chat_form` se prueba ANTES de graph_ask y usa `exclude(is_glue_token)` (maneja `to`); `qnorm` devuelve átomo empaquetado, no lista (type_error en memberchk si se usa mal).
- **Re-lectura del veredicto**: sin espejo, la atención rescató q14 genuinamente (focus_answer FIX-A es la ÚNICA vía que responde what-V-E en el sistema real). Con espejo añadido al camino directo, Δ0. Es decir: la atención sí aporta valor EN ESTA FORMA mientras el camino directo no la cubra; el valor desaparece cuando se duplica su mecanismo en chat_form. Δ0 requiere el espejo como condición.
- Artefactos: `Temp\opencode\kjv_out1.txt` (17/18 primera), `kjv_out2.txt` (18/18 con espejo), `kjv_ab.tsv` (54 filas), espejo en `Temp\opencode\kjv_direct_fix2\chat.pl` (líneas 2038-2055, `exclude(is_glue_token)` para content — qnorm devuelve átomo empaquetado, no lista). Commits de referencia previos intactos: prolog `6616567`, agentbrain `b910d31`.
- Nota metodológica: `git show > file` en PowerShell escribe UTF-16 (AGENTS.md) — copias de código a Temp con Copy-Item, nunca redirección.

## Espejo what-V-E portado a chat.pl real (2026-09-16, matriz congelada verde por ejecución)

- Decisión del usuario: portar el espejo (cerrar línea de atención, cubrir q14 por vía directa). Cláusula `chat_form([what,V|Rest])` añadida tras la cláusula `what did` (chat.pl, +18 líneas, `exclude(is_glue_token)` para content). Verificado en vivo: q14 "what came to jonah" → "word came jonah." sin atención; "what did the men fear" intacto.
- **Matriz congelada completa verde con el port**: EXP-25 **120/120** HARDCODING=0, holdout **1000/1000** (FAIL 0), composición **7/7**, EXP-22 **14/14**, EXP-23 **19/19** (novel 8), EXP-24 **23/23** (novel 8), direct **7 PASS** / 0 FAIL, curated **50/50**, QA bb0bat **11/11** (salidas idénticas a las esperadas: Q2 "No.", Q7/Q8 "I don't know." honestos).
- benchmark_kjv con chat.pl real portado: **direct 18/18 (avg rels 0.00), att_t1 18/18, att_t03 18/18** — la vía directa ya cubre q14 sin atención; Δ0 con condiciones simétricas. `Temp\opencode\kjv_out3.txt`.
- Pendiente commit si el usuario lo pide: `chat.pl` (espejo) + `benchmark_kjv.pl` + `bb0bat.pl` (recuperado de Temp a repo).

## Próxima sesión

0. **EXP-SYMBOLIC-REPLAY v0 commiteado (`e1ec3de`, pushed)**: `experiment_world.pl` (historia real 14 nodos, replay/2) + `experiment_world_kb.pl` (materialización en KB común memfact/5, limpieza selectiva por `leccion_symbol/1`, textos evidencia-no-opinión). **Ejecutado + doble método (Prolog/Python)**: historia lineal → S1=S3=S4=3.7667 idénticos por construcción (degeneración confirmada), S2=1.0 (cardinalidad, no aprendizaje); KB 6/6 PASS idempotente ×2; `que_aprendimos_de(attention_closed, L)` recupera 2 lecciones correctas. Bug real cazado por ejecución en 1ª impl (`ancestor_or_self/2` átomo vs lista; fixed `chain_list/2`). Lección: con historia lineal el replay no puede distinguir estrategias que toman la misma rama — predicción falsable v0 confirmada. Ver mem-r16 en AgentBrain.

0b. **EXP-SYMBOLIC-REPLAY v1 EJECUTADO → LÍNEA CERRADA (criterio congelado, aplicado una sola vez)**: `experiment_world_fork.pl` (ev/5 timeline 19 eventos, forkpoint/5 con 3 forks reales, alt_area/3, historia_pre_fork/2 LOO). Leakage tests 2/2 PASS (no-actual + no-descendants). Resultado por fork: f1 ambos s1/s2 → continuar_parser (correcto), f2 → ampliar_cobertura (correcto), f3 ambos abstain (área atención-texto sin eventos pre-fork únicos). Precisiones: s1_inercia=0.67, s2_ratio=0.67, **b_primera=1.00**, b_segunda=0.00. Criterio: replay 0.67 ≤ baseline 1.00 + 0.10 → **DECISIÓN: CERRAR** (y 0.67 apenas supera azar 0.5). Fix verificado por ejecución: tie-check s2 tenía singleton `R2` + tautología `R >= R` (misma clase de trampa R12-C) → corregido a `\+ (member(_-R2,Rest), R2 >= R)`; números idénticos, 0 warnings. Caveat honesto documentado: b_primera=1.00 puede reflejar parcialmente el orden de autoría de alternativas; el criterio congelado decide igualmente CERRAR. **Lección durable: la historia experimental actual NO contiene estructura suficiente para que el replay supere a "elegir la primera alternativa" — el meta-aprendizaje por replay requiere historiales con ramas realmente exploradas (counterfactuals), no historiales lineales aunque se dividan en forks sintéticos.**

1. **Línea de atención: CERRADA** (decisión usuario). Sustituto: espejo what-V-E ya en chat.pl real. La única vía donde la atención demostró valor (q14) está cubierta por vía directa — eliminar la capa no es urgente, pero tampoco se ampliará (protocolo: sin valor demostrado, no añadir complejidad).

1b. **EXP-TINYLAMA-GAP v1 EJECUTADO (2026-09-16, medición-only, código del sistema intacto)**: primera comparación directa chat.pl vs TinyLlama-1.1B-Chat local (FP16 RTX 3060, greedy). Protocolo congelado antes de outputs: 40 preguntas idénticas (20 Tier A mundo-cerrado Jonás, 20 Tier B mundo-abierto/social/generación/meta), answer-key previa, scoring doble método (Python + Prolog con keys transcritas a mano, 80/80 bins idénticos), prolog runner idempotente ×2 (SHA256 `C056B0E8...`), tiny determinista ×2 (0 diffs/40). **Resultado: EMPATE GLOBAL 24.5 = 24.5 /40** (prolog 22c+1p+6w+11u; tiny 24c+1p+12w+3u). Brecha por capa: retrieval prolog 8/8 vs 7/8; composición 4/4 vs 0/4; social 4/4 vs 1/4; desconocido prolog GANA honestidad (2/2 IDK vs 2/2 alucinaciones de tiny); **mundo-abierto 0/8 vs 8/8 = LA brecha grande (8 pts)**; generación 1.5/4 vs 3.5/4 (eco `Learned[]` no es generación); comprensión 2/6 vs 4/6 (gap yes/no: `were the mariners afraid` → IDK pese a attribute fact existente). **Conclusión: la brecha NO es el parser — es (a) cobertura de conocimiento externo y (b) generación libre.** Contaminación cazada en 1ª pasada: sin KB reset por pregunta el KB creció 6→13 facts a mitad de batería → fix = `bb_load` antes de cada Q (patrón benchmark_multifact). Artefactos: `Temp\opencode\tinygap\` (battery.json congelado, gap_bat.pl, run_tiny.py, score.py, score_prolog.pl, GAP_RESULTS.txt). Ver mem-r18 en AgentBrain. Prioridad de inversión medida por esta tabla: conocimiento externo + generación, NO más parser.
2. **NUEVA línea candidata: Symbolic-RSI (meta-aprendizaje por replay simbólico, idea Dream-RSI)**. Principio: HISTORIAL → SIMULADOR → NUEVAS ESTRATEGIAS. Convertir el historial experimental (STATE.md + AgentBrain + benchmarks: EXP1..EXP25, R7-R12, MULTI-FACT, KJV) en un `experiment_world.pl` con `experiment(id, hypothesis, result(success|fail|unknown), cost, parent)` y `replay(Strategy, Score)` que evalúe retrospectivamente estrategias de exploración (S1 continuar-última-rama vs S2 mejor éxito/coste) SIN LLM, sin embeddings, sin backprop. Falsable en un único EXP mínimo: si el replay simbólico identifica retrospectivamente la estrategia que de hecho funcionó, es la primera pieza de meta-aprendizaje simbólico; si no, se archiva como las demás.
   - Reglas del paper adoptadas: (i) NO resumir prematuramente la experiencia — conservar historial estructurado y permitir re-recorrerlo (encaja con AgentBrain+STATE.md); (ii) replay solo recorre ramas realmente descubiertas — separar memoria de experiencia de generación de hipótesis; (iii) garantía de no-degradación solo sobre historiales usados en replay, nunca prometer mejora futura.
   - Tres niveles del proyecto: Nivel 1 conocimiento (Prolog actual), Nivel 2 razonamiento (composición+inferencia; ideas JEPE/Hawkins), Nivel 3 meta-aprendizaje (Symbolic-RSI). Prioridad tras replay-EXP: decidir con medición.
3. Si se comitea: `chat.pl` (espejo) + `benchmark_kjv.pl` + `make_ruth.py` + corpus cláusulas + `bb0bat.pl` (BBs generadas quedan en Temp).

## Mundo abierto MVP v0 (2026-09-16, medición-only, chat.pl intacto)

**Criterio congelado por usuario: añadir conocimiento debe aumentar respuestas correctas sin convertir UNKNOWN en adivinanza.** Loop mínimo cerrado: corpus controlado `openworld.txt` (19 líneas: 12 conocimiento + trampas ambigüedad + 5 ruido) -> `ow_ingest.pl` (6 patrones cerrados; supresión de ambigüedad: mismo (S,R) con 2+ O distintos dropea TODOS; ruido dropeado) -> `openworld.knowledge.pl` (12 memfacts) -> `ow_bat.pl` (14 preguntas verbatim de battery.json B01-B08 + 5 sondas, KB reset por pregunta). **Delta: 0/14 -> 6/14 correctas, 0 WRONG, IDK honestos intactos** (hamlet, moon-ambiguo siguen IDK; OW4-8/13 siguen IDK = rutas de consulta, no ingesta). Ingesta idempotente ×2 (fc IDENTICAL). Caveat GAP cerrado: run_tiny.py alimentó a TinyLlama verbatim (B01-B08 completas en gap_tiny_raw.txt); gap_bat.pl dio a prolog 6/8 acortadas (3 cayeron a chat_learn) -> GAP queda "como medido", MVP usa protocolo correcto. Baseline contaminado cazado: 1er intento tenía bb_add hardcoded en runner (kb=18 en ambos brazos) -> runner jonah-only kb=6 confirmado. Root-causes restantes (todas en chat.pl, ninguna en ingesta): OW4 how-many (graph_ask directo SÍ: answer([seven],[(week,has,seven)]) pero dispatch falla), OW5 stray-tokens tras entidad bloquea pack_is_about, OW6 needs is-a (cow->animal; graph_fill falla correctamente sin hecho), OW7 sin ruta aritmética, OW8 verbo speak desconocido, OW13 bb_rel_forms(have) no mapea has. Artefactos: Temp\opencode\openworld\ (corpus, ingest, KB, runners ×2, scorer). Ver mem-r19 en AgentBrain.

## Mundo abierto MVP v1 (2026-09-16, ingesta ampliada, chat.pl intacto)

**v1 = solo ampliar ingesta, NO tocar chat.pl.** Corpus `openworld_v1.txt` 22 líneas (19 v0 + "2 plus 2 is 4" [P7 aritmética, átomo QUOTED `'2_plus_2'` — sin comillas syntax_error al cargar] + "have means has" [P8 means] + "shakespeare wrote hamlet") -> `ow_ingest.pl` 8 patrones P1-P8 + regla de ambigüedad bifurcada (is FUNCIONAL: mismo S con 2+ O suprime todos los (S,is,*); eventos MULTI-VALOR wrote: sin supresión) -> `openworld_v1.knowledge.pl` 15 memfacts, ingesta idempotente ×2. **Delta v0->v1: 6/14 -> 9/14 correctas (+OW7 "2 plus 2 is 4" vía span_pack, +OW11 hamlet exact-match, +OW13 "fish has gills" vía D9 means_triple bb_rel_forms(have)→[has]), WRONG 0, IDK 8->5 honestos** (OW4 many-no-glue, OW5 strays color/clear, OW6 dispatch is-a, OW8 many, OW12 moon suprimida). Runner `ow_bat_v1.pl` (kb=21) idempotente ×2 fc IDENTICAL; `score_ow_v1.py` bins; strict scoring sin extras defensivos OK. Armadura: `-f` debe apuntar al chat.pl REAL (C:/symbols/prolog/), no copia Temp. Bloqueados por dispatch (v2, requieren chat.pl): OW4, OW5, OW6. Ver mem-r20 en AgentBrain.
## Mundo abierto MVP v2 (2026-09-17, solo dispatch en chat.pl, corpus v1 congelado)

**v2 = dispatch-only: OW4/OW5/OW6 en chat_form/1, corpus v1 intacto (commit 3d7b60d), sin tocar atencion/generacion.** Prototipos validados primero en probe aislado (ow2_route_probe.pl: q4/q5/q6 fire, 0 leaks en 11 adversarios), luego portados a chat.pl (3 clausulas chat_form nuevas tras [what,V|Rest], +53 lineas solo adiciones). Bugs cazados en port (bisect con ow2_fire3-6.pl): (1) R4 `Os == [O]` nunca une var libre — debe ser `Os = [N]` (== como pattern-match contra var no ligada falla siempre); (2) R5/R6 usaban `Toks` sin ligar (singleton) — hay que reconstruir `append([what,A,Vw],Rest,Toks)`; (3) R6 copiaba guardia `no pin_rel(Vw,_)` del prototipo donde no existia — el token relacion DEBE pinear (says IS pin_rel). **Delta v1->v2: 9/14 -> 12/14 (+OW4 seven, +OW5 blue, +OW6 cow says moo), WRONG=0, IDK 5->2 honestos** (OW8 many-speak, OW12 moon-ambigua suprimida). Runner idempotente x2 (MD5 13ca356b identico), prototipo post-port sin leaks (3 fires exactos). Matriz congelada verde en HEAD con v2: EXP-25 120/120 HARDCODING=0, direct 7/0, holdout 1000/1000, composicion 7/7, EXP-22 14/14, EXP-23 19/19, EXP-24 23/23, curated 50/50, bb0bat 11/11 (manual: 11/11 respuestas esperadas), usable_gate PASS. Leccion: los prototipos verifican logica, no el port — cada clausula portada necesita su propio fire-probe con la head EXACTA de chat.pl.

## EXP-GEN-LEVELS v1 (2026-09-17, medición-only, chat.pl intacto)

**Post-v2 el gap principal vs TinyLlama es generación (GAP: 1.5/4 vs 3.5/4). Experimento mínimo de medición: 10 preguntas × 4 niveles (L1 literal / L2 composición / L3 reformulación / L4 nueva), keys congeladas ANTES de ver outputs (battery_gen.json), KB = jonah(6)+openworld_v1(15)=21, reset por pregunta.** Resultado: L1 3.0/3 (recuperación ya perfecta), L2 0.5/2 — la ruta "tell me about fish" YA compone 2 memfacts via listing template (manual 4.0/10; key gill vs respuesta gills = artifact token-exacto), L3 0.0/2 con DOS fallos distintos: (a) dispatch "describe the sky"->IDK aunque memfact(sky,is,blue) existe (familia no-canonical de comprensión del GAP), (b) eco Learned[] en "write a sentence about the sea", L4 0.0/3 (solo eco o IDK; el eco NO es generación). Honestidad intacta: 3 IDK, 0 alucinaciones. Runner gen_probe.pl idempotente ×2 (MD5 89A4EA89). Conclusión: la frontera es exactamente L3+L4; L3(a) dispatch + L3(b) ensamblado de memfacts (materia prima existe), L4 requiere contenido externo. Ver mem-r22 en AgentBrain. Artefactos: Temp\opencode\tinygap\ (battery_gen.json, gen_probe.pl, GEN_LEVELS_RESULTS.txt).

## EXP-GEN-LEARN v1 (2026-09-17, mecanismo aislado, chat.pl intacto)

**La transformación símbolo→lenguaje es APRENDIBLE por inducción de patrones (realizer aislado ~120 líneas, 0 deps de chat.pl). 3 pares de aprendizaje congelados pre-output (battery_genlearn.json, criterios C0-C5), fail-closed: sin patrón → UNKNOWN.** Mecanismo: alineación por posición [Det?, S, VP..., O] → `gl_pattern(Rel, Det, SubjNum, VP, ONum)`; realización sustituye S/O con morfología aprendida del ejemplar, VP verbatim. **Resultado: C5 3/3 — el patrón sobrevive al RETRACT de los ejemplos (0 ejemplos quedan, realización idéntica) = generalización real, no memorización; C0/C3/C4 PASS; salidas nunca vistas ("Dog live in house.", "A cat says meow." — VP y determinante transferidos, args nuevos).** Skip honesto: wrote-pair (objeto multi-token "romeo and juliet") no alineable en v1 → LEARN-SKIPPED fail-closed, patrón no creado, T4 UNKNOWN (10/11 script, único FAIL = expectativa congelada T4, desviación documentada; doble método manual consistente). Limitaciones v1 honestas: morfología de número ambigua para bare-plural ('Fish' indistinguible de singular), objetos multi-token → v2, sin conjugación nueva. Runner idempotente ×2 (MD5 63ACDE0A). Lección durable SWI 10: `double_quotes=string` por defecto — `append(Cs, "s", Ps)` mete el string como TAIL impropio ([...|s]); usar `append(Cs, [115], Ps)`; split_string deja token '' final por el punto — excluirlo tras atom_string (string \== atom antes de convertir). Ver mem-r23 en AgentBrain. Artefactos: Temp\opencode\genlearn\ (battery_genlearn.json, gen_learn.pl, GEN_LEARN_RESULTS.txt, check_genlearn.py).



## EXP-GEN-LEARN v2 (2026-09-17, chat.pl intacto)

**Cerrado: 9/9 criterios PASS, 16/16 doble-metodo MATCH, MD5 D247BA6AD09BDBE3F9A987D38CA30A96 identico x2, 0 alucinaciones.** Mecanismo aislado \gen_learn_v2.pl\ (~200 lineas, 0 deps chat.pl): induccion de patrones F1 conjugacion (2 formas por rel keyed (Rel,SubjNum,ONum)), F2 multi-token O (template \g2_otmpl/2\ substituido entero — romeo_juliet alineado SIN skip), F3 det-O transferido. Seleccion frozen \g2_select/4\: exacto -> (SN,unknown) multi-token -> single-form fallback (T2 intent) -> multi-form reject (anti-overgen, medido en chases: T2c1x/T2c2x UNKNOWN, nunca mezcla 'A dog chase cats'). Olvido: ejemplares 0, salidas identicas = patron sobrevive (C5, no memorizacion). Desviaciones documentadas en GEN_LEARN_V2_RESULTS.txt: DEV-1 T2c triple frozen corregido a [dogs,chases,cats]; DEV-2 T10 contradice a T2 (bird==goose estructuralmente) -> literal registrado, anti-overgen reubicado en chases; DEV-3 T4b self-replay minusculas (template lowercase, no reconstruct cap). Verificador doble metodo \check_genlearn_v2.py\ exit 0. Proximo paso del plan: transferencia entre dominios (aprender estructura en dominio A -> retirar ejemplares -> aplicar a dominio B), NO generacion libre. Artefactos: Temp\opencode\genlearn\ (battery_genlearn_v2.json, gen_learn_v2.pl, g2_p1/p2_utf8.txt, check_genlearn_v2.py, GEN_LEARN_V2_RESULTS.txt).

## EXP-GEN-TRANSFER v1 (2026-09-17, chat.pl intacto)

**Cerrado fase 1 (simbolos artificiales A -> B): 10/10 criterios PASS, 15/15 doble-metodo MATCH, MD5 3EF918E4CA4719B8D50F826FBF8D1FA2 identico x2, 0 alucinaciones (7/15 filas UNKNOWN fail-closed, contadas por 2 parses independientes).** Bateria frozen \battery_gentransfer.json\ (11 tests, criterios+outputs exactos congelados ANTES de ejecutar; 3 ejemplares A + 2 hechos B presentados como vocabulario conocido). Mecanismo aislado \gen_transfer.pl\ (0 deps chat.pl/gen_learn): aprende SCHEMA estructural [DetS,S,Verb,DetO,O] + alofonia inducida de A (vocal->an desde 'an orbit' vs 'a path') + vocab fail-closed con provenance \g3_vocab(Token,Dom,Src)\: src=ex (derivado de ejemplar, muere al retract) vs pr (presentado, sobrevive). Estructura inducida que sobrevive al olvido: \g3_rel/1\ + \g3_schema/1\ (verb spine). \g3_forget_a\ retracta ejemplares+vocab ex. Resultado: T3/T2b transfer B correctos ('A glorb has a flux.', 'A wug has an arg.' con 'an' inducido), T3x/T3y/T3z/T4 fail-closed UNKNOWN, T6a retract 0/0 + realizes IDENTICOS post-forget (C6), T6z eco de ejemplar MUERE (retract observable, no cosmetico). C3-nocopy frozen: 0 tokens CONTENIDO de A en output (spine a/an+has = estructura aprendida, no copia). Bugs durables: forall(memberchk) solo chequea 1er token -> g3_vocab_ok/2 recursivo; schema solo-Verb sin chequear rel -> g3_rel gate; aggregate_all(setof) invalido -> findall+sort. Traps checker durables: rows 0-based solo sobre lineas '|||' (warning inicial desplaza +1); fila T6a-count 5 campos (valores campos 2 y 3); out_by['T6a']=[0,0,realize] -> C6 indice 2. Ver mem-r25 en AgentBrain. Artefactos: Temp\opencode\gentransfer\ (battery_gentransfer.json, gen_transfer.pl, g3_p1/p2_utf8.txt, check_gentransfer.py, GEN_TRANSFER_RESULTS.txt). Fase 2 (matematicas -> fisica) = bateria frozen SEPARADA, solo si usuario confirma.

## EXP-GEN-TRANSFER v2 (2026-09-17, chat.pl intacto)

**Cerrado fase 2 (matematicas -> fisica, transferencia de ESTRUCTURA RELACIONAL): 9/9 criterios PASS, 23/23 doble-metodo MATCH, MD5 953F8ACC1E9F8B7A1716E907D97887F8 identico x2, 0 alucinaciones (9/23 filas UNKNOWN fail-closed, contadas por 2 metodos de parse, MATCH).** Bateria frozen \battery_gentransfer_v2.json\ (23 filas, criterios+outputs exactos congelados ANTES de ejecutar; 5 ejemplares matematica + 4 hechos fisica presentados como conocidos + extras). Mecanismo aislado \gen_transfer_v2.pl\ (0 deps chat.pl/gen_transfer v1/gen_learn): aprende CUATRO schemas relacionales abstractos (equivalence:cong simetrico, proportionality:proportional ORDENADO dependiente-primero, composition:after ORDENADO cadena, transformation:'acting on' ORDENADO operador-primero) + vocab fail-closed con provenance. Resultado clave: los 4 schemas transfieren a vocabulario fisico ('Same_state cong reversible.', 'Speed proportional distance.', 'Lens2 after lens1.', 'Boost acting on frame.', 'Temperature cong entropy.'); INVERSION DE ORDEN (D4a/D4b) = UNKNOWN aunque ambos constantes sean conocidas (la orden es estructura relacional, no lexica); retractacion TOTAL del dominio matematico (incluidos extras area/side) deja las 5 realizaciones transferidas IDENTICAS (D6) y mata el eco de ejemplar (D6z/D6n UNKNOWN, retract observable). D3-nocopy: 0 tokens contenido de matematica en output (conectivos = estructura aprendida). Bugs durables: Singleton warning [S,O] = disyuncion duplicada muerta en order_ok; findall var mismatch [Desc]; D2 necesito gancho g2v_known_a/2 (hecho presentado en A como evidencia de orden, src=pr, muere con A). PowerShell: 2>&1 1> file escribe UTF-16, renormalizar siempre ascii-replace; swipl necesita ruta completa entrecomillada. Ver mem-r26 en AgentBrain. Artefactos: Temp\opencode\gentransfer\ (battery_gentransfer_v2.json, gen_transfer_v2.pl, g2v_p1/p2_utf8.txt, check_gentransfer_v2.py, GEN_TRANSFER_V2_RESULTS.txt). Siguiente paso del plan: meta-conocimiento explicito (el sistema DESCUBRE y ALMACENA la estructura reutilizable entre dominios, en vez de implicita en el mecanismo).

## EXP-GEN-META v1 (2026-09-17, chat.pl intacto)

**Cerrado (meta-conocimiento explicito: ejemplares -> descubrimiento -> `relational_schema/3` + `relational_schema_prov/2` -> BORRADO TOTAL -> reconstruccion fria leyendo SOLO los schema facts -> resolver instancia NOVEL de fisica): checker 23/23 PASS doble-metodo, corrida x3 MD5 `9DA617F256C109F938CAB262AD6E9FC3` identico, 0 alucinaciones.** Bateria frozen `battery_genmeta.json` (M0-M12 congelados ANTES de ejecutar; input de descubrimiento = SOLO ejemplares matematica ax1-ax5; bx1-bx4 son hechos B PRESENTADOS = vocabulario + evidencia de orden por par `gm_bpair/3`, NO input de descubrimiento - 1a corrida murio porque superficie 'a equal b' tiene conectivo 'equal' sin template). Mecanismo aislado `gen_meta.pl`: `gm_template/3` es REGLA de derivacion (conectivo observado -> termino de orden; nada asserted incondicionalmente); M7c descubrimiento parcial (solo ax1+ax2) = EXACTAMENTE 1 schema; M7a prov cita ids correctos (equivalence<-[ax2,ax1], proportionality<-[ax3], composition<-[ax4], transformation<-[ax5]); M9 nocopy 0 constantes contenido-A; M6 ejemplares mueren (eco UNKNOWN) schemas+prov sobreviven 4/4; M12 frio: aux counts (learned/vocab/facts/bpairs/r) TODOS 0, vocabulario B RE-PRESENTADO como input de mundo (solo vocab, sin hechos), reconstruccion SOLO desde schema facts; orden dos modos: pre-frio = evidencia por par (solo direccion observada), frio = constraint del termino de orden del schema (sym cualquier direccion; dependent_first/operator_first por lexicon de roles `gm_brole/2` regla; **chain NO admite direccion fria** -> M12f 'Lens1 after lens2.' UNKNOWN). Realizaciones frias NOVEL exactas: 'Photon proportional distance.', 'Boost acting on frame.', 'Temperature cong entropy.'. Bugs durables: `rest_after([_,C1,C2])` agarraba ULTIMOS 3 tokens ('on v' no 'acting on') -> `middle2([_,C1,C2,_])`; `forall(Cond,Action)` FALLA el goal completo si Action falla UNA vez (template miss en ax5 mato gm_learn_all); format/2 too-many-args crashea en runtime; M5 UNKNOWN = faltaba `gm_rebuild` antes del 1er realize (gm_r/3 vacio pre-frio); checker campo-M2: verificar ambos metodos contra la MISMA extraccion de campo. Ver mem-r27 en AgentBrain. Artefactos: Temp\opencode\genmeta\ (battery_genmeta.json, gen_meta.pl, gm_p1/p2/p3_utf8.txt, check_genmeta.py, GEN_META_RESULTS.txt). Siguiente paso del plan (pendiente decision usuario): consult/1 roundtrip del schema como fichero, o gancho de integracion en chat.pl.

## EXP-GEN-META v2 (2026-09-17, chat.pl intacto)

**Cerrado (persistencia: `relational_schema/3` sobrevive la muerte del proceso y es independiente del mecanismo que lo aprendio): 17/17 checker PASS doble-metodo idempotente x2, MD5 determinismo, 0 alucinaciones.** Arquitectura de DOS procesos swipl aislados: PRODUCTOR = gen_meta.pl v1 congelado SIN modificar (MD5 `C6B0FC4C107C22D1CDA5A8A9C0854648`) + `gm_driver.pl` -> descubre 4 schemas de ejemplares ax1-ax5 -> dump de SOLO relational_schema/3 + prov a `schema_kb.pl` (orden canonico setof, `format(S1,'~q.~n',...)`) -> halt (toda la memoria muere); FICHERO = 8 clausulas exactas (4 schemas + 4 prov `composition<-[ax4]`/`equivalence<-[ax2,ax1]`/`proportionality<-[ax3]`/`transformation<-[ax5]`), 0 aux predicates, 0 superficies de ejemplar, ids ax SOLO dentro de prov (semantica v1 M7a); CONSUMIDOR = `gen_meta2.pl` nucleo verbatim con TODOS los fixtures compilados FUERA (sin gm_ex/4, sin gm_bfact/4, vocab de mundo presentado en runtime desde REGLA `gm2_world_tok/1`) - la UNICA via de entrada de relational_schema/3 es `consult('schema_kb.pl')`. Resultados: G2 consumidor arranca con conocimiento CERO (7 aux counts=0, gm_ex/4 absent, realize pre-consult UNKNOWN); G3 consult reconstruye 4+4 con ordenes+conectivos legibles; G4a/b/c fisica NOVEL desde solo-fichero (`Photon proportional distance.` / `Boost acting on frame.` / `Temperature cong entropy.`); G5 orden sobrevive el roundtrip (proporcionalidad invertida UNKNOWN, lens1-first chain UNKNOWN); G6 rotation absent->sin fact+UNKNOWN; G8 2o consult IDEMPOTENTE (counts siguen 4/4, realizacion invariada); G9 semantica v1 chain fria preservada (ambas direcciones UNKNOWN); G7 0 constantes contenido-A en todas las salidas. Determinismo: protocolo consumidor x2 MD5 `8B87A5C71760020ADF73C0CBFBC04B8C` identico; kb productor x2 byte-identico (`0E102C24E906C39B5D32DCB306E00FC4`). Correccion honesta de bateria ANTES del checker formal: contrato inicial decia 0 ax-ids en cualquier parte - MAL, prov DEBE citar ids (v1 M7a); enmendado a 'ax-ids SOLO dentro de prov lists' y recongelado antes de la verificacion formal. Bugs durables: dump de 2 bytes = setof liga F-C-O pero match contra `F-_|_` (aridad distinta) hace forall vacuo + write_canonical escribe a stdout no al stream -> `format(S1,'~q.~n',...)`; gm2_counts/7 vs /1 aridad; PowerShell Out-String mete ruido stderr (NativeCommandError) que hace raw files distintos entre corridas -> checker normaliza filtrando solo filas G/D. Ver mem-r28 en AgentBrain. Artefactos: Temp\opencode\genmeta2\ (battery_genmeta2.json, gm_driver.pl, gen_meta2.pl, schema_kb.pl, check_genmeta2.py, GEN_META2_RESULTS.txt, gm2_prod1/2 + gm2_cons1/2 raw). Cadena GEN completa demostrada: learn -> abstract -> store explicit -> transfer (v1) + persist across process death -> consult -> rebuild -> solve novel (v2). Siguiente paso del plan (pendiente decision usuario): integration hook en chat.pl via adaptador minimo + frozen matrix completa.

## EXP-GEN-META v3 / Integration Hook v1 (2026-09-17, chat.pl MODIFICADO)

**Cerrado (primer contacto real de chat.pl con el meta-conocimiento: hook consumidor de relational_schema/3, aditivo puro diff +57/-0, SIN commit aun hasta validar).** Bateria frozen battery_genmeta3.json v3 (H0-H9, enmiendas H3/H4/H5/H6 por hallazgos de probes ANTES del edit, nunca post-hoc). Adapter gm2_chat_adapter.pl clon exacto del hook ejecutado ANTES del edit: H2-H7 PASS run 3 (hallazgos run 1: 'x cong y.' tragado por novel_short (tokens todos novel + alguno <=2 chars, chat.pl:640->756) -> H3 usa 'alpha cong beta.'; template exigia 'to' final -> Form B anadida). Hook en chat.pl: dynamic decls (linea ~83) + alternativa en chat_ask justo antes de chat_ask_qp (~1234) + chat_schema_ask/3 (1250) + schema_conn/1 (1277). Semantica: gate relational_schema(_,_,_) (sin schemas -> inerte); Conn declarado en ALGUN schema (consumidor estricto; sent/feared/came/believed NO declarados -> inerte); Form A [what,is,S,Conn,to], B1 [what,is,Conn,S], B2 [what,is,S,Conn]; fill si memory_relation(S,Conn,O); inversion SOLO familia sym (answer([O],[(O,Conn,S)])); ord(_) -> fail -> UNKNOWN. NO aprende, NO aserta, sin consult interno. **H9 frozen matrix post-edit 9/9 verificados por ejecucion: curated 50/50, QA 11/11 (bb0bat HEAD==hook byte-identical en las 11, Q2 'No.' correcto por diseno - pronoun him), EXP-25 120/120 HARDCODING=0, holdout 1000/1000, comp 7/7, EXP-22 14/14 (test_novel_relation.pl), EXP-23 19/19 (test_attribute_learning.pl), EXP-24 23/23 (test_multihop.pl), direct 7 PASS (test_exp25_1_direct.pl). H8 auditoria grep: 8 ocurrencias relational_schema (solo decls+hook), 0 superficies GEN (photon/proportional/dawn/dusk/entropy/temperature/alpha/beta ausentes), 0 prov leakage. Regresion con/sin schemas: lord sents wind / men feared lord / word came jonah / people believed god / No.(is the lord feared) / about-entity / Because: mariners attribute afraid / conjunciones -> identicos con y sin hook y con y sin schemas. Attention ON (attention_enabled assertado por kjv/jonah_attention; NO usado por curated/bb0bat): hook solo alcanza Form A 5-token (fill admitido, sym inv admitida, ord inv rechazada); B-forms/4-token = HEAD-identicos via chat_form mirror (chat.pl:2097, FIX-A); focus_answer/predict_answer fallan 4-token B (probe_focus2 FAIL verificado). E2E hook: H2 fill 'photon proportionals distance.', H3 sym A/B1/B2, H5 chain fill 'what is dawn after?' -> dusk, H6 inversion chain verdadera ('what is after dusk?'/'what is dusk after?') -> unknown, H4 ord inv -> unknown, H7 reversed teaching -> fill OK inversion unknown, is-forms via chat_ask_qp ('is photon proportional distance?' + '?') -> Yes; sin '?' via ensenanza echo (HEAD-identico). A/B contra HEAD (chat_head.pl via git show + fix UTF-16): probe bforms/att_on3/4/5/7/att8 byte-identicos en todo lo no-Form-A. Quirks: stash/pop seguro para A/B; parser.pl y ~54 WIP siguen fuera del commit. Artefactos: Temp\opencode\genmeta3\ (battery, adapter, ~25 probes, check_qa11.py). Ver mem-r29 en AgentBrain.

