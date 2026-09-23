# Symbols: a deterministic symbolic engine and verified-change experiment

**Antonio Linares, FiveTech Software**

Repository: <https://github.com/FiveTechSoft/symbols>

Status: research prototype, active development

Canonical status source: this paper plus [`ROADMAP.md`](ROADMAP.md)

## Abstract

Symbols studies how far a native C engine can go when knowledge, inference, plans, edits, and verification are represented as explicit data instead of hidden model weights. Its core is deterministic: a probabilistic model may propose a candidate at the boundary, but it does not authorize or validate a change. Compilers, tests, contracts, and repository invariants decide whether a candidate is accepted.

The repository already contains graph storage and reasoning, text retrieval, natural-language realization, a bounded coding-agent loop, safe patch operators, shell execution, and a small persistent fact store. The strongest coding evidence is narrow rather than general: two compiler-guided C repair operators have been evaluated on separated development and evaluation fixtures. Work outside those operators must abstain.

This paper distinguishes four statuses:

- **Implemented**: code exists in `master`.
- **Demonstrated**: a named test or fixture exercises the behavior.
- **Planned**: specified in [`ROADMAP.md`](ROADMAP.md), but not implemented.
- **Hypothesis**: a research direction that has not passed a release gate.

No statement in this paper should be read as a guarantee of general intelligence, zero hallucination, constant latency, or correctness outside the cited evidence.

## 1. Research question and governing principle

Symbols asks whether a useful engineering assistant can be built from mechanisms that **perceive, test, correct, consolidate, and abstain**:

> No deberíamos codificar cada conducta. Deberíamos codificar mecanismos para percibir, probar, corregir, consolidar y abstenerse.

The authority path stays deterministic. Candidate generation may be heuristic or probabilistic, but a state change is accepted only after deterministic checks. This makes Symbols closer to a verifier, corrector, and repository guardian than to a general-purpose language model.

Harbour is the first reference domain in the roadmap. C and C++ remain the substrate for the engine, Harbour runtime, extensions, and portability tests.

## 2. Current status

| Area | Status | Evidence | Boundary |
|---|---|---|---|
| Symbol, relation, numeric, and embedding stores | Implemented and unit-tested | `include/{symbol,relation,numeric,embedding}.h`; graph/model tests | Hash operations are expected-average time, not a worst-case constant-time proof |
| Relation representation | Demonstrated as 32 bytes on current tested ABIs | `RELATION` in `include/relation.h`; benchmark builds | ABI-dependent; indexes and strings add memory beyond the struct |
| Rule and graph reasoning | Implemented and fixture-tested | `graph_reasoning`, `neuro_prolog`, `neuro_rules`, multi-hop and reasoning tests | Closed-world fixtures do not establish open-domain correctness |
| Literal text retrieval and symbolic attention | Implemented and tested | `src/text_lex.c`; `test_textlex`, `test_open_qa` | Ranking quality is corpus-dependent; no universal QA guarantee |
| CCG/VSA/commonsense/persona layers | Implemented and unit-tested | dedicated modules and CTest targets | Optional large data snapshots are not bundled in every run |
| Patch preflight, apply, diff, and rollback | Implemented and tested | `agent_patch`; `test_agent_patch`, `test_agent_hard_tasks` | Process-local rollback is not a substitute for transactional filesystem/Git operations |
| Shell execution, timeout handling, and explicit shell routing | Implemented and tested | `agent_shell`, `test_agent_shell`, `test_shell_routing_e2e` | Shell remains an escape hatch; structured operations are preferred |
| Bounded planner/runner loop | Implemented and tested | `agent_planner`, `agent_runner`; runner tests | It is not a general autonomous programmer |
| OpenCode session isolation and contextual active-file line swap | Implemented and demonstrated | server protocol/session tests and authentic OpenCode runs | Bounded active-file operation, not arbitrary semantic editing |
| Native Git status and repository preflight | Implemented and demonstrated | `agent_git`, server Git inquiry tests, authentic OpenCode run | Read-only inspection and preflight only; mutation remains planned |
| Compiler `did-you-mean` repair | Demonstrated on separated C fixtures | `test_agent_runner_external` | Requires a compiler-confirmed suggestion and narrow preconditions |
| Missing-header repair | Demonstrated on separated C fixtures | same fixture suite | Requires one unambiguous repository-local declaration source |
| Persistent conversational facts | Implemented and restart-tested | `episodic_memory`; `test_episodic_memory` | Flat TSV triples only; see Section 6 |
| Structured filesystem, safe Git, build/test intelligence, Clang AST | Planned | Roadmap Phases 1-4 | Not release-ready capabilities |
| Persistent workflows and verified engineering memory | Planned | Roadmap Phases 5-6 | Not implemented |
| Harbour end-to-end adapter | Planned | roadmap domain orientation | No Harbour release gate has passed |

