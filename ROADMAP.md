# Symbolic LLM Roadmap: Towards Complete Native Intelligence in Pure C11

This roadmap formalizes the scientific, architectural, and engineering milestones required to expand **Symbolic LLM** from a high-throughput deterministic knowledge engine and coding assistant into a fully autonomous, embedded cognitive agent operating natively in ISO C11 without GPU hardware or external API dependencies.

---

```
                               ┌──────────────────────────────────────────────┐
                               │           CURRENT ENGINE FOUNDATIONS         │
                               │  • O(1) Knowledge Graph (32 bytes/rel)      │
                               │  • STRIPS Planner & Abductive Diagnosis      │
                               │  • Fail-Closed Verbatim Extractive Store     │
                               │  • Cross-Platform Self-Healing Shell Subsys  │
                               └──────────────────────┬───────────────────────┘
                                                      │
                                                      ▼
                      ┌────────────────────────────────────────────────────────────────┐
                      │              THE FOUR COGNITIVE ROADMAP PILLARS                │
                      └───────┬────────────────┬────────────────┬────────────────┬─────┘
                              │                │                │                │
                              ▼                ▼                ▼                ▼
                     ┌─────────────────┐ ┌───────────┐ ┌─────────────────┐ ┌───────────┐
                     │    PILLAR 1     │ │ PILLAR 2  │ │    PILLAR 3     │ │ PILLAR 4  │
                     │  Hyperdimension │ │ Dynamic   │ │ Large-Scale     │ │ Pragmatic │
                     │  VSA Substrate  │ │ Surface   │ │ Commonsense     │ │ Persona   │
                     │  (128/256D SIMD)│ │ Realizer  │ │ (ConceptNet KB) │ │ Filters   │
                     └─────────────────┘ └───────────┘ └─────────────────┘ └───────────┘
```

---

## Pillar 1: Hyperdimensional Computing & Vector Symbolic Architectures (VSA / HDC)

