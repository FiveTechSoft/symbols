# Abstain and ask: explicit stdout-missing pilot

Status: slice 1 is shipped (commit a2ddbfec). Slice 2 below is proposed,
not shipped until independently gated. The natural-language ask classifier
failed an independent safety gate: seven unsafe questions in 24 must-not-ask
cases. It was replaced, not patched with more text exclusions.

## Slice 1: typed CLI request, no answer or file edit

Ordinary CLI tasks, OpenCode and server keep their current abstention behavior.
`--ask-missing-goal` does not infer a question from task prose. The only
eligible request is the exact task token `stdout-goal-missing` supplied along
with that opt-in CLI flag. This token is an explicit, external assertion that
this input-free C program needs an exact stdout target that has not yet been
supplied; it is not a classifier result. It carries no answer and no edit
permission. A real user-facing adapter must not manufacture the token from
free-form text. Ambiguous or untrusted intent stays silent.

The CLI runs the ordinary solve first. A separate read-only gate asks only if
there was no edit and no candidate attempted, the fixed-form abstention says
`cc=0 ccw=nogoal`, and the report is bound to the exact task and current
workspace content. The single-file workspace must consist of one C source
with one `main`, no input use, no tests or build scripts; its ordinary probe
must build and exit 0. A stale report or changed workspace is ineligible. The
question asks for exact stdout and does not propose current stdout as truth.
The gate neither reads stdin nor writes files. This is narrow on purpose:
losing potential asks is preferable to asking a false question.

The separate `ccw=no_output` reason remains a veto for explicit no-output
statements in the existing parser. A natural-language request can never use
the typed question gate merely by containing stdout or repair words, even if
its parser returns `nogoal` or `noval`. Reports with an attempted operator or
other abstention reasons never ask. A successful build/run does not prove a
natural-language repair needs a new stdout target: a refactor can already
satisfy its intended output.

## Slice 2: CLI-only typed answer-to-edit continuation

A separate noninteractive command receives exactly `stdout-goal-missing`, the
workspace key printed by the slice-1 question, and a `--stdout-answer` value.
It never reads stdin, infers intent from natural language, or runs on server or
OpenCode. The caller is responsible for sourcing the goal from the user. The
CLI cannot authenticate the typist: the provenance label is **user-asserted
via typed CLI**, not a claim that the answer is independently true.

The continuation checks the typed task, 16-lowercase-hex key, and a nonempty
single-line printable ASCII answer shorter than 128 bytes, with no control
characters. It checks that the workspace still matches the key and has only
one C text source with one input-free `main`, no tests or build scripts, and
that the original program builds and exits 0. It reruns the slice-1 no-edit
ask gate in a scratch workspace. It refuses a goal the current program already
prints. The answer enters a `C_CONTRACT` data field, never task prose. Program
output is compared after trailing CR/LF are stripped; this is normalized
single-line stdout, **not** exact-byte matching. Multiline, dynamic, empty,
nonprintable, truncated and oversized output is outside this pilot.

The existing candidate tiers are tried in throwaway copies. A candidate must
build, exit 0, and print the asserted normalized stdout. Only exactly one
candidate at the lowest passing tier can be chosen; the whole tier is
examined before a decision, and a build-budget exhaustion cannot prove
uniqueness. A literal inserted solely to echo the answer is excluded: compile/run
matching a user assertion does not prove a direct replacement of the printed
constant is the intended repair. An asserted number whose only route is
changing the literal being printed is therefore a designed unreachable class,
not a missed positive. Independent gates: I was rejected for an unsafe same-tier two-edit
landing; J was rejected because a comma-separated initializer before another
declarator generated no candidate, despite its tier-scan repair. K passed its
first-run gate with 0 unsafe edits in 26 cases, all three paired-declarator
shapes refusing, all four regression ambiguity tiers refusing, replay checks
on real landings, and 6/6 positives. The scan now completes the tier, and each supported initialized
declarator is considered separately. The chosen edit is checked again in a
throwaway copy, then the single real C source is atomically replaced and rebuilt/run for final
verification. A failed final check attempts atomic restoration of the original
bytes; if restoration fails, the CLI warns explicitly rather than claiming no
effect. No unsuccessful assertion becomes a verified episode. No durable
episode is written in this first version: without an atomic source-plus-record
transaction, persistence might claim a success whose source was rolled back.
The CLI returns the two truth levels without conflating them: goal =
user-asserted via typed CLI; edit = executed (unique, normalized match, exit 0,
no regression). Its narrow success claim is: "Verified: your stated goal is
reachable by exactly one safe edit." That proves reachability of the supplied
goal, not the semantic truth of the goal. A wrong-but-reachable assertion may
therefore land; the caller must not present the tool as a judge of goal
correctness.

This pilot also does not solve concurrent edits by other processes during its
probe/write window. A production adapter needs locking or an equivalent
compare-and-swap file guard before broadening the scope.

Independently authored fresh holdouts require zero unsafe edits. Engineering
banks, self-test, CTest and CI on the committed SHA are separate gates. This
document does not authorize automatic user messages.
