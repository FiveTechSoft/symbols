# Opt-in C source-fact QA (slice A)

This is an explicit command, not a natural-language question classifier or a
repair mode. It answers a narrow question about a named C symbol in one
translation unit. It does not compile or run C code, edit source, infer a
program's output, read hidden checks, or claim that an arbitrary question has
been answered. The older corpus/Wikidata QA line remains parked.

Install the optional pinned `libclang==18.1.1` package and supply a compilation
database as described in [the AST inspector](ast-inspect.md). Example:

```sh
python3 tools/ast_inspect/qa.py --db build-ast/compile_commands.json \
  --source src/task_ops.c --variant 0 --fact declaration --symbol TaskOpsSolve
```

`--fact` accepts `declaration`, `references`, or `calls`. A symbol is a literal
C identifier, not a question or expression. The command selects exactly one
source-file declaration by that name; absent or repeated names (including
shadowing) yield `unknown` (exit 2). The reply is JSON with `status: complete` (exit
0), the chosen declaration, selected source-file facts, and source, command,
database and included-header SHA-256 provenance. `matches: []` means no such
fact in this selected complete TU's source-file facts, not no reference or call
anywhere in the repository or linked libraries. A header declaration used as a
call target may appear in the target location, but header facts are not
enumerated. `calls` includes only resolved direct calls, not dynamic targets.

The command inherits the AST inspector's fail-closed rules: missing or
ambiguous compile variant, diagnostics, unresolved bindings, unsupported
compiler options, or input changes detected during inspection return `unknown`
(exit 2), not partial answers. Other compiler flags are passed through to
libclang without a general compatibility or trust guarantee. A TU that changes concurrently outside the inspector's pre/post hash
check is outside the guarantee. Paths and source facts in JSON may be private;
users should not publish this output without reviewing it. This is a read-only
source-inspection interface, not a sandbox for running untrusted programs.

Program-output QA is a separate slice. This command intentionally abstains on
"what does this program print?", C arithmetic semantics, runtime behavior,
and cross-TU program behavior. Such questions need their own evidence and,
where execution is involved, a reviewed opt-in isolation contract. No outcome
on the old burned QA set is claimed here.
