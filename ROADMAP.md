# Symbolic LLM Engineering Roadmap

This roadmap turns the current native C11 engine into a dependable engineering agent through measured capability gates. It is ordered by dependency, not by calendar date. A phase exits only when its evidence is reproducible in CI on the final commit.

## Operating principles

This roadmap does not encode one behavior per situation. It builds mechanisms: **perceive** the repository and its context, **test** candidate changes against the world, **correct** without hiding failure, **consolidate** only verified experience, and **abstain** when evidence is insufficient. Each phase below strengthens specific mechanisms; each gate exists to prove them.

1. **Deterministic core.** Repository state, plans, actions, and verification are explicit data. A probabilistic model may propose work at the boundary, but it is never the source of truth for permissions, invariants, or success.
2. **Fail closed.** Unknown state, ambiguous paths, stale preconditions, unsupported syntax, and incomplete verification stop the affected action without partial side effects.
3. **Structured operations before shell commands.** Shell execution remains an escape hatch. Filesystem, Git, build, test, and code operations gain typed contracts and narrower capabilities.
4. **Evidence over claims.** Every milestone names its fixture, benchmark, invariant, or CI job. Local success is progress; cross-platform CI on the final SHA is the release gate.
5. **Reversible changes.** Mutations use dry runs, precondition checks, atomic replacement where possible, and explicit rollback or compensation.
6. **Facts and derived knowledge stay separate.** Source facts carry provenance. Heuristics, scores, diagnoses, and prior experiences are marked as derived and can be invalidated.
7. **Natural conversation, grounded in evidence.** symbols behaves as a technical partner, not a legacy command bot. It reports what it did, what it verified, and what it does not know in plain language, grounded in the real state of the work. Conversational quality is a measured capability with its own fixtures, never a cosmetic layer over canned responses.

## Domain orientation

- **Current focus: shell, git, and C.** The core is built and proven on systems work first: safe shell execution, git operations, and C/C++ builds, diagnostics, and repair. The core comes before any new front.
- **Language-agnostic contracts.** Language knowledge stays behind contracts (toolchain, semantics, diagnostics, corpus, evaluations), so a later language is added by supplying those, never by rewriting the engine. No other language is in scope until the systems core meets its gates.
- **C/C++ is the substrate and the first domain.** The engine and its extensions stay on the C pipeline; its portability and memory-safety gates remain mandatory.

## Verified foundation

The repository already contains substantial symbolic and agent infrastructure. The next phases build on it rather than replacing it.

### Native engine and measured C components

- A hash-indexed relation store (expected-average, not worst-case constant time) with a 32-byte relation representation on the tested ABIs, snapshot persistence, provenance, rule reasoning, planning, diagnosis, and graph traversal.
- CCG realization, VSA operations, commonsense ingestion, and persona projection with dedicated unit tests and benchmarks documented in `README.md` and the completed milestones in the previous roadmap.
- Stress benchmarks for relations and embeddings exist, but this roadmap does not cite their timing or memory figures as evidence. The `bench_1m` summary counter is internally inconsistent, and timings depend on compiler, build type, OS, and hardware; see `README.md` section 4.5. Figures become evidence only after the harness is corrected and CI records the environment on the final SHA.

### Coding-agent operators

The codebase already has operators for:

- repository indexing and code-graph construction;
- blast-radius traversal and impact analysis;
- unified-diff parsing, preflight checks, atomic application, and rollback;
- STRIPS planning and replanning;
- compiler/linter diagnostic parsing and abductive suggestions;
- subprocess execution with captured stdout/stderr and timeouts;
- an `AgentRunner` pipeline that connects inspect, plan, patch, verify, retry, and report stages;
- a representative SWE-bench harness that verifies candidate patches rather than claiming unguided issue resolution.

These operators are useful primitives. Their presence does not yet prove reliable autonomous repository administration.

### Delivery workflow

`.github/workflows/apply-patch.yml` currently provides a guarded patch path: exact-base validation, patch validation, application, Ubuntu build and CTest, commit, and push. This gives changes a reproducible transaction boundary.

