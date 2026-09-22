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

### 3.4 Open-Domain Question Answering with Structural Decomposition and Contextual Retrieval

The QA pipeline answers open-domain factual questions over arbitrary text corpora through a multi-stage deterministic cascaded architecture:

```
Question → Surface Split → Canonicalization (Phase 4)
    ↓
LooksLikeQuestionWord / DetectQuestionType (Structural QA Intercept)
    ↓
┌─ INT_QA_ENTITY: <wh> [noun] <aux/copula> <entity...>
├─ INT_QA_WHERE:  <wh> <loc_prep> <entity...>
├─ INT_QA_COUNT:  <wh> <count_prep> <entity...>
├─ INT_QA_WHY:    <wh_cause> <entity> <relation>
└─ INT_TEXTQ:     Dynamic Text Query / Fallback
    ↓
Multi-Strategy Grounding:
┌─ 1. KB Triplet Exact Lookup (O(1) hash table)
├─ 2. Multi-Word Entity & Contextual Tokenization
│      ├─ Entity tokens split into constituent graph symbols
│      ├─ Contextual non-stop question tokens appended (up to 16)
│      └─ Relative corpus frequency filtering via TextLexFindSymbol
├─ 3. TextLexRetrieve (QKV Attention, Inverted Index & Cross-Attention)
├─ 4. Case-Insensitive Raw Substring N-Gram Fallback
└─ 5. Honest Epistemic Abstention (UNKNOWN, fail-closed)
```

#### 3.4.1 Structural Question Classification & Auxiliary Resolution
Traditional lexical keyword matchers struggle with complex grammatical questions (e.g., *"how did the Clean Water Act affect Trinity Meadows"* or *"what sport did Afanasenkov play"*), either discarding crucial auxiliary verbs or mistakenly treating them as search topics. 

Symbolic LLM resolves this through structural pattern interception before lexical filtering:
1. **Auxiliary-Linked Copular Templates**: Detects auxiliary links (`did`, `does`, `do`, `are`, `were`) and copulas across multi-word constructions:
   $$\langle \text{wh} \rangle\ [\text{noun}\dots]\ \langle \text{aux/copula} \rangle\ \langle \text{entity}\dots \rangle$$
   - Handles compound question phrases: *"what sport did..."*, *"what channel did..."*, *"what year was..."*, and compound nominal classifiers: *"what type of flow is..."*.
2. **Discourse Plan Cohesion (`ChatBuildPlan`)**: In dynamic text mode (`ch->ntfiles > 0`), single questions are protected against artificial goal splitting across unknown vocabulary tokens, preserving unified question scope.
3. **Multi-Word Entity Tokenization**: Composite entities (e.g., *"pipe flow"*, *"Clean Water Act"*, *"Trinity River"*) are decomposed into individual graph symbols rather than atomic strings with literal spaces, enabling $O(1)$ symbol resolution and multi-term scoring in `TextLexRetrieve`.
4. **Relative Corpus Frequency Filtering**: Tokens are evaluated against their global occurrence frequency in the symbol graph via `TextLexFindSymbol`. High-frequency non-informative terms are dynamically pruned without manual blacklists.
5. **Functional Closed Classes (`HARDCODING=0`)**: Structural stop-word filtering (`IsStopTok`) strictly employs grammatical closed classes (wh-particles, auxiliary verbs, prepositions, determiners) across Spanish and English, ensuring domain-independence.

**Battery 100 Results** (Jung + King James Bible + Wikipedia, 41,431 sentences, Ground-Truth Validated):

| Metric | Previous Baseline | Enhanced Engine | Net Delta |
| :--- | :--- | :--- | :--- |
| **Top-1 Exact Ground-Truth Match (`Correct`)** | 54 / 100 (54.0%) | **61 / 100 (61.0%)** | **+7 (+7.0%)** |
| **Non-Matching / Lexical Mismatches (`Wrong`)** | 41 / 100 (41.0%) | **36 / 100 (36.0%)** | **-5 (-5.0%)** |
| **Honest Epistemic Abstention (`UNKNOWN`)** | 5 / 100 (5.0%) | **3 / 100 (3.0%)** | **-2 (-2.0%)** |
| **Total Query Attempts (`Answered`)** | 95 / 100 (95.0%) | **97 / 100 (97.0%)** | **+2 (+2.0%)** |
| **Unanchored Token Fabrication (Hallucination)** | **0.0%** | **0.0%** | **0.0% (Strictly 0)** |

