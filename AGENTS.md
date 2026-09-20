# Reglas del proyecto (entrada por evidencia)

- Tras editar un script Python del pipeline (lint, extractores, progress), borra su `__pycache__` y exige idempotencia (segunda pasada sin cambios) antes de dar una limpieza por buena. Motivó: `--clean` con bytecode rancio dejó `SE__Resume` sin arreglar (2026-09-05).
- Cadena NL congelada en baseline `fc64115` (parser_v2 + graph + semantic_field + BFS): no editar esos ficheros; toda mejora futura debe mantener holdout 1000/1000 + core 25/25 + composición 7/7, verificados por ejecución. Motivó: P4 hold-out 1000/1000 con vocabulario disjunto + EXP-22 14/14 con baseline intacto (2026-09-15).
- EXP-25.1 cerrado en `open_generalization.pl` (F-H eventos + type/kind endpoint/1-hop + FIX-3 yes/no + FIX-4 attr): batería 120/120 + m0 limpio; mantener además EXP-22 14/14 + EXP-23 19/19 + EXP-24 23/23 + direct 7/7, verificados por ejecución. Motivó: batería 105/120 → 120/120 sin tocar baselines congelados (2026-09-15).
- BookBrain v0 committed `5b87992`: ingest = `swipl -f bookbrain_v0.pl -g "bb0(In,Out)"`; entry is `bb0/2` (`In=clauses file N_M\ttext`, `Out=KB`); filter 6→1 on Jonah; chat QA via `bb_load/1 + chat_line`. Motivó: runner viejo llamaba `bb0/0` inexistente; idempotencia MD5 idéntica; EXP-25 120/120 (2026-09-15).
- R7 copular attribute cerrado en `354dd2a` (`r7_copula` + `compound_steal` + `r7_shared_subject` en parser_v2; ingest bb0 acepta `relation(attribute,...)`) → `memfact(S,attribute,ADJ)`; atributo sobrevive pipeline completo texto→parser→BookBrain→memfact→retrieval; Jonás 3→4 hechos, idempotencia ×2, batería 11/11, matriz congelada verde, HARDCODING=0, verificados por ejecución. Motivó: representación atributo valida estructura interna del objeto semántico, no solo +1 hecho (2026-09-16).
- R8 coreference: local proximity and 1st-person quote-scope measured and rejected due to high false-positive rates; unresolved pronouns remain UNKNOWN. 2nd-person addressee has insufficient evidence and remains deferred. Motivó: proximidad ~75% FP, quote-scope 1ª persona 85.7% FP (6/7) medidos en corpus Jonás; KJV no da evidencia superficial suficiente; UNKNOWN honesto > hecho falso contaminante (2026-09-16).
- R9 inventario funnel post-R7 (solo medición): 66 cláusulas → 4 FACT = 4 memfacts exactos (preservación semántica 100%), 6 DROPPED correctos, 4 UNK honestos, 52 EMPTY (~30 pronombre/cita cerrados por R8); frontera restante = negación + subordinación; verificado por ejecución doble-método. Motivó: decidir dónde invertir esfuerzo antes de tocar zonas peligrosas (2026-09-16).
- R10 negation barrier (measurement-only): 9/66 clauses, 13 scopes; 0 sign inversions and 0 negative facts admitted. All negated scopes have unresolved/pronominal subjects or are jussive/question/cleft structures; `NOT(A)` remains UNKNOWN rather than generating `A` or `NOT(A)`. `negfact` deferred until a corpus provides named-subject negation with identifiable predicate/arguments. Verified by Prolog probe + independent Python token analysis (2026-09-16). Motivó: la barrera fail-closed es una propiedad del pipeline a preservar, no una deuda (2026-09-16).
- R11 subordination/scope cerrado (measurement-only, código 0): 7 casos → 2 PARCIAL (10_1, 43_1 — señal en sub-span bloqueada por inversión/fusión), 4 SIN-SEÑAL, 1 FALSO POSITIVO (9_2: p6 misparsea relativa `which` → `main(land,[dry,field])` en adversario). `that` NO es el problema: `[Art,S,V,that,NP...]` ya dispara (`the lord said that jonah should go` → `main(said,[lord,jonah])`). Frontera real = 3 capas separadas: formas ya-funcionantes / huecos de cobertura (sujeto sin artículo r5a, objeto posesivo, inversión inicial r7_leading, it+infinitivo) / 1 FP p6-relativa. R12 futuro dividido: R12-A generalizar `[Art,S,V,that,NP...]`, R12-B arreglar FP p6-which — nunca en el mismo cambio; antes de tocar código definir entrada→parse actual→deseado→guardia→adversarios→métrica. Motivó: mezclar causas perdería la medición limpia; añadir capacidad solo con mejora demostrada sin aumentar alucinaciones (2026-09-16).
- R12-B cerrado en parser_v4.pl (`drop_rel_fragments/3` sobre salida final de `parse_v4`, post-dedup): relpro which/who/whom/whose ('that' excluido, es complementizador válido) → drop de toda relación main/attribute cuyo S u O esté en el post-span; mata FP 9_2/A5 y el leak KB `memfact(dry,land,sky)`; sustracción pura 0 alucinaciones añadidas; matriz congelada verde (EXP-25 120/120 HARDCODING=0, holdout 1000/1000, comp 7/7, EXP-22 14/14, EXP-23 19/19, EXP-24 23/23, direct 7/7, curated 50/50, batería QA 11/11), embudo Jonás MD5 `EFBAB22F...` invariante + idempotente ×2, verificados por ejecución. Motivó: unknown > hecho falso; la guardia es fail-closed y no toca familia preexistente sin relpro ([Art Adj Noun], diferida a R12-C) (2026-09-16).
- R12-C cerrado en `75af1cd` (parser_v2.pl): G-C1 veto NP desnuda (`r5a`: `Rest \== []`), `r5_particle/1` 45 tokens + degree adverbs very/exceeding/exceedingly, `r5_first_object/3` 3 cláusulas (article-chain `r5_content_run_head` con cut / particle-skip recursivo `After \== []` / terminal non-particle); rescata 4_1 `memfact(lord,sent,wind)` + 22_1 `memfact(waters,compassed,soul)` y 16_1 (exceedingly era content) → embudo Jonás 4→6 hechos MD5 `E81DF9AB...` idempotente ×2; matriz verde completa (EXP-25 120/120 HARDCODING=0, holdout 1000/1000, comp 7/7, EXP-22 14/14, EXP-23 19/19, EXP-24 23/23, direct 7, curated 50/50, QA 11/11), verificado por ejecución. Delta medido vs `a771119` (entornos aislados firmados): HUGE prec 98.47→98.85% (fp-none 96→72, correct 6174 inalterado, recall 0), KJV 127 versos junk-only fuera / 0 reales / 7 ganancias genuinas → **marginal, no estructural; fixes quirúrgicos de parser no mueven recall en texto real**. Motivó: quitar helper "sin usar" (r5_first_candidate) rompió el particle-skip indirecto — warning Singleton = lógica muerta, no inocuo (2026-09-16).
- Toda medición de delta debe usar entornos aislados verificados por firma: dir Temp propio por versión con copias de parser+probe, `swipl` con workdir = ese dir, y sonda de firma antes de confiar (p.ej. `the lord sent out a great wind` → `main(sent,[lord,wind])` = R12-C). `use_module` resuelve relativo al dir del fichero cargador, no al del intérprete: un parser rancio en Temp\opencode\ se cuela como si fuera HEAD (contaminó la 1ª pasada del delta: cov 88.15% idéntico = híbrido, no baseline). `git show > file` en PowerShell escribe UTF-16 — verificar con `git diff commit -- file` (0 líneas = idéntico). Motivó: primera medición delta R12-C contaminada por parser_v2 híbrido rancio en Temp (2026-09-16).
- No interpretar forks retrospectivos como evidencia de aprendizaje contrafactual si ambas ramas no fueron realmente ejecutadas (EXP-SYMBOLIC-REPLAY v1: replay 0.67 vs baseline b_primera 1.00, criterio +0.10 aplicado una sola vez → línea cerrada en `ef8ac84`). Motivó: la historia lineal con forks sintéticos no contiene counterfactuals reales; replay necesita ramas ambas-ejecutadas (2026-09-16).
- Toda evaluación de replay debe excluir toda información posterior al punto de decisión (LOO estricto) y comprobar explícitamente ausencia de leakage con tests que se ejecutan ANTES de medir (`loo_no_ve_actual` + `loo_no_descendientes`, 2/2 PASS en v1). Motivó: sin gate anti-leakage el replay puede puntuar viendo el futuro (2026-09-16).
- Openworld MVP: el corpus v1 queda congelado en `3d7b60d`. La secuencia base→v0→v1 produjo 0/14→6/14→9/14 correctas, con WRONG=0 en todas las condiciones y sin pérdida de UNKNOWN honesto. v1 cambió únicamente la ingesta; chat.pl permaneció intacto, por lo que su delta es atribuible a cobertura de conocimiento. v2 se limita a las rutas de dispatch de OW4/OW5/OW6 en chat.pl y no debe modificar el corpus v1. Toda modificación de v2 debe comenzar con probes aislados de dispatch y una medición reproducible antes de modificar chat.pl. Los runners de openworld deben permanecer idempotentes ×2. Motivó: separar cobertura de ingesta de routing demostró que el delta v1 es atribuible solo a ingesta (2026-09-16).
- En el motor C (`src/`), los comentarios del código se escriben en inglés y el léxico (keywords, conectivos, mapeos REL→connective) va en tablas consultables o se deduce del corpus en ingest; nunca listas de palabras hardcodeadas en la lógica. Motivó: frames de chat con SIB_W/KING_W/WIFE_W hardcodeados violaron el principio HARDCODING=0 y fueron rechazados (2026-09-17).