### Persistent episodic fact memory

Commit `1c9dd550241b8cd0aaebac118d8bdaa04bde6615` lands the atomic fact-memory substrate: crash-safe atomic replacement of the on-disk store, selective forgetting of individual facts, and honest failure reporting when persistence fails. A failed save never claims success and leaves the previous store byte-identical. Evidence: the guarded apply workflow plus the full Linux, MSVC, and MSVC-ASan matrix on the final SHA, and a live end-to-end proof covering learn, query, server restart, query, selective forget, immediate absence, second restart, continued absence, and read-only-filesystem failure honesty. This substrate is what Phase 7 stores its episodes through.

The shell hardening at commit `ba90e1d4ba648e844c3e975ad8e870df38877c89` adds three important invariants:

- an invalid working directory fails without running the command;
- concurrent large stdout and stderr are drained without a false timeout, with explicit truncation;
- timeout cleanup terminates the spawned process tree rather than leaving children alive.

Local evidence for that patch was 70/70 `test_agent_shell` assertions, 82/82 in the exercised suite, and repeated stress runs passing. The guarded apply workflow also passed its Ubuntu build and CTest gate.

### Known verification gap

A push made by `github-actions[bot]` from the apply workflow does not trigger workflows configured only for `on: push`. As a result, the independent Linux/MSVC/ASan matrix did not run for the shell-hardening SHA. Until CI can be dispatched or called explicitly for an exact SHA and that run passes, Windows behavior for this patch remains unverified. The roadmap therefore treats reusable, final-SHA CI as an immediate gate, not as completed evidence.

## Dependency chain

```text
safe shell
    |
    v
structured filesystem
    |
    v
safe Git
    |
    v
build/test intelligence
    |
    v
Clang AST + compile_commands.json
    |
    v
persistent workflows
    |
    v
verified episodic memory
    |
    v
reflexion learning loop
```

Security capabilities, provenance, metrics, and cross-platform CI cut across every phase. Later phases may be prototyped early, but they do not exit before their dependencies.

### Sequencing decisions (September 2026)

These decisions change the order of work, not the dependency chain above. No pulled-forward item closes its phase early; each still has to meet its phase's exit criteria when that phase is reached.

- **Minimal Reflexion loop before full Phase 7.** A small slice of Phase 7 is built now: attempt, structured reflection ("tried X, failed because Y") with source and timestamp, read back on a retry of the same task. Only reflections whose feedback came from the real toolchain are stored or read back. It is measured on analogue tasks against a reflection-off baseline, under the same gates as any other change (bank runs with zero wrong edits, full test suite, CI green on the final SHA). The rest of Phase 7 (full episodic record, bounded cross-task retrieval, restart and forgetting exit criteria on the external benchmark) stays where it is.
- **AST design spike before Phase 4 proper.** A design spike starts now: tree-sitter versus libclang, the cost of each in the CMake build on Linux and MSVC, and which consumer comes first (candidate generation for the repair operators, or impact analysis). The spike produces prototypes and a written recommendation only; it changes no core code. Phase 4 deliverables and exit criteria are unchanged, and a syntax-only parser is not treated as compiler-grounded semantics (see "Explicitly out of scope").

## Phase 0: Safe shell execution

**Goal:** make subprocess execution a bounded, observable primitive rather than an implicit source of repository state.

**Mechanisms:** test. Bounded, observable execution is what makes trying a candidate against reality safe.

### Deliverables

- Complete POSIX and Windows process-tree termination semantics.
- Bounded stdout/stderr capture with separate byte counts, explicit truncation flags, exit status, signal/exception status, elapsed time, timeout status, and spawn error.
- Validated `cwd`, environment allow/deny rules, command/argument separation, and no ambient shell interpretation unless explicitly requested.
- Stable error categories that callers can use without parsing prose.
- Exact-SHA CI entry point covering Ubuntu, MSVC, and MSVC ASan, reusable by `apply-patch` after commit creation.

### Exit criteria