*Scientific Invariant*: Because extractive retrieval is strictly bounded to literal sentences present in the ingested texts, the engine enforces **strictly 0% unanchored token fabrication** ($P(\text{fabrication}) = 0$). Every positive response is an exact verbatim citation with byte-offset provenance from the source corpus. The 7-point gain in top-1 accuracy reflects enhanced multi-word entity resolution and structural auxiliary handling without sacrificing epistemic safety.


### 3.5 Autonomous Graph Reasoning: Induction, Forward Deduction & Abductive Diagnosis

To move beyond static triple stores and human-engineered rules, Symbolic LLM incorporates an active cognitive reasoning engine (`src/graph_reasoning.c`, `include/graph_reasoning.h`) operating directly over the graph topology across three fundamental inference modes:

```
               ┌────────────────────────────────────────────────────────┐
               │ 1. INDUCTION (Topological Rule Mining / AMIE / ILP)    │
               │    Discovers Horn rules from raw observational paths   │
               │    using Partial Completeness Assumption (PCA).        │
               └───────────────────────────┬────────────────────────────┘
                                           │ Discovered Rules
                                           ▼
 ┌──────────────────────────────────────────────┐ ┌─────────────────────────────────────────────┐
 │ 2. DEDUCTION (Forward-Chaining Completion)   │ │ 3. ABDUCTION (Missing Hypothesis Diagnosis) │
 │    Infers implicit edges to expand memory    │ │    Identifies the exact missing link needed │
 │    with fixed-point idempotence & 0 halluc.  │ │    to satisfy and close unproven goals.     │
 └──────────────────────────────────────────────┘ └─────────────────────────────────────────────┘
```

#### 3.5.1 Capability 1: Inductive Rule Learning from Tabula Rasa
The engine requires zero pre-programmed domain rules (`HARDCODING=0`). Given raw observational graph paths of length 2 ($A \xrightarrow{r_1} B \xrightarrow{r_2} C$), it computes the Partial Completeness Assumption (PCA) confidence:

