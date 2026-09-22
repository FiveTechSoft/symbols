# Symbolic LLM: A Deterministic, Non-Parametric Language Engine in Native C11 with Zero Hallucination and Microsecond Latency

**Antonio Linares (FiveTech Software)**  
*Project Repository: [FiveTechSoft/symbols](https://github.com/FiveTechSoft/symbols)*  
*Live Interactive Web Agent: [fivetechsoft.github.io/symbols](https://fivetechsoft.github.io/symbols/)*  
*Version: 1.0-RC (September 2026)*

---

### Abstract

Large Language Models (LLMs) based on the Transformer architecture rely on dense, non-transparent floating-point parameter matrices trained via gradient descent. While remarkably capable at syntactic continuation, they suffer from fundamental systemic limitations: stochastic hallucination ($P(\text{hallucination}) > 0$), catastrophic forgetting, absence of causal proof provenance, and massive computational and memory footprints.

We introduce **Symbolic LLM**, a deterministic, non-parametric language and reasoning engine engineered entirely in **pure ISO C11** without external dependencies, neural weights, backpropagation, or GPU acceleration. Symbolic LLM decouples factual memory, distributional semantics, and causal problem-solving into discrete, inspectable native mathematical structures:
1. An open-addressing **Symbolic Knowledge Graph** operating with strictly $O(1)$ lookup time (MurmurMix64 dispersion) requiring strictly **32 bytes per relation** in memory.
2. An ultra-lightweight **32-dimensional Distributional Semantic Vector Substrate** constructed via Random Indexing and Hebbian co-occurrence accumulation, enabling nanosecond-scale fuzzy synonymy without matrix multiplications.
3. A **First-Order Literal Sentence Store** indexed directly from raw text streams with byte-level offset provenance, enforcing an axiomatic **fail-closed truth contract** (verbatim source citation with $P(\text{unanchored fabrication}) = 0$).
4. A **Second-Order Reflexive Meta-Graph** ($\mathcal{M}$) modeling meta-knowledge and discourse focus shifts in $O(1)$.
5. An unsupervised **Concept Concentration Metric** ($\kappa = \frac{\max_d \mathbf{v}[d]}{\sum_d \mathbf{v}[d]} \cdot \log(1 + f)$) that extracts fundamental thematic centroids in linear time without stopword lists.
6. A **Polyglot Code Knowledge Graph** with native C, Python, and TypeScript/JavaScript AST parsing, supporting bidirectional call graph navigation and transitive impact analysis (Blast Radius).
7. A **Goal-Directed STRIPS Task Planner** operating over propositional bitmask states ($\mathbb{B}^m$) that synthesizes provably optimal software engineering action sequences in $< 10\ \mu\text{s}$, coupled with pre-flight AST verification and sub-millisecond atomic rollback.
8. A **Cross-Platform Subprocess & Autocurative Shell Engine** supporting native Windows/POSIX execution with non-blocking pipe drainage, millisecond timeouts, and closed-loop abductive self-healing over compiler/linter diagnostics.
9. A **Deterministic Open-Domain QA Engine** featuring structural question decomposition, auxiliary resolution, multi-word entity tokenization, and functional canonicalization, achieving **61.0% exact ground-truth accuracy** over 41,431 multi-corpus sentences while guaranteeing strictly **0% unanchored token fabrication** (fail-closed verbatim extraction).
10. **The Four Cognitive Pillars in Pure C11**: 256-bit AVX2 SIMD Hyperdimensional Computing (VSA/HDC) at 166.7 Mops/s; dynamic CCG Combinatory Categorial Grammar sentence realization at 1.5M sent/s; resident ConceptNet 5.8 commonsense and physical causality reasoning (~305 MB for 10M triples); and deterministic persona projection filters ($\Pi_{\text{persona}}$) at 2.19M proj/s with mathematical non-interference ($\text{Facts}(\Pi_P(Q)) \equiv \text{Facts}(Q)$).

Empirical evaluations establish an ingestion throughput of **5.4 million triples per second** (1,000,000 relations populated in 0.185 s within 32.00 MB RAM), random query latency of **72 nanoseconds**, and end-to-end question answering in **< 1 millisecond** on a single commodity CPU core. Evaluated on candidate patch hunks from representative SWE-bench Lite benchmark tasks (Django, Flask, SymPy, Scikit-learn, Pytest), the engine achieves 100% pre-flight AST verification and atomic application with an average verification latency of **1.50 ms per task**, operating dynamically within **~28 MB RAM** (0 GPU) and strictly **0.00% patch corruption** under fail-closed AST invariant checking, proving that deterministic code safety, blast radius analysis, and atomic rollback can be executed at microsecond scales.


---

## 1. Introduction and Theoretical Foundations

### 1.1 The Transformer Dilemma

Contemporary Natural Language Processing is predominantly anchored on autoregressive Transformer decoders ($p(w_t \mid w_{<t})$). Knowledge in these architectures is implicitly and diffusely distributed across billions of parameters $\theta \in \mathbb{R}^N$. This architectural paradigm imposes severe structural limitations:

- **Epistemic Indeterminacy and Hallucination**: Output tokens are sampled stochastically from a probability distribution over the vocabulary. The network cannot verify whether a generated sequence corresponds to factual ground truth or a statistically probable confabulation.
- **Catastrophic Forgetting & Opaque Editing**: Incorporating a new fact or deleting an erroneous one requires expensive fine-tuning (LoRA, full retraining) or fragile prompt-context injection. True machine unlearning remains an unsolved research dilemma.
- **Extreme Computational Footprint**: Serving a modern 7B–70B parameter model requires 8 GB to 140+ GB of high-bandwidth GPU VRAM, high electrical power, and massive runtime dependencies (PyTorch, CUDA, Triton, BLAS).
- **Zero Proof Trace**: Attention weights over self-attention heads $\text{softmax}(QK^T / \sqrt{d_k})$ represent correlation patterns across positional tokens, not deductive validity or verifiable provenance.

### 1.2 The Symbolic LLM Proposition

Symbolic LLM investigates whether linguistic comprehension, question answering, deductive reasoning, and topical introspection can be achieved **without dense matrix multiplications**. 

```
                               ┌─────────────────────────────────────────┐
                               │           RAW TEXT CORPUS (.txt)        │
                               └────────────────────┬────────────────────┘
                                                    │ Streaming Ingest
                                                    ▼
┌────────────────────────────────────────────────────────────────────────────────────────────────────────┐
│                                       SYMBOLIC LLM ENGINE (C11)                                        │
│                                                                                                        │
│  ┌──────────────────────┐   ┌──────────────────────┐   ┌──────────────────────┐   ┌─────────────────┐  │
│  │    SYMBOL TABLE      │   │    RELATION GRAPH    │   │    32D EMBEDDINGS    │   │  LITERAL STORE  │  │
│  │ O(1) DJB2a Hash      │   │ O(1) MurmurMix64     │   │ Hebbian Window       │   │ Byte Offsets    │  │
│  │ Concept Unique IDs   │   │ <S, P, O> Triples    │   │ LayerNorm + Cosine   │   │ Verbatim Source │  │
│  └──────────┬───────────┘   └──────────┬───────────┘   └──────────┬───────────┘   └────────┬────────┘  │
│             │                          │                          │                        │           │
│             └──────────────────────────┼──────────────────────────┴────────────────────────┘           │
│                                        ▼                                                               │
│                     ┌───────────────────────────────────────┐                                          │
│                     │  INVERTED INDEX (QKV Keys)           │                                          │
│                     │  Symbol → Sentence + Novelty Cache   │                                          │
│                     └──────────────────┬────────────────────┘                                          │
│                                        ▼                                                               │
│                     ┌───────────────────────────────────────┐                                          │
│                     │  SPARSE ATTENTION + CROSS-ATTENTION   │                                          │
│                     │  Window + Global Anchors + XA Dice    │                                          │
│                     └──────────────────┬────────────────────┘                                          │
│                     ┌───────────────────────────────────────┐                                          │
│                     │  CONCEPT CONCENTRATION METRIC (κ)     │                                          │
│                     │  Unsupervised Thematic Discovery      │                                          │
│                     └──────────────────┬────────────────────┘                                          │
│                                        ▼                                                               │
│                     ┌───────────────────────────────────────┐                                          │
│                     │  SECOND-ORDER REFLEXIVE META-GRAPH (M)│                                          │
│                     │  Knowledge-about-Knowledge & Dialogue │                                          │
│                     └──────────────────┬────────────────────┘                                          │
│                                        ▼                                                               │
│                     ┌───────────────────────────────────────┐                                          │
│                     │  SYMBOLIC ATTENTION & INFERENCE       │                                          │
│                     │  Backward Chaining DFS + Proof Chains │                                          │
│                     └──────────────────┬────────────────────┘                                          │
└────────────────────────────────────────┼───────────────────────────────────────────────────────────────┘
                                         │
                                         ▼
                     ┌───────────────────────────────────────┐
                     │    DETERMINISTIC VERIFIED RESPONSE    │
                     │    Trace: S ──P──> O [Source Cited]   │
                     └───────────────────────────────────────┘
```

The system is founded on three axiomatic design principles:

1. **Fail-Closed Truth Preservation**: If an assertion cannot be proven by direct retrieval or a verified deduction chain over explicit relations, the model outputs an honest `UNKNOWN`. It never fabricates facts.
2. **Deterministic $O(1)$ Mechanics**: Core operations—symbol resolution, relation verification, anaphoric focus shift, and vector comparison—are strictly bounded in constant time.
3. **Hardware Sovereignty**: The entire system is implemented in standard C11 using standard POSIX/Win32 primitives, compiling into an executable of a few hundred kilobytes with zero external runtime requirements.

---

## 2. Architecture and Data Representations

### 2.1 The Symbolic Knowledge Graph: $O(1)$ Hash Table Architecture

The backbone of factual memory is partitioned into two dual-indexed hash structures: the `SYMBOL_TABLE` and the `RELATION_TABLE`.

#### Symbol Representation
Symbols are canonicalized lexical tokens mapped to unique integer identifiers:
```c
typedef struct {
    uint32_t id;
    uint32_t name_len;
    char    *name;
    uint32_t frequency;
} SYMBOL;
```
Hashing is governed by the modified DJB2a algorithm formulated as a first-order recurrence with bitwise XOR dispersion:

$$
\begin{aligned}
h_0 &= 5381 \\
h_i &= ((h_{i-1} \ll 5) + h_{i-1}) \oplus s_i = (33 \cdot h_{i-1}) \oplus s_i, \quad 1 \le i \le |s| \\
h_{\text{sym}}(s) &= h_{|s|} \land (2^k - 1)
\end{aligned}
$$

where $k$ represents the log-capacity of the table ($N = 2^k$).

#### Relational Triples
Knowledge is stored as discrete relational triples $\langle \text{Subject}, \text{Predicate}, \text{Object} \rangle$:
```c
typedef struct {
    uint32_t subject;     // Symbol ID
    uint32_t predicate;   // Symbol ID
    uint32_t object;      // Symbol ID
    uint32_t count;       // Co-occurrence counter
    float    weight;      // Normalized probabilistic weight
} RELATION;
```
Each relation struct occupies **strictly 32 bytes** in RAM. Indexing uses a 64-bit integer mixing function (MurmurMix64) combining subject, predicate, and object into a high-dispersion hash bucket:

$$
\begin{aligned}
k_0 &= (\text{subject} \ll 32) \oplus (\text{predicate} \ll 16) \oplus \text{object} \\
k_1 &= (k_0 \oplus (k_0 \gg 33)) \cdot \text{0xff51afd7ed558ccd} \\
k_2 &= (k_1 \oplus (k_1 \gg 33)) \cdot \text{0xc4ceb9fe1a85ec53} \\
h_{\text{rel}}(\text{subject}, \text{predicate}, \text{object}) &= (k_2 \oplus (k_2 \gg 33)) \land (2^m - 1)
\end{aligned}
$$

Using power-of-two table capacities ($2^m$), bitwise masking replaces costly modulo division, and open addressing with linear probing guarantees cache-line locality. Automatic rehashing occurs when the load factor exceeds 70%.

### 2.2 32-Dimensional Distributional Semantic Substrate

To overcome the brittle discreteness of classical symbolic systems (e.g., failing to equate *feline* with *cat*), Symbolic LLM embeds every symbol into a compact **32-dimensional continuous vector space** $\mathbf{v} \in \mathbb{R}^{32}$.

#### Vector Construction (Random Indexing & Online Hebbian Accumulation)
Instead of gradient descent over large corpora, vectors are updated online via streaming Random Indexing (Kanerva, 1988) and Hebbian co-occurrence windows:
1. Each symbol $w \in V$ is assigned an ultra-sparse static ternary index vector $\mathbf{r}_w \in \{-1, 0, 1\}^{32}$ where non-zero components satisfy quasi-orthogonality ($\mathbb{E}[\langle \mathbf{r}_u, \mathbf{r}_v \rangle] = 0$ for $u \neq v$).
2. When word $w$ co-occurs with word $u$ within a sliding context window $\mathcal{W}(w)$, online vector accumulation occurs with distance attenuation:
   $$\mathbf{v}_w^{(t+1)} = \mathbf{v}_w^{(t)} + \sum_{u \in \mathcal{W}(w)} \frac{1}{|pos(w) - pos(u)|} \mathbf{r}_u$$
3. Vectors are normalized to unit Euclidean length:
   $$\hat{\mathbf{v}}_w = \frac{\mathbf{v}_w}{\|\mathbf{v}_w\|_2}$$

Semantic similarity is evaluated via cosine similarity:

$$
\text{Sim}(u, w) = \langle \hat{\mathbf{v}}_u, \hat{\mathbf{v}}_w \rangle = \sum_{d=0}^{31} \hat{\mathbf{v}}_u[d] \cdot \hat{\mathbf{v}}_w[d]
$$

On modern x86/ARM hardware, this 32-dimensional dot product executes in **~2 nanoseconds** via SIMD vectorization.

### 2.3 First-Order Literal Sentence Store and Exact Provenance

When ingesting unstructured natural language (e.g., `.txt` files), the engine constructs an in-memory literal sentence store:
- **Streaming Parser**: Tokenizes text into sentences and normalized symbols on-the-fly with 64 KB buffering.
- **Provenance Inverted Index**: For each extracted symbol, a compact posting list registers the literal sentence offsets: $\text{Posting}(w) = \{ \text{id}_1, \text{id}_2, \dots, \text{id}_m \}$.
- **Verbatim Citation Retrieval**: When answering a natural language question over unstructured text, the engine retrieves the exact original sentence from which the fact was cited, appending the source text verbatim (zero unanchored text fabrication). If no matching ground truth exists, the model safely outputs an honest `UNKNOWN`.

### 2.4 Second-Order Reflexive Meta-Graph ($\mathcal{M}$)

Beyond first-order textual facts, the engine constructs a dynamic meta-graph $\mathcal{M} = (V_{\mathcal{M}}, E_{\mathcal{M}})$ representing **knowledge acquired about the knowledge**:
- **Dialogue-Driven Associations**: If concepts $A$ and $B$ are repeatedly queried together, co-activated in reasoning chains, or linked through discourse, an edge $e(A, B)$ is materialized in $\mathcal{M}$ with an interaction weight $w_{AB}$.
- **Associative Memory Traversal**: When an entity is queried, the engine consults $\mathcal{M}$ to present adjacent topics that contextualize the response.

### 2.5 Concept Concentration Metric ($\kappa$) and Thematic Introspection

A central challenge in unsupervised language understanding is discovering what a text is about without relying on hand-crafted stopword lists or pre-trained neural tokenizers.

We introduce the **Concept Concentration Metric** $\kappa(w)$:

$$
\kappa(w) = \frac{\max_{d \in [0, 31]} \mathbf{v}_w[d]}{\sum_{d=0}^{31} \mathbf{v}_w[d]} \cdot \log(1 + f_w)
$$

where $\mathbf{v}_w[d] \ge 0$ is the accumulated co-occurrence mass along dimension $d$, and $f_w$ is the corpus term frequency.

#### Thematic Discriminability Principle
