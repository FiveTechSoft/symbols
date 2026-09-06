# Roadmap: symbols and relations

Core doctrine: the map holds only symbols and relations. Meaning is
never hardcoded: it is read off the live graph (morphology, ranking,
embeddings) or taught explicitly (`/learn`). Unknowns are stored or
admitted, never fabricated.

## Goal: tinyllama-class behavior, with zero LLMs

The target is to **behave like a small LLM** — converse fluently,
know things, reason — using the purely symbolic architecture.
**No external LLM may support it** (decided 2026-09-05; the hybrid
TinyLlama/llama.cpp proposal is rejected, see `ROADMAP_TINYLLAMA.md`).
Parity is about observable behavior, not architecture: exact O(1)
memory, auditable answers, and a bounded NLG that only echoes stored
symbols.

## Measured state (verified 2026-09-05)

- Suite 32/32 (incluye `test_eval_count` 22/22, `test_eval_negation`
  20/20, `test_eval_default` 18/18, `test_eval_reverse` 8/8 y
  `test_neuro_prolog` 36/36), eval 87/87, hygiene 87/87,
  lint 4155/0/0.
- Model: 5,783 symbols / 3,430 relations (math + Iconclass included,
  post P3 corpus junk-cleanup: junk_pred/junk_obj fuera del corpus).
- Dynamic vocabulary: question tokens resolve against used relations
  (exact, stemmed, affix-ranked, embedding-ranked); learned words
  work immediately.
- Trust tiers: curated triples (with provenance) outrank grown noise.
  Bulk text (Quijote, 3513 lines) ingests without degrading curated
  answers (87/87 on clean and grown maps alike).
- Tree ingest: input → clauses → symbols → relations, positional,
  multi-triple per input, coordination splitting via census.
  Anaphora wired in: file ingest resolves across sentences and
  pushes entities back.
- Honest answers: exact triples or unknown; unknowns route to storage.
- Descriptors ranked (trust, rarity, affix); answers ordered by
  semantic-area coherence (composed relation vectors).
- Commands: `/find`, `/area` (neighborhood), `/about` (topic mass),
  `/learn`, `/ingest` (2 passes), `/alias`, `/synonyms`, `/analogy`.
- 32D embeddings + per-relation composition vectors: area-coherence
  ranking (`ParserRankByArea`) and semantic descriptor resolution
  (`ResolveRelationEmbed`).
- Cosine-based "attention" (`GraphQueryAttended`, `GraphQueryByEmbedding`)
  is a light, interpretable, deterministic ranking — NOT positional
  encodings and NOT learned self-attention (no QKV, no learnable
  weights; architecture decision 2026-09-05).
- CI: MSVC + ASan (Windows) and gcc (Linux); `lint_corpus` is a gate;
  ingest tests use scratch bins so the tracked golden `wiki_model.bin`
  is never clobbered.

## Active plan (camino propio — no LLM)

- [x] P1 — Bucle de crecimiento medido: extractores → lint → regen →
  eval (`tools/progress.py` + `tools/progress.csv`, columns
  model_relations/model_symbols).
- [ ] P2 — Profundidad de QA: multi-hop, conteo, comparación, negación.
  Doctrina: sin tipos nuevos en el core; agregación en el camino de QA
  y datos tipados colgados de símbolos, igual que `source`. Cada hito
  trae su set (`tests/qa_eval_<cap>.tsv`, ~20 filas curadas + verificadas
  por ejecución), umbral 90, sin regresión (87/87, 38/40, suite) e
  inmunidad Quijote. Baselines medidos 2026-09-06 (dorado actual):