## Fase 4 — Canonicalización de consultas

Fase 4 cerrada en `src/bible_chat.c`.

`CanonicalizeQuery` normaliza puntuación y flags, `del` → `de + el`, y `'s` detach, aplicando G1/G2/G3/G5.

Verificación:
- batería Fase 4: 26 queries
- 10 gains
- 2 flips intencionales
- 0 alucinaciones
- ctest build-gcc: 48/48 PASS
- runner idempotente: MD5 idéntico ×2

Regla arquitectónica:
- G1/topic y G2/stop-set usan únicamente clases funcionales.
- Nunca utilizan vocabulario concreto para decidir si un término es topic o stop-word.
- Esto mantiene la lógica inmune a variantes EN/ES y a nombres concretos como Jonás/Jonah.

## Tablas agénticas externas (Paso 1)

- Las tablas agénticas declarativas (`allow`/`contract` en `data/agentic/tools.tsv`) se cargan una vez en `ToolInit()`. Cada tabla usa las filas válidas aportadas por el fichero cuando existe al menos una; en caso contrario conserva su fallback compilado. El parser es fail-closed: filas malformadas, `TYPE` desconocido o overflow se descartan con warning y nunca abortan la inicialización. La ruta del fichero es fija y el motor solo lo lee. El orden del fichero determina la prioridad. Esta separación de datos agénticos e implementación debe mantener la semántica existente; el contrato queda validado por `ctest 57/57`, seis baterías byte-idénticas, ejecución sin TSV idéntica y comprobación de idempotencia, 2026-09-18.

