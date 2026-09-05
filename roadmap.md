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

- Suite 25/25, eval 98/98, hygiene 98/98, lint 7724/0/0.
- Model: 13,600 symbols / 6,870 relations (math + Iconclass included).
- Dynamic vocabulary: question tokens resolve against used relations
  (exact, stemmed, affix-ranked, embedding-ranked); learned words
  work immediately.
- Trust tiers: curated triples (with provenance) outrank grown noise.
  Bulk text (Quijote, 3513 lines) ingests without degrading curated
  answers (98/98 on clean and grown maps alike).
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
- [ ] P2 — Profundidad de QA: multi-hop, conteo, comparación, negación;
  eval por categorías. Multi-hop y numérico quedan fuera del core por
  diseño: se implementan como sidecars tipados colgados de símbolos,
  igual que `source`.
- [ ] P3 — NLG acotada y honesta ("no lo sé" como feature) + i18n:
  - Pasar UI y respuestas de QA a **EN por defecto**.
  - Capa i18n de ES/FR (conectores, aperturas, respuestas, desconocidos),
    conmutable en caliente sin tocar el modelo ni sus datos.
  - Entrada agnóstica de idioma: sin detección de idioma; la resolución
    es por similitud semántica contra el mapa vivo.
- [ ] P4 — Razonamiento con confianza: exponer pesos y contradicciones
  en las respuestas (infraestructura ya existe: `RELATION.weight`,
  `GraphCheckContradiction`, `CONFLICT_POLICY`).
- [ ] P5 — Set held-out con verdad humana (deuda explícita:
  `tests/qa_eval_hard.tsv` lo referencia `tools/progress.py` y aún no existe).

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
   Gate: grown-map eval stays 98/98.
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
