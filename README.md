# Symbolic LLM in C

A lightweight symbolic language model written in **pure C11**. No matrices, no backpropagation, no PyTorch. Symbols, relations, 32D embeddings, and local probabilistic learning.

This model represents a paradigm shift from the dominant trend in artificial intelligence. While **current LLMs (Large Language Models)** based on the Transformer architecture rely on brute force — massive dense matrix multiplications, global backpropagation, billions of opaque parameters, and enormous energy consumption — this system proposes a **lightweight, deterministic, neuro-symbolic probabilistic approach implemented in pure C**.

---

## What Makes This Model Unique?

### 1. Explicit Knowledge Representation Instead of Diffuse Weights

In a Transformer, a fact (like *"the cat eats fish"*) gets diluted across billions of floating-point parameters. In this model, knowledge resides in a **graph of discrete symbolic relations** ⟨Subject, Predicate, Object⟩. The knowledge is inspectable, auditable, and storable directly as structured data.

### 2. Local Probability Without Neural Networks

To predict the next word, it doesn't require an autoregressive inference pass through dense layers and softmax functions over 128k-token vocabularies. Prediction is computed via **relative frequencies and local empirical normalization**:

```
P(Object | Subject, Predicate) = count(S, P, O) / Σ count(S, P, O')
```

This reproduces language predictive capability without dense matrices.

### 3. Ultra-Lightweight Hybrid Vector Architecture (32D)

Solves the classic "rigidity" problem of symbolic AI (like Prolog or classical graphs) by incorporating **32-dimensional co-occurrence vectors**. This enables capturing semantic similarity and synonyms (*cat* ≈ *feline*) via ultra-fast cosine similarity in nanoseconds, without requiring 4096 or 8192-dimensional embeddings.

### 4. Working Memory and Anaphora Resolution in O(1)

Replaces quadratic self-attention (O(N²) or the gigabyte KV-cache) with a structured entity registry with temporal decay and morphosyntactic agreement. Resolves pronouns (*"he"*, *"she"*, *"this"*) and elliptical subjects in constant time.

### 5. Transparent Causal and Deductive Reasoning (Backward Chaining)

Capable of inferring unseen knowledge (e.g., Siamese → Cat → Mammal → Lungs) through an acyclic chaining engine with confidence attenuation per logical hop, delivering a step-by-step trace of why it asserts something.

---

## Comparison: This Model vs. Current LLMs (Transformers)