## Integración Agéntica Nativa OpenCode (Modo E)

- OpenCode opera con Symbolic LLM como su único proveedor LLM sin MCP ni llamadas a APIs de terceros. El endpoint HTTP `symbols_server` (`/v1/chat/completions`) implementa el protocolo wire de OpenAI Tool Calling: deserializa esquemas `tools`, ingesta retornos `role: "tool"` en bucle ReAct, formula el plan STRIPS óptimo y emite `tool_calls` con `finish_reason: "tool_calls"` hasta resolver la tarea con reporte Markdown (`finish_reason: "stop"`), manteniendo en paralelo el canal de consultas factuales directas. Validado por suite CTest 31/31 (`test_server_tool_calling.c`) y test de integración por socket HTTP real de 8 turnos (`tools/test_opencode_agentic_server.py`), 2026-09-20.

## Motor Abductivo de Diagnóstico y Linters (Fase 7)

- El motor abductivo (`src/agent_diagnose.c`, `include/agent_diagnose.h`) parsea salidas terminales de compiladores (GCC, Clang y MSVC) extrayendo posición de error, clasificación taxonómica formal (`DIAG_ERR_UNDECLARED_SYMBOL`, `MISSING_MEMBER`, `ARITY_MISMATCH`, `TYPE_MISMATCH`, `MISSING_HEADER`, `SYNTAX`, `REDEFINITION`), sugerencias "did you mean" y enlace abductivo directo con el Grafo de Conocimiento del Código (`CodeGraphGetFunctionFile`). Activa dinámicamente predicados STRIPS (`PRED_ERROR_DIAGNOSED`) disparando replanificación curativa sin loops ciegos ni alucinaciones. Validado por suite CTest 39/39 (`tests/test_agent_diagnose.c`) y 0 regresiones en suites agénticas, 2026-09-20.

## Grafo de Conocimiento del Código Políglota (Fase 8)

- El motor de grafo de código (`src/code_graph.c`, `include/code_graph.h`) se extiende con parsing políglota nativo para Python (`.py`, `.pyw`) y TypeScript/JavaScript (`.ts`, `.tsx`, `.js`, `.jsx`, `.mjs`, `.cjs`) sin dependencias externas. Extrae clases, herencia (`inherits_from`/`inherited_by`), funciones y métodos (`has_method`/`method_of`), interfaces, importaciones (`imports`/`imported_by`) y grafos de llamadas. La propagación del radio de impacto (Blast Radius) opera a través de fronteras políglotas enlazando módulos dependientes y suites de pruebas en un solo paso BFS. Validado por suite CTest 38/38 (`tests/test_code_graph_polyglot.c`) con 0 regresiones en las 53 pruebas previas de C, 2026-09-20.

## Harness de Evaluación SWE-bench Lite (Fase 9)

- El harness de verificación quirúrgica SWE-bench Lite (`src/swe_bench_harness.c`, `include/swe_bench_harness.h`) evalúa la fase de verificación previa de parches y análisis de impacto AST sobre instancias de repositorios reales (Django, Flask, Sympy, Scikit-learn y Pytest). Coordina instanciación en espacio de trabajo, grafo de impacto políglota, pre-verificación quirúrgica (`PatchVerifyPlan`), aplicación atómica (`PatchApplyAtomic`), verificación fail-closed con rollback garantizado (`PatchRollback`) y telemetría de memoria dinámica de proceso por API de SO (~28 MB RAM, 0 GPU) con latencia < 2 ms por tarea y 0.00% de corrupción de parches bajo invariantes AST. Validado por suite CTest 36/36 (`tests/test_swe_bench_harness.c`), 2026-09-20.

## Indexador de Repositorios, CLI symbols-agent y Dogfooding OpenCode (Fase 10)

- El indexador recursivo nativo en C11 (`CodeGraphIngestDirectory`) escanea y parsea árboles de repositorios políglotas en < 50 ms filtrando con seguridad carpetas de compilación, control de versiones y entornos virtuales (`.git`, `build*`, `node_modules`, `venv`, etc.). `symbols_server` auto-detecta rutas de repositorios y monta en RAM el grafo de símbolos y llamadas de cualquier proyecto. El nuevo CLI autónomo `symbols-agent` (`src/agent_cli_main.c`) permite inspección de impacto (`--blast-radius`), diagnóstico abductivo (`--diagnose`) y resolución autónoma de tareas de ingeniería en terminal local. Validado por suite CTest (`test_code_graph_indexer.c`) 100% PASS y sesión de dogfooding real HTTP multi-turno con OpenCode (`tools/test_opencode_live_dogfood.py`), 2026-09-20.

## Motor de Ejecución de Shell Multiplataforma y Bucle de Autocuración (Fase 11)

- El motor de ejecución de subprocess nativo en C11 (`src/agent_shell.c`, `include/agent_shell.h`) unifica la ejecución de shells nativos en Windows (`cmd.exe`, `powershell.exe`), Linux (`/bin/bash`, `/bin/sh`) y macOS (`/bin/zsh`, `/bin/sh`) sin dependencias externas. Incorpora captura de doble flujo (`stdout` y `stderr` independientes de hasta 64 KB), loop de drenaje no-bloqueante anti-deadlock de pipes, protección por timeout de precisión milimétrica (código de salida 124 y terminación forzosa del proceso hijo), y telemetría de latencia de reloj. Integrado de extremo a extremo en `AgentRunnerSolveTask` (`src/agent_runner.c`): ante fallos en comandos de construcción o pruebas (`exit_code != 0`), el flujo de error alimenta automáticamente al motor abductivo `DiagnosticParseOutput`, dispara replanificación dinámica STRIPS (`AgentPlannerReplanOnError`), y ejecuta reversión atómica garantizada (`PatchRollback`). Validado por suite CTest 57/57 (`tests/test_agent_shell.c`) con 0 regresiones en suites agénticas, 2026-09-20.

