# Symbolic LLM in C

A lightweight symbolic language model written in pure C. No matrices, no backpropagation, no PyTorch. Symbols, relations, 32D embeddings, and local learning.

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
```

## Key Features

- **Symbolic reasoning**: O(1) exact lookups, transitive inference
- **32D embeddings**: cosine similarity for synonym detection (GATO ~ FELINO)
- **Hybrid queries (H4 verified)**: exact search first, vector fallback for unknowns
- **Probabilistic output**: "El gato come pescado (66.7%), carne (33.3%)"
- **Anaphora resolution**: "El compila" -> "ANTONIO compila"
- **Binary persistence V2**: graph + 32D vectors in one file, V1 compatible
- **Benchmark**: 50K symbols+embeddings in 7.5MB, load in <20ms

## Quick Start

### Compile

```bash
gcc -std=c11 -Wall -Iinclude \
    src/symbol.c src/relation.c src/embedding.c src/graph.c \
    src/learning.c src/context.c src/generator.c src/model.c \
    src/main_cli.c -o sllm_cli.exe -lm
```

### Run

```bash
./sllm_cli.exe
```

### Interactive REPL

```
Tu > El gato come pescado.
  Aprendido. (6 relaciones)

Tu > El gato come carne.
  Aprendido. (7 relaciones)

Tu > ¿Qué come el gato?
IA > El gato come pescado y carne.

Tu > /alias felino gato
  Alias: 'FELINO' ~ 'GATO' (similitud: 99.9%)

Tu > ¿Qué come el felino?
  [sinonimo] 'FELINO' se aproxima a 'GATO' (similitud: 99.9%)
IA > El felino come pescado y carne.

Tu > /save model.bin
  Modelo V2 guardado en 'model.bin'.

Tu > /exit
```

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

## How Learning Works

When you type a sentence like `El gato come pescado.`:

1. **Parse**: Extract triple: GATO, COME, PESCADO
2. **Store**: Insert/increment relation in graph (count++, weight += 0.01)
3. **Embed**: Update 32D vectors via Hebbian co-occurrence
4. **Context**: Push entity to working memory for anaphora

Probability is computed as:
```
P(O | S, P) = count(S,P,O) / sum_O' count(S,P,O')
```

## How Hybrid Queries Work (H4)

When you ask `¿Qué come el felino?` and only GATO has knowledge:

1. **Exact search**: FELINO has no relations -> 0 results
2. **Embedding lookup**: Find nearest neighbor of FELINO in 32D space
3. **Match found**: cos(FELINO, GATO) = 0.84 > threshold 0.70
4. **Resolve**: Use GATO's relations, report synonym to user
5. **Generate**: "El felino come pescado y carne."

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
  [id, name_len, name_bytes, frequency] x SymbolCount

RELATION BLOCK
  [subject, predicate, object, count, weight] x RelCount

EMBEDDING BLOCK
  [symbol_id, float[32]] x EmbeddingCount
```

V1 files (without embeddings) are still loadable.

## Project Structure

```
include/
  symbol.h        SYMBOL_TABLE: Create, Add, Find, Get, Count
  relation.h      RELATION_TABLE: Add, Find, Strengthen, queries
  embedding.h     32D vectors: RandomInit, Cooccur, Cosine, FindSimilar
  graph.h         GRAPH: queries, transitive inference, fuzzy resolution
  learning.h      Sentence parsing, probabilistic prediction
  context.h       Anaphora, pronoun resolution, working memory
  generator.h     Relation -> natural language text
  model.h         MODEL: Create, Save V2, Load (V1+V2 compatible)

src/
  symbol.c        Symbol table implementation
  relation.c      Relation table implementation
  embedding.c     32D embedding math + similarity search
  graph.c         Graph queries + hybrid fuzzy resolution
  learning.c      Corpus learning + prediction
  context.c       Context management + anaphora
  generator.c     Text generation from relations
  model.c         Binary serialization V2
  main_cli.c      Interactive REPL

tests/
  test_symbol.c        Symbol table unit tests
  test_graph.c         Graph + transitive inference
  test_learning.c      Probabilistic corpus learning
  test_context.c       Anaphora resolution
  test_generator.c     Text generation
  test_embedding.c     32D embedding similarity
  test_model.c         Persistence V1
  test_model_embeddings.c  Persistence V2 with vectors
  test_graph_fuzzy.c   Hybrid fuzzy query (H4)
  bench_stress_50k.c   50K benchmark
```

## Benchmark Results

```
=========================================================
  SYMBOLIC LLM - BENCHMARK DE ESTRES (50.000 32D)
=========================================================

  Simbolos & Vectores: 50000
  Tamano en disco    : 7.53 MB
  Escritura a disco  : 25.49 ms (295 MB/s)
  Lectura y montado  : 15.52 ms (3221 simbolos/seg)
  Integridad         : 100% bit a bit
```

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
bench_stress_50k         PASS (50K symbols, 7.5MB, 100% integrity)
```

## Memory Footprint

| Component | Size |
|---|---|
| 50K symbols (names + metadata) | ~1.5 MB |
| 50K embeddings (32D float) | ~6.4 MB |
| 50K relations | ~0.5 MB |
| Total disk (V2) | 7.53 MB |
| Total RAM (runtime) | ~12 MB |

## Building Tests

```bash
# All tests (except tokenizer)
SRC="src/symbol.c src/relation.c src/embedding.c src/graph.c src/learning.c src/context.c src/generator.c src/model.c"

for t in tests/test_*.c; do
    name=$(basename "$t" .c)
    gcc -std=c11 -Wall -Iinclude $SRC "$t" -o "$name.exe" -lm
done

# Benchmark
gcc -std=c11 -Wall -Iinclude $SRC tests/bench_stress_50k.c -o bench_stress_50k.exe -lm
```

## License

Public domain. Use freely.
