# Roadmap: symbols and relations

Core doctrine: the map holds only symbols and relations. Meaning is
never hardcoded: it is read off the live graph (morphology, ranking,
embeddings) or taught explicitly (`/learn`). Unknowns are stored or
admitted, never fabricated.

## Measured state

- Suite 25/25, eval 98/98, hygiene 98/98, lint 7609/0/0.
- Model: 13,392 symbols / 6,755 relations (math corpus included).
- Dynamic vocabulary: question tokens resolve against used relations
  (exact, stemmed, embedding-ranked); learned words work immediately.
- Trust tiers: curated triples (with provenance) outrank grown noise.
- Tree ingest: input → clauses → symbols → relations, positional,
  multi-triple per input, coordination splitting via census.
- Honest answers: exact triples or unknown; unknowns route to storage.
- Bulk text (Quijote, 3513 lines) ingests without degrading curated
  answers (98/98 on clean and grown maps alike).

## Whole-text processing

- Units keep storage order (chronology is free, no schema change).
- Cross-sentence anaphora: structural topic tracking; TODO: wire it
  into the `/ingest` file path (today only the dialog path resolves).
- Corpus gate: 50-triple precision sample before bulk ingest.
- Areas: ranking by area coherence exists; TODO: `/area` command
  (extractive neighborhood report, no generative summaries).
- Global questions ("what is this text about"): aggregate by
  relation mass per area. Statistics, not semantics.

## Symbol datasets

- Freud (Gutenberg, public domain): piloted, rejected for bulk —
  English text against a Spanish map yields preposition predicates.
- Quijote (Gutenberg, public domain): piloted, rejected for bulk —
  literary syntax overwhelms the tree; short canonical input wins.
- Iconclass 25G subtree (open data): 115 triples fetched, not wired.
- CLDR Spanish annotations: proposed, not fetched.
- Jung: gray area (borrow-only scans, copyrighted translations).
  Process local copies only.

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
