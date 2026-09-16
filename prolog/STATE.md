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

## Próxima sesión

1. Dataset ~50 diálogos con atribución de fallo por nivel (INPUT → PARSER → KB → ATENCIÓN → REASONER → RESPUESTA).
2. Decisión sobre atención: sin texto ruidoso que demuestre valor, NO añadir complejidad (protocolo del usuario); candidata: medir atención sobre corpus KJV real con KB multi-fact ruidosa.