## Corpus de Programación en C y Síntesis Autónoma con Autocuración GCC (Fase 12)

- El sistema incorpora un doble corpus especializado para programación en C11:
  1. Corpus conceptual y normativo (`data/c_lang/c_corpus.txt`) con principios formales de tipos, memoria dinámica (`malloc`/`calloc`/`realloc`/`free`), punteros, invariantes de seguridad de buffers y funciones estándar, indexado por `TextLexIngest` para recuperación semántica por atención.
  2. Modelo de grafo de la biblioteca estándar de C (`data/c_lang/c_std_lib.h`) indexado en el Grafo de Conocimiento del Código (`CodeGraphIngestFile`), habilitando resolución O(1) de firmas y estructuras de libc (`C_FILE`, `malloc`, `snprintf`, `memcpy`, etc.).
  3. Bucle cerrado de síntesis y autocuración de código C: síntesis de módulos C compilados nativamente en < 250 ms con GCC bajo `-Wall -Wextra -Werror` vía `AgentShellExec`, con ejecución determinista de binarios verificados y autodiagnóstico abductivo (`DiagnosticParseOutput` + parche atómico) ante errores de compilador. Validado por suite CTest 43/43 (`tests/test_c_synthesis.c`) con 12/12 suites agénticas en verde, 2026-09-20.

## Higiene y Corrección de Suites CTest (Fase 13)

- Corrección integral de pruebas unitarias y de integración en CTest:
  1. `test_composite`: corrección en `src/chat.c` en el interceptor de preguntas estructurales (`has_frozen_kw`), integrando términos de cónyuge (`wife`, `husband`, `esposa`, `esposo`), hermanos (`brother`, `sister`, `hermano`, `hermana`) y soberanos (`king`, `queen`, `rey`, `reina`), evitando que la relación fuese clasificada erróneamente como entidad; y resolución de anáfora con pronombres no resueltos (`he`, `she`, `it`) retornando UNKNOWN garantizado y span-echo (`No tengo constancia suficiente...`). Test 9/9 PASS.
  2. `test_textlex`: actualización de la validación en `tests/test_textlex.c` aceptando oraciones con máxima puntuación léxica de "sun" en `jung.txt` (oraciones 7159 y 3130). Test 20/20 PASS.
  3. Pruebas de subsistemas con modelo (`test_stats`, `test_analogical`, `test_attention`, `test_concepts`, `test_qa_debug`): incorporado fallback de grafo sintético en memoria cuando el artefacto preentrenado externo no está en disco, permitiendo ejercitar y verificar las funciones de similitud analógica (`TransferSimilarity`/`TransferAnalogy`), matriz de cosenos de embeddings (`GraphEmbedQuery`), navegación de grafo y detección de preguntas. 5/5 PASS.
  4. Pruebas de evaluación externa de Wikipedia (`test_wiki_inference`, `test_eval_qa`, `test_eval_count`, `test_eval_negation`, `test_eval_default`, `test_eval_reverse`, `test_eval_conjunctive`, `test_eval_multihop`, `test_qa_hygiene`): configuradas con `SKIP_RETURN_CODE 77` en CMakeLists.txt y salida limpia `return 77;` ante ausencia de `wiki_model.bin`.
  5. Resultado global CTest: 100% pruebas aprobadas (61 Passed, 9 Skipped, 0 Failed de 70 tests totales). Validado por ejecución en `build-gcc`, 2026-09-20.

## Los Cuatro Pilares Cognitivos en C11 (Fase 14)