- [x] M2 — Conteo (primero: fino y sobre triples existentes). Preguntas
  `¿Cuántos/cuántas X tiene Y?` agregan los objetos recuperados y
  verbalizan el numeral (`RelationCountBySubjectRelation` exacto, nunca
  muestra truncada; `is_count` en `QUESTION`). Hoy: `tests/qa_eval_count.tsv`
  22/22 en suite (`test_eval_count`). Sin datos nuevos.
  - [x] M1 — Negación: ingesta `X no es Y` → triple NEGATIVO
    (polaridad V4 persistida; ALLOW_BOTH auditable); preguntas
    `¿X no es Y?` verifican (S,R,O) con swap documentado y responden
    Sí/No/i18n + disputas contadas; los negativos nunca listan como
    hechos. `tests/qa_eval_negation.tsv` 20/20. Glue cerrado como
    hechos gramaticales; `NO` reservado en resolución.
  - [ ] M4 — Multisalto capado a 2 (composición, no tipos): resolver la
    interior, sustituir por la entidad, resolver la exterior
    (`la capital del país con moneda X` → país → capital). Baseline: 0/1.
    Set: `tests/qa_eval_multihop.tsv`.
  - [ ] M3a — Corpus numérico (prerrequisito): el extractor hoy tira los
    números; medidas con unidad como sidecar tipado (valor+unidad
    colgado del símbolo, nunca objeto-relación).
  - [ ] M3b — Comparación (`más/menos`, `mayor/menor`, `antes/después`)
    sobre el sidecar M3a. Baseline: 0/1. Set: `tests/qa_eval_compare.tsv`.
  Orden: M2 → M1 → M4 → M3a → M3b (valor/coste).
  - [x] M5 — Defaults no-monótonos por especificidad (adelantado: el
    sustrato estaba listo). Sin cuantificadores: se camina ES hacia
    arriba y el nivel más cercano con evidencia decide por objeto
    (positivo lista, negativo excluye; todo denegado responde No.).
    Read-only (nada derrotable se materializa). Pingüino nada pero no
    vuela; Piolín el canario sí vuela. `tests/qa_eval_default.tsv`
    18/18.
  - [x] M7 — Variables en query de un hueco (reverse QA, adelantado por
    el zebra: gap #3). Roles por evidencia: slot vacío + reverso
    no vacío ⇒ swap ((S,R)→∅ y (R,S-como-O)→lleno responde sujetos).
    Sin búsqueda: un hueco, escaneo indexado, disputa igual que slots.
    `tests/qa_eval_reverse.tsv` 8/8. Multihueco y backtracking quedan
    fuera (solver-sidecar futuro).

## Reasoning distance (measured, not claimed)

Reasoning here = composition over stored facts where every step cites
triples (unlike LLM chain-of-thought, unverified text). Single-hop
lookup is retrieval, not reasoning. The Reasoning Index is a VECTOR
(no single-number gaming); each row names set, baseline, gate:

- R1 lookup: `qa_eval` 87/87 (+hard 38/40). Retrieval baseline.
- R2 counting: `qa_eval_count` 22/22 (M2 ✓, aggregation over sets).
- R3 negation: `qa_eval_negation` 20/20 (M1 ✓: polar ingest +
  (S,R,O) verification + dispute format; V4 persists polarity).
- R4 2-hop chaining: 0/1 → M4 (substitute-then-ask over ground facts).
- R5 comparison: 0/1 → M3 (no numeric data today).
- R6 analogy: unit-green (`test_analogical`), map-silent 0/4
  (ROMA/PARIS vs ROMA/EURO all score 0.00 on the golden). Needs a
  firing investigation (threshold vs embedding coverage) before any
  ranking set.
- R7 contradiction: storage + policies unit-tested
  (`test_contradictions`), never ingested, never surfaced (M1).
- R8 default inheritance: `qa_eval_default` 18/18 (M5 ✓: ES-walk with
  per-object specificity, denials exclude, all-denied answers No.).
- R9 single-hole variables: `qa_eval_reverse` 8/8 (M7 ✓: evidence
  roles, indexed scan, no search). Multi-hole joins need the solver
  sidecar (queued).

Zebra probe (`houses_puzzle.pl`, SWISH, 15 rules): reified givens in
a fresh graph answer stored lookups 3/3 and honest-unknown on
everything else (L0 ✓, inference 0); the machine-checked oracle
(all 15 rules asserted in Python, playing Prolog's role outside the
engine) answers 8/8. The gap decomposes exactly: (1) no variables —
ground-only triples cannot hold unknown bindings; (2) no backtracking
search — nothing explores assignments (M4 chains ground facts only,
a weaker thing); (3) no reverse QA — `GraphQueryObject` exists but
QA never uses it, so "who owns X" is unaskable even answered.
Side note: the SWISH code admits 2 full solutions (green/white swap);
both agree on water/zebra — machine-checking earns its keep.

Protocol for reasoning probes: scratch TSVs + fresh graphs + freshly
linked harnesses, NEVER the golden (a read-only CLI session once grew
it +3/+2 via YO self-seeding + autosave). Since the stale-lib
incident (2026-09-06: `libsymbolic.a` predated the headers and a
harness read `valid=222`), every rebuild is timestamp-checked
(obj > src, lib > obj) before measuring.
- [x] P3 — NLG acotada y honesta ("no lo sé" como feature) + i18n:
  - [x] Pasar UI y respuestas de QA a **EN por defecto** (`c1df0c6`:
    prefijo "AI >", UI de main_cli y todos los conectores de
    `nlg.c`/`dialog.c` en inglés vía `i18n.c`).
  - [x] Capa i18n de ES/FR (conectores, aperturas, respuestas,
    desconocidos), conmutable en caliente sin tocar el modelo ni sus
    datos: comando `/lang EN|ES|FR` + `LangSet/LangGet` global.
  - [x] Entrada agnóstica de idioma: sin detección de idioma; la
    resolución es por similitud semántica contra el mapa vivo
    (exacto → stemmeado → embedding → atención; ya existía en el parser).
  - [x] Auto-referencia honesta: copulas conjugadas (soy/eres/somos…
    → ES; estoy/… → ESTAR, solo si la base es relación usada, tabla
    cerrada no semántica); interrogante "QUIEN" sin entidad →
    autoconcepto (YO) si existe en el grafo; el REPL siembra
    YO--ES-->MODELO_SIMBOLICO/LLM_DE_CONVERSACION (idempotente) y NLG
    emite "Soy /I am /Je suis …". Las preguntas nunca se ingieren
    ('?' o tokens cerrados solo-de-pregunta), sin punctuation ni con
    ella: no hay triples fabricados (QUIEN--SOY-->YO). "Quedando
    fuera": QUE/DONDE/CUANDO/COMO comparten sintaxis con
    declarativas, piden el signo de interrogación. Suite 27
    (`tests/test_self_reference.c`).
- [x] P4a — Núcleo neuro-Prolog sobre el grafo (`src/neuro_prolog.c`,
  `include/neuro_prolog.h`, `tests/test_neuro_prolog.c`): `NPProve` une la
  consulta (sujeto/predicado/objeto, 16 variables) contra los hechos con
  checkpoints de frame (copia de 80 B), cierre de identidad por BFS
  (Leibniz: A ES B, B R C ⇒ A R C y simétrico), decaimiento 0.9^n por
  salto, umbral `min_conf`, tope `max_depth` (defecto 8) y circuito de
  soluciones con dedup + confianza máxima. Puente fuzzy γ entre el
  predicado pedido y el guardado (hoy coseno 32D; API agnóstica para el
  swap a Hamming 256-bit), solo si la fase exacta/inferida queda vacía.
  Sin tautologías X ES X. **Medición: 36/36 checks** — hecho directo,
  enumeración, P libre, 1–2 saltos, ambos extremos, min_conf,
  max_depth, ciclo sin loop, fallos honestos = 0, pinguino/NAF documentado
  como fuera de alcance, fuzzy on/off/gate y ~0.1 µs/consulta. Suite
  32/32, lint 4155/0/0. Frontera medida con M5: P4a solo ve
  triples positivos (polarity-blind por diseño) y responde VOLAR para
  Piolín por herencia; M5 ve la denegación específica (PINGÜINO) y
  responde No. Acuerdan en cadenas puramente positivas; divergen ante
  denegación — esa es exactamente la línea P4b (NAF).
- [ ] P4b — Horn + corte sobre P4a: reglas head/body declaradas, corte
  Prolog (`!`), NAF (pingüino no vuela aunque sus aves sí),
  cuantificadores y exposición de `RELATION.weight`,
  `GraphCheckContradiction` y `CONFLICT_POLICY` en las respuestas NL.
- [x] P5 — Set held-out con verdad humana: `tests/qa_eval_hard.tsv` creado
  (40 hechos verificados a mano, sin solapes con `qa_eval.tsv`, solo para
  medir). `tools/progress.py` portado a Linux (detecta `build/` y binarios
  sin `.exe`). Aspiracional sin umbral: hoy 38/40 = 95%; los 2 restos son
  artefactos del set, no del motor (KORONA vs CORONA_NORUEGA de es:WP;
  HUNGRARO es typo por HÚNGARO). El progreso = ver `hard_pass` subir
  sin tocar este set.

## Language & i18n

- Output: **English by default**; ES/FR added as pure i18n layers over
  the same symbols. Switching language must never mutate the graph.
- Input: any language. The "language" of a question is irrelevant; what
  matters is semantic similarity of its tokens to the live vocabulary
  (exact → stemmed → embedding → attention fallback).
- The cross-lingual gap (France ↔ FRANCIA, "capital" ↔ CAPITAL) is a
  vocabulary problem, not a parsing one: close it with multilingual
  aliases in the symbol table or multilingual embeddings (a data-side
  task, no core change).

## Whole-text processing

- Units keep storage order (chronology is free, no schema change).
- Cross-sentence anaphora wired into `/ingest` and dialog.
- Corpus gate: 50-triple precision sample before bulk ingest.
- Areas: coherence ranking + `/area` command (extractive only).
- Global questions: `/about` aggregates relation mass. Statistics,
  not semantics.

## Symbol datasets

- Freud (Gutenberg, public domain): piloted, rejected for bulk —
  English text against a Spanish map yields preposition predicates.
- Quijote (Gutenberg, public domain): piloted, rejected for bulk —
  literary syntax overwhelms the tree; short canonical input wins.
- Iconclass 25G subtree (open data): 115 triples wired in.
- CLDR Spanish annotations: proposed, not fetched.
- Jung: gray area (borrow-only scans, copyrighted translations).
  Process local copies only.

## Next (in order)

1. Frequency-purity creation gate: novel spans join only rare tokens,
   so bulk literary text stops gluing articles. Unlocks whole books.
    Gate: grown-map eval stays 87/87.
2. CLDR Spanish annotations: Spanish symbol→word vocabulary at scale.
3. Feedback-driven ranking weights (bandit over corrections + eval):
   today's hand-ordered ranks become learned, auditable numbers.
4. Retrieval scale: predicate-indexed prefilter past ~100k relations.
5. Sidecars (order beyond storage, numeric compare, n-ary): typed,
   hanging off symbols, never core changes.

## Known limits (sidecars, never core changes)

- Order beyond storage sequence, multi-hop QA, numeric comparison,
  n-ary relations, quantifier scope, virgin-map cold start, and the
  cross-lingual vocabulary gap (above).
- Strategy: typed sidecars hanging off symbols (as `source` does),
  never new primitive types in the core.

## Non-goals

- No external LLM of any kind (llama.cpp, GGUF, API models) as
  support: 100% symbolic path (decided 2026-09-05).
- No semantic special-casing in the core (kinship, counting,
  inference rules were removed on purpose and stay removed).
- No hardcoded word lists (open-class). Closed-class glue
  (delimiters, `?`, articles in the legacy path) is syntax.
- No generative NLG: verbalization echoes stored symbols.

## Decisions log

- 2026-09-05: external LLM / hybrid TinyLlama proposal rejected.
- 2026-09-05: architecture — per-relation composed embeddings +
  cosine "attention"; no positional encodings, no learned self-attention.
- 2026-09-05: i18n English by default (ES/FR); input is
  language-agnostic, resolved by semantic similarity against the map.
- 2026-09-05: P3 implemented in `c1df0c6` — `include/i18n.h` +
  `src/i18n.c` (15 template keys x 3 languages), connector strings no
  longer hardcoded in `nlg.c`/`dialog.c`, new `/lang EN|ES|FR` CLI
  command, `tests/test_i18n.c` (suite now 26).
- 2026-09-05: P5 baselines set — `tests/qa_eval_hard.tsv` (40
  human-verified held-out facts, 35/40 aspirational today; the 5 misses
  are the target: NO capital/currency, PL/SV/HU language).
  `tools/progress.py` now port-detects `build/` (Linux) vs
  `build-gcc/*.exe` (Windows).
- 2026-09-05: corpus honesto + retrieval — re-extracción es:WP con split
  brace-aware y limpieza fixpoint (`extract_infoboxes.py`); el linter
  dropea fragmentos (`markup_fragment`) y ausencias (`null_marker`) en
  vez de lavarlos; descriptores con co-ocurrencia de sujeto, prioridad
  exacta en desempates, recall completo en respuestas, folding de
  tildes en la medida. Eval curado a verdad de corpus (87 filas).
  Hard 35/40 -> 38/40 (techo honesto: KORONA/HUNGRARO son artefactos
   del set). Modelo dorado regenerado determinista (6165/3627).
- 2026-09-06: P4a medido — `src/neuro_prolog.c` +
  `include/neuro_prolog.h`: `NPProve` (unificación de 3 términos + 16
  variables con frame de 80 B checkpoint/rollback; cierre de identidad
  BFS, decaimiento 0.9^n, circuito de soluciones con dedup, umbral
  min_conf y tope max_depth; sin tautologías X ES X). Puente fuzzy γ solo
  si no hay hecho exacto/inferido y existe tabla de embeddings
  (coseno hoy, Hamming 256-bit mañana). Benchmark
  `tests/test_neuro_prolog.c`: 36/36 (~0.1 µs/consulta, cadena 2 saltos).
  Documentado como fuera de alcance hasta P4b: NAF, reglas head/body,
  cut (!), cuantificadores. Suite 28/28.
