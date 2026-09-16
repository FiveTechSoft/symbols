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