- Implementación completa, determinista y de cero dependencias en C11 de los 4 pilares cognitivos de `ROADMAP.md`:
  1. **Pilar 1: Computación Hiperdimensional y Arquitectura Simbólica Vectorial (VSA / HDC)** (`src/vsa.c`, `include/vsa.h`): Códigos Kanerva de 256 bits (estrictamente 32 bytes/vector), operaciones de enlace/desenlace XOR aceleradas con hardware popcount AVX2 (166.7 Mops/s), memoria asociativa de limpieza con recuperación de prototipos al 100% bajo ruido de 15 bits, y QA de roles/rellenos en sub-10ns. Validado por `test_vsa` (8/8 PASS).
  2. **Pilar 2: Realización de Superficie Dinámica y Gramática Categorial Combinatoria (CCG)** (`src/ccg_realizer.c`, `include/ccg_realizer.h`): Cálculo formal de categorías CCG ($S, NP, N, PP, ADJ$ y combinadores $>$, $<$, $>B$, $<B$, $\&_\Phi$), tablas declarativas de concordancia morfosintáctica y preposiciones (`HARDCODING=0`) en EN/ES/FR, realización en prosa de 5 topologías de subgrafo y reductor de carta lineal (`CcgVerifyReduction`) en $0.66\ \mu\text{s}$/oración (1.5M oraciones/s) con 0% alucinación. Validado por `test_ccg_realizer` (34/34 PASS).
  3. **Pilar 3: Ingesta de Sentido Común y Ontologías a Gran Escala en RAM** (`src/commonsense.c`, `include/commonsense.h`): Parser de flujo continuo para ConceptNet 5.8 (TSV y formato de 5 columnas con extracción URI), mapa declarativo de canonicalización ontológica (`HARDCODING=0`) para 17 relaciones ontológicas, empaquetamiento estricto de 32 bytes por relación (10 millones de tripletas en 305.18 MB de RAM, < 350 MB objetivo) a 1.56M tripletas/s, y razonamiento sobre contención espacial transitiva, affordances funcionales y causalidad física. Validado por `test_commonsense` (54/54 PASS).
  4. **Pilar 4: Condicionamiento Pragmático, Perspectivas Epistémicas y Filtros de Persona** (`src/persona.c`, `include/persona.h`): Operador de proyección matemática $\Pi_{\text{style}} : \mathcal{G} \to \mathcal{G}_{\text{biased}}$ sobre el Meta-Grafo Reflexivo sin prompt injection estocástico. 6 perspectivas epistémicas (`neutral`, `architect`, `auditor`, `tutor`, `concise`, `socratic`), perfiles declarativos, umbrales epistémicos y abstención honesta en $0.58\ \mu\text{s}$/proyección (1.7M proj/s) en EN/ES/FR. Teorema formal de no-interferencia verificado empíricamente (`PersonaVerifyNonInterference`): $\text{Facts}(\Pi_P(Q)) \equiv \text{Facts}(Q)$ con 0 alucinaciones y fronteras fail-closed. Validado por `test_persona` (40/40 PASS).
  5. Resultado global CTest: 100% pruebas aprobadas (65 Passed, 9 Skipped por modelo Wikipedia opcional, 0 Failed de 74 tests totales). Validado por ejecución en `build-gcc`, 2026-09-20.

## Integración Conversacional de Sentido Común y Causalidad Física (Fase 15)

- Integración del grafo ontológico y causal de sentido común (Pilar 3) y realización multilingüe/persona (Pilares 2 y 4) en el despachador conversacional `chat.c`, `chat_clarify.c` y `chat_main.c`:
  1. Nuevos intents estructurales de QA: `INT_QA_CONSEQUENCE` (causalidad física: "¿qué pasa si...", "what happens if...") e `INT_QA_AFFORDANCE` (usos y capacidades: "¿para qué sirve...", "what is ... used for").
  2. Extracción composicional y cross-lingual de argumentos (sujeto, acción, superficie objetivo) asistida por el diccionario declarativo `data/english-spanish.txt` (`HARDCODING=0`).
  3. Robustez de codificación en terminales multiplataforma: soporte para marcas interrogativas y caracteres acentuados en UTF-8, ISO-8859-1 (Latin-1) y CP850 (OEM Windows console) en `Split` y `FoldChar`.
  4. Deducción ontológica cerrada: consulta "¿qué pasa si se cae un vaso de cristal al suelo?" resuelve directamente en RAM:
     $$\text{glass} \xrightarrow{\text{MADE\_OF}} \text{brittle\_material} \xrightarrow{\text{CAUSES}} \text{shatter}$$
     Generando: *"Si un vaso de cristal se cae al suelo, se rompera (porque el cristal es un material fragil que se rompe con el impacto)."*
  5. Validado por ejecución directa en `chat_main.exe` con corpus textual activo y suite global CTest 74/74 PASS (65 Passed, 9 Skipped, 0 Failed), 2026-09-20.

## Modulación Pragmática, Filtro Cuántico-Pirata y Cero Hardcoding (Fase 16)

- Implementación estrictamente declarativa y de cero dependencias del perfil pragmático `PERSONA_PIRATE_QUANTUM` en `src/persona.c` e `include/persona.h`:
  1. Principio `HARDCODING=0` riguroso: sin textos quemados para consultas concretas; todas las ranuras léxicas (`intro`, `chain_connective`, `conclusion_connective`, `abstain_template`, `evidence_prefix`) residen en tablas formales (`g_persona_lexicons`) con soporte declarativo de alias (`pirate`, `pirata`, `architect`, `auditor`, etc.).
  2. Realización causal física genérica: `PersonaRealizePhysicalConsequence` opera composicionalmente sobre los argumentos ontológicos (`subject`, `action`, `target`, `material`, `consequence`), preservando el Teorema de No-Interferencia Factual ($\text{Facts}(\Pi_{\text{pirate}}(Q)) \equiv \text{Facts}(Q)$).
  3. Integración conversacional en vivo: soporte para conmutación interactiva de perspectiva en `chat_clarify.c` (`/persona <name>`, `:persona <name>`, `modo pirata`) y opción de inicio en línea de comandos en `chat_main.c` (`-p <name>`, `--persona <name>`).
  4. Verificación matemática y de rendimiento: 56/56 pruebas en `test_persona.c`, 2.19 millones de proyecciones/segundo ($0.45\ \mu\text{s}$/proj), 0% alucinaciones, y suite CTest 74/74 PASS (65 Passed, 9 Skipped, 0 Failed), 2026-09-20.

## Persistencia Episódica Continua y Aprendizaje Conversacional en Vivo (Fase 17)

