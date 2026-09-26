# AST design spike: parser choice and first consumer

This closes the design spike in ROADMAP.md's sequencing decisions, **not**
Phase 4. The existing read-only pilot and its limits are in
[ast-inspect.md](ast-inspect.md). No parser is made an edit authority here.

## What was measured

- The optional Python/libclang 18.1.1 inspector consumes one selected C
  translation-unit/variant from `compile_commands.json`; it returns source
  declarations, references, resolved direct calls, macro expansions,
  diagnostics and input hashes. Missing or ambiguous inputs, unresolved calls,
  diagnostics and changed inputs yield `unknown`, not a partial complete
  answer. The local synthetic corpus covers shadowing, conditional variants,
  header closure, unresolved/indirect calls and provenance changes. This is
  one-TU inspection, not a project-wide graph or a C++ test.
- The native `ast-inspect-windows-ninja` job installed pinned Python 3.11,
  libclang 18.1.1 and Ninja 1.12.1, generated a compilation database, and
  passed the corpus on commit `2f1ee43`
  ([run](https://github.com/FiveTechSoft/symbols/actions/runs/36133603060)).
  The job remains in the exact-SHA CI matrix; it does not test an MSVC
  Visual Studio compilation database or native C libclang linkage.
- Two opt-in semantic vetoes for `c_contract` were rejected by counterexamples:
  an external declaration read and then an external declaration write each
  caused a legitimate, build/run-verified repair to be vetoed. Neither policy
  shipped. The exact counterexamples and unsupported header-macro/external-
  definition edit families are in [ast-inspect.md](ast-inspect.md#slice-4-semantic-veto-experiment-not-shipped).

## Choice: tree-sitter versus libclang

| Question | tree-sitter | libclang 18.1.1 pilot |
| :--- | :--- | :--- |
| Syntax and edits | Syntax trees and incremental reparsing are supported by the upstream API ([docs](https://tree-sitter.github.io/tree-sitter/using-parsers/3-advanced-parsing.html)). This could help a syntax-only index. | The upstream API exposes translation-unit AST cursors, source locations and types ([docs](https://clang.llvm.org/docs/LibClang.html)); the pilot demonstrates selected declarations, resolved direct calls and provenance. |
| Compiler context | Syntax alone would not establish binding/type/conditional-variant facts under the actual build flags. A compiler-backed check would still be needed for Phase 4. This is an architectural inference, not a measured failure of tree-sitter here. | Uses the selected compilation command and include/define context, but only for that selected TU; unsupported flags or unresolved facts produce `unknown`. |
| In-repo evidence | **No tree-sitter prototype, CMake integration, corpus result or timing measurement exists here.** Its accuracy and cost in this repo are unknown. | The read-only Python pilot and native Windows Ninja corpus exist; their tests do not measure graph-wide precision, edit safety or cost distribution. |

Choose **libclang for the next read-only compiler-grounded experiment**. It
already has a bounded and portable-on-Ninja pilot, and Phase 4 asks for
compiler-contextual identities rather than syntax shapes alone. This is a
recommendation based on demonstrated coverage and a stated requirement, not
an assertion that libclang is faster or that tree-sitter cannot be useful.

## CMake and platform cost

**Measured configuration:** the pilot runs outside the C11 `symbolic` target.
On Linux its documented setup creates an optional Python venv and a Ninja
`compile_commands.json`; the ordinary GCC build does not require libclang.
On Windows the CI job creates a separate Ninja build directory and installs
its pinned Python packages, leaving the normal MSVC Visual Studio build and
link unchanged. The inspected Windows wheel contains an 83,988,992-byte
`libclang.dll` plus Python bindings, but no clang-c headers/import library.
CMake's compilation database export works with Ninja/Makefile generators,
not the existing Visual Studio generator
([CMake docs](https://cmake.org/cmake/help/latest/variable/CMAKE_EXPORT_COMPILE_COMMANDS.html)).

**Inferred cost:** using the current adapter adds an optional package install,
separate configuration and parsing step, but no mandatory core-library link.
Native C linkage would need a different developer package and build contract;
tree-sitter would likewise need a runtime, C grammar and CMake integration
that this repo has not implemented. **Unknown:** reproducible install time,
parse wall time/peak memory, native MSVC linkage cost, tree-sitter integration
cost and relative accuracy. Do not assign benchmark numbers or promote the
optional job into a production performance gate without measuring them on the
same fixture/toolchains.

## First consumer and evidence gate

Start with **read-only call/reference impact reporting adjacent to
`src/code_graph.c`'s `CodeGraphComputeBlastRadius`**, using
`tools/ast_inspect/inspect.py` output as a separate, clearly tagged comparison
against the current lexical graph. It should identify a selected changed C
symbol's direct callers/references for one exact TU/variant, report source
locations and input hashes, and say `unknown` for unresolved edges or missing
TU coverage. It must not replace the graph's answer, select tests, veto
`c_contract`, rank/generate an edit, or label a whole repository complete.
Measure precision and recall against compiler-derived golden calls and
references, especially cross-TU and conditional-variant cases; measure Ninja
configuration, dependency install, parse time and memory on Linux and Windows.
Only after a corpus and fail-closed aggregation for *all* relevant TUs exists
should the Phase 4 graph consumer or changed-file test selection be revisited.
The rejected vetoes are direct evidence against making AST metadata an edit
gate before such a contract exists.
