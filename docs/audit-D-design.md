# D: honest fixture-only state-machine contract

Base live master679c45ba0677276675d2345fd081d408b43b7d5d (C closed). Design before implementation. Parent06:22:26 scoped D to a minimal honest fixture-only declaration in source and public header, or parameterization. Choose documentation-only fixture contract, not a new repair engine.

## Evidence and decision

agent_core.c's state-machine observation logic actively contains src/engine.c:10 fallback, OldFunction/NewFunctionFixed and wrong.h/correct.h substitutions. Its source/header say HARDCODING=0, and tests describe real autonomous repair even though their observations are injected. Source search finds direct session-state-machine calls in tests/test_agent_core.c only; the common symbolic library still links the implementation. Include relationships and shared OpenCodeToolId exist in planner/runner, so it would be false to say the whole header is unused or unlinked in production.

Declare the session functions AgentSessionInit, AgentDecideNextAction and AgentProcessObservation fixture-only for deterministic protocol/state-transition demonstrations. They are callable, not runtime-enforced or hidden from the library. Do not execute their generated replacement/run-command actions against arbitrary projects. Their fixed strings and exit-code-based COMPLETED transition are not general diagnosis, a safety verifier, or evidence that a real task is fixed. Keep tool ID/TOOL_CONTRACT_SPEC/AgentGetToolContract as shared tool metadata, not fixture-only types. Current server path uses server_taskops/TaskOpsSolve; AgentRunnerSolveTask is a separate runner route. These are observed call paths, not a claim that they transparently substitute for this API or make all production uses safe.

Parameterizing target/replacement is deferred: accepting strings would not solve diagnostic selection, intent validation, capabilities, source identity or tool-result trust. It widens a misleading demo rather than solving those gaps. No runtime or ABI changes are required to fix the scoped contradiction.

## Exact intended scope

Five paths: src/agent_core.c (comments only), include/agent_core.h (comments only), tests/test_agent_core.c (comments and human-readable display labels only, no assertions/inputs/expected values/exit behavior), docs/audit-D-design.md and tests/agent_core_contract_D_gold.json. No CMake/workflow/tool/API additions. Do not change the action prompts in implementation, including its existing final 'formally verified' phrase; instead the public contract must explicitly limit such emitted wording to simulated results, not certify real work. This minimizes behavior change and makes the preserved limitation visible.

Source/header opening comments remove universal HARDCODING=0/fail-closed/general-abduction claims and name the exact fixed strings. Function comments identify fixture-only session routines and shared registry separately. The test's title/comments/display headings identify simulated observations, not an executed compiler or autonomous repair. It may say ACTION_FINAL is a state transition, not formal proof.

## Gold and verification

Pre-code gold has20cells: accurate source/header/test declarations, shared registry boundary and absence of unsupported promises; token identity of source/header after lexing comments/whitespace; executable statements and string values in original test check(...) assertions unchanged; unchanged production string literals; exact direct-call inventory outside tests excluding definition/declaration; existing runtime test_agent_core and test_code_graph pass; shared planner/runner compilation passes; no API/ABI/CMake/E/C/v2 edits. Documentation source/header/test drift mutants must be rejected by a private checker while control passes; changing an instruction token or runtime literal must be rejected by token comparison. The checker is supporting validation in the workspace, not new repository production code or an expanded CI gate. Do not claim a comment creates compile-time/runtime access control.

Local serialj1 inherited permission, no Charles. Parent review design+gold first; then exact patch/SHA review; support docs+gold readback, comments patch expectedHEAD/readback; final native MSVC Release+ASAN full green and existing core/code-graph + E/C/v2 gates executed. Successful CTest hides inner output: report actual gate execution, not nonexistent marker logs. Preserve unrelated reds and ask before retry unless a specific grant applies. No premature D closure from comment diff alone.
