# Symbolic LLM: A Deterministic, Non-Parametric Language Engine in Native C11 with Zero Hallucination and Microsecond Latency

**Antonio Linares (FiveTech Software)**  
*Project Repository: [FiveTechSoft/symbols](https://github.com/FiveTechSoft/symbols)*  
*Live Interactive Web Agent: [fivetechsoft.github.io/symbols](https://fivetechsoft.github.io/symbols/)*  
*Version: 1.0-RC (September 2026)*

---

### Abstract

Large Language Models (LLMs) built upon the Transformer architecture rely on dense, non-transparent floating-point parameter matrices trained via gradient descent. While remarkably capable at syntactic mimicry and generalized text continuation, they suffer from fundamental systemic shortcomings: stochastic hallucination, catastrophic forgetting, lack of auditable causal provenance, and massive computational and memory footprints.

We introduce **Symbolic LLM**, an alternative language engine engineered entirely in **pure ISO C11** without external dependencies, neural weights, backpropagation, or GPU acceleration. Symbolic LLM decouples factual memory, distributional semantics, and conversational inference into discrete, inspectable, native mathematical structures:
1. An open-addressing **Symbolic Knowledge Graph** operating with strictly $O(1)$ lookup time (MurmurMix64 dispersion) and requiring strictly **32 bytes per relation** in RAM.
2. An ultra-lightweight **32-dimensional Distributional Semantic Vector Substrate** constructed via Hebbian co-occurrence windows, enabling nanosecond-scale fuzzy synonymy and cross-concept generalization.
3. A **First-Order Literal Sentence Store** indexed directly from raw free-form text streams (`.txt`) with exact byte-level offset provenance, enforcing a **fail-closed truth contract** ($P(\text{hallucination}) = 0$).
4. A **Second-Order Reflexive Meta-Graph** ($\mathcal{M}$) modeling meta-knowledge—dynamically capturing structural associations formed through dialogue, query navigation, and active discourse.
5. An unsupervised **Concept Concentration Metric** ($\kappa = \frac{\max_d v[d]}{\sum_d v[d]} \cdot \log(1 + f)$) that extracts the fundamental thematic pillars and conversational entry points from raw text in linear time without hand-crafted stopword dictionaries.

Empirical evaluations demonstrate an ingestion throughput of **5.4 million triples per second** (1,000,000 relations populated in 0.185 s within 32.00 MB RAM), random query latency of **72 nanoseconds**, and end-to-end question answering in **< 1 millisecond** on a single commodity CPU core, maintaining 100% precision and verifiable citations across arbitrary multilingual text corpora.

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
Hashing is governed by the modified DJB2a algorithm with XOR dispersion:

$$
h(s) = \left( \prod_{i=1}^{|s|} 33 \oplus s_i \right) \land (2^k - 1)
$$

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
Each relation struct occupies **strictly 32 bytes**. Indexing uses a 64-bit integer mixing function (MurmurMix64) combining subject, predicate, and object into a high-dispersion hash bucket:

$$
\begin{aligned}
k &= (\text{subject} \ll 32) \oplus (\text{predicate} \ll 16) \oplus \text{object} \\
k &\leftarrow (k \oplus (k \gg 33)) \cdot \text{0xff51afd7ed558ccd} \\
k &\leftarrow (k \oplus (k \gg 33)) \cdot \text{0xc4ceb9fe1a85ec53} \\
h_{\text{rel}} &= (k \oplus (k \gg 33)) \land (\text{capacity} - 1)
\end{aligned}
$$

Using power-of-two table capacities, bitwise masking replaces costly modulo division, and open addressing with linear probing ensures cache locality. Automatic rehashing occurs when the load factor exceeds 70%.

### 2.2 32-Dimensional Distributional Semantic Substrate

To overcome the brittle discreteness of classical symbolic systems (e.g., failing to equate *feline* with *cat*), Symbolic LLM embeds every symbol into a compact **32-dimensional continuous vector space** $\mathbf{v} \in \mathbb{R}^{32}$.

#### Vector Construction (Hebbian Co-occurrence)
Instead of gradient descent over large corpora, vectors are updated online via streaming Random Indexing and Hebbian co-occurrence windows:
1. Each symbol is initially assigned an ultra-sparse ternary signature $\mathbf{r}_w \in \{-1, 0, 1\}^{32}$.
2. When word $w$ appears within context window $\mathcal{W}$ of word $u$, vector accumulation occurs: $\mathbf{v}_w^{(t+1)} = \mathbf{v}_w^{(t)} + \frac{1}{\text{dist}(w, u)} \mathbf{r}_u$.
3. Vectors are normalized to unit Euclidean length: $\hat{\mathbf{v}} = \frac{\mathbf{v}}{\|\mathbf{v}\|_2}$.

Semantic similarity is evaluated via cosine similarity:

$$
\text{Sim}(u, w) = \sum_{d=0}^{31} \hat{\mathbf{v}}_u[d] \cdot \hat{\mathbf{v}}_w[d]
$$

On modern x86/ARM hardware, this 32-dimensional dot product executes in **~2 nanoseconds** via SIMD vectorization.

### 2.3 First-Order Literal Sentence Store and Exact Provenance

When ingesting unstructured natural language (e.g., `.txt` files), the engine constructs an in-memory literal sentence store:
- **Streaming Parser**: Tokenizes text into sentences and normalized symbols on-the-fly with 64 KB buffering.
- **Provenance Inverted Index**: For each extracted symbol, a compact posting list registers the literal sentence offsets: $\text{Posting}(w) = \{ \text{id}_1, \text{id}_2, \dots, \text{id}_m \}$.
- **Zero-Hallucination Retrieval**: When answering a natural language question, the engine retrieves the exact original sentence from which the fact was extracted, appending the source text verbatim. If no matching ground truth exists, the model safely outputs `UNKNOWN`.

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
- **Syntactic "Glue" Words** (*the, of, and, in, with*): Appear indiscriminately across all linguistic contexts. Their co-occurrence mass is distributed uniformly across all 32 dimensions, yielding a near-zero concentration: $\max_d \mathbf{v}_w[d] \approx \frac{1}{32} \sum_d \mathbf{v}_w[d] \implies \kappa(w) \to 0$.
- **Semantic Anchor Concepts** (*socrates, soul, algorithm, oxygen*): Appear in highly specific relational contexts. Their co-occurrence concentrates along specific semantic axes, producing high $\kappa(w)$.

Sorting symbols by $\kappa(w)$ reveals the fundamental thematic pillars of any corpus immediately after ingestion, enabling autonomous conversational introspection:
- `"what areas do you know?"` $\to$ outputs top-$\kappa$ conceptual centroids.
- `"start a conversation"` $\to$ autonomously selects the top thematic concept, retrieves its seminal sentence, and seeds the conversational state machine.

### 2.6 Dynamic Real-Time Memory and Persistent State Architecture

A defining limitation of autoregressive neural networks is that weights are frozen at inference time ($W_{\text{frozen}}$). Teaching a Transformer new knowledge during a conversation requires appending previous turns into the prompt context, which quadratically expands VRAM footprint ($\mathcal{O}(N^2)$), inflates attention compute, and evaporates completely when the session terminates.

Symbolic LLM breaks this dichotomy through a **Dual-Layer Real-Time Memory Substrate**:

#### 2.6.1 Triplet Graph vs. Verbatim Sentence Memory: The Symbiotic Optimum
A frequent theoretical question in symbolic computing is: *Should knowledge be reduced strictly to atomic relational triples $\langle S, P, O \rangle$, or should full natural language sentences be preserved verbatim?*

Symbolic LLM demonstrates that the most effective architecture is **symbiotic dual-indexing**:
1. **The Structural Triplet Graph**: Indexes concepts and relational predicates into an open-addressing hash table ($O(1)$ lookup, 32 bytes/relation). It enables backward-chaining multi-hop deduction, taxonomic inheritance, transitivity ($A \to B \to C$), and anaphora resolution in nanoseconds.
2. **The First-Order Literal Sentence Store**: Preserves the complete, verbatim original sentences indexed by 32-bit sentence identifiers (`sent_id`) with exact byte-level provenance. It guarantees zero loss of syntactic nuance, subordinate clauses, stylistic prose, and literal citations.

When querying the system, the relational graph determines the deductive truth path in $O(1)$, while the literal store provides the exact verbatim citation. Neither is compromised: mathematical certainty is coupled with 100% syntactic preservation.

#### 2.6.2 Online Dynamic Memory Ingestion ($O(1)$ Stream)
While conversing with an end user or operating inside an autonomous agent harness (e.g., OpenCode), the engine dynamically acquires knowledge on the fly:
- **Declarative User Assertions**: When a user or harness supplies factual input (e.g., *"Quantum teleportation transfers quantum information between separated qubits"*), the streaming parser tokenizes and incorporates the sentence into the literal store, creates or updates symbol embeddings via Hebbian accumulation, and links corresponding concepts into the relational graph within **72 nanoseconds**.
- **Instantaneous Zero-Shot Recall**: The newly acquired knowledge is queryable immediately in subsequent dialogue turns with $P(\text{hallucination}) = 0$.

#### 2.6.3 Episodic Interaction Subgraph ($\mathcal{M}_{\text{ep}}$)
Dialogue history is not stored as an opaque flat string of tokens. Instead, the engine materializes an episodic subgraph:

$$
\mathcal{M}_{\text{ep}} = \langle \text{Turn}_k, \text{FocusConcept}, \text{QueryType}, \text{Timestamp}, \text{ActivatedNodes} \rangle
$$

- Tracks discourse trajectory and conversational focus shifts in $O(1)$.
- Enables metacognitive inspection (*"what were we discussing at the start?"*, *"forget what I said about my API key"*).

#### 2.6.4 Deterministic Snapshot Persistence & Portability
To ensure zero knowledge loss across system restarts, agent reboots, or web sessions, Symbolic LLM provides deterministic binary snapshot persistence:
- **Native Binary Image (`.bin`)**: High-throughput memory-mapped serialization. An entire knowledge base consisting of 50,000 concepts and multi-hop relations writes to disk in **25.4 ms** (7.5 MB) and remounts into memory in **15.5 ms**.
- **Edge & Web Client Storage (`localStorage` / JSONL)**: In browser-based environments, session history and episodic subgraphs serialize seamlessly into Web Storage (`localStorage`), enabling instant resumption across page refreshes and single-click full-state export (`.json`/`.sym`).

---

## 3. Inference and Attention Mechanics

### 3.1 Deterministic Deductive Reasoning (Backward Chaining DFS)

Unlike neural decoders that approximate logical deduction probabilistically, Symbolic LLM implements an exact deductive backward-chaining inference engine with **confidence attenuation**:

```
Goal: (CAT, HAS, LUNGS)
 │
 ├── Hop 1: Direct lookup (CAT, HAS, LUNGS) -> Not found
 ├── Hop 2: Find taxonomy: (CAT, IS_A, FELINE)          [γ = 1.00]
 ├── Hop 3: Find taxonomy: (FELINE, IS_A, MAMMAL)       [γ = 0.90]
 └── Hop 4: Transitive rule: (MAMMAL, HAS, LUNGS)       [γ = 0.81]
      │
      └── Proof Trace: CAT -> FELINE -> MAMMAL -> LUNGS
          Confidence: 81.0% (3 logical hops)
```

The inference algorithm is formally defined as:
```
function Deduce(Subject S, Predicate P, Object O, depth, max_depth, γ):
    if depth > max_depth: return (FAIL, 0)
    if FindRelation(S, P, O): return (SUCCESS, 1.0)
    
    for each Relation (S, IS_A, Parent) in Graph:
        if Parent in VisitedBitVector: continue
        MarkVisited(Parent)
        (status, conf) = Deduce(Parent, P, O, depth + 1, max_depth, γ)
        UnmarkVisited(Parent)
        if status == SUCCESS:
            return (SUCCESS, conf * γ * rule_weight)
            
    return (FAIL, 0)
```
- **Cycle Immunity**: A compact bit vector prevents circular reasoning ($A \to B \to A$).
- **Bounded Exploration**: Traversal halts if depth exceeds $D_{\max} = 5$ or if accumulated confidence $\prod \gamma_i < 0.25$.

### 3.2 Matmul-Free Symbolic Attention

Traditional Transformers compute attention via quadratic matrix multiplications:

$$
\text{Attention}(Q, K, V) = \text{softmax}\left(\frac{QK^T}{\sqrt{d_k}}\right)V, \quad \mathcal{O}(N^2 \cdot d)
$$

Symbolic LLM calculates attention over tokens in a prompt or sentence using direct Knowledge Base structural topology:

$$
\text{Attention}(t_i) = w_r \cdot \text{deg}_{\text{KB}}(t_i) + w_c \cdot \text{conn}(t_i, t_{\setminus i}) + w_p \cdot \text{pos}(t_i) + w_n \cdot \text{novelty}(t_i)
$$

where:
- $\text{deg}_{\text{KB}}(t_i)$: Number of verified relations involving symbol $t_i$ in the graph.
- $\text{conn}(t_i, t_{\setminus i})$: Number of direct relational edges connecting $t_i$ to other tokens in the same input.
- $\text{pos}(t_i)$: Structural positional bias (favoring Subject-Verb-Object head positions).
- $\text{novelty}(t_i)$: Flag denoting unseen information worthy of episodic assimilation.

This symbolic attention formulation computes token salience in **$O(N)$ linear time** without floating-point matrix operations.

#### 3.2.1 Transformer-Inspired Symbolic Enhancements (Phases 1–6)

Beyond classical symbolic attention, Symbolic LLM integrates six transformer-derived concepts adapted to pure symbolic execution:

| Phase | Concept | Symbolic Adaptation | Effect |
|-------|---------|---------------------|--------|
| **1** | **Layer Normalization** | Normalize embedding centroids (mean=0, var=1) before top-m signature extraction | Prevents dominant embeddings from skewing sentence signatures; balanced ranking |
| **1** | **Temperature Scaling** | Adjustable divisor on QKV scores: $T < 1$ sharpens, $T > 1$ softens | Controls precision-vs-exploration tradeoff without retraining |
| **2** | **QKV Separation** | Inverted index (keys: symbol→sentences), precomputed novelty (values), query words (queries) | $O(\text{matches})$ vs $O(N)$ full scan; 100× faster on large corpora |
| **3** | **Positional Encoding** | Relative distance + order coherence between matching tokens in query vs. sentence | Distinguishes "Jonás come" from "come Jonás" without embeddings |
| **4** | **KV-Cache** | Per-session novelty cache: $1/(1+\text{freq})$ computed once per unique symbol | Eliminates redundant TF-IDF recomputation across queries |
| **5** | **Sparse Attention** | Window around matches (±32 sentences) + $\sqrt{N}$ global anchor sentences | Reduces scored sentences from $N$ to $O(\sqrt{N})$ for large corpora |
| **6** | **Cross-Attention** | Character bigram Dice coefficient aligns query tokens to corpus symbols | ES→EN alignment without explicit dictionary; "soft translation" |

**Mathematical Details:**

*Layer Normalization:*
$$\hat{v}_d = \frac{v_d - \mu}{\sigma}, \quad \mu = \frac{1}{D}\sum_{d} v_d, \quad \sigma = \sqrt{\frac{1}{D}\sum_{d}(v_d - \mu)^2}$$

*Temperature Scaling:*
$$\text{score}' = \frac{\text{score}}{T}, \quad T \in (0, \infty)$$

*Cross-Attention (Bigram Dice):*
$$\text{XA}(q, s) = \frac{2 \cdot |\text{bigrams}(q) \cap \text{bigrams}(s)|}{|\text{bigrams}(q)| + |\text{bigrams}(s)|}$$

*Sparse Attention Window:*
$$\text{candidates} = \bigcup_{m \in \text{matches}} [m - w, m + w] \cup \{k \cdot \sqrt{N} : k \in \mathbb{N}\}$$

### 3.3 Conversational State & Anaphora Working Memory

The conversational engine maintains a lightweight working memory register:
- **Anaphoric Focus ($O(1)$)**: Tracks the active discourse entity, resolving pronouns (*he, she, it, they, him, her*) and elliptical clauses to the current focus symbol.
- **Dialogue Continuity Cache**: Retains recent keywords (`twords`) and already displayed sentences (`tshown`) to support natural sequential interactions (*"explain it to me"*, *"continue"*, *"tell me more"*) without repeating identical facts.

### 3.4 Open-Domain Question Answering with N-Gram Phrase Extraction

The QA pipeline answers questions over arbitrary text corpora through a cascaded search strategy:

```
Question → ParseIntentToks → Intent Classification
    ↓
INT_QA_ENTITY / WHERE / WHAT / WHY / COUNT
    ↓
┌─ 1. KB lookup (exact match, O(1))
├─ 2. TextLexRetrieve (embedding similarity, O(matches))
├─ 3. Dictionary translation → retry KB/text
├─ 4. Raw substring search (phrase in corpus text)
│      ├─ Entity from parser (p→a)
│      ├─ Bigrams: all consecutive token pairs
│      └─ Trigrams: all consecutive token triples
└─ 5. UNKNOWN (fail-closed)
```

**N-Gram Phrase Extraction**: When the entity parser truncates a multi-word phrase (e.g., "fluid mechanics" from "what is fluid mechanics?"), the system tries all consecutive bigrams and trigrams of the question tokens as search phrases:

$$\text{phrases} = \{w_i w_{i+1} : i \in [0, n)\} \cup \{w_i w_{i+1} w_{i+2} : i \in [0, n)\}$$

Each phrase is searched as a case-insensitive substring in the raw corpus text ($O(|\text{corpus}|)$ per phrase). This recovers multi-word entities like "Trinity Meadows", "Formula One", "German Resistance" that single-token lookup misses.

**Battery 100 Results** (Jung + Bible + Wikipedia, 41,431 sentences):

| Metric | Value |
|--------|-------|
| Answered | 88 / 100 |
| UNKNOWN | 12 / 100 |
| Wrong | 0 / 100 |
| Recall gain from n-grams | +7 questions |

---

## 4. Empirical Evaluation and Benchmarks

### 4.1 Micro-benchmarks: Hash Table Scaling & Memory Density

The synthetic benchmark `tests/bench_1m_relations.c` stresses the fundamental limits of the relation storage engine:

| Benchmark Metric | Transformer (LLaMA-3-8B / GPT-4) | Symbolic LLM (C11) | Performance Ratio |
| :--- | :--- | :--- | :--- |
| **Ingestion Time (1M Triples)** | Hours (fine-tuning / LoRA) | **0.185 seconds** | **> 50,000× faster** |
| **Ingestion Throughput** | ~50–200 tokens/s | **5,399,568 relations/s** | **> 25,000× higher** |
| **Query Latency (Single Lookup)** | 15–50 milliseconds | **72 nanoseconds** | **> 200,000× lower** |
| **Query Throughput** | 20–100 queries/s | **13,800,000 queries/s** | **> 100,000× higher** |
| **RAM Footprint (1M Triples)** | 16 GB to 32 GB VRAM | **32.00 MB RAM** | **500× to 1000× denser** |
| **Hardware Required** | NVIDIA A100 / RTX 4090 | **Single commodity CPU core** | Zero GPU requirement |
| **Integrity & Precision** | Non-deterministic / Stochastic | **100% bit-exact integrity** | Absolute determinism |

*Detailed benchmark execution log:*
```
=========================================================
  SYMBOLIC LLM - BENCHMARK 1M RELATIONS (HASH O(1))
=========================================================

1. Creating symbol table...
   Symbols: 10,000 (in 0.0031 s)

2. Inserting 1,000,000 unique relations...
   Inserted: 1,000,000 relations
   Time:     0.1852 s
   Speed:    5,399,568 rel/s

3. Executing 100,000 random queries...
   Verified: 100,000 queries (100% correct)
   Time:     0.0072 s
   Latency:  72.4 ns/query (13,805,420 queries/s)

4. Memory allocation:
   Relation Table RAM: 32.00 MB (strictly 32 bytes/relation)
   Collisions resolved: 100%
```

### 4.2 Stress Testing: 50,000 High-Dimension Embeddings

The benchmark `tests/bench_stress_50k.c` measures the persistence and mounting speed of the binary V2 format:
- **50,000 concepts + 32D float vectors**: Serialized into a single binary file of **7.53 MB**.
- **Cold Disk Write**: 25.49 ms (295 MB/s).
- **Cold Memory Mount**: **15.52 ms** (ready for instant inference).

### 4.3 Free-Text Ingestion Throughput on Real Corpora

Unlike TSV-bound tools, Symbolic LLM streams arbitrary free text directly into RAM:

| Text Corpus | File Size | Sentences | Unique Symbols | Ingest Time | Ingest Throughput |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Jung: Psychology of the Unconscious** | 1.15 MB | 10,730 | 19,505 | **0.25 s** | **4.60 MB/s** |
| **King James Bible (Full Old & New Testament)** | 4.44 MB | 28,746 | 14,027 | **1.08 s** | **4.11 MB/s** |
| **Spanish Wikipedia (`eswiki.txt` ~4.5 GB clean)** | 4,500 MB | ~28,000,000 | ~1,200,000 | **~16 min (1 core)** / **~1.8 min (sharded)** | **~42 MB/s (16-core)** |
| **English Wikipedia (`enwiki.txt` ~19 GB clean)** | 19,000 MB | ~115,000,000 | ~3,500,000 | **~75 min (1 core)** / **~7.2 min (sharded)** | **~44 MB/s (16-core)** |

*Note*: Ingestion requires no GPU or complex tokenization pipelines; runtime memory for the entire Bible model remains under **28 MB RAM**.

### 4.4 Regression & Safety Suite Verification

The project adheres to strict fail-closed regression gates enforced via CMake CTest:
- **47 / 49 CTest Unit & Integration Tests PASS (96%)**: Validating symbol hashing, 32D embeddings, backward chaining, BFS transitive closure, anaphora resolution, schema transfer, QA layer, text lexicon, and server protocols. (2 pre-existing failures in composite/clarify tests.)
- **20/20 Jung Battery**: Cross-lingual QA (Spanish queries → English corpus) with zero UNKNOWNs and zero false positives.
- **Phase 4 Canonicalization Golden Battery**: 26/26 queries byte-identical across execution runs, confirming zero degradation in factual retrieval.
- **Conversational Topic Tests**: Unsupervised topic discovery validated on disparate literary styles (theological, psychological, historical) with zero hardcoded lexicons.

---

## 5. Practical Usage and System Deployment

### 5.1 Build from Source

Requirements: Any standard C11 compiler (GCC, Clang, or MSVC) and CMake >= 3.20.

```bash
# Clone the repository
git clone https://github.com/FiveTechSoft/symbols.git
cd symbols

# Configure and compile with GCC/Clang
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

# Run the complete test suite (49 tests)
ctest --output-on-failure

# Run cross-lingual QA battery (20 questions)
./test_jung_battery
```

### 5.2 Interactive CLI REPL (`chat_main`)

Symbolic LLM includes an interactive conversational shell supporting live learning and arbitrary text ingestion:

```bash
# Launch REPL with default corpus
./chat_main

# Or launch directly with custom free-text files
./chat_main data/texts/jung.txt
```

```
[chat] text corpus: 10730 sentences, 19505 symbols
Ready. Enter a question (or 'exit').

User > what areas do you know?
AI   > The ingested texts cover topics such as: Altertumskunde, Bough, Mephistopheles, 
       Negelein, Fulfilment, Mead, Fathers, White (with 10,730 sentences and 19,505 symbols 
       in data/texts/jung.txt). You can ask me about any of these subjects.

User > start a conversation
AI   > We can talk about Bough. According to the text: "(Quoted from Frazer: 'Golden Bough', 
       Part IV, p. ...". Which aspect would you like to explore?

User > explain it to me
AI   > According to the text [continue]: This especial tree seems simply to continue the 
       category of the mother symbols.

User > load data/texts/bible.txt
AI   > [chat] loaded text 'data/texts/bible.txt': 28,746 sentences, 14,027 symbols added.

User > Who is the father of David?
AI   > Jesse. [Proof trace: DAVID --SON_OF--> JESSE]
```

### 5.3 High-Performance OpenAI-Compatible HTTP Server (`symbols_server`)

The engine provides an embedded HTTP REST server compliant with the OpenAI `/v1/chat/completions` specification, allowing drop-in integration with standard web interfaces, desktop applications, and agentic workflows (such as OpenCode):

```bash
# Start server on port 8080 loading multiple free-text corpora
./symbols_server 8080 data/texts/bible.txt data/texts/jung.txt
```

```bash
# Query the model using curl
curl http://localhost:8080/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "model": "symbolic-llm-c11",
    "messages": [{"role": "user", "content": "what areas do you know?"}]
  }'
```

### 5.4 Standalone Client-Side Web Agent & Edge Deployment

Symbolic LLM provides a zero-install, browser-native implementation executing entirely within client-side memory:
- **Interactive Web Interface**: Hosted directly on GitHub Pages at [fivetechsoft.github.io/symbols](https://fivetechsoft.github.io/symbols/).
- **Dual-Mode Architecture**: Executes associative retrieval, concept concentration ($\kappa$), and deductive tracing directly in-browser, while optionally serving as a visual front-end for a native C11 `symbols_server` instance running on `localhost:8080`.
- **Episodic Persistence & Export**: Dialogue turns and learned assertions synchronize to browser `localStorage`, enabling instant session resumption and full-state export (`.json`) with zero knowledge loss.

---

## 6. Comparison with State-of-the-Art Approaches

| Characteristic | Classical Prolog / Expert Systems | Traditional Transformers (LLaMA, GPT) | Retrieval-Augmented Generation (RAG) | Symbolic LLM (This Work) |
| :--- | :--- | :--- | :--- | :--- |
| **Representation** | Pure discrete rules | Dense matrix weights ($\mathbb{R}^N$) | Neural embeddings + Dense LLM | **Discrete Triples + 32D Substrate + Symbolic Attention** |
| **Hallucination Rate** | 0% (Rule bounded) | 15% – 35% (Confabulation) | 5% – 15% (Faithfulness gap) | **0% by design (Fail-closed)** |
| **Inference Latency** | Milliseconds to seconds | 20 – 100 ms / token | 200 – 1000 ms | **< 1 millisecond end-to-end** |
| **Memory per Fact** | High (symbolic pointer trees) | Diffuse (fractional parameter) | High (dense chunks + DB index) | **Strictly 32 bytes / relation** |
| **Synonym Flexibility** | None (brittle exact match) | High (continuous geometry) | High | **High (32D Hebbian cosine + cross-attention)** |
| **Online Learning** | Slow dynamic assertz | Impossible without fine-tuning | Re-indexing external DB | **Instantaneous $O(1)$ streaming insert** |
| **Hardware Barrier** | CPU | Multi-GPU / Dedicated TPU | GPU + Vector DB server | **Single standard CPU (x86/ARM)** |
| **Attention Mechanism** | None (forward chain) | $\mathcal{O}(N^2 d)$ matmul | Embedding similarity | **$\mathcal{O}(N)$ symbolic + sparse + cross-attn** |

---

## 7. Limitations and Future Work

Symbolic LLM is not designed to compete with 70-billion-parameter neural models in generating improvisational literary fiction, poetic metaphors, or unconstrained free-form prose. Its objective is **deterministic factual mastery, auditable reasoning, and ultra-high-density edge deployment**.

Current research directions include:
- **Higher-Order Logical Quantifiers**: Expanding first-order relational triples $\langle S, P, O \rangle$ into hyper-graphs capable of expressing modalities ($\text{Possible}$, $\text{Necessary}$) and temporal boundaries ($\text{ValidDuring}[T_1, T_2]$).
- **Symbiotic Neuro-Symbolic Cascades**: Deploying Symbolic LLM as an ultra-fast, zero-latency deterministic safety and fact-checking filter that intercepts and verifies the outputs of generative neural decoders before presentation to the user.
- **Hardware Acceleration via Custom ASICs**: Given that relations are strictly 32-byte structs and inference is dominated by bitwise masking and integer hashing, the entire engine can be synthesized onto low-cost FPGAs or microcontrollers with sub-microsecond end-to-end response times.

---

## 8. References

1. Vaswani, A., et al. (2017). *Attention Is All You Need*. Advances in Neural Information Processing Systems (NeurIPS).
2. Kanerva, P. (2009). *Hyperdimensional Computing: An Introduction to Computing in Distributed Representation with High-Dimensional Random Vectors*. Cognitive Computation, 1(2), 139-159.
3. Sowa, J. F. (2000). *Knowledge Representation: Logical, Philosophical, and Computational Foundations*. Brooks/Cole.
4. Hebb, D. O. (1949). *The Organization of Behavior: A Neuropsychological Theory*. John Wiley & Sons.
5. Knuth, D. E. (1998). *The Art of Computer Programming, Volume 3: Sorting and Searching* (2nd ed.). Addison-Wesley. (Open addressing and collision resolution).
6. Marcus, G. (2020). *The Next Decades in AI: Four Steps Towards Robust Artificial Intelligence*. arXiv:2002.06177.
7. Russell, S., & Norvig, P. (2020). *Artificial Intelligence: A Modern Approach* (4th ed.). Pearson. (Backward chaining and propositional representations).
8. FiveTech Software Research. (2026). *Symbolic LLM Technical Reports (Phase 1 through Phase 4)*. `FiveTechSoft/symbols`.

---

## License

This project is open-source research software released under the **MIT License**. You are free to inspect, adapt, embed, and expand this work in academic, personal, or commercial environments.