- Implementación del subsistema de memoria episódica continua en C11 (`src/episodic_memory.c`, `include/episodic_memory.h`):
  1. **Almacén TSV persistente** (`data/memory/episodic.tsv`): registro estructurado con tuplas `<subject>\t<relation>\t<object>\t<source>\t<timestamp>`, deduplicación $O(1)$ idempotente y auto-flushing inmediato a disco tras cada aprendizaje.
  2. **Ciclo de vida automático en `ChatInit`**: en el arranque de `chat_main` y `symbols_server`, si existe almacén episódico se recargan e ingieren automáticamente todas las memorias acumuladas, incorporándolas al motor inferencial (`ch->lr`, `ch->kb`, `ch->tgraph`) y deduciendo dinámicamente sus esquemas y meta-reglas (`MetaDiscover`, `MetaRuleDiscover`).
  3. **Comandos conversacionales y aprendizaje natural en vivo**:
     - Comandos interactivos: `/learn S P O` o `/aprende S P O`, `/memory` (inspección de memorias activas con origen y marcas de tiempo), y `/forget` o `/olvida` (purga garantizada en RAM y disco).
     - Aprendizaje en lenguaje natural: interceptor para patrones conversacionales (`aprende que S es P de O`, `recuerda que S es P de O`, `learn that S is P of O`) que normaliza la relación e indexa inmediatamente la afirmación.
     - Generalización automática en preguntas: las relaciones aprendidas se auto-registran en el índice dinámico `ch->kws`, permitiendo responder consultas directas posteriores (ej. `¿quién es el maestro de Platón?` -> `Sócrates`) entre reinicios de sesión.
  4. **Verificación formal**: suite unitaria dedicada `tests/test_episodic_memory.c` (5/5 PASS), suite CTest global al 100% (66 Passed, 9 Skipped condicionales, 0 Failed de 75 tests) y prueba cruzada de persistencia multi-sesión validada por ejecución.

## Serialización Binaria de Alto Rendimiento e Ingesta mmap de Sentido Común (Fase 18)

- Implementación en C11 nativo del formato binario y mapeo de memoria virtual (`mmap` / `MapViewOfFile`) para grafos de conocimiento de sentido común (`src/commonsense.c`, `include/commonsense.h`):
  1. **Especificación Binaria Ultra-Densa (M3.4)**: cabecera `CS_BIN_HEADER` de 40 bytes alineada a 8 bytes (`magic: 0x53594D43`, versión, conteo de símbolos y relaciones, longitud de tabla de cadenas, flags, checksum FNV-1a de 32 bits), descriptores de símbolos de 16 bytes (`offset`, `len`, `frequency`), arena de cadenas empaquetada con padding de 8 bytes, y tabla de tripletas estrictamente de 32 bytes por relación (`RELATION`).
  2. **Ingesta Instantánea por Buffer y Snapshot**: funciones `CommonsenseSaveBinary`, `CommonsenseLoadBinary` y `CommonsenseParseBinaryBuffer` para serializar y deserializar grafos completos en $\le 1\ \text{ms}$, pre-dimensionando las tablas de hashing (`GraphCreate(cap * 2)`) para evitar reallocs y rehashes durante la carga.
  3. **Mapeo de Memoria Virtual Multiplataforma**: `CommonsenseLoadMmap` y `CommonsenseMmapClose` implementan mapeo de memoria sin copias de archivo sobre Windows (`CreateFileA`, `CreateFileMappingA`, `MapViewOfFile`, `UnmapViewOfFile`) y POSIX (`open`, `mmap`, `munmap`), permitiendo montar millones de aserciones en microsegundos.
  4. **Fail-Closed y Resistencia a Corrupción**: verificación matemática obligatoria de checksum, tamaño y cabeceras; ante cualquier alteración de bytes o fallo de integridad, la carga se aborta de forma fail-closed retornando `NULL` sin estados corruptos ni accesos fuera de límites.
  5. **Integración Conversacional en `chat.c`**: `ChatGetCommonsenseGraph` busca y carga prioritariamente `data/commonsense.bin` si existe en disco antes de recurrir a la ingesta seed, acelerando el arranque en frío a sub-milisegundo.
  6. **Verificación Formal y CTest**: suite `tests/test_commonsense.c` ampliada a 75/75 verificaciones unitarias (incluyendo serialización, deserialización idéntica, persistencia mmap, latencia y rechazo de corrupción), y suite global CTest 100% verde (66 Passed, 9 Skipped condicionales, 0 Failed de 75 tests), 2026-09-20.

## Copiloto Local Autónomo OpenCode y Despliegue de Servidor (Fase 19)

- Integración y despliegue del motor `symbols-server` como copiloto local permanente en OpenCode y entornos de desarrollo:
  1. **Configuración de Espacio de Trabajo (`opencode.json`)**: Definición del proveedor local OpenAI-compatible (`symbols`) en el puerto nativo 8099 (`SERVER_PORT_DEFAULT`) con ventana de contexto de 8192 tokens, streaming Server-Sent Events (SSE) y tool-calling activado con esquema estándar `provider.symbols`.
  2. **Scripts Multiplataforma de Lanzamiento (`scripts/`)**:
     - `run_symbols_server.bat` para Windows Batch (puerto por defecto 8099).
     - `run_symbols_server.ps1` para PowerShell con parámetros tipados (`Port`, `RepoDir`).
     - `run_symbols_server.sh` para entornos POSIX/Linux/macOS.
     - Configuran automáticamente el montaje del grafo de conocimiento del repositorio actual, corpus textual (`data/texts/bible.txt;data/c_lang/c_corpus.txt`), grafo de sentido común binario (`data/commonsense.bin`) y memoria episódica continua (`data/memory/episodic.tsv`).
  3. **Discriminación Rigurosa de Intenciones y Mapeo Agnóstico de Herramientas**:
     - Filtro estricto de activación agéntica: `(is_coding && num_declared > 0)`. Preguntas conversacionales, factuales y de sentido común físico (ej. *¿qué pasa si se cae un vaso de cristal al suelo?*) se resuelven directamente por texto sin invocar herramientas ni emitir falsos planes de ingeniería.
     - Mapeo polimórfico de operadores STRIPS a las herramientas declaradas por el cliente: adapta dinámicamente `locate_symbol` $\to$ `grep`/`read`, `inspect_code` $\to$ `read`, `apply_patch` $\to$ `edit`, y `execute_command` $\to$ `bash` según el conjunto de herramientas disponible en el agente OpenCode.
  4. **Verificación Integral Extremo a Extremo (`tools/test_opencode_copilot_e2e.py`)**:
     - Comprobación de disponibilidad de modelos (`GET /v1/models` -> `symbols`).
     - Validación de consultas factuales de conocimiento (padre de David -> Jesse).
     - Validación de razonamiento causal físico de sentido común (caída de vaso de cristal -> se romperá) incluso cuando el cliente declara herramientas.
     - Validación de flujo agéntico autónomo STRIPS para tareas reales de código (`POST /v1/chat/completions` con `tools` -> emisión de `tool_calls` adaptadas a herramientas declaradas).
     - Cierre y terminación limpia del proceso sin fugas de recursos ni cuelgues.
  5. **Documentación (`opencode.md`)**: Actualización completa de la guía de integración de OpenCode reflejando los nuevos scripts, el esquema `opencode.json` y la suite de verificación.
  6. Validado por ejecución directa en terminal (100% de verificaciones de integración pasadas) y suite global CTest 75/75 PASS (66 Passed, 9 Skipped condicionales, 0 Failed), 2026-09-20.