### 2.1 Progress metrics (measured, not estimated)

Every number in this table comes from a real run of `python3 tools/metrics.py --write --ci` on the stated commit; nothing is filled in by hand. Anything that cannot be measured yet is shown as "not measured". The per-commit history is in [`tools/metrics_history.csv`](tools/metrics_history.csv), and the per-phase targets are in [`ROADMAP.md`](ROADMAP.md#measurable-targets-per-phase). The fixed task bank has its own per-commit JSONL report and CI gate in [`scripts/bank_report.py`](scripts/bank_report.py) (workflow `bank`); this table reads the same runner output.

<!-- METRICS:BEGIN -->
Measured on commit `2729745` on 2026-09-23 (linux, build) with `python3 tools/metrics.py`.

| Metric | Value | How it is measured |
|---|---|---|
| CTest suite | 91/101 pass, 0 fail, 10 skipped | full `ctest`; skipped = optional data missing |
| Unit asserts | 835 pass, 0 fail | sum of `TEST RESULTS` over all tests |
| Agent: C repair, evaluation | 12/25 resolved (48%), 0 harmful edits, 13 correct abstentions | `test_agent_runner_external` / `_heldout` (separated fixtures) |
| Agent: C repair, development | 2/4 resolved (50%), 0 harmful edits, 2 correct abstentions | `test_agent_runner_external` / `_heldout` (separated fixtures) |
| Agent: C repair, held-out | 2/4 resolved (50%), 0 harmful edits | `test_agent_runner_external` / `_heldout` (separated fixtures) |
| Grounded QA (fixed set of 32) | precision 78%, recall 82% (TP 14, FP 4, FN 3, TN 11) | `tools/metrics/qa_battery.tsv` against the server; labels from the corpus, not from the engine |
| Invented or wrong answers | 4 of 32 | FP from the row above |
| Server latency | p50 1.4 ms, p95 2.1 ms | same set, local |
| Procedural memory: repeat benefit | 6 real commands: 6 probes the first time → 0 on repeat | same list twice, the probe really runs; non-commands are re-probed on purpose |
| C edit operator (OpenCode) | 46 asserts pass, 0 fail | `test_c_edit_ops` |
| Engineering task bank (56 tasks, 8 categories) | `symbols-agent`: 19/56 pass (34%; dev 14/32, held-out 5/24), 0 wrong edits, 37 untouched; by category: build_ci 0/7, compiler_repair 5/7, debug 2/7, docs 3/7, multi_file 5/7, refactor 3/7, shell 0/7, test_authoring 1/7 | `tools/bank_harness.py` (before/ + task.md + check.py; golden after/ only in `--self-test`, self-test 56/56) |
| Wikidata QA evaluation (`test_eval_*`) | not measured | `wiki_model.bin` missing (not bundled) |
| CI per platform | apply: success, ci / asan-msvc: in_progress, ci / build-test-linux: success, ci / build-test-msvc: in_progress | [run](https://github.com/FiveTechSoft/symbols/actions/runs/35864655747); failing tests are listed in each job log |
| Hand-written rules (declared) | 36 rules; tables: `tools.tsv` 16 rows, `fixtures.tsv` 5 rows, `english-spanish.txt` 357 rows | `tools/metrics/declared_rules.tsv` (the script checks each one is still in the code) |
<!-- METRICS:END -->

## 3. Architecture

### 3.1 Base structures

Symbols uses two structural implementations:

1. **`GRAPH`** for symbols, relations, embeddings, numeric values, reasoning, and domain-specific graphs.
2. **`META_GRAPH`** for conversational emphasis and continuity over transferred truth.

`CODE_GRAPH` is a specialization for code relations such as declarations, calls, includes, types, and inheritance. It is not a third general graph implementation.

A relation records subject, relation, object, polarity, evidence count, weight, and source:

```c
typedef struct {
    SYMBOL_ID         subject;
    SYMBOL_ID         relation;
    SYMBOL_ID         object;
    RELATION_POLARITY polarity;
    uint64_t          count;
    float             weight;
    SYMBOL_ID         source;
} RELATION;
```

The current embedding substrate uses 32 floating-point dimensions. It supports deterministic initialization, co-occurrence updates, normalization, cosine similarity, and relation composition. These vectors are a retrieval aid, not proof of semantic understanding.

### 3.2 Text and provenance

`text_lex` preserves a corpus byte image and sentence offsets for literal retrieval. It also builds indexes and ranking signals used by question answering. A retrieved sentence can therefore be tied back to source text, while inferred answers can carry relation sources or proof steps.

Provenance coverage is not yet uniform across every subsystem. Source-backed retrieval and selected reasoning paths have trace data; generated conversational text does not have a system-wide proof that every phrase is anchored.

### 3.3 Reasoning and realization

The repository contains exact relation queries, polarity and contradiction handling, transitive and rule-based inference, bounded proof search, CCG realization, VSA operations, commonsense lookup, personas, and passage generation.

These components are deterministic for fixed inputs and configuration. Their tests demonstrate named fixtures, not complete natural-language coverage. When a parser, rule family, corpus fact, or repair operator does not cover a request, the correct result is `UNKNOWN`, failure, or abstention.

### 3.4 Coding-agent path

The current engineering path is:

1. index repository text and code relations;
2. form a bounded plan;
3. execute through explicit tool contracts;
4. parse diagnostics;
5. propose a patch only when an operator's preconditions match;
6. apply with a retained backup;
7. run the evaluator;
8. keep the patch on success or roll it back and replan within a fixed budget.

The validated repair operators are intentionally narrow:

- **compiler-confirmed identifier typo** (`did-you-mean`);
- **unambiguous missing local header**.

A syntax error, ambiguous header, unsupported diagnostic, or unverified candidate is out of coverage and must not be reported as solved.

## 4. Reproducible evidence

### 4.1 Build and core suite

Requirements are CMake 3.10 or newer and a C11 compiler. Some targets also rely on platform APIs and external commands such as the system compiler and shell.

```bash
git clone https://github.com/FiveTechSoft/symbols.git
cd symbols
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
ctest --test-dir build --output-on-failure
```

At commit `26d0de4ad7f969171c2f121e7f5a4307f42ffff2`, a clean local Linux Release run registered 88 CTest tests: 76 passed, 12 were skipped because optional external data or an enabled end-to-end server configuration was absent, and none failed. This is a reproducible audit observation, not a cross-platform release claim.

### 4.2 External C repair fixture

Run:

```bash
./build/test_agent_runner_external
```

The fixture is documented in [`tests/fixtures/agent_runner_external/MANIFEST.md`](tests/fixtures/agent_runner_external/MANIFEST.md). Its development and evaluation cases are separated.

At the audited commit:

| Split | Cases | Expected repairs resolved | Correct abstentions | False positives |
|---|---:|---:|---:|---:|
| Development | 4 | 2 | 2 | 0 |
| Evaluation | 25 | 12 | 13 | 0 |

The 12 resolved evaluation cases cover only the two operators above. The 13 abstentions include negative, ambiguous, and out-of-coverage cases. This benchmark does not measure general bug fixing.

### 4.3 OpenCode integration evidence

The deployed model was exercised through the authentic OpenCode interface rather than a recreated screen:

- an explicit shell request executed `uname` and returned the host result;
- a contextual two-line swap succeeded outside a Git repository;
- inside a Git repository, native status inspection and repository preflight succeeded, while an unsupported or unsafe action abstained.

These runs demonstrate the named paths only. They do not turn shell access into a structured repository API, prove arbitrary editing, or authorize Git mutation. The corresponding committed tests are `test_shell_routing_e2e`, the session/context fixtures, and `test_git_inquiry_e2e`.

### 4.4 CI status and release rule

The CI workflow builds normal MSVC and GCC configurations and includes Windows sanitizer jobs and corpus linting. The release rule is stricter than a local green run: evidence must belong to the exact SHA under discussion.

Durable run [#27](https://github.com/FiveTechSoft/symbols/actions/runs/35703732949) validated the integration sequence leading to this paper: Linux and MSVC Release passed. The sanitizer job remained red on two pre-existing performance thresholds under sanitizer instrumentation; its AgentShell memory-safety test passed and reported no AddressSanitizer memory error. This is partial cross-platform evidence, not a fully green release matrix.

### 4.5 Performance numbers

The repository includes relation and embedding stress benchmarks, but their timings depend on compiler, build type, operating system, and hardware. They are development measurements, not latency guarantees.

The current `bench_1m` report also has an internally inconsistent summary counter. Until that harness is corrected and CI records the environment, this paper does not publish its timing or memory figures as canonical results.

## 5. Interfaces

The CMake build exposes, among others:

- `chat_main`: interactive conversation over configured corpora;
- `symbols-server`: HTTP server with a `/v1/chat/completions`-style compatibility endpoint;
- `symbols-agent`: repository indexing, diagnosis, blast-radius, and bounded task commands;
- `symbolic-learn`: persistence and learning experiments;
- benchmark and test executables.

Example:

```bash
./build/chat_main data/texts/jung.txt
./build/symbols-server 8080 data/texts/bible.txt data/texts/jung.txt
./build/symbols-agent --help
```

"Compatibility" here means the implemented request/response subset. It does not claim complete behavioral compatibility with every OpenAI client or API feature.

A browser demo is hosted at <https://fivetechsoft.github.io/symbols/>. It is a separate WebAssembly/browser surface and should not be used as evidence that every native feature is present in the browser build.

## 6. Persistent memory: implemented behavior and limits

The current episodic store persists records of:

```text
(subject, relation, object, source, timestamp)
```

It loads them at startup, deduplicates triples case-insensitively, grows dynamically, and can rewrite or clear a TSV file. `test_episodic_memory` exercises initialization, append, duplicate handling, save/load into a second store, attributes, timestamps, accumulation, and clear.

It is a prototype, not the verified episodic memory specified in Roadmap Phase 6:

- lookup is a linear scan;
- records do not contain repository SHA, problem signature, diagnosis, chosen operator, patch hash, evaluator evidence, outcome, or negative episode;
- there is no confidence model, contradiction resolution, selective correction, or invalidation policy;
- save rewrites the destination directly rather than using atomic replacement and durability checks;
- an auto-save failure is not currently propagated by append;
- clearing the store does not prove that an already injected fact stops influencing the live graph in the same process.

These are active correctness gaps. A message saying that a fact was learned must not be treated as durable evidence until write errors are propagated and tested.

## 7. Boundaries

Symbols does **not** currently claim:

- zero hallucination for arbitrary input;
- a worst-case constant-time system;
- universal or open-domain question answering;
- general autonomous software engineering;
- native semantic ASTs for every advertised language;
- upstream SWE-bench resolution results;
- production-grade Git mutation or filesystem transactions;
- verified engineering-episode memory;
- a completed Harbour adapter;
- a green cross-platform release matrix on the current final SHA.

The project also does not remove the need for compilers, tests, corpora, operating-system services, or human decisions. Its purpose is to make those sources of truth explicit and to refuse changes it cannot verify.

## 8. Roadmap

[`ROADMAP.md`](ROADMAP.md) is ordered by dependency and evidence gates:

0. safe shell execution;
1. structured filesystem operations;
2. safe Git operations;
3. build and test intelligence;
4. Clang AST and `compile_commands.json`;
5. persistent workflows;
6. verified episodic memory.

A phase is complete only when its named fixtures and the full CI matrix pass on the final SHA. Planned work must stay labeled as planned until that gate closes.

## 9. How to cite results from this repository

A result should name:

- the exact commit SHA;
- the executable or test target;
- the fixture or corpus and whether it was development or held out;
- compiler, build type, OS, and hardware for performance claims;
- pass, failure, skip, and abstention counts;
- the CI run URL for release claims;
- known unsupported cases.

Do not convert a synthetic fixture, local run, generated report, or demo into a broader product claim.

## License

See [`LICENSE`](LICENSE).
