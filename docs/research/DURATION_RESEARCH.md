# Scoped duration research

Not default behavior, not arbitrary QA, and not Windows execution.

Reproduce on Linux or a CMake/MSVC build host, with Python3 available:

```
cmake -S . -B build-duration -DSYMBOLS_DURATION_JOIN_RESEARCH=ON
cmake --build build-duration --config Release --target symbols-duration-research duration-join-research-cli
ctest --test-dir build-duration -C Release -R "^duration_.*research_directed$" --output-on-failure
```

These four tests are directed development checks, not independent holdout tests.
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

The additional request-frame slice accepts `How do I sleep|wait|pause for
NUMBER UNIT in SHELL?` and explicit command questions with optional `for`,
such as `Which CMD command waits five seconds?`. Quantity, unit and qualifier
parsing is shared with the existing question frame. This adds no facts, shells,
unit conversion or synthesized commands. Complete token consumption still
refuses negation, extra actions and unsupported conditions. Hyphenated number
words and arbitrary paraphrases remain unsupported. The request-frame matrix
is directed development testing, not an independent language holdout.

Controlled corpus conflict handling recognizes the exact unconditional denial
`LABEL never uses COMMAND in SHELL.` for the same activity label, exact command
and shell as a supported positive clause. Matching assertions report
`SOURCE_CONFLICT` with both full assertion byte ranges, regardless of order.
This describes conflicting corpus assertions, not invalid vendor documentation
or general contradiction detection. Other subjects, shells, quantities,
quoted text and conditional clauses are not treated as matching denials.
Activity labels are ASCII and shorter than 128 bytes. This narrow syntax does
not infer an unstated denial or promote conditional text into a fact.
