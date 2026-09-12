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
  data, M3a/M3b), analogy structurally silent on city leaves (R6
  investigated 2026-09-12: ROMA has 0 outgoing, PARIS absent; hubs
  fire at 1.00),
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

## 2. Destination

A chat engine that learns from interaction and behaves as much like
an LLM as measurably possible: coherent multi-turn conversation,
knowledge it can cite, honest unknowns. How close it gets is decided
by dialogue gates, never by claims. Small, local and auditable are
constraints, not selling points.

Non-overlap with SWI-Prolog (surveyed + probed 2026-09-12, binding):
SWI covers inference — Horn, cut, NAF, tabling termination,
s(CASP) justifications + both negations, CLP, ProbLog-style weights.
We borrow there and stop reimplementing. SWI does NOT cover our build
zone: text/dialogue→triples learning (open vocab, positional,
concept/skill discovery), 32D fuzzy retrieval layer, provenance and
trust-tier sidecars, streaming ingest at 32 MB scale, the teach→prove
loop with eval gates. If a future item falls on SWI's side, it is
borrowed, not built — otherwise we walk in circles for nothing.

Borrow-first rule (binding): no new reasoning capability enters P4
without a written borrow-check (pack surveyed, probe or reason
recorded, verdict: borrow vs build). Past violators stay documented
(P4a/P4b reimplement tabling/NAF by hand: kept, working, frozen —
future inference work goes SWI-side).

Capability split (surveyed + probed 2026-09-12, no smoke):
learn-facts-from-text (parser borrowable via link_grammar, loop ours);
induce-rules (aleph/liftcover/phil exist: borrow-check before
extending discover); learn-weights (cplint/ProbLog: borrow);
stemming/similarity (snowball/porter/isub installed: borrow);
reason (tabling/NAF/s(CASP): borrow); store+provenance (rdf_db
partial: tiers/policies ours); streaming embeddings (ours);
scale/embed (ours on footprint); teach→prove loop+gates (ours).
The moat is the integrated learning product + measurement, never a
single capability.

Novelty position (2026-09-12, binding): the value is the measured
artifact, not formalism novelty. Horn + cut + NAF, Jaccard, Zipf,
SLD, embeddings-cosine all predate us and mostly run better elsewhere
(Prolog with tabling, ProbLog, standard IR). We reimplement a subset
in C for embeddability and measure it honestly on real data. The
README must never claim otherwise.

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
- [x] P1 — Curated demo KBs only (done 2026-09-12): `prolog/demo.knowledge.pl`
  (20 hand-verified lexical facts, stem-compatible verbs) passes a
  16-question gate at 17/17 (who/what/did/why + bare why? + honest
  unknown, all with `curated_N` proofs). Rule stands: the chat demos
  exclusively on gated KBs.
- [ ] P2 — BookBrain regeneration gate (FAILED 2026-09-12, see below):
  pipeline extended with the positional fallback (same doctrine as
  chat_learn), then run on 2,647 Gutenberg-Alice sentences: 637 stored
  (24%; 2 via open_vocab, 635 positional), manual 50-gate (seed 42):
  1/50 = 2% vs 80% threshold. Failure mode: dialogue fragments as
  subjects (`"she --can't_explain_it..."`), whole clauses as relations
  (`--and_hurried_off_to_the_garden-->`), quote-glued atoms. This
  confirms positional.pl's documented limit (rigid-SVO short input
  only); the missing piece is input canonization (sentence
  simplification), which does not exist. Queued in P4. The regen KB
  was deleted, not committed. Gate stands; no demo use.
- [x] P3 — Dialogue feeds discovery (done 2026-09-12): `told_rel/1`
  tracks session-taught relations; explicit `discover [rel]` runs
  `learn_cycle_guided` (explicit, not automatic: cost follows vocabulary,
  EXP51/52). Chat QA falls back to question_parser when memory alone
  fails (facts first, induced rules second, unknown last). Gate: 2
  taught examples of `comes_from :- [lives_in, belongs_to]`, held-out
  unknown pre-discovery, rule kept (F1=0.80 as reported), held-out
  `peru. (reasoned)` with rule proof + ground steps post-discovery
  (8/8); P1 still 17/17.
