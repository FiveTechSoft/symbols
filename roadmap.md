# Roadmap: symbols and relations (honest edition, 2026-09-12)

Core doctrine: the map holds only symbols and relations. Meaning is
never hardcoded: it is read off the live graph (morphology, ranking,
embeddings) or taught explicitly. Unknowns are stored or admitted,
never fabricated.

This file states direction and gates. Process rules live in AGENTS.md.
Everything below is revisable on evidence (see section 7).

## 1. Where we actually are (verified)

C engine (reported 2026-09-05/06, not re-run since):
- Suite 33/33, eval 87/87, hygiene 87/87, lint 4155/0/0.
- Model: 5,783 symbols / 3,430 relations.
- Working: lookup, counting (22/22), negation (20/20), non-monotonic
  defaults (18/18), single-hole reverse QA (8/8), conjunction (10/10),
  held-out hard set 38/40 (2 misses are set artifacts, documented).
- Not working: 2-hop chaining 0/1 (M4), comparison 0/1 (no numeric
  data, M3a/M3b), analogy map-silent 0/4 (needs investigation),
  contradiction never surfaced, multi-hole joins need the solver sidecar.
- README benchmark numbers (13.8M queries/s, 1M relations) are
  microbenchmarks, not production measurements. Read them as such.

Prolog lab (re-run 2026-09-12: EXP30 20/20, EXP44 39/39, EXP50 32/32,
EXP51 23/23, EXP52 27/27, EXP53 20/20, EXP45 demo 16/16):
- 53 experiments green, but on synthetic toy worlds; transfer is
  between generated corpora. Mechanism research, not generality evidence.
- Honest boundary documented: zebra probe — stored lookups 3/3,
  inference 0; gap decomposed exactly (no variables, no backtracking,
  no reverse QA in the QA path).

Chat / BookBrain (new, untracked, verified 2026-09-12):
- Loads, answers with proofs + chapter refs, learns assertions
  in-session (loop closed: unknown → taught → answered with proof),
  with anti-pollution probes (questions never ingested, DELTA=+1 exact).
- No hardcoded content lexicon: content filter is memory-derived
  (`bb_content/1`); only closed-class syntax is fixed (`?`, `.`,
  interrogative/auxiliary guards, commands).
- KB gate (new, see section 5): `alice.knowledge.pl` FAILS at 22% precision
  (50-sample, seed 42, Gutenberg grounding + manual review, 2026-09-12).
  File kept, quarantined: no demos on it until a regeneration passes.
- Preflight: chat.pl, ask.pl, bookbrain.pl PASS. Project gate still
  red only on `gaps.pl` (separate WIP).

## 2. Destination (achievable, demoable)

Not LLM parity — that stays a north star, never an operational goal.
The reachable, valuable thing this project can uniquely offer:

**The machine that knows what it knows**: a tiny, local, auditable,
teachable assistant for closed domains (a book, a manual, a knowledge
base) that learns when told, cites sources, reinforces on repetition,
and says "I don't know" instead of inventing. No LLM does all four.

The 5-minute demo that decides everything: teach it a fact, ask about
it, demand the proof. If that loop holds on real text, the project
lives. If not, nothing else matters.

Division of labor: the Prolog lab investigates, the C engine deploys
(32 MB, embeddable, no Python). Research that never reaches the
deployable artifact is play; keep the bridge (P4a done, P4b pending)
walkable.

## 3. Next steps, in order, each with a gate

- [x] P0 — Close the loop (done 2026-09-12): teach → stored →
  answered with proof, anti-pollution verified, preflight clean.
- [ ] P0b — Session persistence: a `save` command so taught facts
  survive the session (same memfact/provfact format). Gate: teach,
  save, reload, ask → same proof.
- [ ] P1 — Curated demo KBs only: small hand-verified KBs (40-fact
  scale, `qa_eval_hard.tsv` style) that pass the 50-gate at ≥80%.
  Rule: the chat demos exclusively on gated KBs. No exceptions.
- [ ] P2 — BookBrain regeneration gate: regenerate a book KB with the
  current pipeline (corpus learn + positional fallback), then the same
  50-gate (seed 42, Gutenberg grounding + manual review, ≥80%) before
  any demo use. Today's pipeline emits a different schema than
  `alice.knowledge.pl`; that is fine, the gate decides, not the schema.
- [ ] P3 — Dialogue feeds discovery: run `learn_cycle` periodically
  over told facts (EXP46 sketch → product). Storing is not enough: the chat must
  induce, not just store. Gate: taught regularities answerable with
  rule-attributed proofs; no regression on P1 KBs.
- [ ] P4 — Research queue, no date promises, ordered by value/cost:
  M4 (2-hop, substitute-then-ask), M3a/M3b (numeric sidecar, then
  comparison), P4b (Horn + cut + NAF over P4a), R6 analogy firing
  investigation. Each ships with its eval set + threshold (90),
  no-merge on regression (87/87, suite, Quijote immunity).

## 4. Explicitly parked

- Learned self-attention / positional encodings (rejected 2026-09-05;
  cosine "attention" is deterministic ranking, not QKV).
- Any external LLM support, hybrid or otherwise (rejected 2026-09-05).
- Bulk whole-book ingest (until a regeneration passes the 50-gate).
- Parity claims in the README (bench numbers stay, labeled as bench).
- Multi-hole joins / solver sidecar (queued behind P4).

## 5. The corpus gate (binding)

No KB feeds the chat, the evals, or a demo without: random 50-triple
sample (seed 42), mechanical grounding (Gutenberg text + stemming),
manual plausibility review against the source, threshold ≥80%.
Method of 2026-09-12 (scripts in temp, reproducible) is the template.
A FAIL quarantines the artifact; filtering is not a fix for role
misassignment (alice: swaps survived every mechanical filter).

## 6. Language & i18n (unchanged)

- Output English by default; ES/FR as pure i18n layers, never mutating
  the graph. Input language-agnostic, resolved by semantic similarity
  against the live map. Cross-lingual gaps are vocabulary (data-side),
  not parsing.

## 7. Openness clause

This roadmap is a snapshot of best judgment, not a contract. Any entry
— including the rejections in section 4 — changes on evidence: a technique
from outside (better symbolic parser, proven role labeler, new
formalism) is adopted if it passes the gates (eval without regression,
preflight clean, 50-gate for corpus), never on hype. If a gate itself
proves wrong, change the gate in the open with the failure cited.

## 8. Decisions log

- 2026-09-05: external LLM / hybrid TinyLlama proposal rejected.
- 2026-09-05: per-relation composed embeddings + cosine "attention";
  no positional encodings, no learned self-attention.
- 2026-09-05: i18n English by default (ES/FR); input resolved by
  semantic similarity against the map.
- 2026-09-05: honest corpus + retrieval re-extraction; golden model
  regenerated deterministically.
- 2026-09-06: P4a measured (neuro_prolog, 36/36); NAF/rules/cut
  deferred to P4b; polarity-blind boundary with M5 documented.
- 2026-09-12: chat learning loop closed (unknown → taught → answered,
  anti-pollution probed, DELTA exact); `gen_closed` word list replaced
  by memory-derived `bb_content/1`; Spanish detection without word
  lists (non-ASCII heuristic).
- 2026-09-12: `alice.knowledge.pl` quarantined (50-gate: 11/50 = 22%;
  role misassignment, phantom ch13 refs, narrator entity). Chat demos
  only on gated KBs from now on.
- 2026-09-12: roadmap rewritten as honest edition; destination fixed
  as "the machine that knows what it knows", LLM parity demoted to
  north star.