- Invalid `cwd` and spawn failures produce zero command side effects.
- A fixture writing at least 100,000 bytes simultaneously to both streams completes without deadlock or false timeout on Linux and Windows.
- Timeout fixtures leave no surviving child or grandchild process on either platform.
- Truncation is distinguishable from timeout and ordinary command failure.
- `test_agent_shell`, the full CTest suite, MSVC build/test, and MSVC ASan pass on the same final SHA.
- Repeating the concurrency and timeout stress set 100 times produces no hang, orphan, or nondeterministic status.

## Phase 1: Structured filesystem

**Goal:** remove routine file mutation from ad hoc shell strings and enforce workspace boundaries as data.

**Mechanisms:** correct. Reversible, journaled mutation replaces destructive, ad hoc edits.

### Deliverables

- Typed operations for stat, list, read, create, replace, move, copy, and remove.
- A workspace-root capability with canonical path checks, symlink policy, and explicit handling for paths that do not yet exist.
- Atomic single-file replacement and a journaled multi-file transaction with rollback.
- Encoding, newline, permission, file-kind, size, and binary/text metadata.
- Dry-run manifests that state every intended read, write, rename, and delete.

### Exit criteria

- Traversal through `..`, absolute-path escape, symlink escape, and rename races fail closed in adversarial fixtures.
- Interrupted multi-file mutations either commit fully or restore the original byte-for-byte state.
- Existing unified-diff behavior is reimplemented on the structured filesystem API with no regression in patch fixtures.
- Linux and Windows fixtures cover separator, case, permission, long-path, newline, and locked-file behavior.
- Fuzzing malformed paths and operation manifests yields no out-of-workspace write.

## Phase 2: Safe Git

**Goal:** make repository inspection and change delivery explicit, race-aware, and reversible.

**Mechanisms:** correct. Changes become atomic, reviewable, and attributable commits guarded by race-aware preconditions.

### Deliverables

- Structured status, diff, log, branch, worktree, index, and object inspection.
- Preconditions for expected HEAD, clean/allowed-dirty paths, branch, remote, and changed-file allowlists.
- Atomic commit preparation with author/message policy and exact staged-content reporting.
- Safe fetch, branch creation, and push with non-fast-forward and remote-advance detection.
- Rebase/cherry-pick support only with explicit conflict states and abort paths. Force push is disabled by default.

### Exit criteria

- Dirty-tree, stale-HEAD, detached-HEAD, conflict, remote-advance, and submodule fixtures fail without losing user changes.
- A produced commit's tree exactly matches the reviewed mutation manifest.
- Retrying after an interrupted operation is idempotent or reports the already-completed result.
- No test path invokes destructive reset, clean, checkout, or force push implicitly.
- Apply-patch uses these Git contracts instead of open-coded repository assumptions.

## Phase 3: Build and test intelligence

**Goal:** understand what to build and test, then preserve complete evidence for the decision and result.

**Mechanisms:** perceive and test. Build and test structure becomes data, and every result becomes evidence with provenance.

### Deliverables

- CMake File API and CTest metadata ingestion for targets, sources, generated files, configurations, tests, labels, and dependencies.
- Build/test graph linked to changed files and code symbols.
- Structured compiler, linker, sanitizer, and test-result records with artifact paths and provenance.
- Conservative affected-test selection with an automatic full-suite fallback when coverage is unknown.
- Reproducible benchmark runner that records toolchain, OS, CPU, configuration, corpus/fixture version, repetitions, and distribution statistics.

### Exit criteria

- On a labeled fixture repository, affected-test selection has 100% recall. Precision is reported, not optimized at the cost of recall.
- Unknown generators, dynamic dependencies, or missing metadata select the full gate rather than guessing.
- Clean builds succeed with GCC, Clang, and MSVC in CI; sanitizer jobs report zero findings on the gated suite.
- Test reports distinguish not-run, skipped, passed, failed, timed out, and crashed.
- Performance claims in `README.md` are produced by versioned benchmark commands, with median and tail latency plus memory high-water mark.

## Phase 4: Clang AST and `compile_commands.json`

**Goal:** replace lightweight C scanning with compiler-grounded semantics for C and C++ while keeping the graph auditable.

