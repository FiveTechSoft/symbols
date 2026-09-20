# UNITED STATES PROVISIONAL PATENT APPLICATION

**INVENTOR**: Antonio Linares  
**CITIZENSHIP**: Spain  
**RESIDENCE**: Marbella / Madrid, Spain  

---

## TITLE OF THE INVENTION
**DETERMINISTIC SYMBOLIC ARTIFICIAL INTELLIGENCE SYSTEM AND CLOSED-LOOP ABDUCTIVE METHOD FOR AUTONOMOUS SOFTWARE SYNTHESIS, VERIFICATION, AND REPAIR**

---

## CROSS-REFERENCE TO RELATED APPLICATIONS
This application claims benefit of the inventor's technical disclosures and commits documented under the open symbolic framework repository.

---

## FIELD OF THE INVENTION
The present invention relates generally to artificial intelligence, automated program synthesis, compiler diagnostic analysis, and software engineering tools. More specifically, the invention relates to a deterministic, non-neural symbolic execution engine configured to perform autonomous code generation, multi-file impact analysis, compiler diagnostic abduction, and fail-closed atomic self-healing without backpropagation, stochastic token guessing, or floating-point tensor approximations.

---

## BACKGROUND OF THE INVENTION
Contemporary artificial intelligence approaches for automated programming predominantly rely on Large Language Models (LLMs) built upon Transformer neural architectures. These statistical models predict successive text tokens based on probabilistic distribution weights trained over internet-scale corpora. Despite substantial compute consumption (megawatts of power and massive GPU clusters), probabilistic approaches suffer from fundamental, well-documented failure modes:

1. **Semantic Hallucination and Fabricated Identifiers**: Generative models frequently output syntactically plausible but nonexistent library functions, non-matching parameter signatures, and hallucinatory variable references.
2. **Absence of Ground-Truth Verification**: Generative models possess no direct, closed-loop interface with the target compiler or operating system environment, resulting in repeated generation of uncompilable code.
3. **Catastrophic Pipeline Deadlocks & Infinite Loops**: Automated tools wrapping language models lack hard millisecond-level process execution controls, frequently freezing test runners when generated code contains unbounded loops.
4. **Repository Inconsistency and Corruption**: Unverified patch application often leaves the codebase in an invalid or partially-patched intermediate state when tests fail.
5. **Excessive Latency and Hardware Footprint**: A single inference pass often takes 5 to 30 seconds and consumes tens of gigabytes of VRAM, making multi-turn iterative repair economically and temporally prohibitive.

Accordingly, there is an urgent technical need in the art of computer engineering for a deterministic, lightweight, fail-closed software agent architecture capable of performing root-cause compiler error diagnosis, zero-ambiguity patch application, and sub-millisecond atomic rollback without relying on stochastic neural network representations.

---

## SUMMARY OF THE INVENTION
The present invention provides a deterministic symbolic programming agent and closed-loop self-healing method that operates natively on standard microprocessors (pure ISO C11) with zero tensor operations, zero GPU requirements, and strictly 0.00% semantic hallucination.

In one aspect of the invention, a **Polyglot AST Code Knowledge Graph** models multi-language software codebases (C, Python, TypeScript, JavaScript) as a bidirectionally indexed graph. The system computes transitive impact closures (*Blast Radius*) in $O(1)$ relational lookups to quantify downstream architectural risk before modifications are applied to persistent storage.

In another aspect of the invention, a **Deterministic Subprocess Execution Engine** executes platform-native shells (Windows `cmd.exe`/`powershell.exe`, Linux `/bin/bash`/`/bin/sh`, macOS `/bin/zsh`/`/bin/sh`) using non-blocking, deadlock-free asynchronous stream draining across distinct `stdout` and `stderr` anonymous pipes. The subprocess engine incorporates hardware-clock microsecond precision timeouts that guarantee process termination upon timeout (exit code 124), preventing infinite loops.

In another aspect of the invention, an **Abductive Diagnostic Reasoner** parses raw, unformatted compiler/linter error streams (e.g., from GCC, Clang, MSVC) and abducts the formal causal defect (e.g., missing struct member, undeclared identifier, arity mismatch, missing header file). The reasoner automatically interrogates the Code Knowledge Graph to locate candidate symbols and directly formulates an abduced surgical repair.

