# CODE_GRAPH: call provenance, file-aware resolution and blast-radius confidence

Scope: `src/code_graph.c`, `include/code_graph.h`. Lexical (text based) C ingestion; no compiler, no model, no new dependency.

## What changed

- **Per-call provenance.** Every call the C lexer sees is kept as a site (caller, callee, file, line). The name-level `calls` and `called_by` relations keep their meaning and now carry the first call site's file in `RELATION.source`. `CodeGraphGetCallEvidence` returns file and line for each site.
- **Labels.** `CodeGraphResolveSite` labels a site `EXTRACTED` (callee defined in the same file), `INFERRED` (defined in another file and a header reachable through `#include` from the call's file declares or defines it), `AMBIGUOUS` (several candidates, includes do not pick one), `UNLINKED` (the name is defined in the corpus but nothing the call's file includes declares it) or `EXTERNAL` (no definition in the corpus). `UNLINKED` is the libc/WinAPI collision case: calls to `strlen` or `fopen` no longer attach to the stub definitions in `data/c_lang/c_std_lib.h`.
- **Macro filter.** Upper-case identifiers of two or more characters (`TEST(...)`, `ASSERT_X(...)`) are not functions or calls. They are counted in `macro_names_skipped`.
- **Lexer fixes.** Three lookahead paths counted a newline twice (line drift). Top-level prototypes now produce `declares_func`; `extern "C" {` no longer counts as a nesting level.
- **Content-hash skip.** Re-ingesting a file with identical content is skipped (`files_skipped_unchanged`). A changed file drops its old sites.
- **Queries.** `CodeGraphGetResolvedCallers/Callees`, `CodeGraphGetEdgeLabel`, `CodeGraphFindCallPath` (shortest path over resolved edges) and `CodeGraphExplainEdge` (label, file:line, target file).

## Behavior change: blast radius

`CodeGraphComputeBlastRadius` now visits callers reached through resolved edges (EXTRACTED or INFERRED, or no recorded site, for example Python and TypeScript) before callers reached only by name. Name-only entries are still included, so recall does not drop, and are marked `low_confidence` (`BLAST_RADIUS_ENTRY.low_confidence`; the report adds `[low confidence: name match only]`). Counts and risk levels are unchanged.

## Limits

- The lexer does not expand macros and does not track function pointers. Includes are matched by basename.
- Python and TypeScript call edges have no sites yet.
- Legacy queries (`CodeGraphGetCallers/Callees`) are unchanged and still cap at 64 results.
- Measured against clang ground truth on one corpus pair; the figures are a snapshot, not a general accuracy claim.