**Mechanisms:** perceive. Compiler-grounded semantics replace guessing about code structure.

### Deliverables

- Import of compilation databases with per-translation-unit flags, working directory, language mode, defines, include paths, and generated-source handling.
- Stable source identities for declarations, definitions, references, scopes, types, overloads, macros, includes, calls, and source ranges.
- Explicit representation of conditional-compilation variants and unresolved compiler states.
- Incremental invalidation keyed by file content, compile command, included-header closure, and parser version.
- Graph queries for callers/callees, references, type compatibility, include impact, and candidate edit ranges.

### Exit criteria

- A versioned C/C++ corpus covers nested scopes, shadowing, function pointers, macros, conditional compilation, headers, and overloads.
- Declaration/reference and call-edge precision and recall are measured against compiler-derived golden data; release requires no false-negative edge in the safety corpus.
- The engine refuses semantic edits for translation units that do not parse under their recorded compile command.
- No stale edge survives a source, flag, macro, or included-header change.
- Existing code-graph and SWE-bench verification fixtures either migrate to AST facts or remain clearly labeled as lexical fallback.

## Phase 5: Persistent workflows

**Goal:** execute long engineering tasks as resumable state machines with durable evidence and compensation.

**Mechanisms:** correct. Interruption and recovery leave no partial or duplicated effects.

### Deliverables

- Versioned workflow schema for steps, inputs, outputs, preconditions, capabilities, retries, timeouts, approvals, and compensation.
- Append-only event log plus checkpointed state, with stable operation IDs and artifact hashes.
- Recovery after process or machine interruption without duplicating commits, pushes, comments, or test dispatches.
- Final-SHA orchestration: pre-commit local gate, commit, reusable CI matrix, observed result, and explicit closeout.
- Human decision nodes for irreversible, ambiguous, or policy-sensitive actions.

### Exit criteria

- Fault injection after every state transition resumes to the same terminal result as an uninterrupted run.
- Replaying a completed workflow produces no duplicate external side effect.
- A failed post-commit CI gate leaves an exact failure record and a safe next action; it never reports completion.
- Every mutation is attributable to an input, capability, workflow step, and verification result.
- Workflow schema migrations preserve the ability to inspect prior runs.

## Phase 6: Verified episodic memory

**Goal:** reuse prior engineering experience without converting past guesses into current facts.

**Mechanisms:** consolidate and abstain. Verified experience becomes reusable knowledge; unverified experience never authorizes an action.

### Deliverables

- Episodes containing repository/version identity, problem signature, observed diagnostics, chosen operators, patch hash, tests, CI result, and final outcome.
- Separate stores for immutable observations, derived hypotheses, and validated outcomes.
- Retrieval based on structural context such as symbol/type/build/test similarity, with explicit confidence and incompatibility checks.
- Invalidation when toolchain, compile command, dependency, file, symbol, or test evidence changes.
- Negative episodes for failed or unsafe approaches.

**Vector search, if added, only proposes.** Today memory is triples plus an episodic TSV log, with small embeddings used only to rank or break ties; there is no vector database. If vector or similarity search is added in this phase, it acts only as a candidate generator: it may suggest episodes to consider, but every suggestion still passes the structural compatibility and evidence checks above, and only verified outcomes are written back. Similarity alone never authorizes an action or satisfies a gate.

### Exit criteria

- Only episodes with completed verification gates can influence automatic action selection.
- Retrieved advice always exposes its source episode and the differences from the current context.
- An incompatible or stale episode cannot silently authorize or satisfy a current gate.
- On a held-out repair corpus, memory improves median steps or time without reducing success rate, safety-gate recall, or determinism.
- Deleting or rebuilding memory changes efficiency only, never the truth of repository facts or verification results.

## Phase 7: Verified Reflexion-style learning loop