In yet another aspect of the invention, a **Fail-Closed Atomic Patching Engine** verifies pre-flight applicability against physical files, enforcing strict zero-ambiguity constraints (rejecting edits matching multiple occurrences) and line-ending transparent alignment (CRLF/LF agnostic). If post-patch compiler or unit test verification yields a non-zero exit status, the system executes an automatic atomic in-memory rollback within 0.001 seconds, restoring the file system to its exact pre-patch byte sequence.

---

## BRIEF DESCRIPTION OF THE DRAWINGS
The accompanying drawings illustrate embodiments of the invention and together with the description serve to explain the principles of the invention:

- **FIG. 1** is a high-level system architecture diagram illustrating the interaction between the Code Knowledge Graph, STRIPS Planner, Subprocess Shell Engine, and Abductive Diagnostic Engine.
- **FIG. 2** is a procedural flow diagram illustrating the closed-loop autonomous verification and self-repair pipeline.
- **FIG. 3** is a schematic diagram of the cross-platform deadlock-free dual-pipe stream drainage engine with hard timeout termination.
- **FIG. 4** is a graph diagram illustrating transitive blast radius computation and caller risk stratification within the Code Knowledge Graph.
- **FIG. 5** is a state-transition diagram of the fail-closed atomic patching mechanism with guaranteed microsecond rollback.

---

## DETAILED DESCRIPTION OF PREFERRED EMBODIMENTS

### 1. Architectural Overview & System Components
Referring to **FIG. 1**, the deterministic symbolic programming system comprises:
- A **Knowledge Storage Layer** comprising an AST Code Knowledge Graph (`CODE_GRAPH`), a Semantic Textual Store (`TEXTLEX`), and a Relational Graph (`GRAPH`).
- A **Goal-Directed Reasoning Layer** comprising a classical STRIPS Planner (`AGENT_PLANNER`) with formal precondition and postcondition state vectors.
- An **Abductive Diagnostic Engine** (`agent_diagnose`) extracting offending symbols, suggested identifiers, file paths, and column/line locations from raw compiler error outputs.
- A **Cross-Platform Subprocess Engine** (`agent_shell`) managing native shell execution across Windows, Linux, and macOS.
- An **Atomic Surgical Patch Engine** (`agent_patch`) executing pre-flight verification, hunk application, and transactional rollback.

### 2. Deadlock-Free Cross-Platform Subprocess Engine
Conventional process execution utilities (such as `system()` or naive pipe redirection) dead-lock when a child process produces standard output or standard error streams exceeding the operating system's internal pipe buffer capacity (typically 4 KB to 64 KB).

The present invention overcomes this limitation by implementing a non-blocking asynchronous drainage loop:
1. Two discrete anonymous pipes (`hOutRead`/`hOutWrite` and `hErrRead`/`hErrWrite` on Win32; `out_pipe` and `err_pipe` on POSIX) are allocated with inheritance flags cleared on the parent reading handles.
2. The child process is launched via native system APIs (`CreateProcessA` with `CREATE_NO_WINDOW` on Windows; `fork()` and `dup2()` on POSIX).
3. The parent execution thread enters an interleaved polling loop slicing time in 10 ms quantum windows. In each quantum:
   - Pipe 1 (`stdout`) is probed using non-blocking inspection (`PeekNamedPipe` or `poll(POLLIN)`). Available bytes are drained into a fixed buffer up to 64 KB.
   - Pipe 2 (`stderr`) is independently probed and drained into a dedicated error buffer.
   - The process handle state is queried (`WaitForSingleObject(pi.hProcess, 10)` or `waitpid(..., WNOHANG)`).
4. If elapsed wall-clock time exceeds the specified threshold (`timeout_ms`), the parent thread forcefully and immediately terminates the child process (`TerminateProcess(pi.hProcess, 124)` or `kill(pid, SIGKILL)`), populating `timed_out = true` and `exit_code = 124`.

### 3. Closed-Loop Compiler-Interfacing Abductive Reasoning
Referring to **FIG. 2**, when a compilation or verification test fails (`exit_code != 0`), the engine executes an abductive reasoning cycle:
1. The isolated `stderr` buffer is ingested by `DiagnosticParseOutput()`.
2. The error message is matched against formal diagnostic grammar rules (e.g., regex/token patterns for GCC, Clang, and MSVC).
3. The parser classifies the failure into a formal taxonomy:
   - `DIAG_ERR_UNDECLARED_SYMBOL`
   - `DIAG_ERR_MISSING_MEMBER`
   - `DIAG_ERR_ARITY_MISMATCH`
   - `DIAG_ERR_TYPE_MISMATCH`
   - `DIAG_ERR_MISSING_HEADER`
   - `DIAG_ERR_SYNTAX`
