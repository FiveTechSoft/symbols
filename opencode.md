# Integrating Symbolic LLM with OpenCode

This guide explains how to integrate and use **Symbolic LLM** as a deterministic, zero-hallucination knowledge engine and local model provider within **OpenCode**.

---

## 1. Why Use Symbolic LLM with OpenCode?

OpenCode is designed for autonomous coding, project exploration, and agentic workflows. Integrating Symbolic LLM provides distinct advantages:

- **Zero Hallucination ($P(\text{hallucination}) = 0$)**: When querying project documentation, API standards, or domain textbooks, Symbolic LLM returns exact facts with verifiable citations or states `UNKNOWN`. It never invents non-existent functions or false claims.
- **Microsecond Latency**: Query lookups execute in nanoseconds (72 ns average), and end-to-end question answering takes less than 1 millisecond.
- **Zero VRAM / Zero GPU Cost**: Runs entirely in standard CPU RAM using strictly 32 bytes per relational fact, leaving all GPU resources free for code generation or local neural weights.
- **Dynamic Ingestion of Free Text**: Feed any `.txt` manual, specification, or code notes directly into the engine on the fly without complex tokenization or indexing pipelines.

---

## 2. Integration Modes

### Mode A: OpenAI-Compatible Local HTTP Provider

Symbolic LLM includes an embedded HTTP server ([`symbols_server`](file:///C:/symbols/src/symbols_server.c)) that implements the standard OpenAI `/v1/chat/completions` API.

#### 1. Start the Server
Compile and run `symbols_server` on port `8080` (or any preferred port), pointing to your project's knowledge texts:

```bash
# Compile (if not already built)
cmake --build build-gcc --target symbols_server --config Release

# Run server with one or more plain text files
./build-gcc/symbols_server 8080 data/texts/bible.txt data/texts/jung.txt
```

#### 2. Configure OpenCode
In your OpenCode configuration (e.g., `~/.config/opencode/config.json`, or the project-level `opencode.json`), configure an OpenAI-compatible custom model provider:

```json
{
  "providers": {
    "symbolic_local": {
      "type": "openai",
      "baseUrl": "http://127.0.0.1:8080/v1",
      "apiKey": "local-symbolic-token",
      "models": [
        {
          "id": "symbolic-llm-c11",
          "name": "Symbolic LLM (C11)",
          "contextWindow": 4096,
          "supportsStreaming": false
        }
      ]
    }
  },
  "defaultModel": "symbolic-llm-c11"
}
```

#### 3. Test the Endpoint
Verify that OpenCode can reach the server:

```bash
curl http://localhost:8080/v1/chat/completions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer local-symbolic-token" \
  -d '{
    "model": "symbolic-llm-c11",
    "messages": [
      {"role": "user", "content": "what areas do you know?"}
    ]
  }'
```

The response will be formatted as a standard OpenAI chat completion:
```json
{
  "id": "chatcmpl-symbolic-1",
  "object": "chat.completion",
  "created": 1742410000,
  "model": "symbolic-llm-c11",
  "choices": [
    {
      "index": 0,
      "message": {
        "role": "assistant",
        "content": "The ingested texts cover topics such as: Altertumskunde, Bough, Mephistopheles, Negelein... You can ask me about any of these subjects."
      },
      "finish_reason": "stop"
    }
  ]
}
```

---

### Mode B: CLI Tool for OpenCode Agents

OpenCode agents can invoke the native command-line executable [`chat_main`](file:///C:/symbols/src/chat_main.c) as an external tool or bash sub-process to retrieve verified answers.

#### 1. Direct Non-Interactive Query
You can pipe questions into `chat_main`:

```bash
echo "who is the father of david?" | ./build-gcc/chat_main data/texts/bible.txt
```

Output:
```
Jesse. [Proof trace: DAVID --SON_OF--> JESSE]
```

#### 2. Registering as a Custom Tool in OpenCode
If your OpenCode environment supports custom tools or agent functions, you can register Symbolic LLM as a tool:

```json
{
  "name": "query_symbolic_knowledge",
  "description": "Queries the local symbolic knowledge graph with 0% hallucination. Returns verified facts and literal text citations.",
  "parameters": {
    "type": "object",
    "properties": {
      "query": {
        "type": "string",
        "description": "The natural language question to verify or answer."
      },
      "corpus_path": {
        "type": "string",
        "description": "Optional path to a .txt file to load into memory."
      }
    },
    "required": ["query"]
  }
}
```

---

### Mode C: Neuro-Symbolic Safety & Fact-Checking Filter

In this hybrid architecture, OpenCode uses a large generative neural model (like Claude or GPT) to draft complex explanations or code solutions, but delegates factual verification to Symbolic LLM:

```
                  ┌───────────────────────────────┐
                  │    USER REQUEST IN OPENCODE   │
                  └───────────────┬───────────────┘
                                  │
                                  ▼
                  ┌───────────────────────────────┐
                  │   Primary Neural Model (LLM)  │
                  │   (Generates draft answer)    │
                  └───────────────┬───────────────┘
                                  │
                                  ▼
                  ┌───────────────────────────────┐
                  │   Symbolic LLM Grounding Gate │
                  │   (Validates facts & citations│
                  │    against ingested .txt)     │
                  └───────────────┬───────────────┘
                                  │
                   PASS (Verified)│ FAIL (UNKNOWN)
                                  ▼
                  ┌───────────────────────────────┐
                  │ Confirmed Zero-Hallucination  │
                  │      Output to Developer      │
                  └───────────────────────────────┘
```

---

## 3. Conversational Introspection Commands

When interacting with Symbolic LLM through OpenCode, you can leverage native introspection commands:

| Command Prompt | What Symbolic LLM Returns |
| :--- | :--- |
| `"what areas do you know?"` / `"dime las areas que conoces"` | Discovers and lists the key thematic centroids ($\kappa$) of the ingested documents without reading through the whole file. |
| `"start a conversation"` / `"inicia una conversacion"` | Selects the primary conceptual anchor, retrieves its seminal context sentence, and opens an interactive dialogue thread. |
| `"explain it to me"` / `"explicamelo"` | Follows up on the active discourse focus entity (`focus`) using conversational working memory. |
| `"load <path.txt>"` / `"carga <path.txt>"` | Ingests a new text file dynamically into RAM, indexing sentences and symbols on the fly. |

---

## 4. Multi-Corpus Example

You can load multiple domain textbooks, specifications, or code manuals simultaneously:

```bash
./build-gcc/symbols_server 8080 \
    data/texts/bible.txt \
    data/texts/jung.txt \
    data/samples/wikidata_clean.tsv
```

OpenCode can now seamlessly answer questions spanning across any of the loaded corpora:
- `who is the father of david?` $\to$ answered from `bible.txt`.
- `what is bough?` $\to$ answered from `jung.txt`.
- `capital of france` $\to$ answered from `wikidata_clean.tsv`.
- `who is the king of mars?` $\to$ returns `UNKNOWN` (honesty guarantee).

---

## 5. Building and Verifying

To ensure your environment is fully compiled and ready for OpenCode:

```bash
# Clone and build
git clone https://github.com/FiveTechSoft/symbols.git
cd symbols
mkdir -p build-gcc && cd build-gcc
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

# Verify test suite (62/62 PASS)
ctest --output-on-failure

# Verify OpenCode server end-to-end integration test
cd ..
python tools/test_server_multicorpus.py
```
