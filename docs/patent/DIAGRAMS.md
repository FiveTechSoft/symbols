# PATENT DRAWINGS / DIAGRAMS (USPTO 35 U.S.C. 113)

**Application Title**: Deterministic Symbolic Artificial Intelligence System and Closed-Loop Abductive Method for Autonomous Software Synthesis, Verification, and Repair  
**Inventor**: Antonio Linares  

---

<div style="page-break-after: always;"></div>

## FIG. 1: OVERALL SYSTEM ARCHITECTURE

```
+------------------------------------------------------------+
|                    SYMBOLIC LLM AGENT                      |
|                                                            |
|  +------------------------------------------------------+  |
|  |             KNOWLEDGE & SYMBOLIC MEMORY              |  |
|  |  +----------------+ +----------------+ +----------+  |  |
|  |  | AST CODE GRAPH | | TEXTLEX STORE  | | TRIPLES  |  |  |
|  |  | (Functions,    | | (Lexical /     | | REL GRAPH|  |  |
|  |  |  Structs, AST) | |  Semantics)    | | (KB)     |  |  |
|  |  +----------------+ +----------------+ +----------+  |  |
|  +------------------------------------------------------+  |
|                           |                                |
|                           v                                |
|  +------------------------------------------------------+  |
|  |             GOAL-DIRECTED REASONING LOOP             |  |
|  |                                                      |  |
|  |  +------------------------------------------------+  |  |
|  |  | STRIPS PLANNER (agent_planner)                 |  |  |
|  |  | State Vector: [PRED_KNOWN ... PRED_VERIFIED]    |  |  |
|  |  +------------------------------------------------+  |  |
|  |                           |                          |  |
|  |                           v                          |  |
|  |  +------------------------------------------------+  |  |
|  |  | ATOMIC PATCH ENGINE (agent_patch)              |  |  |
|  |  | Pre-flight Ambiguity Gate + CRLF/LF Alignment  |  |  |
|  |  +------------------------------------------------+  |  |
|  |                           |                          |  |
|  |                           v                          |  |
|  |  +------------------------------------------------+  |  |
|  |  | SUBPROCESS ENGINE (agent_shell)                |  |  |
|  |  | Win (cmd/ps), Linux (bash), macOS (zsh)        |  |  |
|  |  | Dual-Pipe Non-blocking Drain + 10ms Timeout    |  |  |
|  |  +------------------------------------------------+  |  |
|  |                           |                          |  |
|  |             +-------------+-------------+            |  |
|  | (exit == 0) |                           | (exit != 0)|  |
|  |             v                           v            |  |
|  |  +--------------------+       +-------------------+  |  |
|  |  | SUCCESS / VERIFIED |       | ABDUCTIVE REPAIR  |  |  |
|  |  | Formally Proved    |       | (agent_diagnose)  |  |  |
|  |  | Zero Regressions   |       | Deduce fix from   |  |  |
|  |  +--------------------+       | compiler stderr   |  |  |
|  |                               +-------------------+  |  |
|  |                                         |            |  |
|  |                                         v            |  |
|  |                               +-------------------+  |  |
|  |                               | ATOMIC ROLLBACK   |  |  |
|  |                               | (0.001s restore)  |  |  |
|  |                               +-------------------+  |  |
|  +------------------------------------------------------+  |
+------------------------------------------------------------+
```

---

<div style="page-break-after: always;"></div>

## FIG. 2: CLOSED-LOOP ABDUCTIVE SELF-HEALING WORKFLOW