- [ ] P4 — Research queue, no date promises, ordered by value/cost:
  D1 anaphora in dialogue ("Where did HE go?", EXP18 exists: wire it),
  D2 ellipsis ("And Madrid?", reuse last question skeleton),
  D3 continuity (entity stack + ask when ambiguous),
  D4 initiative (probe missing roles, EXP48 pattern). Each with
  scripted multi-turn dialogue sets; same gate discipline as P1/P3.
  input canonization for book ingest (short-SVO sentence simplification;
  prerequisite: P2 measured 2% without it; lead 2026-09-12: probe the
  link_grammar pack before inventing our own simplifier),
  borrow-check aleph vs discover (rule induction exists as SWI pack;
  justify ours by streaming + eval-gated niche or adopt),
  RSI level 2 — self-tuning rank weights (offline sweep, then bandit;
  optimize HELD-OUT only, never training sets — Goodhart guard),
  evaluate building on SWI-Prolog (tabling/CLP/Janus) vs reimplementing
  the reasoning core (open question 2026-09-12: the lab already runs in
  SWI; formalism novelty is disclaimed in section 2).
  VERDICT 2026-09-12 (surveyed + probed, SWI 10.1.14): borrow the
  reasoning, keep the memory. Tabling terminates cyclic closure,
  built-in NAF, and s(CASP) adds justifications + both negations —
  our P4a/P4b reimplements all three by hand. What SWI does NOT give:
  text→triples learning, 32D fuzzy layer, provenance/weights sidecars,
  streaming ingest at scale, 32 MB embeddable C core. Direction: C =
  memory/ingest/retrieve, SWI = inference via a bridge (TBD); future
  reasoning work (quantifiers, solver sidecar, R6 incoming-similarity)
  goes SWI-side unless measured otherwise. ProbLog noted for the
  probabilistic layer (our 0.9^n decay is unprincipled).
  R6 analogy ranking set (needs PARIS data + incoming-role similarity +
  frequency-gated cosine first). Each ships with its eval set +
  threshold (90), no-merge on regression (87/87, suite, Quijote
  immunity).
- [x] P4b — Horn rules + cut + NAF (done 2026-09-12, engine core):
  `neuro_rules.h/c` over untouched P4a: declared head/body rules with
  frame-checkpoint SLD and renamed-apart variables, shallow cut
  (first-solution commit, documented), NAF (unprovable, never binds),
  denial veto transitive at every level (the penguin line: explicit
  denials poison all derivations through them; fact-level contradiction
  stays with conflict policies). Confidence decays per rule step.
  Gate: `test_neuro_prolog_rules` 28/28 (facts, inheritance, cut 1 vs
  union 2, NAF both ways, veto direct + transitive, chains, depth cap,
  cycles terminate, malformed rejected); suite 36/36, eval 87/87.
   Queued explicitly: quantifiers, NL surfacing of weight/conflict
   policy, persist of declared rules.
- [x] M3b — comparison (done 2026-09-12): superlative argmax/argmin
  over valued holders plus binary winner, all on shared units (ties,
  missing values and unit mismatch are honest unknown; years order
  temporally via min/max, menos = antes). QUESTION gains is_compare +
  cmp_op/cmp_binary/cmp_other, M8 precedent; binary shape tried before
  superlative; Y/NO/count vetoed. Gate: 20-row set
  (`qa_eval_compare.tsv`, 5 INDEPENDENCIA years added to the fixture)
  20/20 via test_numeric (same Detect/Answer path; golden V4 has no
  sidecars, so it runs there after regen); suite 35/35, eval 87/87.
- [x] M3a — numeric sidecar (done 2026-09-12): measures as typed
  (value, unit) hanging off object symbols (`numeric.h/c`, never new
  core relation types); Spanish-format heuristic documented in code
  (thousands dots, decimal comma, '/' rejects dates); persisted as V5
  block (V1-V4 load untouched, golden intact); extractor keeps pure
  numbers (slash-forms still out). Gate: 13 parse cases + 20-row
  fixture (`qa_numeric_fixture.tsv`) with sidecar asserts + 20-row QA
  set (`qa_eval_numeric.tsv`, same Detect/Answer path) 20/20 + V5
  round-trip on scratch bin; suite 35/35, eval 87/87. Golden regen
  with numeric data is queued explicitly (not this turn).
- [x] M4 — 2-hop chaining (done 2026-09-12): substitute-then-ask over
  positive ground facts in the QA path (QUESTION gains is_multihop +
  inner_rel/inner_obj, M8 precedent; outer candidates ranked by
  trust/rarity with end-to-end evidence before committing; Y/NO/count
  vetoed to their owners). `tests/qa_eval_multihop.tsv` 20/20 (baseline
  1/20 pre-change); suite 34/34, eval 87/87, hard 38/40, hygiene PASS.
- [x] R6 analogy firing investigation (done 2026-09-12, binary audit
  of golden: parse tail_ok): country hubs fire (ITALIA/FRANCIA sim=1.00
  + transfers). City leaves are silent BY CONSTRUCTION, not by
  threshold: ROMA has 0 outgoing relations (leaf, freq 4) so
  Jaccard-over-outgoing is always 0.00; PARIS is absent from the map
  entirely (vocabulary gap); low-freq embeddings are noise (ROMA top
  cosine 47% = floor, vectors exist but carry no signal).
  (`test_analogical` stays unit-green; `/synonyms A B` pair form is
  unimplemented — lists synonyms of A.)

## 4. Explicitly parked