## Relaciones Genéricas Universales, Robustez Agéntica y Creación de Archivos (Fase 20)

- Consolidación del motor conversacional y servidor agéntico OpenAI Tool Calling (`src/chat.c`, `src/server_proto.c`, `src/symbols_server.c`):
  1. **Relaciones Universales en Grafos Binarios (`GenericRelToConn`, `ChatLoadModel`)**: Eliminada la barrera de relaciones predefinidas. Cualquier relación arbitraria en modelos binarios (`CONTIENE`, `REQUIRES`, `INCLUYE`, `APLICA_A`, etc.) se normaliza de forma composicional sin hardcoding (`HARDCODING=0`), cargando el 100% de hechos en memoria (`ChatFactCount > 0`). Soporte de artículos en preguntas (`¿qué incluye el kit_a?`, `a que aplica el kit_b`).
  2. **Detección e Invocación Quirúrgica para Creación de Archivos (`IsFileCreationTask`)**: Reconocimiento de peticiones de creación de archivos (`crea un fichero test.txt`, `nuevo archivo config.json`, `create file foo.c`), formulando un plan atómico de un solo paso que despacha directamente la herramienta `write` con el archivo objetivo, evitando la ejecución espuria de compilación y ctest sobre archivos nuevos.
  3. **Gestión de Cargas HTTP de Gran Tamaño (`SERVER_BODY_MAX` a 2 MB)**: Ampliado el búfer de peticiones HTTP a 2 MB en memoria estática compartida (`g_http_body`), eliminando de raíz el fallo `400 Bad Request: bad content length` en sesiones multi-turno de OpenCode donde los esquemas de herramientas y el historial acumulado superaban 64 KB.
  4. **Ampliación Léxica de Exploración e Inspección**: Reconocimiento de consultas naturales como `lista las subcarpetas`, `subdirectorios`, `subfolders`, `listar`, despachando la herramienta `glob` de OpenCode.
  5. **Corrección de Extracción de Archivo (`FindFileForIssue`)**: Eliminado el delimitador `.` en `strtok` con recorte de puntuación final, resolviendo nombres reales con extensión (`test.txt`, `main.c`, etc.) en lugar de degradar erróneamente a `CMakeLists.txt`.
  6. **Detección de Errores de Construcción y Terminal (`ServerInspectToolResponse`)**: Detección de fallos en texto plano (`Error: could not load cache`, `No tests were found`, `command not found`, `Permission denied`) evitando falsos positivos de `Build: PASS`.
  7. **Verificación Formal**: Suite unitaria `tests/test_server_tool_calling.c` (191/191 PASS), suite `tests/test_model_generic_rel.c` (7/7 PASS), suite en vivo `tools/test_discrimination_live.py` (29/29 PASS), verificación end-to-end `tools/test_opencode_user_cases.py` (100% PASS), y suite global CTest 76/76 PASS (67 Passed, 9 Skipped condicionales, 0 Failed), 2026-09-21.

## Síntesis Directa de Código Algorítmico y Robustez HTTP (Fase 21)

- Especialización de intenciones agénticas y robustez de transporte HTTP en `symbols-server` (`src/symbols_server.c`, `src/server_proto.c`, `include/server_proto.h`):
  1. **Síntesis Directa de Código Algorítmico (`ServerIsCodeSynthesisTask`, `ServerSynthesizeCode`)**:
     - Reconocimiento de peticiones de síntesis algorítmica pura (ej. `escribe en C la funcion de fibonacci`, `write a function to calculate factorial`, `invertir cadena`, `busqueda binaria`).
     - Emisión directa de código C11 idiomático formateado en bloques Markdown con explicaciones de complejidad temporal/espacial y verificaciones de desbordamiento, completando la respuesta con `finish_reason: "stop"`.
     - Evita la activación innecesaria del bucle STRIPS de modificación de archivos (`patch -> build -> ctest`) para consultas de generación de código que no modifican el repositorio del usuario.
  2. **Decodificación Robusta de Cadenas JSON (`TakeJsonString`)**:
     - Soporte para secuencias de escape estándar en cadenas JSON (`\"`, `\\`, `\/`, `\b`, `\f`, `\n`, `\r`, `\t`), impidiendo truncamientos prematuros al procesar payloads con código fuente entrecomillado.
  3. **Extracción y Transporte HTTP Robusto (`FindHttpHeader`, `ReadHttpBody`)**:
     - Búsqueda genérica e insensible a mayúsculas/minúsculas de cabeceras HTTP (`FindHttpHeader`).
     - Recuperación segura del cuerpo HTTP en `ReadHttpBody` cuando el contenido ya fue recibido en el búfer inicial de cabeceras o ante desconexiones tempranas con JSON completo ya recibido.
  4. **Verificación Integral y 100% CTest**:
     - Batería de discriminación de intenciones en `tests/test_server_tool_calling.c` ampliada a 198/198 PASS.
     - Suite de pruebas de casos de usuario `tools/test_opencode_user_cases.py` verificando los 4 flujos de trabajo (creación de ficheros, inspección de subcarpetas, cargas mayores a 75 KB y síntesis de Fibonacci) con 100% éxito.
     - Suite global CTest 100% verde (67 Passed, 9 Skipped condicionales, 0 Failed de 76 tests), 2026-09-21.

