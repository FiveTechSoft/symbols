# Integrating Symbols with OpenCode

Symbols provides a local CPU-based HTTP server and command-line tools for
retrieval and supported coding tasks. This guide describes the configuration
stored in this repository. Check it against the OpenCode version you use;
configuration and tool support can change between versions.

## Scope and limits

A citation shows where retrieved text came from. It does not, on its own, prove
that the text supports the whole question, that a proposed edit is correct, or
that general domain reasoning is valid. Inspect the source, conditions and
verification results. An `UNKNOWN` response or a refused edit is not evidence
that the requested fact or solution does not exist.

This guide makes no zero-hallucination guarantee and reports no current latency,
throughput or memory benchmark. Measure those on a named build, corpus, machine
and workload before using numbers for a decision.

## 1. Build and start the server

Run from the repository root:

```bash
cmake -S . -B build-gcc -DCMAKE_BUILD_TYPE=Release
cmake --build build-gcc --config Release
ctest --test-dir build-gcc -C Release --output-on-failure
```

CTest output is the result for that build, not a fixed test-total guarantee.
On Windows, CMake 3.22 or later is required for the shell-test environment.
Install Git for Windows with `usr/bin/sh.exe`, or configure an explicit path:

```powershell
cmake -S . -B build-gcc -DSYMBOLS_TEST_SH="C:/Program Files/Git/usr/bin/sh.exe"
```

The [`symbols-server`](src/symbols_server.c) executable accepts a port, repository
and corpus. These launch scripts select `data/c_lang/c_corpus.txt`, the current
C-language corpus used by the Phase 23 setup, on port 8099:

```bat
rem Windows Command Prompt, after a single-config build:
scripts\run_symbols_server.bat 8099 .
```

```powershell
# PowerShell, after a single-config build:
.\scripts\run_symbols_server.ps1 -Port 8099 -RepoDir .
```

```bash
# Linux / macOS:
./scripts/run_symbols_server.sh 8099 .
```

The scripts search specific build directories. If your generator puts the binary
in a configuration subdirectory, run it directly from the repository root:

```powershell
.\build-gcc\Release\symbols-server.exe 8099 . data/c_lang/c_corpus.txt
```

On a single-config Unix build, the equivalent explicit command is:

```bash
./build-gcc/symbols-server 8099 . data/c_lang/c_corpus.txt
```

## 2. Configure OpenCode

The repository's [`opencode.json`](opencode.json) selects the `symbols/symbols`
model through `@ai-sdk/openai-compatible`. Its local base URL is
`http://127.0.0.1:8099/v1`, and its model configuration enables tool calling.
Use that file as the starting configuration; this is not a claim that every
OpenCode version or every OpenAI API feature has been tested.

Test the endpoint separately before debugging the editor integration:

```bash
curl http://127.0.0.1:8099/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{"model":"symbols","messages":[{"role":"user","content":"What is signed integer overflow in C?"}]}'
```

This is a request example, not a recorded response. Inspect the actual response
and compare any returned statement and citation with
[`data/c_lang/c_corpus.txt`](data/c_lang/c_corpus.txt). Do not substitute the
output of another corpus or treat a plausible answer as a passing check.

## 3. Command-line access

The [`chat_main`](src/chat_main.c) REPL also accepts a corpus path. For example:

```bash
printf 'What is signed integer overflow in C?\n' | ./build-gcc/chat_main data/c_lang/c_corpus.txt
```

No exact answer or proof trace is promised by this example. Read the output from
your build. When wrapping the CLI as an editor tool, describe it as a retrieval
or coding aid, not a source of automatically verified facts.

For multiple corpora, pass only paths that exist in your checkout after the
port and repository argument:

```bash
./build-gcc/symbols-server 8099 . data/c_lang/c_corpus.txt /absolute/path/to/another.txt
```

The last path is a placeholder for your own text, not a bundled corpus.

## 4. Optional integration checks

The repository includes HTTP/editor integration scripts:

```bash
python tools/test_opencode_agentic_server.py
python tools/test_opencode_web_ingest.py
python tools/test_opencode_copilot_e2e.py
```

Inspect each script's prerequisites and result for the build you are testing.
A passing scripted fixture does not establish general coding success or factual
correctness. Tool calls, edits, compilation and tests each need their own checks.
External text loaded into a corpus also needs source review; ingestion does not
make that text true or grant it authority to direct actions.
