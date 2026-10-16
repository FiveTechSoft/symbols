# E: deterministic compiler environment and checked arity evidence

Preparation base: d7eb5a33ee012f47c3dad9dabc29ec5ca8eb7386. Design and independent gold before code. The parent has approved the approach, not an implementation patch. No E source edits or publication have occurred.

## Source findings and boundary

agent_diagnose.c uses atoi for expected/have fields; malformed or overflowing input has no checked result. Its severity/classification grammar is English. compile_repair.c plans text edits but does not launch the compiler. Actual owned compiler commands are in task_ops.c; AgentRunner launches a build_command and then parses its diagnostics. Parent locale is currently inherited by AgentShellExec.

E has two parts: child-only compiler environment and fail-closed arity parsing. It does not change shell quoting, command text, command-policy authority, process ownership, job/group cleanup, stream bounds, timeouts, result structs, public struct layout, repair operators, or the v2 Windows spawn contract. It does not translate arbitrary captured diagnostics. A caller can still explicitly override its environment in a command or build script; that is not hidden language normalization.

## Compiler execution API

Add AgentShellExecCompiler with the AgentShellExec signature, plus a guarded compiler variant for the existing AgentRunner build-command route. Both reuse the same native engine and its cleanup paths. Existing Exec/Explicit/Guarded APIs retain current inheritance. Introduce an internal compiler-environment flag, not command-prefix string manipulation. Do not change cmd.exe, /c or any quoted command, and do not use setlocal/set/export prefixes to dodge the environment contract.

For compiler mode, copy the inherited environment in the parent and replace exactly LC_ALL=C, LC_MESSAGES=C and LANG=C. Windows also replaces VSLANG=1033. Preserve PATH, HOME, TEMP/TMP and every unrelated entry including special Windows drive-current-directory entries. Match Windows variable names case-insensitively, remove duplicate variants of replaced keys, and supply a correctly sorted, double-NUL-terminated environment block to CreateProcessA. Preserve the native encoding/API choice rather than claiming Unicode support added by this slice.

POSIX builds the copied environment before fork. In the child, select that already allocated environment for the same existing shell exec path. Do not call malloc, setenv, putenv, locale functions or locks after fork. Do not mutate the parent's environ, call setlocale, or temporarily change global environment and restore it. Snapshot construction presumes other application code does not mutate the global environment concurrently; E does not create a general concurrent setenv protocol.

If environment copying/allocation/validation fails, fail closed before launching a child, with initialized execution_failed result. Never silently fall back to inherited compiler locale. Free all owned copies on success, launch error, invalid directory, timeout and refusal paths. The guarded compiler route must perform exactly the existing command-policy check and operator-grant check before launching. A blocked command remains blocked; no new permission comes from compiler mode.

Normal commands and test commands do not use compiler mode. Tests must prove the difference, not merely inspect command construction. Parent environment remains byte-for-byte unchanged after success, failure, timeout and concurrent compiler/normal executions.

## Invocation inventory and migration

Migrate owned GCC compilation/probe calls in task_ops: compile_and_run compilation only; compiler_fixits; implicit-declaration probe; authored-test compilation only; per-object and final harness compilation; warning-feature compilation; program warning diagnostics; ccc_check compilation only; compile_repair compilation; and first-error classification syntax compilation. Recheck every GCC command against the final source and carry a call-site inventory in the patch evidence. Run commands immediately following a compilation continue to inherit the normal environment.

For build-repair evidence, compiler mode covers CMake configure/build, make dry-run, and generated build-script checks whose output/status is used as toolchain evidence. Use an explicit helper parameter or separate run_ok_compiler to keep cleanup commands on normal execution. CMake cleanup, Git operations, shell syntax/script-contract execution and compiled program execution stay normal. This is an explicit call-site choice, not an automatic heuristic based on a substring such as 'gcc'.

AgentRunner's build_command uses guarded compiler mode; its test_command keeps ordinary guarded execution. Even if build_command is a script or build tool, its child environment is compiler mode because this route produces build diagnostics. Keep policy evaluation on the original command. Arbitrary external log import, general tool output and user-invoked shell sessions do not gain a guarantee of English text.

## Arity grammar and result contract