| Dimension | Current LLMs (LLaMA, GPT, Claude) | This Symbolic Model in C |
| --- | --- | --- |
| **Memory Usage** | 8 GB to hundreds of GB of GPU VRAM. | **32 MB RAM** for 1 million complete relations. |
| **Hardware Required** | Dedicated accelerators (NVIDIA GPUs / TPUs). | **Any standard CPU** (Windows, Linux, embedded). |
| **Inference Speed** | 20 to 150 tokens/second (ms latency). | **> 13 million queries/second** (~70 ns per query). |
| **Continuous Learning** | Impossible at runtime (catastrophic forgetting; requires retraining or LoRA). | **Instant O(1) streaming insertion** without forgetting anything. |
| **Hallucinations** | Frequent and hard to detect (stochastic black box). | **Zero hallucination**: if no path exists in the graph, it deterministically responds that it doesn't know. |
| **Explainability** | Opaque (attention weights don't indicate logical causality). | **100% auditable**: exact logical trace of every deduction. |
| **Cold Start** | Seconds or minutes loading tensors into memory. | **< 20 milliseconds** to load 50,000 concepts from disk. |
| **Model Size** | Checkpoints from 4 GB to 140 GB. | **Under 1 MB** for tens of thousands of facts. |
| **Dependencies** | Python, PyTorch/CUDA, BLAS, complex libraries. | **Standard C11**, no external frameworks or dependencies. |

---

## Key Strategic Advantages

- **Edge Computing Sovereignty**: Can run on embedded systems, microcontrollers, routers, or integrate as a simple native DLL (`symbolicllm.dll`) within desktop applications in C, C++, Harbour/FiveWin, or Python without requiring internet connectivity or expensive hardware.
- **Immediate Maintenance and Correction**: If a stored fact is wrong, it is located and deleted from the hash table in O(1). In a traditional LLM, unlearning a specific fact without degrading the rest of the model remains an unsolved research problem.
- **Domain Specialization**: For systems where precision, structured reasoning, legal/medical/technical traceability, and speed are mandatory, this approach offers a cost/performance ratio several orders of magnitude superior to deploying a local LLM.

It doesn't compete with an LLM in improvising generalist creative prose, but demonstrates that **a fundamental part of linguistic intelligence, reasoning, and prediction can be achieved with much cleaner, faster, and more interpretable mechanisms than a massive neural network**.

---

## Architecture

```
SYMBOL TABLE     -> Unique named concepts (IDs + frequency)
RELATION TABLE   -> Triples: subject --predicate--> object (count + weight)
GRAPH            -> Queries, transitive inference, fuzzy synonym resolution
LEARNING         -> Sentence parsing, probabilistic prediction, canonicalization
CONTEXT          -> Anaphora resolution, pronoun handling, working memory
GENERATOR        -> Relations -> natural language with probabilities
EMBEDDINGS       -> 32D vectors: Random Indexing + Hebbian co-occurrence
MODEL            -> Persistence V2: binary format (graph + 32D vectors)
INFERENCE        -> Backward chaining, materialization, compositional rules
SHARD            -> Corpus splitting, binary model merge, parallel training
INGEST           -> Streaming TSV parser with 64KB I/O buffers
PRUNE            -> Zipf pruning: remove low-count noise relations
```

## Key Features

- **Symbolic reasoning**: O(1) exact lookups, transitive inference, backward chaining with confidence attenuation
- **32D embeddings**: cosine similarity for synonym detection (GATO ≈ FELINO)
- **Hybrid queries (H4)**: exact search first, vector fallback for unknowns
- **Probabilistic output**: "El gato come pescado (66.7%), carne (33.3%)"
- **Anaphora resolution**: "Él compila" → "ANTONIO compila"
- **Deep inference**: Siamese → Cat → Mammal → Lungs (72.9% confidence, 4 hops)
- **Compositional rules**: ES + TIENE => TIENE (automatic materialization)
- **Binary persistence V2**: graph + 32D vectors in a single file, V1 compatible
- **Sharding**: corpus partitioning for parallel training + exact merge
- **Noise pruning**: statistical filtering by Zipf's Law (removes 70-80% of noisy relations)
- **Benchmark**: 1M relations in 32 MB RAM, 70 ns per query

---

## Quick Start

### Compile

```bash
# Interactive REPL
gcc -std=c11 -Wall -Iinclude \
    src/symbol.c src/relation.c src/embedding.c src/graph.c \
    src/learning.c src/context.c src/generator.c src/model.c \
    src/ingest.c src/prune.c src/shard.c src/inference.c \
    src/main_cli.c -o sllm_cli.exe -lm

# Mass training pipeline
gcc -std=c11 -Wall -Iinclude \
    src/symbol.c src/relation.c src/embedding.c src/graph.c \
    src/learning.c src/context.c src/generator.c src/model.c \
    src/ingest.c src/prune.c src/shard.c src/inference.c \
    src/train_shard.c -o train_shard.exe -lm
```

### Run REPL

```bash
./sllm_cli.exe
```

### Example Session

```
Tu > El gato come pescado.
  Aprendido. (6 relaciones)

Tu > El gato come carne.
  Aprendido. (7 relaciones)

Tu > ¿Qué come el gato?
IA > El gato come pescado y carne.

Tu > ¿Tiene pulmones el siames?
IA > Sí. Demostración: SIAMES --ES--> GATO --ES--> FELINO --ES--> MAMIFERO --TIENE--> PULMONES
     (Confianza: 72.9%, 4 saltos)

Tu > /alias felino gato
  Alias: 'FELINO' ~ 'GATO' (similitud: 99.9%)

Tu > /save model.bin
  Modelo V2 guardado en 'model.bin'.

Tu > /exit
```

### Train with Massive TSV Corpus

```bash
# Generate test corpus (550K triples)
python tools/generate_test_tsv.py

# Direct training with pruning
./train_massive.exe tools/test_corpus.tsv model.bin 3

# Training with sharding (4 shards)
./train_shard.exe tools/test_corpus.tsv model.bin 3 4
```

---

## REPL Commands

| Command | Description |
|---|---|
| `/graph` | Show all relations in the knowledge graph |
| `/context` | Show working memory entities |
| `/embed` | Show 32D embedding vectors |
| `/synonyms PAL` | Find similar concepts by cosine |
| `/synonyms A B` | Cosine similarity between two concepts |
| `/alias NEW BASE` | Define synonym manually |
| `/save <file>` | Save model V2 to disk (graph + vectors) |
| `/load <file>` | Load model from disk |
| `/clear` | Reset context |
| `/help` | Show all commands |
| `/exit` | Quit |

---

## Deep Inference Engine

### Backward Chaining with Confidence Attenuation

```
Query: "Does the cat have lungs?"
                         │
                         ▼
  Level 0: CAT
  Level 1: CAT ──IS──> FELINE      (w = 1.00)
  Level 2: FELINE ──IS──> MAMMAL   (w = 1.00 × 0.90 = 0.90)
  Level 3: MAMMAL ──HAS──> LUNGS   (w = 0.90 × 0.90 = 0.81)
                         │
                         ▼
  CAT ──HAS──> LUNGS  (confidence = 81%, 3 hops)
  Trace: CAT -> FELINE -> MAMMAL -> LUNGS
```

### Compositional Rules

```
If A ──IS──> B  and  B ──HAS──> C
then A ──HAS──> C  (conf × rule_weight × γ)
```

### Cycle Protection

The engine uses a `visited[]` vector to detect cycles in the graph and stops exploration when:
- Depth exceeds `max_depth` (typically 5)
- Accumulated confidence falls below `min_confidence` (typically 0.25)
- A cycle is detected (node already visited in the current chain)

---

## Binary Format V2

```
HEADER (24 bytes)
  Magic:     0x53594D42 ("SYMB")
  Version:   2
  Symbols:   uint32
  Relations: uint32
  Embeddings:uint32
  EmbedDim:  32

SYMBOL BLOCK
  [id, name_len, name_bytes, frequency] × SymbolCount

RELATION BLOCK
  [subject, predicate, object, count, weight] × RelCount

EMBEDDING BLOCK
  [symbol_id, float[32]] × EmbeddingCount
```

V1 files (without embeddings) remain compatible.

---

## Hash Table Optimization (O(1))

Both `SYMBOL_TABLE` and `RELATION_TABLE` use open-addressing hash tables with linear probing:

- **Power-of-2 capacities**: Bitwise `& mask` instead of modulo `%`
- **70% load factor**: Automatic rehash to keep collisions minimal
- **DJB2a (XOR)**: Fast string hashing for symbols
- **MurmurMix64**: High-dispersion integer hash for (subject, predicate, object) triplets
- **Dual indexing**: Dense `items[]` array for sequential access + `buckets[]` for O(1) lookup

---

## Project Structure

```
include/
  symbol.h         SYMBOL_TABLE with hash index: O(1) Find
  relation.h       RELATION_TABLE with hash index: O(1) Find
  embedding.h      32D vectors: RandomInit, Cooccur, Cosine, FindSimilar
  graph.h          GRAPH: queries, transitive inference, fuzzy resolution
  learning.h       Sentence parsing, probabilistic prediction
  context.h        Anaphora, pronoun resolution, working memory
  generator.h      Relations -> natural language text
  model.h          MODEL: Create, Save V2, Load (V1+V2 compatible)
  inference.h      Backward chaining, materialization, compositional rules
  shard.h          Corpus splitting, binary model merge
  ingest.h         Streaming TSV parser with 64KB buffers
  prune.h          Noise pruning: PruneByMinCount, PruneByMinWeight

src/
  symbol.c         Hash table with DJB2a + linear probing
  relation.c       Hash table with MurmurMix64 + linear probing
  embedding.c      32D embedding math + similarity search
  graph.c          Graph queries + hybrid fuzzy resolution
  learning.c       Corpus learning + prediction
  context.c        Context management + anaphora
  generator.c      Text generation from relations
  model.c          Binary serialization V2
  inference.c      Backward chaining DFS + transitive materialization
  shard.c          DJB2a splitting + count-summation merge
  ingest.c         Streaming TSV parser with 64KB buffer
  prune.c          Compact-in-place pruning with hash rebuild
  main_cli.c       Interactive REPL
  train_massive.c  Pipeline: ingest → prune → save
  train_shard.c    Pipeline: split → train shards → merge → prune → save

tests/
  test_symbol.c         Symbol table unit tests
  test_graph.c          Graph + transitive inference
  test_learning.c       Probabilistic corpus learning
  test_context.c        Anaphora resolution
  test_generator.c      Text generation
  test_embedding.c      32D cosine similarity
  test_model.c          Persistence V1
  test_model_embeddings.c  Persistence V2 with vectors
  test_graph_fuzzy.c    Hybrid fuzzy query (H4)
  test_deep_inference.c Backward chaining, 4-level chain, no hallucination
  test_ingest.c         Ingest → prune → persist pipeline
  bench_stress_50k.c    50K symbols + embeddings benchmark
  bench_1m_relations.c  1M relations benchmark

tools/
  extract_triples.py    spaCy OpenIE triple extractor
  generate_test_tsv.py  Zipf-distributed test corpus generator
```

---

## Benchmark Results

### 1M Relations (Hash O(1))

```
========================================================
  SYMBOLIC LLM - BENCHMARK 1 MILLION RELATIONS
========================================================

  Relations inserted   : 1,000,000
  Insertion speed      : 5,399,568 rel/s (0.185s)
  Query latency        : 72 ns/query (13.8M queries/s)
  RAM memory           : 32.00 MB (32 bytes/relation)
  Integrity            : 100%
```

### 50K Symbols + Embeddings

```
=========================================================
  SYMBOLIC LLM - STRESS BENCHMARK (50,000 32D)
=========================================================

  Symbols & Vectors   : 50000
  Disk size           : 7.53 MB
  Disk write          : 25.49 ms (295 MB/s)
  Load and mount      : 15.52 ms (3221 symbols/s)
  Integrity           : 100% bit by bit
```

### Massive Pipeline (550K Triples)

```
============================================================
  SYMBOLIC LLM - MASSIVE INGEST (Sharded + Pruned)
============================================================

  Triples read        : 550,000
  Shards              : 4
  Final relations     : 49,434 (post-pruning, count >= 3)
  Pruned              : 76.7%
  Disk size           : 2.75 MB
  Total time          : ~0.8 s
  Effective speed     : ~697K triples/s
```

---

## Test Results

```
test_symbol              PASS (CRUD, bulk, frequency)
test_graph               PASS (queries, transitive inference)
test_learning            PASS (70/30 probabilities, prediction)
test_context             PASS (anaphora, pronoun resolution)
test_generator           PASS (relation -> text, probabilities)
test_embedding           PASS (32D cosine, synonym detection)
test_model               PASS (persistence V1)
test_model_embeddings    PASS (persistence V2 with vectors)
test_graph_fuzzy         PASS (hybrid FELINO -> GATO resolution)
test_deep_inference      PASS (backward chaining, 4-level chain, no hallucination)
test_ingest              PASS (ingest -> prune -> persist pipeline)
bench_stress_50k         PASS (50K symbols, 7.5MB, 100% integrity)
bench_1m_relations       PASS (1M relations, 32MB, 13.8M queries/sec)
```

---

## Memory Footprint

| Component | Size |
|---|---|
| 50K symbols (names + metadata) | ~1.5 MB |
| 50K embeddings (32D float) | ~6.4 MB |
| 50K relations | ~0.5 MB |
| Total disk (V2) | 7.53 MB |
| Total RAM (runtime) | ~12 MB |

---

## License

Public domain. Use freely.