```
                      [START: Task Initiated]
                                 |
                                 v
                [Ingest Codebase into CodeGraph]
                                 |
                                 v
               [Compute Blast Radius & Callers]
                                 |
                                 v
                     [Formulate STRIPS Plan]
                                 |
                                 v
               {Pre-Flight Verification: Safe?}
                 /                            \
      (Ambiguous/None)                      (Safe)
               /                                \
              v                                  v
    [FAIL-CLOSED: Abort]             [Apply Atomic Patch]
                                                 |
                                                 v
                                     [Execute Verification]
                                     (agent_shell Subprocess)
                                                 |
                                                 v
                                       {Exit Code == 0?}
                                         /            \
                                    (Yes)              (No)
                                     /                    \
                                    v                      v
                       [Emit Senior PR Report]     [Capture Stderr]
                                    |                      |
                                    v                      v
                       [SUCCESS: Task Done]        [Classify Defect]
                                                           |
                                                           v
                                                  [Abduce Remedy]
                                                           |
                                                           v
                                                  [Atomic Rollback]
                                                  (0.001s snapshot)
                                                           |
                                                           v
                                                  [Dynamic Replan]
                                                           |
                                                           +---> (Loop to Plan)
```

---

<div style="page-break-after: always;"></div>

## FIG. 3: DEADLOCK-FREE DUAL-PIPE SUBPROCESS ENGINE

```
                     +-----------------------+
                     |  PARENT AGENT PROCESS |
                     +-----------------------+
                         |               |
              [CreatePipe: Stdout]  [CreatePipe: Stderr]
                         |               |
              +----------+---------------+----------+
              |                                     |
              v                                     v
       hOutRead (Parent)                     hErrRead (Parent)
       (Inherit = FALSE)                     (Inherit = FALSE)
              ^                                     ^
              | Non-blocking Peek / poll            | Non-blocking Peek / poll
              | 10ms Quantum Drain                  | 10ms Quantum Drain
              |                                     |
       hOutWrite (Child)                     hErrWrite (Child)
       (Inherit = TRUE)                      (Inherit = TRUE)
              ^                                     ^
              +----------+---------------+----------+
                         |               |
                         | dup2 / Handles|
                         |               |
                     +-----------------------+
                     | CHILD PROCESS (SHELL) |
                     | GCC / Clang / Pytest  |
                     +-----------------------+
                                 |
                                 v [Timer Check]
                   If Elapsed Time >= Timeout_MS:
                                 |
                +----------------------------------+
                | TerminateProcess / kill(SIGKILL) |
                | Exit Code forced to 124          |
                | Zero Leaked Background Tasks     |
                +----------------------------------+
```

---

<div style="page-break-after: always;"></div>

## FIG. 4: TRANSITIVE BLAST RADIUS & RISK STRATIFICATION

```
           [TARGET FUNCTION MODIFIED: MathMultiply]
                               |
            +------------------+------------------+
            | Depth 1                             | Depth 1
            v                                     v
     [CalculateArea]                       [VolumeCompute]
            |                                     |
            | Depth 2                             | Depth 2
            v                                     v
   [GeometryController]                  [RenderPipeline]
            |                                     |
            +------------------+------------------+
                               | Depth 3
                               v
                     [IntegrationTestSuite]

      * Blast Radius Metrics:
        - Affected Callers: 5 functions
        - Affected Files:   3 files (math.c, geometry.c, render.c)
        - Risk Level:       HIGH (> 2 files affected)
        - Action:           Enforce multi-file verification gate.
```

---

<div style="page-break-after: always;"></div>

## FIG. 5: ATOMIC PATCH TRANSACTION & FAIL-CLOSED GATE

```
                [Original File on Persistent Disk]
                                |
                                v (Read into RAM)
                    [Pre-Flight Verification]
                    - Context Matching
                    - Buggy Match Count == 1
                    - Transparent CRLF / LF Normalization
                                |
                +---------------+---------------+
                | Status == OK                  | Status == AMBIGUOUS
                v                               v
    [In-Memory Backup Snapshot]         [REJECT MUTATION]
                |                       - Disk 100% untouched
                v                       - Zero side effects
      [Write Hunk to Disk]
                |
                v
       [Compiler Verification]
                |
        +-------+-------+
        | Exit == 0     | Exit != 0
        v               v
    [COMMIT]        [ATOMIC ROLLBACK]
    - Success       - Restore original bytes (< 0.001s)
    - Validated     - Disk identical to pre-test state
```
