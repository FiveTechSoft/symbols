# Abstain and ask: explicit stdout-missing pilot

Status: proposed slice 1, not yet shipped. The natural-language ask classifier
failed an independent safety gate: seven unsafe questions in 24 must-not-ask
cases. It is replaced, not patched with more text exclusions.

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

## Future answer-to-edit continuation, not in slice 1

Only a separate validated handoff can pair the original request, workspace
revision, and user's exact answer. The answer is a proposed oracle, not proof
of a correct edit. It must never be appended as unescaped free-form
instructions. Bound and validate an exact stdout value and terminal newline;
reject dynamic, ambiguous, conflicting or unsupported multiline values. The
existing C contract candidate path can then try edits in throwaway copies,
requiring one passing candidate at the lowest tier, exit 0, exact stdout,
intent, scope, and no regression. Any failed check leaves the workspace
unchanged. Never treat an unanswered question as a verified episode.

Independently authored fresh holdouts require zero unsafe questions. Engineering
banks, self-test, CTest and CI on the committed SHA are separate gates. This
document does not authorize automatic user messages.