**Goal:** implement the complete learning loop from *Reflexion: Language Agents with Verbal Reinforcement Learning* (Noah Shinn, Federico Cassano, Edward Berman, Ashwin Gopinath, Karthik Narasimhan, Shunyu Yao, 2023; arXiv:2303.11366; https://arxiv.org/abs/2303.11366) with deterministic components: the agent attempts a task, the environment returns feedback, the agent writes a verbal reflection, stores it in episodic memory, retrieves it on the next attempt, and measurably improves. No weight updates anywhere, and nothing consolidates without external evidence.

**Mechanisms:** all five, in one loop. Perceive the task and workspace, test each attempt against the real toolchain, correct from the feedback, consolidate only verified episodes, abstain when no verified experience applies.

**Relation to the paper.** Reflexion reinforces a language agent through linguistic feedback kept in an episodic memory buffer instead of gradient updates. Symbols keeps that loop shape and replaces the probabilistic actor with the deterministic planner and operators; the evaluator is always the real toolchain: compiler, tests, sanitizers, and tools. This adaptation is a deliberate difference from the paper, not a claim of equivalence.

### Deliverables

- Bounded attempt loop: each task runs a capped number of attempts, and every attempt yields a candidate patch plus captured external feedback (compiler diagnostics, test results, sanitizer output, tool exit codes). Self-evaluation never substitutes for a real toolchain signal.
- Deterministic verbal reflection: each attempt is summarized from its diagnostics into structured text - what was tried, what the environment reported, why it failed, what to change on the next attempt.
- Persistent episodic record: every attempt stores task, attempt number, feedback, reflection, patch hash, outcome, and provenance (repository SHA, toolchain, fixture version) through the atomic fact-memory substrate, keeping its crash-safety and persistence-failure honesty guarantees.
- Bounded retrieval: the next attempt receives the most relevant prior episodes for the same task, plus explicit warnings when the context differs, with a hard cap on how much history may enter a decision.
- Negative episodes: failed approaches stay recorded as failures so they are not retried blindly; they inform avoidance, never authorization.
- Correction and forgetting: correcting or forgetting an episode stops its influence immediately and across restarts, reusing the selective-forgetting machinery.
- Abstention: when no episode passes its evidence gate, the loop reports that it has no verified experience instead of guessing.

### Exit criteria

- End-to-end run on the external C benchmark: attempt, compiler/test/ASan feedback, reflection, persisted episode, retrieval on the next attempt, verified live against the server and across a process restart.
- Measured improvement against a no-memory baseline: on a held-out benchmark split, the memory-enabled runner beats the same runner with memory disabled on success rate or median attempts, with no regression in safety-gate recall or determinism. Results are published with fixture versions and the final SHA.
- No consolidation without external evidence: an episode may influence a later attempt only if its feedback came from a real compiler, test, sanitizer, or tool run; internally generated confidence never promotes an episode.
- Forgetting restores the baseline: after a selective forget, behavior for that task matches the no-memory runner, immediately and after a restart.
- Honest abstention: with an empty or invalidated store, the runner states that it has no verified experience rather than fabricating recall.
- No weight updates: the implementation stores and retrieves episodes only; it changes no model parameters.

## Measurable targets per phase

Phases are measured with the same metrics the README publishes (Section 2.1, regenerated by `tools/metrics.py`). A phase is never closed on an estimate: its target has to appear in a real run on the final commit. These targets are an initial proposal and will be adjusted with data.

| Metric | Today (see README) | Phase 0-1 | Phase 2-3 | Phase 4-5 | Phase 6-7 |
|---|---|---|---|---|---|
| Agent: C repair, evaluation (resolved) | 12/25 | ≥ 12/25 | ≥ 15/25 | ≥ 18/25 | ≥ 20/25 |
| Harmful edits (any suite) | 0 | 0 | 0 | 0 | 0 |
| Varied engineering task bank (resolved, hidden split) | blind 2/24 (Mimo, counts only; public bank 27/56, see README) | bank exists (≥ 50 tasks, ≥ 8 categories) | ≥ 20% | ≥ 40% | ≥ 60% |
| Grounded QA: precision | see README | ≥ 85% | ≥ 90% | ≥ 95% | ≥ 95% |
| Grounded QA: recall | see README | ≥ 80% | ≥ 85% | ≥ 90% | ≥ 90% |
| Invented or wrong answers on the QA set | see README | ≤ 2 | ≤ 1 | 0 | 0 |
| Memory: probes when repeating already-learned commands | 0 | 0 | 0 | 0 | 0 |
| Memory: repeated tasks solved faster or with fewer attempts | not measured | not measured | measured | ≥ 50% | ≥ 80% |
| CI on the final SHA (Linux, MSVC, MSVC ASan) | fully green on recent `master` SHAs (sanitizer skips only throughput thresholds) | green except performance thresholds | fully green | fully green | fully green |
| Declared hand-written rules | see README | does not grow undeclared | decreasing | decreasing | decreasing |

## Cross-cutting release gates

Every phase must maintain these gates:

| Gate | Required evidence |
| :--- | :--- |
| Correctness | Versioned unit, integration, adversarial, and regression fixtures |
| Portability | Linux GCC/Clang and Windows MSVC on the same final SHA |
| Memory safety | ASan where supported, plus platform-appropriate runtime checks |
| Determinism | Same inputs and state produce the same plan, manifest, patch, and status |
| Fail-closed behavior | Unsupported or ambiguous state causes no partial mutation |
| Performance | Reproducible benchmark with environment, repetitions, median, tail, and memory |
| Provenance | Inputs, derived facts, artifacts, and verification outcomes remain traceable |
| Recovery | Interruption and retry do not duplicate effects or lose user work |

Benchmark thresholds belong beside versioned fixtures and should be tightened only from reproducible measurements. A single development-machine number is a baseline, not a portable service-level objective.

## Risks and controls

- **Platform semantic drift:** POSIX and Windows differ in process, path, locking, and signal behavior. Control: behavioral fixtures on both platforms, not preprocessor equivalence alone.
- **Time-of-check/time-of-use races:** filesystem, Git, and remote state may change after inspection. Control: content hashes, expected HEAD, atomic primitives, and preconditions checked immediately before mutation.
- **Graph staleness:** cached code or build edges can outlive their inputs. Control: dependency-based invalidation and refusal to use incomplete semantic state for mutation.
- **Benchmark overfitting:** small internal fixtures can reward narrow operators. Control: held-out corpora, adversarial variants, published fixture versions, and separate verification from generation.
- **False autonomy claims:** verifying a supplied candidate patch is not the same as resolving an issue from a natural-language report. Control: report proposal, selection, mutation, and verification success separately.
- **Capability creep:** a general shell or Git token can bypass narrower contracts. Control: least-privilege capabilities and a complete mutation manifest.
- **Memory poisoning:** an unverified episode can reinforce an earlier mistake. Control: verification-gated promotion, provenance, negative results, and invalidation.
- **Server handles one request at a time (known infrastructure gap):** `symbols-server` accepts and serves connections serially, so one slow request blocks every other client. Control until fixed: clients run sequentially and use timeouts; closing the gap needs a bounded worker model with per-request isolation, measured under the concurrency and timeout stress set before it is claimed.

## Explicitly out of scope for this roadmap

- A probabilistic LLM inside the trusted execution, permission, repository-state, or verification core.
- Premature general administration of arbitrary machines, networks, cloud accounts, or package ecosystems.
- Autonomous force push, history rewriting, secret handling, privilege escalation, or destructive cleanup by default.
- Treating a lexical scanner as compiler-equivalent C/C++ semantics.
- Claiming full SWE-bench issue resolution from candidate-patch verification results.
- Expanding to languages beyond C/C++ before their semantics, build/test evidence, and workflow recovery meet their gates.
- Maximizing conversational breadth at the expense of engineering reliability.
- Building a tool-protocol adapter (for example MCP) into the core. Such an adapter is a possible future distribution layer that would expose existing, already-gated operators to other agents; it adds no capability and no trust, and it is not scheduled in any phase.

## Next gate

Finish Phase 0 by making the existing CI matrix callable for an exact SHA, invoking it from the patch workflow after commit creation, and obtaining green Linux, MSVC, and MSVC ASan evidence for the shell-hardening commit. Then begin the structured filesystem API without weakening the shell invariants.
