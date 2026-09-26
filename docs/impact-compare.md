# Optional one-TU impact comparison

This is a read-only experiment beside `CodeGraphComputeBlastRadius`, not a
replacement. It compares direct caller **names** from the existing lexical
C graph with libclang-resolved calls to one exact function definition in one
selected C translation unit. It also reports source locations, references,
input hashes, and selected-TU name precision/recall against libclang facts.
Differences are reported as `ast_only` and `lexical_only`, not adjudicated or
used to rank, select tests, generate or veto edits. An absent libclang,
ambiguous compilation variant, diagnostics, unresolved call, unsupported
flag, changed input, missing definition, or ambiguous call owner returns
`unknown` (exit 2). Never turn a `complete` selected-TU result into a claim
of full repository impact. An external caller from another TU will not appear.
Lexical names have no binding or call-site locations; name metrics do not
measure global precision, reference precision, or edit safety. Null precision
or recall means the corresponding denominator is zero.

With pinned libclang in an optional virtual environment and a Ninja compilation
database, from the repository root:

```sh
python3 -m venv .venv-ast
.venv-ast/bin/python -m pip install libclang==18.1.1
cmake -S . -B build-ast -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-ast --target lexical_impact
.venv-ast/bin/python tools/ast_inspect/compare_impact.py \
  --db build-ast/compile_commands.json --source src/code_graph.c \
  --symbol CodeGraphComputeBlastRadius --line 1720 \
  --lexical-bin build-ast/lexical_impact
LEXICAL_IMPACT_BIN="$PWD/build-ast/lexical_impact" \
  .venv-ast/bin/python tests/test_impact_compare.py -v
```

Use the exact definition line shown by `rg -n
'^int CodeGraphComputeBlastRadius' src/code_graph.c` for this checkout; the
example line is not a versioned API. Supply `--variant N` if the compilation database has
more than one command for the source. Windows uses `Scripts/python.exe` and
`build-ast/lexical_impact.exe`. The regular MSVC build does not need libclang
or Ninja. The existing optional Windows Ninja CI job runs the inspector pilot but does
not yet build or execute this comparator. The guarded patch dispatcher cannot
change workflow files; native Windows comparator evidence therefore remains
open. The normal Release and GCC ASan CTest suites include a comparator test
that skips if the optional package is absent.

The lexical helper ingests exactly the named source through the existing graph
and invokes `CodeGraphComputeBlastRadius` at depth 1. Both source and database
are hashed across the comparison, along with the inspector's source and header
hashes. This narrows accidental staleness; concurrent writes are not
transactionally prevented. No graph cache or source edits are made. The
golden corpus includes direct calls, shadowing, conditional variants,
indirect-call abstention and cross-TU incompleteness. Dependency install time,
parse wall time, peak memory, and project-wide precision remain unmeasured;
this slice is not a Phase 4 exit claim.