4. If a compiler suggestion token is present (e.g., `'did you mean X'`), the reasoner assigns $X$ as the candidate symbol.
5. If no suggestion token is present, the reasoner queries the `CODE_GRAPH` using `CodeGraphGetFunctionFile()` to deduce the missing header or correct struct member.
6. The state vector is dynamically updated to assert `PRED_ERROR_DIAGNOSED`, triggering `AgentPlannerReplanOnError()` to compute an updated repair patch.

### 4. Fail-Closed Atomic Patch Verification and Rollback
To prevent software degradation, modifications are governed by strict verification invariants:
1. **Pre-flight Ambiguity Filter**: `PatchVerifyPlan()` reads the target file from disk into memory and searches for the exact context and buggy snippet. If the search detects zero occurrences or more than one occurrence, the patch is rejected immediately without modifying disk storage.
2. **Transparent Line-Ending Normalization**: The matcher aligns CRLF (`\r\n`) and LF (`\n`) representations transparently, ensuring atomic patch applicability across heterogeneous operating systems.
3. **Atomic Execution**: If pre-flight verification succeeds, `PatchApplyAtomic()` creates an in-memory backup snapshot, writes the verified hunk, and preserves original file attributes.
4. **Guaranteed Rollback Gate**: If compiler invocation or test suite execution yields `exit_code != 0`, `PatchRollback()` restores the pre-patch byte snapshot within 0.001 seconds, guaranteeing zero contamination of the persistent workspace.

---

## REPRESENTATIVE CLAIMS (SUBJECT MATTER)

1. A computer-implemented method for autonomous software synthesis and closed-loop self-repair, the method comprising:
   - maintaining in memory a deterministic AST code knowledge graph representing functions, structures, call dependencies, and file relationships of a multi-file codebase;
   - generating, via a deterministic planner, a surgical patch plan targeting a source file in persistent storage;
   - performing pre-flight ambiguity verification on said target source file, wherein said patch plan is rejected if a target code pattern matches multiple locations in said source file;
   - applying said surgical patch plan atomically to said target source file;
   - executing an external verification command within a platform-native shell via an asynchronous dual-pipe subprocess engine that independently drains standard output and standard error streams;
   - upon detecting a non-zero exit code from said verification command:
     - parsing said standard error stream via an abductive diagnostic engine to classify a compiler or runtime defect into a formal taxonomic category;
     - automatically rolling back said target source file to its exact pre-patch byte sequence in persistent storage; and
     - dynamically formulating a revised patch plan based on said abductive classification.

2. The method of claim 1, wherein said subprocess engine executes non-blocking pipe polling in time slices to prevent buffer deadlock, and terminates said child process upon reaching a microsecond-precision timeout threshold with exit status 124.

3. The method of claim 1, wherein said deterministic code knowledge graph calculates a blast radius comprising a transitive closure of all direct and indirect callers affected by said target source file prior to patch application.

4. The method of claim 1, wherein said abductive diagnostic engine queries said code knowledge graph to identify a missing header file when the compiler outputs an undeclared symbol diagnostic.

5. A deterministic system for autonomous code engineering comprising one or more processors and a memory storing pure ISO C11 instructions configured to perform the method of claim 1 with zero neural network inference and zero floating-point tensor approximations.

---

## ABSTRACT OF THE DISCLOSURE
A deterministic symbolic artificial intelligence system and closed-loop abductive method for autonomous software synthesis, verification, and repair. The system operates in pure ISO C11 without neural networks, floating-point weights, or backpropagation. The architecture comprises a polyglot AST code knowledge graph computing caller blast radius in $O(1)$ time, a non-blocking asynchronous dual-pipe subprocess engine executing native operating system shells with guaranteed timeout termination, and an abductive diagnostic reasoner coupling raw compiler error streams directly to code graph entities. Patch applications are verified through zero-ambiguity pre-flight checks and protected by an automatic 0.001-second fail-closed atomic rollback gate when post-patch compiler or test verification fails.