Replace atoi only for arity fields with an ASCII decimal parser that checks each digit against UINT32_MAX before multiplication/addition. No sign, locale-dependent digit or decimal separator, hex/exponent, partial token or overflow is accepted. Accept zero and UINT32_MAX; no sentinel value is stolen from the numeric domain. Leading zeros are valid decimal if in range. Check the end delimiter: end-of-message, ASCII whitespace, comma, semicolon or period are allowed; an adjacent letter/digit continuation, underscore, sign, decimal fraction or non-ASCII byte is not. A period followed by a decimal digit is not a valid integer end. Check token boundaries before expected/have keys and reject duplicate occurrences as ambiguous. Exact accepted field labels remain 'expected ' and 'have '; this is not a new general compiler grammar.

When an arity-mismatch phrase is present, both fields must be present and valid to classify DIAG_ERR_ARITY_MISMATCH and publish their numeric values. On any absent/invalid/ambiguous field, preserve the diagnostic item as an error (or warning according to its actual severity), raw message, location and symbol evidence, but set type UNKNOWN and both arity fields zero. Do not drop the line or invent a numeric remedy. This intentionally abstains for 'too few arguments' messages that provide no numeric expected/have pair. Existing supported non-arity classifications and UTF-8 quote extraction remain unchanged.

DiagnosticAbduceRemedy must use the first actual error item selected by root-error policy, not report.items[0] when warnings precede it. Validate selection before reading numbers; if the report has no matching error item or is inconsistent, do not manufacture a numeric claim. Preserve the no-error behavior and current UNKNOWN fallback. No layout change or new validity flags are required because incomplete arity evidence is UNKNOWN.

## Gold and verification

GOLD.json is written before code and includes valid bounds, malformed/absent/ambiguous fields, warning-before-error, and preservation of all error evidence without numeric remedy. Assert direct parsed fields and actual remedy text/absence, not only a PASS string. Tests exercise the production parser with assertions enabled in Release.

A portable child fixture prints environment values and exits according to arguments. Seed the parent with non-C values and unrelated sentinel keys in a private test process. Run both compiler and normal APIs and inspect actual captured child output and parent environment after each. Include normal and compiler calls in concurrent threads, guarded refusal, invalid working directory, exit nonzero, timeout and environment-allocation failure through a private test target. Verify helpers do not leak production failpoints or alter normal call semantics.

Actual GCC/Clang emission under compiler mode must be parsed as English on a known fixture. Real Spanish catalogs are not installed in this workspace (only C, C.utf8 and POSIX locales observed); a fake parent locale plus child env echo proves environment propagation but does not prove translated compiler output was reproduced. Label the evidence accordingly. Native MSVC test verifies VSLANG1033 in the launched child, with mixed-case preexisting keys, preservation of PATH and special environment entries; test native cl emission if the CI developer shell exposes it. A compiler that ignores these environment keys is not certified to emit English solely because the keys exist.

Required Windows regression: run the unmodified v2 test_shell_spawn_win through its frozen exit-and-complete-evidence gate on native MSVC Release and MSVC ASAN, including all27 actual rejections, observer/count-only controls, complete PSS identities, one stable File candidate, exact NT NamedPipe root and PIPE3 at calls1/27, and candidate-not-closed. Ordinary API semantics must remain the same. Preserve v1 history, v2 source/gold/runner bytes and the current explicit CI evidence steps. Any changed v2 result is a blocker to diagnose, not permission to rebaseline counts or relax an oracle.

Mutants must fail for skipped compiler-mode routing, missing each pinned key, global parent-environment mutation, inherited fallback after allocation failure, guarded-policy bypass, atoi/unchecked overflow, accepted partial tokens, dropping invalid arity lines, fabricated numeric remedy and warning-item numbers used for the root error. Native evidence cannot be replaced with POSIX simulation.

## Delivery gates

After review of this design+gold, prepare the exact implementation patch, hash and call-site manifest for a separate review before dispatch. Require local build, targeted parser/environment/caller tests, full supported-layout Linux tests, strict exit-status gates, native MSVC Release and ASAN, and unchanged native v2 evidence. No E implementation, commit or CI dispatch is authorized by this design document itself.
