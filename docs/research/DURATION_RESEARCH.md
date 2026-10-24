# Scoped duration research

Not default behavior, not arbitrary QA, and not Windows execution.

Reproduce on Linux or a CMake/MSVC build host, with Python3 available:

```
cmake -S . -B build-duration -DSYMBOLS_DURATION_JOIN_RESEARCH=ON
cmake --build build-duration --config Release --target symbols-duration-research duration-join-research-cli
ctest --test-dir build-duration -C Release -R "^duration_.*research_directed$" --output-on-failure
```

These two tests are directed development checks, not independent holdout tests.
They are registered only when the research flag is ON. No workflow or guard
changes are needed. A fresh default configuration has neither these targets
nor these tests. MSVC execution has not yet been verified.

The source assertion syntax is deliberately controlled: a single ASCII
activity label ending in `ing`, then ` uses `, followed by complete command
clauses and explicit CMD, Bash or PowerShell labels. Quotes, operators,
condition tails and extra subject words are refused. It is not a prose scope
parser. Negative and conditional assertions cannot supply a positive command.
Every coordinate clause must pass scope validation, not only the selected one.

The fact adapter is specific to reviewed Microsoft CMD timeout documentation.
The sidecar must match the reviewed byte identity. This is not generic JSON
fact loading. `/nobreak` concerns keyboard interruption, not exact wall time.
No command is synthesized: quantity and unit must join a command present in
the supplied corpus to the reviewed fact. Unsupported requests return limits.
