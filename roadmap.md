# Roadmap: symbols and relations

Core doctrine: the map holds only symbols and relations. Meaning is
never hardcoded: it is read off the live graph (morphology, ranking,
embeddings) or taught explicitly (`/learn`). Unknowns are stored or
admitted, never fabricated.

## Measured state

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

- Order beyond storage sequence, numeric comparison, n-ary relations,
  quantifier scope, virgin-map cold start (first-contact joins glue).
- Strategy: typed sidecars hanging off symbols (as `source` does),
  never new primitive types in the core.

## Non-goals

- No semantic special-casing in the core (kinship, counting,
  inference rules were removed on purpose and stay removed).
- No hardcoded word lists (open-class). Closed-class glue
  (delimiters, `?`, articles in the legacy path) is syntax.
- No generative NLG: verbalization echoes stored symbols.