### Objective
Expand the existing 32-dimensional continuous Random Indexing vector space into a high-capacity **Vector Symbolic Architecture** (Kanerva's Binary Spatter Codes / Holographic Reduced Representations) to enable dynamic role-filler binding and recursive conceptual composition without dense matrix multiplications.

### Scientific Foundations
- **The Dimensionality Barrier**: While 32 dimensions provide instantaneous similarity rankings for single words, they cannot cleanly represent composite role-filler bindings (e.g. $\text{Agent} \otimes \text{Dog} \oplus \text{Patient} \otimes \text{Cat}$) without catastrophic interference.
- **VSA Mechanics in Native C11**:
  - **High-Dimensional Binary/Ternary Vectors**: Representing symbols as dense binary vectors $\mathbf{v} \in \{0, 1\}^D$ with $D \in \{128, 256, 1024, 10000\}$.
  - **SIMD Bitwise Binding ($\otimes$)**: Utilizing hardware bitwise XOR (`_mm256_xor_si256` / `_mm512_xor_si512`) to bind semantic roles to entities in $< 5\ \text{ns}$.
  - **Superposition ($\oplus$)**: Majority-rule bundling across bit slices to aggregate multiple facts into a single holistic distributed representation.
  - **Associative Cleanup Memory**: Exact Hamming distance lookup ($O(1)$ via popcount instructions: `__builtin_popcountll` / `_mm_popcnt_u64`) to clean noisy unbundled vectors against canonical symbol prototypes.

### Milestones
- [ ] **M1.1**: Implement SIMD AVX2/AVX-512 bitwise vector operations (`vsa_bind`, `vsa_bundle`, `vsa_unbind`, `vsa_similarity`).
- [ ] **M1.2**: Implement $O(1)$ hardware popcount cleanup memory for symbol retrieval.
- [ ] **M1.3**: Validate compositional role-filler binding on recursive sentence parsing without parameter training.

---

## Pillar 2: Dynamic Surface Realization & Generative Grammar (NLG)

### Objective
Transition the Natural Language Generation subsystem from extractive verbatim citations (`Segun el texto: ...`) and static grammatical templates (`deep_nlg`, `passage_nlg`) to an autonomous, dynamic sentence realizer based on **Combinatory Categorial Grammar (CCG)** and dependency graph traversal.

### Scientific Foundations
- **Symbolic Surface Realization**: Given an active subgraph of facts in memory $\mathcal{G}_{\text{active}} \subset \mathcal{G}$, the realizer must synthesize syntactically fluent, stylistic, and communicative sentences on the fly.
- **Formal Grammar Invariant**:
  - Linear-time chart parser and realizer operating directly in C11 standard library.
  - Morphosyntactic agreement engine (gender, number, person, tense) adhering strictly to grammatical functional tables (`HARDCODING=0`).
  - Dynamic clause composition: assembling coordinated, subordinated, and relative clauses from multi-hop reasoning proofs.

### Milestones
- [ ] **M2.1**: Implement native C11 morphosyntactic agreement tables and functional inflection rules.
- [ ] **M2.2**: Develop a dependency-guided sentence assembler that converts arbitrary subgraphs $\langle S, P, O \rangle^+$ into fluent prose.
- [ ] **M2.3**: Benchmark generative fluency and grammatical accuracy against human-authored ground truth with zero neural decoding.

---

## Pillar 3: Large-Scale Commonsense & World Knowledge Ingestion

### Objective
Ingest and index large-scale, open-domain commonsense knowledge ontologies (such as **ConceptNet 5.8** and **WordNet 3.1**) into the engine's 32-byte relation table, providing intuitive physical, spatial, and functional world knowledge.

### Scientific Foundations
- **Memory Density Advantage**: Because Symbolic LLM requires strictly **32 bytes per relation struct** in RAM:
  $$\text{RAM}(10,000,000\ \text{triples}) = 10^7 \times 32\ \text{bytes} \approx 320\ \text{MB}$$
  An entire human-scale commonsense ontology can reside completely resident in commodity RAM, executing random relation lookups in **72 nanoseconds**.
- **Intuitive Reasoning Capability**:
  - Physical consequence modeling: $\langle \text{Glass}, \text{CapableOfFallingOnto}, \text{Concrete} \rangle \implies \langle \text{Glass}, \text{HasResult}, \text{Shatter} \rangle$.
  - Spatial relationships: $\langle \text{Kitchen}, \text{PartOf}, \text{House} \rangle$, $\langle \text{Refrigerator}, \text{LocatedIn}, \text{Kitchen} \rangle$.
  - Eliminating honest `UNKNOWN` on everyday tacit questions while preserving zero-hallucination guarantees.

### Milestones
- [ ] **M3.1**: Construct a streaming parser for ConceptNet TSV/CSV assertions, filtering relations into canonical S-P-O triples.
- [ ] **M3.2**: Benchmark ingestion throughput on 10M+ triples (target: $< 2.0\ \text{seconds}$ ingest, $< 350\ \text{MB}$ RAM).
- [ ] **M3.3**: Validate commonsense QA accuracy on physical reasoning and qualitative challenge sets.

---

## Pillar 4: Pragmatic Conditioning, Epistemic Perspectives & Persona Filters

### Objective
Provide in-context persona and rhetorical adaptation without stochastic prompt injection, conditioning the engine's communicative style through deterministic activation bias masks over the Reflexive Meta-Graph ($\mathcal{M}$).

### Scientific Foundations
- **The Prompt Injection Vulnerability in Neural LLMs**: Neural models simulate personas by prepending tokens to a prompt, exposing them to prompt injection attacks, drift, and accidental disclosure of system prompts.
- **Deterministic Perspective Filtering**:
  - In Symbolic LLM, a "persona" or "style" is a **mathematical projection operator** $\Pi_{\text{style}}$ over the graph:
    $$\Pi_{\text{style}} : \mathcal{G} \to \mathcal{G}_{\text{biased}}$$
  - Modulates synonym selection, rhetorical connective choice, sentence length, and vocabulary density without modifying underlying ground-truth facts.
  - Provable containment: facts remain invariant; only surface rhetorical choices adapt to the requested communicative role (e.g. senior software architect, technical auditor, didactic tutor).

### Milestones
- [ ] **M4.1**: Implement structural persona masks in the Reflexive Meta-Graph ($\mathcal{M}$).
- [ ] **M4.2**: Design deterministic style-transfer rules for surface lexical selection.
- [ ] **M4.3**: Verify non-interference: prove mathematically that style masks cannot alter factual deduction chains or introduce false assertions.

---

## Summary of Architectural Milestones

| Pillar | Focus Area | Key Technology | Target Latency / Memory | Verification Gate |
| :--- | :--- | :--- | :--- | :--- |
| **Pillar 1** | Hyperdimensional VSA | 128D/256D AVX2 SIMD XOR/Popcount | $< 10\ \text{ns}$ binding / $< 1\ \text{MB}$ | Role-filler retrieval accuracy > 99% |
| **Pillar 2** | Generative Grammar | C11 CCG / Dependency Realizer | $< 500\ \mu\text{s}$ per sentence | Grammaticality 100%, Hallucination 0% |
| **Pillar 3** | Commonsense KB | ConceptNet / WordNet Ingestion | $< 2.0\ \text{s}$ ingest / ~320 MB RAM | Commonsense Benchmark > 85% |
| **Pillar 4** | Persona / Pragmatics | Graph Activation Masking ($\Pi_{\text{style}}$) | $O(1)$ mask overhead / 0 bytes | Invariant Factual Equivalence Proof |