- Learned self-attention / positional encodings (rejected 2026-09-05;
  cosine "attention" is deterministic ranking, not QKV).
- Any external LLM support, hybrid or otherwise (rejected 2026-09-05).
- Bulk whole-book ingest (until a regeneration passes the 50-gate).
- Unmeasured parity claims (numbers stay only with gates attached).
- Multi-hole joins / solver sidecar (queued behind P4).
- RSI level 3 — code/architecture self-modification (science fiction
  with current means: no safe variation generator, 87-row sets too
  small to select on, no self-model). Unparks only with all three
  present; training-set optimization stays forbidden (Goodhart).

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

## 8. Measures and progress index (binding, 2026-09-12)

One number to know where we stand, with every input re-measurable.
Index = gated-green milestones / total milestones. A FAIL counts only
when measured (never assumed); red stays red until re-measured.

| ID | Gate | State | Numbers |
|----|------|-------|---------|
| P0 loop-1 | teach→ask→proof, 1 fact | green 09-12 | 1/1 |
| P0b save | same proof after reload | green 09-12 | 1/1 |
| P1 curated KB | 16 questions, all proved | green 09-12 | 17/17 |
| P2 regen | 50-gate on regenerated KB | RED 09-12 | 1/50 |
| P3 discover | taught rule answers held-out | green 09-12 | 8/8 |
| LOOP50 | `tools/loop50_gate.py`, 50 real facts | green 09-12 | 20/20 |
| M4 2-hop | `qa_eval_multihop.tsv` | green 09-12 | 20/20 (was 1/20) |
| M3a sidecar | parse + fixture + QA + V5 | green 09-12 | 13+20+20 |
| M3b compare | `qa_eval_compare.tsv` | green 09-12 | 20/20 |
| P4b-core | `test_neuro_prolog_rules` | green 09-12 | 28/28 |
| P4b-rest | quantifiers, NL surfacing, persist | pending | — |
| R6-inv | firing investigation delivered | green 09-12 | diagnosis |
| R6-rank | ranking set (blocked: data) | pending | — |
| canonization | short-SVO simplifier + gate | pending | — |
| M5 defaults | `qa_eval_default.tsv` | green (prior) | 18/18 |
| M7 reverse | `qa_eval_reverse.tsv` | green (prior) | 8/8 |
| M8 conjunction | `qa_eval_conjunctive.tsv` | green (prior) | 10/10 |
| M1 negation | `qa_eval_negation.tsv` | green (prior) | 20/20 |
| M2 count | `qa_eval_count.tsv` | green (prior) | 22/22 |
| R7 contradiction | surfaced (stored only today) | pending | — |
| golden-regen | numeric data in golden | pending | — |

**Index: 15/21 = 71% (2026-09-12).**

Destination estimate: ~40% (judgment, not a gate). The index counts
milestones equally; difficulty does not: the 6 pending contain the two
hardest problems (real-text ingest, golden regen), and the destination
itself (books, manuals) measures 2–22%, not 71%. What is built —
loops, persistence, QA depth on curated data, gates — is the solid
~40% of a product; most of the rest is one wall (un-simplified text)
plus hardening. Standing commitment: keep measuring everything,
report misses as data, never move a goalpost silently.

Regression floor (any red = stop, regardless of the index):
suite 36/36 (`ctest --test-dir build-gcc`), eval 87/87, hard ≥38/40,
hygiene PASS, `git status --short wiki_model.bin` empty.

Evidence rules: baseline pre-change recorded (M4 1/20, R6 0.00s);
thresholds fixed (90 eval/unit suites, 80 corpus gate, 100 taught or
curated answers); counts by double method; every gate re-runnable by
its documented command (ctest, `test_eval_qa`, `tools/progress.py`,
`tools/loop50_gate.py`, `swipl` harnesses).

## 9. Decisions log

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
- 2026-09-12: roadmap rewritten as honest edition (superseded same
  day: destination now flat — chat engine learning from interaction,
  LLM-like behavior as measured operational goal; D-family queued
  first in P4).
- 2026-09-12: novelty disclaimed (value = measured artifact, not
  formalism); README de-smoked (microbenchmarks labeled, no parity or
  zero-hallucination claims); SWI-Prolog surveyed + probed (tabling
  terminates, NAF built-in, s(CASP) documented) → direction: C is
  memory, SWI is inference; incremental probes only, no product built
  on unexamined ground.
- 2026-09-12: destination sharpened to what SWI-Prolog cannot do
  (LEARN: text/dialogue→triples); non-overlap table + borrow-first
  rule binding (P4a/P4b kept and frozen as past violators).
- 2026-09-12: capability split recorded (9 rows: borrow inference,
  weights, stemming, ILP-pending-check; build learning loop, fuzzy
  layer, sidecars, scale, gates); link_grammar lead for canonization;
  aleph-vs-discover borrow-check queued.