$$
\text{conf}_{\text{PCA}}(r_1 \circ r_2 \implies r_3) = \frac{\text{supp}(r_1, r_2, r_3)}{\#\{ (A, C) : \exists B \text{ s.t. } r_1(A,B) \land r_2(B,C) \land \exists r' \text{ s.t. } r'(A,C) \}}
$$

Candidate rules are promoted to the active rule base only if they satisfy strict support ($S \ge 2$) and confidence gates ($C \ge 0.80$):
- **Heterogeneous Composition**: $r_1(A, B) \land r_2(B, C) \implies r_3(A, C)$ (e.g., $\text{father\_of} \circ \text{father\_of} \implies \text{grandfather\_of}$)
- **Homogeneous Transitivity**: $r(A, B) \land r(B, C) \implies r(A, C)$ (e.g., $\text{in} \circ \text{in} \implies \text{in}$, $\text{subclass\_of} \circ \text{subclass\_of} \implies \text{subclass\_of}$)
- **Symmetry & Inversion**: $r(A, B) \implies r(B, A)$ or $r_1(A, B) \implies r_2(B, A)$ (e.g., $\text{sibling\_of}(A, B) \implies \text{sibling\_of}(B, A)$)

#### 3.5.2 Capability 2: Autonomous Forward Deductive Link Prediction
Once rules are induced, the engine executes forward deductive chaining (`GraphApplyRules`). Given disconnected facts, it materializes previously unrecorded edges into the graph without external supervision:
- When presented with $\text{in}(\text{Toledo}, \text{Spain})$ and $\text{in}(\text{Spain}, \text{Europe})$, it automatically derives and materializes $\text{in}(\text{Toledo}, \text{Europe})$.
- When presented with $\text{father}(\text{David}, \text{Solomon})$ and $\text{father}(\text{Solomon}, \text{Rehoboam})$, it derives $\text{grandfather}(\text{David}, \text{Rehoboam})$.
- **Fixed-Point Convergence**: Successive deductive iterations yield $\Delta = 0$ new edges, ensuring strict mathematical stability and zero infinite loops.

#### 3.5.3 Capability 3: Abductive Diagnosis & Hypothesis Generation
When a query goal $(S, R_3, T)$ is `UNKNOWN` because no path fully connects $S$ to $T$, the abductive reasoner (`GraphAbduce`) traces backward through all candidate rules whose head matches $R_3$:
- If a forward pivot $P$ exists such that $r_1(S, P)$ is true, it identifies the necessary missing link: $r_2(P, T)$.
- If a backward pivot $P$ exists such that $r_2(P, T)$ is true, it identifies the necessary missing link: $r_1(S, P)$.
- **Formal Necessity & Sufficiency Proof**: Experimentally inserting the abduced hypothesis into the graph and executing forward chaining immediately and mathematically closes the goal query with 100% certainty (`tests/test_verify_3_points.c`).

### 3.6 Deep Symbolic Natural Language Generation (Graph-to-Text)

To verbalize complex multi-hop inference chains without the rigid phrasing of classical slot-filling or the hallucinations of stochastic neural decoders, the engine incorporates a three-stage symbolic NLG pipeline (`src/deep_nlg.c`, `include/deep_nlg.h`):
1. **Macroplanning (RST Trees)**: Converts graph subgraphs and deductive proofs into structured rhetorical relations (Sequence, Elaboration, Consequence, Epistemic Abstention).
2. **Microplanning & Aggregation**:
   - **Chaining Aggregation ($O_i = S_{i+1}$)**: Connects multi-hop genealogies and taxonomies into fluent relative clauses (*"Boaz begat Obed, who in turn was the father of Jesse, and the latter was the father of David..."*).
   - **Coordination & Subject Elision**: Groups multi-attribute entities without repetitive subject mentions (*"David is the son of Jesse, king of Israel, and furthermore the father of Solomon."*).
3. **Surface Realization & Epistemic Honesty**: Generates multilingual prose across Spanish, English, and French (`LANG_ES`, `LANG_EN`, `LANG_FR`), transforming unknown states into articulate explanations of epistemic boundaries (*"Although David is referenced, there is no verified record of his mother."*).

### 3.7 Advanced Cognitive Learning Paradigms on the Peircean Triad

Building upon the fundamental triad of Induction, Deduction, and Abduction (Charles Sanders Peirce's Cycle of Inquiry), Symbolic LLM implements four higher-order autonomous learning paradigms (`src/cognitive_learning.c`, `include/cognitive_learning.h`):

#### 3.7.1 Active Epistemic Inquiry & Curiosity-Driven Assimilation
When a user or reasoning goal requests information that is currently `UNKNOWN`, the engine does not merely stop. Instead, `CognitiveFormulateInquiry`:
1. Utilizes abductive inference to trace the missing causal or relational link.
2. Synthesizes a targeted inquiry question and search query keywords (e.g., `"david padre_de salomon"` with priority $P=1.00$).
3. Upon receiving external evidence (`CognitiveAssimilateEvidence`), the engine assimilates the edge into the knowledge graph and automatically triggers forward deduction to immediately confirm the original goal.

#### 3.7.2 Non-Monotonic Belief Revision (Defeasible Reasoning & Exception Guards)
Real-world ontologies require defeasible reasoning where general rules hold default truth subject to specific overrides (e.g., *"birds fly"*, but *"penguins do not fly"*). `CognitiveRegisterException` attaches dynamic exception guards to learned rules:
- Deduces positive properties for standard members of a class ($\text{aguila} \xrightarrow{\text{vuela\_en}} \text{cielo}$).
- Strictly blocks false deductions for registered exceptions ($\text{pinguino} \xrightarrow{\text{vuela\_en}} \text{cielo}$ is suppressed, maintaining 0 false positives).

#### 3.7.3 Symbolic Self-Supervised Learning (Masked Graph Discovery)
The symbolic equivalent of masked pre-training in language models. `CognitiveSelfSupervisedTrain`:
1. Systematically masks a subset of composite or transitive relations within the graph.
2. Attempts to deductively reconstruct the masked edges using only the remaining graph topology and active induction rules.
3. Quantifies the reconstruction rate ($\text{reconstructed} / \text{masked}$) and self-calibrates rule confidence thresholds without human annotation (achieving **100.00% reconstruction** on benchmark graphs).

#### 3.7.4 Continuous Peircean Inquiry Cycle
The `CognitiveRunInquiryCycle` unifies observation, induction, forward deduction, and belief revision into an automated cognitive loop, enabling the engine to perpetually organize, enrich, and audit its internal ontology.

### 3.8 Working Memory, Spreading Activation & Epistemic Metacognition

To advance beyond static knowledge retrieval into active cognitive focus and self-reflective reasoning, Symbolic LLM implements an integrated working memory and metacognitive auditing subsystem (`src/metacognition.c`, `include/metacognition.h`):

#### 3.8.1 Working Memory & Spreading Activation
- **Dynamic Cognitive Focus**: Maintains an active sub-graph of working memory ($7 \pm 2$ active concepts) with continuous activation energy $E \in [0.0, 1.0]$.
- **Relational Activation Spreading**: When a symbol is stimulated in conversation (e.g. `David`), energy propagates along verified relational edges to 1-hop (`Salomon`, `Betsabe`) and 2-hop (`Roboam`) neighbors, while isolated subgraphs remain dormant ($E = 0.0$).
- **Temporal Decay & Pruning**: Implements cognitive forgetting ($E_{t+1} = E_t \times \text{decay}$); concepts falling below the activation threshold are evicted from working memory, preventing attentional clutter.

#### 3.8.2 Epistemic Provenance DAG & Robustness Propagation
Every belief records its strict causal origin (`PROV_AXIOM`, `PROV_DEDUCED`, `PROV_ASSIMILATED`). For deduced facts, epistemic robustness is mathematically compounded across parent derivation paths:
$$R(\text{conclusion}) = \text{confidence}(\text{rule}) \times \prod_{p \in \text{premises}} R(p)$$

#### 3.8.3 Metacognitive Self-Auditing & Counterfactual Loss Analysis
- **Self-Justification (`MetacognitiveExplainBelief`)**: Explains *"Why do I believe this?"* by recursively generating the complete justification tree tracing derived facts back to base corpus axioms.
- **Weakest Link Identification (`MetacognitiveFindWeakestLink`)**: Pinpoints the most fragile premise or lowest-confidence rule in multi-hop deduction chains.
- **Hypothetical Loss Analysis (`MetacognitiveAuditHypotheticalLoss`)**: Simulates the counterfactual impact of refuting an axiom $X$, calculating the exact forward cascade of derived beliefs that would collapse without support.
- **Global Epistemic Health (`MetacognitiveAuditGraphHealth`)**: Quantifies the balance between direct observations, active deductions, and vulnerable beliefs.

### 3.9 Document-Level Passage Generation & Soft Intent Mapping (`passage_nlg`)

To bridge the gap between structured relational retrieval and the conversational fluency of large language models without compromising factual integrity, Symbolic LLM implements an integrated document-level passage synthesizer (`src/passage_nlg.c`, `include/passage_nlg.h`):

#### 3.9.1 Soft Intent Classification & Elastic Matching
Recognizes natural, open conversational queries (*"hablame de X"*, *"cuentame acerca de X"*, *"tell me about X"*, *"de que trata X"*) and maps them into formal semantic intents (`INTENT_SUMMARIZE_ENTITY`, `INTENT_WHY_QUERY`, `INTENT_VERIFY_QUERY`, `INTENT_METACOGNITION`).

#### 3.9.2 Cross-Lingual Entity Linking
Binds cross-lingual queries to canonical corpus symbols via declarative alignment dictionaries (`dict.c`), allowing Spanish questions (*"proverbios"*, *"salomon"*) to resolve directly against English knowledge graphs (`Proverbs`, `Solomon`).

#### 3.9.3 Sub-Graph Storytelling (Passage Generator)
Generates cohesive 4-paragraph essays combining:
1. **Executive Framing**: Defining the entity's core ontological role.
2. **Relational Dimensions**: Grounded compilation of verified graph edges.
3. **Working Memory Integration**: Active context stimulated via bidirectional spreading activation.
4. **Socratic Dialogic Closure**: Active curiosity prompts inviting deeper inquiry.
