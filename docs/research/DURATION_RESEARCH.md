# Scoped duration research

Not default behavior, not arbitrary QA, and not Windows execution.

Reproduce on Linux or a CMake/MSVC build host, with Python3 available:

```
cmake -S . -B build-duration -DSYMBOLS_DURATION_JOIN_RESEARCH=ON
cmake --build build-duration --config Release --target symbols-duration-research duration-join-research-cli
ctest --test-dir build-duration -C Release -R "^duration_.*research_directed$" --output-on-failure
```

These six tests are directed development checks, not independent holdout tests.
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

The research conflict lookup indexes complete source lines once per request,
without copying or rewriting their bytes. Exact byte comparison and source
offset ordering preserve the first identical denial occurrence. Positive
clause scan order and duplicate/ambiguity decisions remain unchanged. Index
allocation failure refuses explicitly. CLI timing measurements are not TUI
latency guarantees or a claim about arbitrary corpus inference.

The symmetric source-reader slice uses the same controlled clause reader for
positive `LABEL uses ...` and negative `LABEL never uses ...` assertions.
A final period is optional for both, and both support complete coordinated
command/shell clauses. Polarity is retained, not normalized away. Exact tuple
lookup uses activity, command and shell; conflict receipts keep full original
assertion ranges and positive command ranges. Tests compare exact offsets,
including CRLF, both line orders and coordinated-clause positions.
A clearly conditional denial is not an unconditional denial and does not
suppress an independent valid positive assertion. External quoted text is
also not promoted into a denial. This slice does not add a keyword-based
uncertainty detector or general conditional/prose semantics.

Storage is counted with the same transactional reader before allocating the
actual clause array. A byte-sized scratch copy is reused for both passes;
the denial index allocates only actual negative clauses. Counting discards
malformed coordinated lines in full, just as the filling pass does. Temporary
valid prefixes cannot overflow an array sized for accepted complete lines.
At the 1 MiB source limit with one valid clause and unrelated filler, measured
malloc requests are 1255 bytes for the fact, 1048577 each for source and scratch,
and 696 each for the single clause and minimum index allocation. This replaces
the previous draft's roughly 90 MiB clause reservation. Allocation failure
returns an explicit unsupported result, with no selected command.

## PowerShell documentation subset (local research)

A separate source-specific byte-pinned sidecar documents the literal
`Start-Sleep -Seconds N` command for the Microsoft.PowerShell.Utility cmdlet,
checked against Windows PowerShell 5.1 and PowerShell 7.6 vendor references.
Only decimal integers 0..99, seconds and the default qualifier are accepted.
This range is a deliberate subset, not the vendor's maximum. Fractions,
milliseconds, Duration, aliases, parameter abbreviations, positional arguments,
pipelines, expressions, conversions and generated commands remain excluded.
Ctrl+C interrupts Start-Sleep; exact-wall and ignore-keyboard claims are not
supported. An unqualified name may be shadowed. Output states the documented
cmdlet assumption and does not claim runtime identity or execution.

Enable only in the opt-in research build, with the independent environment
variable `SYMBOLS_POWERSHELL_DURATION_FACT` pointing to
`docs/research/parameter_facts/powershell-start-sleep.json`. The existing
`SYMBOLS_DURATION_FACT` remains the CMD fact path. `DurationJoinResearch` keeps
its old single-fact scope; `DurationJoinResearchWithFacts` takes both paths
explicitly. The CLI's optional fifth argument supplies the PowerShell fact.

A natural PowerShell question without a platform now builds an explicit
`platform=unspecified shell=PowerShell` research envelope, not an inferred
Windows claim. An explicit Windows/Linux platform for PowerShell is rejected
as unreviewed when PowerShell evidence is enabled. Existing explicit envelopes
are not changed or reinterpreted; CMD keeps its documented Windows contract.
The first slice does not add a platform-aware How grammar: extra platform
words there fail complete request consumption. No Bash adapter is added.

Sources (retrieved 2026-10-24):
- https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.utility/start-sleep?view=powershell-5.1
- https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.utility/start-sleep?view=powershell-7.6
- https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.core/about/about_command_precedence?view=powershell-7.6

Version differences are preserved: 5.1 documents Seconds as Int32; 7.6 documents
Double and fractional support since 6.2. Neither fact is generalized into a
cross-version fractional contract. No PowerShell runtime, Windows execution,
MSVC opt-in build or independent holdout is claimed by these directed tests.

Compatibility claim is limited: 200 CMD responses were compared byte for byte
with the prior reader. The legacy single-fact API still rejects PowerShell,
but its OUT_OF_SCOPE message and natural-language normalization wording differ;
no full legacy-response equivalence is claimed. With only the PowerShell fact
path configured, PowerShell works and CMD reports MISSING_FACT without loading
an unrelated fact or crashing.
