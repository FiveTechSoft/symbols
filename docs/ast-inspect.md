# Read-only AST inspection pilot (slice 3)

This optional command reports compiler-grounded facts for one C translation
unit. It **never edits**, writes a graph cache, feeds an edit operator, or
provides edit authority. Its `complete` label applies only to the selected
translation unit and the fact types printed here. It does not claim complete
project-wide impact analysis, all references, safe source edits, or C++ support.

Install the pinned Python package in an isolated environment:

```sh
python3 -m venv .venv-ast
.venv-ast/bin/python -m pip install libclang==18.1.1
cmake -S . -B build-ast -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
.venv-ast/bin/python tools/ast_inspect/inspect.py \
  --db build-ast/compile_commands.json --source src/task_ops.c --variant 0
.venv-ast/bin/python tests/test_ast_inspect.py
```

On Windows, use the venv's `Scripts/python.exe` and a Ninja generator to
produce the database. Native Windows execution is measured by the
`ast-inspect-windows-ninja` CI job on `windows-latest`: Python 3.11 x64,
`libclang==18.1.1`, `ninja==1.12.1`, and CMake's Ninja generator produce
`compile_commands.json` and run `tests/test_ast_inspect.py -v`. This corpus
passed on commit `2f1ee43` ([CI run](https://github.com/FiveTechSoft/symbols/actions/runs/36133603060)).
The 18.1.1 Windows wheel was inspected: it contains libclang.dll (83,988,992
bytes) and Python bindings, but not clang-c headers or an import library.
Existing MSVC/Visual Studio-generator CI does not emit `compile_commands.json`;
CMake only exports it with Makefile and Ninja generators. No native C linkage
or new library requirement has been added to the C core.

Choose an exact TU and, when the database has multiple entries for that file,
pass the zero-based database `--variant` index explicitly. The command reads
`arguments` when present; otherwise a `command` string. The Python package
version is checked as 18.1.1 before parsing; a missing or mismatched binding
fails closed. It does not execute
the compiler or trust task prose. It preserves flags, working directory and
include paths from the selected compilation entry, stripping only the compiler
name, compile-only switch, input source, and output artifact. Response files
and forced include options are refused, not guessed. Command hash, database
hash, source hash, and included-header hashes are returned as provenance. No
facts survive in a cache. The command is read-only with respect to the source
workspace; a caller should treat relative or nonstandard compiler flags not
covered here as unsupported until tested. On a TU with diagnostics (including
warnings), missing includes, missing database/variant, an indirect call or
unresolved binding, it returns `status: unknown` and exits 2. It does not
print a partially inferred impact answer as complete. `status: complete`
exits 0 and lists source-file declarations, references, resolved direct calls,
macro expansions (ranges explicitly not edit-safe), diagnostics and include
hashes. Header declarations may be call targets but header facts are not
enumerated. A check of input hashes before/after parsing narrows accidental
staleness; concurrent writes remain outside the guarantee.

Tests cover nested shadowing, conditional variants, header closure, unresolved
and indirect calls, and changed source/command provenance. They are skipped
when the optional package is absent. The optional corpus ran successfully on
native Windows Ninja CI for commit `2f1ee43`. The official LLVM developer
archive is much larger than this opt-in adapter; no archive or binary is
vendored.

Sources: [libclang C interface](https://clang.llvm.org/docs/LibClang.html),
[CMake compilation database generator limit](https://cmake.org/cmake/help/latest/variable/CMAKE_EXPORT_COMPILE_COMMANDS.html),
[LLVM Windows release packages](https://github.com/llvm/llvm-project/releases/tag/llvmorg-22.1.8).