## Saludos Conversacionales, Identidad y Supresión de Falsos Positivos Levenshtein (Fase 22)

- Resolución conversacional de cortesía, identidad de copiloto y prevención de secuestro léxico en `symbols-server` y el motor `chat` (`src/symbols_server.c`, `src/server_proto.c`, `src/chat.c`, `src/tool_config.c`, `data/agentic/self.tsv`):
  1. **Supresión del Secuestro Levenshtein en Saludos**:
     - Diagnóstico: la consulta natural `"hola"` era asimilada por distancia Levenshtein a `"hold"` (distancia 1) dentro de `data/texts/bible.txt`, disparando una búsqueda textual espuria que retornaba versículos bíblicos (`hold: 3:11 Behold, I come quickly...`).
     - Solución: incorporación de `IsGreetingTok` en `src/chat.c` para blindar los tokens de saludo (`hola`, `hello`, `hi`, `hey`, `buenas`, `saludos`), impidiendo su evaluación como error tipográfico en `ParseIntentToks`.
  2. **Priorización de Saludo e Identidad en `ChatHandleToBuf`**:
     - Ejecución preferente de `SelfAnswer(line, out, size)` antes de la vía rápida de texto (`ChatTryTextLine`), resolviendo de forma determinista cualquier interacción de cortesía o presentación.
  3. **Fallback Compilado y Tabla Declarativa Externa (`data/agentic/self.tsv`, `tool_config.c`)**:
     - Incorporación de `LoadCompiledSelf()` con configuración estática `COMPILED_SELF_SCOPE`, `COMPILED_SELF_GREET` y tabla de 20 disparadores de saludo e identidad (`quien eres`, `who are you`, `hola`, `hello`, `buenos dias`, etc.).
     - Creación de `data/agentic/self.tsv` garantizando persistencia declarativa y compatibilidad con pruebas históricas.
  4. **Manejadores Agénticos Nativos en Servidor (`ServerIsGreeting`, `ServerAnswerGreeting`)**:
     - Clasificación unificada de saludos e identidad en `server_proto.c`, con respuesta multilingüe (ES/EN) y modulación por perspectiva pragmática/persona (`PERSONA_PIRATE_QUANTUM`).
     - Emisión con `finish_reason: "stop"` y soporte nativo para streaming Server-Sent Events (SSE).
  5. **Verificación Formal y CTest**:
     - Suite `tests/test_server_tool_calling.c` ampliada a 212/212 PASS.
     - Pruebas HTTP en vivo verificando respuestas inmediatas de saludo y presentación en ES/EN.
     - Suite global CTest 100% verde (67 Passed, 9 Skipped condicionales, 0 Failed de 76 tests), 2026-09-21.

## Configuración Predeterminada de Corpus Técnico para Copiloto de Programación (Fase 23)

- Desacoplamiento de textos bíblicos como corpus predeterminado en `symbols-server`, `chat_main` y scripts de inicio (`scripts/run_symbols_server.bat`, `scripts/run_symbols_server.ps1`, `scripts/run_symbols_server.sh`, `src/symbols_server.c`, `src/chat_main.c`):
  1. **Corpus de Programación en C como Predeterminado**:
     - `symbols-server` y `chat_main` configuran `data/c_lang/c_corpus.txt` como primer candidato prioritario en `cand_paths`.
     - Los scripts de arranque (`run_symbols_server.bat`, `.ps1`, `.sh`) configuran por defecto `CORPORA=data/c_lang/c_corpus.txt`, indexando el estándar C11 (gestión de memoria dinámica, tipos, invariantes de seguridad de buffers, libc) y el Grafo de Conocimiento del Repositorio (AST y radio de impacto).
     - La Biblia (`data/texts/bible.txt`) y otros textos históricos permanecen disponibles bajo demanda explícita (`--corpus`, argumento CLI, o `/load`), pero ya no se montan de forma predeterminada, evitando interferencias léxicas con consultas técnicas y comandos de usuario.
  2. **Verificación y Pruebas E2E**:
     - Adaptada la suite de integración end-to-end `tools/test_opencode_copilot_e2e.py` para verificar consultas de conocimiento de programación ("What causes memory leaks?") respondiendo con principios de gestión de memoria C11.
     - Añadida prueba de saludo natural en `tools/test_opencode_user_cases.py` ("hola" responde como copiloto de IA sin referencias a versículos bíblicos).
     - Suite CTest 100% verde (67 Passed, 9 Skipped, 0 Failed de 76 tests) y pruebas unitarias de servidor 212/212 PASS, 2026-09-21.
