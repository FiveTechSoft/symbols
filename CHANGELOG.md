# Changelog

This file records notable changes to Symbolic LLM / symbols-server, newest first, using a [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) style. Test counts are results from the dated historical runs, not a claim that every job is green on the current SHA. [README sections 6-7](README.md) give current limits. A fixture result never proves zero hallucinations, fixed latency, universal intent recognition or general autonomous engineering.

## Bounded work after Phase 26 - September 25, 2026

- `a2ddbfec`: the opt-in CLI question for typed `stdout-goal-missing` asks for exact stdout only after a no-edit abstention on a single input-free C program. An earlier natural-language classifier was rejected after unsafe questions on a holdout. See `docs/abstain-and-ask.md`.
- `1f07a11`: a separate CLI continuation accepts a caller-supplied stdout answer and workspace key. It examines the full first passing candidate tier and requires a unique edit that compiles and runs. Comparison uses normalized single-line output, not exact bytes; reaching a stated goal does not establish that goal's semantic truth. No success episode is persisted.
- `650741f`: an optional read-only libclang inspector reports facts for one C translation unit and an explicit `compile_commands.json` variant. It returns `unknown` on unsupported or unresolved cases; it neither edits nor authorizes editing and does not claim C++, all-project impact or safe AST rewrites.
- `2f1ee43` and `80bdf08`: native Windows CI gained a Ninja compilation-database job with Python 3.11 and `libclang==18.1.1`. The optional corpus passed at SHA `2f1ee43`, as recorded in `docs/ast-inspect.md`; later SHAs need their own CI evidence.
- `a04ab07`: the patch harness keeps its 50 MB threshold for normal builds but omits that gate under ASan because sanitizer overhead triggered it. This is not a fixed production memory bound.

## Phase 26 - September 21, 2026

- Expanded `data/c_lang/c_corpus.txt` with paired Spanish/English terms for C11 types, pointers, memory, algorithms, structures, file I/O and diagnostic tools (AddressSanitizer, UndefinedBehaviorSanitizer, Valgrind, GDB and CMake).
- Added `C_NODE`, `C_VECTOR` and more libc signatures to `data/c_lang/c_std_lib.h`. They feed a **lexical code-symbol graph**, not the later libclang AST inspector.
- `ServerIsInspectionTask`, `MatchesAlgorithmKeyword` and `ServerIsCodeSynthesisTask` give tested linked-list and dynamic-array phrases precedence over filesystem inspection. The direct C11 template catalog covers Lomuto quicksort, a singly linked list, a growing array, `fgets` file reading, a bounded LIFO stack and `qsort`.
- Historical checks: server unit assertions 232/232; user-case HTTP harness 11/11; global CTest 67 passed, nine optional skips and zero failures out of 76.

## Phase 25 - September 21, 2026

- Added bounded edit distance (at most two edits) and prefix matching for named algorithms such as Fibonacci and factorial, recognizing tested misspellings without turning them into repository patch requests.
- Added noun-phrase prompts with a code/algorithm term and language specification, without requiring an imperative verb. These are selected patterns, not general intent recognition.
- Historical checks: server assertions 226/226; the Fibonacci-typo E2E case returned Markdown with `finish_reason: "stop"` and zero tool calls; global CTest 67 passed and nine skipped out of 76.

## Phase 24 - September 21, 2026

- `ServerInspectToolResponse` now checks the value of JSON status/error fields, ignores neutral `null`/`false` values, and parses numeric exit codes rather than confusing source-code strings for failures.
- Read-only tools such as `glob`, `read` and `grep` are excluded from compiler-error text scans in the tested flow, so source content containing `error:` or `FAILED` does not itself trigger a replan. The session retains `last_tool_call_name` when a client tool return omits the name.
- `TakeJsonContent` handles tested plain-string, text-part-array and raw-file-list returns. `ServerFormatInspectionOutput` formats those match lists as readable Markdown lines.
- Historical checks: four added server cases, 221/221 assertions; two directory-inspection HTTP cases; global CTest 67 passed and nine skipped out of 76.

## Phase 23 - September 21, 2026

- Set `data/c_lang/c_corpus.txt` as the default technical corpus for `symbols-server`, `chat_main` and launch scripts. Bible and Jung texts remain available through explicit loading.
- For tested stateless one-message HTTP requests, reset selected dialogue focus and shown-sentence state. `INT_TEXTQ` wraps after exhausting matching sentences for a direct question.
- A tested memory-leak question retrieves C11 guidance; this reduces the observed lexical hijack, not every possible interference or outdated answer.
- Historical checks: 212/212 server assertions, OpenCode user-case/copilot harnesses, and global CTest 67 passed, nine skipped and zero failed out of 76.

## Phase 22 - September 21, 2026

- `IsGreetingTok` stops tested greetings such as `hola` from being mistaken by edit distance for Bible-corpus words such as `hold`.
- `SelfAnswer`, `ServerIsGreeting` and `ServerAnswerGreeting` handle supported English/Spanish greeting and identity triggers, with the compiled fallback and 20-trigger `data/agentic/self.tsv` table. Supported replies return `finish_reason: "stop"`.

## Phase 21 - September 21, 2026

- `ServerIsCodeSynthesisTask` and `ServerSynthesizeCode` recognize tested C algorithm requests and return C11 templates in Markdown rather than starting a repository patch cycle. Template complexity explanations and overflow checks apply to the covered functions, not arbitrary generated programs.
- `TakeJsonString` gained standard JSON escape handling. `FindHttpHeader` and `ReadHttpBody` improved handling of header capitalization, whitespace and complete bodies already received when a connection closes.

## Phase 20 - September 21, 2026

- `GenericRelToConn` and `ChatLoadModel` normalize the custom binary relation names exercised by `test_model_generic_rel`. Canonical synonyms live in `COMPILED_RELMAP`; `ChatFactCount` reports loaded facts. The fixture ingested its valid triples; this is not a 100% ingestion guarantee for every graph or preservation of arbitrary relationship semantics.
- `/v1/model/load` and `/load` report `facts_loaded` for tested binary/text loading paths; compound entity names are normalized for the expected `LearnerLearnLine` syntax.
- Direct C algorithm templates can return a Markdown code block and `main()` in supported cases, without launching a project patch workflow. HTTP header/body parsing and JSON decoding gained tests for larger multi-turn payloads.
- Recognized file-creation requests can call the client's declared `write` tool once. A one-step plan is not a transactional filesystem write. The request buffer grew to 2 MB; finite response buffers can still truncate long outputs. The tested terminal-error detector reduces false build-PASS reports, but does not prove all tool failures are recognized.
- Historical checks: 191/191 server assertions, 7/7 generic-relation tests, 29/29 live discrimination cases, a successful OpenCode user-case harness, and global CTest 67 passed/nine skipped out of 76.

## Phase 19 - September 20, 2026

- Implemented the tested OpenAI-style tool-calling subset of `/v1/chat/completions` in C11. A bounded ReAct/STRIPS loop dispatches declared tools and closes covered engineering flows. This is not general autonomous task resolution or an optimality proof.
- Routes selected shell, inspection and bounded text-patch intentions separately from factual queries. It does not claim general semantic AST editing.

## Phase 18 - September 20, 2026

- Added binary graph serialization and memory mapping for tested commonsense data. Mapping avoids text parsing during that step, not all latency. Binary persistence in fixtures is not a durable transaction for the separate episodic TSV store.

## Phase 17 - September 20, 2026

- Added `data/memory/episodic.tsv` storage for supported `/learn` facts and retrieval after restart. The store uses linear duplicate lookup, non-atomic rewrite and incomplete save-error propagation; it is not verified engineering-episode memory. Clearing the file does not prove a live graph has forgotten an injected fact.

## Phase 16 - September 20, 2026

- Added `PERSONA_PIRATE_QUANTUM`, declarative lexical slots and interactive perspective switching (`/persona`, `:persona`, `modo pirata` (the supported Spanish command)). `PersonaVerifyNonInterference` checks specific fixtures, not a theorem of factual non-interference for arbitrary questions.

## Phase 15 - September 20, 2026

- Added tested `INT_QA_CONSEQUENCE` and `INT_QA_AFFORDANCE` intents, including a glass-fall ontology example and UTF-8, Latin-1 and CP850 handling in selected console paths. These closed-world examples do not establish general physical reasoning.

## Phase 14 - September 20, 2026

- Implemented VSA/HDC 256-bit vectors and associative cleanup, a CCG-based English/Spanish/French surface realizer, commonsense streaming ingestion and six persona perspectives. Historical dedicated tests cover these modules. Neither the 32-byte relation struct (ABI-dependent) nor test fixtures establish a 10-million-triple production memory bound, general zero hallucination or fixed latency.

## Historical implementation and fixture detail inventory

The following preserves dated feature names, mechanisms and fixture evidence behind the phase summaries. Counts are for the historical September 20-21 runs; skipped tests were not passes on the absent optional model.

- Phase 26's C corpus Section 15 paired Spanish/English terms for pointers, dynamic memory, leaks, buffer overflow, linked lists, resizing arrays, LIFO stack, FIFO queue, file I/O, quicksort, binary search, bit operations, preprocessing, structs and build flags. It indexed ASan, UBSan, Valgrind, GDB and CMake guidance. The declaration model added `C_NODE`, `C_VECTOR`, `fopen`, `fgets`, `fseek`, `qsort`, `bsearch`, conversions, `system`, string/memory functions, `ctype.h`, `math.h`, `time.h`, `perror` and `strerror`. `ServerIsInspectionTask` excludes `lista enlazada`/`linked list` from directory-command detection; `MatchesAlgorithmKeyword` and `ServerIsCodeSynthesisTask` prioritize selected `lista enlazada`, `array dinamico`, `leer archivo`, `qsort`, `pila` and `cola` requests. Its C templates used Lomuto quicksort, singly linked list, geometric x2 array growth, `fgets`, bounded stack and strict `qsort` comparator. The assertions were 232/232, added E2E cases three, user-case harness 11/11, and global CTest 67 passed/nine skipped of 76.
- Phase 25's `EditDistance`/`MatchesAlgorithmKeyword` used an edit-distance ceiling of two and prefix matching. Tested typos included `funcion fibinacci en C`, `fibonaci`, `facturial`; noun phrases included `funcion ... en C`, `algoritmo de ordenamiento en C`, `busqueda binaria en c` and `ejemplo de punteros en C`. These avoid spurious `CMakeLists.txt`, `cmake --build`, `ctest` mutation plans for the tested information requests. The `test_fibinacci_typo_synthesis` E2E returned Markdown and no tool calls. Dedicated assertions 226/226 and global 67 passed/nine skipped of 76 were the dated checks.
- Phase 24 checked `"status"` against `"error"`/`"fail"` and treated `"error": null`, `"error": false`, empty strings and zero as neutral where appropriate; explicit `"isError": true`, `"is_error": true` and string-valued errors were recognized in tested payloads. Numeric `"exit_code"`, `"returncode"` and `"exitCode"` required integer values, so `"code": "printf(...)"` was not mistaken for an exit status. Supported read tools were `glob`, `read`, `grep`, `locate_symbol`, `view_file`, `find_by_name` and `grep_search`; `DiagnosticParseOutput` scans of text such as `error:`, `FAILED` and `Permission denied` were bypassed for them. `last_tool_call_name` handled tool returns missing `"name"`. `ServerExtractLastToolResponse`/`TakeJsonContent` accepted `"content": "..."`, `"content": [{"type":"text","text":"..."}]` and raw arrays like `["file1", "file2"]`; `ServerFormatInspectionOutput` formatted `"matches"`/`"files"` lists under the directory exploration Markdown heading. Dedicated test cases 6f-6i reached 221/221; `test_dir_wildcard_flow` and `test_dir_dot_flow` passed in real HTTP; global 67 passed/nine skipped of 76.
- Phase 23 changed launch defaults in `run_symbols_server.bat`, `run_symbols_server.ps1`, `run_symbols_server.sh`, `symbols-server` and `chat_main`: C11 technical corpus first, with `bible.txt` and `jung.txt` explicitly available using an argument or `/load`. A tested answer to `What causes memory leaks?` described failure to free memory rather than retrieving an unrelated text; no universal anti-hijack result is claimed. `nmsg <= 1` resets conversation focus and `ntshown = 0`; `INT_TEXTQ` wraps when `!p->t_following`. User-case and copilot E2E harnesses passed; server assertions 212/212, global 67 passed/nine skipped of 76.
- Phase 22's compiled self fallback `COMPILED_SELF_SCOPE`, `COMPILED_SELF_GREET` and `LoadCompiledSelf()` offered 20 triggers, including `hola`, `hello`, `hi`, `hey`, `buenas`, `saludos` and `quien eres`. `IsGreetingTok`, `SelfAnswer` and `ParseIntentToks` addressed the `hola` to `hold` Levenshtein false positive. Both direct chat and `symbols-server` returned tested ES/EN stop responses with perspective modulation; unit assertions 212/212 and global 67 passed/nine skipped of 76 were dated results.
- Phase 21's examples included Fibonacci, factorial, reverse string and binary search C code in Markdown, with a template `main()` and supported overflow checks. Code-only queries in the tested flow avoided `patch -> build -> ctest`. `TakeJsonString` handled `\"`, `\\`, `\/`, `\b`, `\f`, `\n`, `\r`, `\t`; `FindHttpHeader` matched case-insensitively with optional whitespace before the colon; `ReadHttpBody` handled a complete JSON body in an initial header read or after early disconnect. Server assertions 198/198, four E2E flows and global 67 passed/nine skipped of 76 were the historical checks.
- Phase 20's sample relation names included `contains`, `part_of`, `requires`, `kit_contains`, `component_of`, `directed_by`. Compiled relation aliases included `CONTIENE`, `CONTAINS`, `PART_OF`, `PARTE_DE`, `REQUIRES`, `REQUIERE`, `COMPONENT_OF`, `COMPONENTE_DE` and `GENTILICIO`; `GenericRelToConn` mapped the tested fixture. `IngestTripleSource` also added facts to session `ch->tgraph`, and `ChatFactCount(const CHAT *ch)` exposed a count. The endpoint returned `"facts_loaded": N`, `"symbols"` and `"relations"` for `binary_v2` and `corpus_text`; `/load <path>` reported the count. Tested questions included the Spanish kit-a inclusion question, `que contiene el kit_pro`, `what includes the kit_a` and `a que aplica el kit_b`. The fixture verified serialization, binary loading and zero dropped facts **within that fixture**, not every arbitrary graph. `IsFileCreationTask` recognized `crea un fichero test.txt`, `nuevo archivo config.json`, `create file foo.c` and called a client `write` with `filePath`; the expected Markdown included a created-file heading. Request `SERVER_BODY_MAX` became 2097152 bytes; static `g_http_body` and response buffers (`content`: 32 KB, `resp`: 64 KB, `last_tool_output`: 16 KB) bounded memory, not arbitrary output preservation. `FindFileForIssue` stopped splitting extensions at `.`, trimming terminal punctuation. `ServerInspectToolResponse` checked plain-text failures `Error: could not load cache`, `No tests were found`, `command not found` and `Permission denied`. `ServerExtractQuery` accepted string and text-part content. Historical checks 191/191 server, 7/7 generic-relations, 29/29 live-discrimination, user-case E2E, global 67 passed/nine skipped of 76.
- Phase 19's native OpenAI-style tool loop handled `/v1/chat/completions`, declared shell/inspection/patch tools, and direct factual questions. It could process `dir`, `ls` and `*.*` in tested flows without a text fallback. STRIPS plan choice was bounded, not globally optimal, and `patches AST` was corrected to bounded text patches. This is neither an upstream SWE-bench solution rate nor general autonomous engineering.
- Phase 18 added `mmap` binary graph loading and compact triples. Mapping skips reparsing the textual corpus at that step, but has no zero-latency promise; its disk persistence is not an atomic verified-episode transaction.
- Phase 17 stored `data/memory/episodic.tsv` learned facts via `/learn`/`ChatLearnTriple`, reloaded them for conversation and exposed immediate retrieval in supported cases. The store is a prototype: linear duplicate lookup, direct non-atomic rewrite, incomplete write-error propagation and no proof an injected live graph fact is purged by clearing the file.
- Phase 16 provided `PERSONA_PIRATE_QUANTUM` via `g_persona_lexicons`, `/persona <name>`, `:persona <name>` and the supported Spanish `modo pirata` command. `PersonaVerifyNonInterference` exercises specific factual fixtures; its empirical check is not a universal theorem.
- Phase 15 covered `INT_QA_CONSEQUENCE` and `INT_QA_AFFORDANCE`, the bounded glass -> brittle material -> shatter causal fixture, and UTF-8/Latin-1/CP850 console paths.
- Phase 14's VSA/HDC vectors were 256 bits with XOR/popcount and associative cleanup; the CCG realizer used categories and combinators with EN/ES/FR agreement; commonsense ingestion tested selected ConceptNet relations and affordances; persona perspectives were `neutral`, `architect`, `auditor`, `tutor`, `concise`, `socratic`. Historical claims of 10M triples in ~305 MB, constant latency and zero hallucinations were not established for arbitrary input and are not release guarantees.

## Evidence dates and bounded-result interpretation

Phase 14-19 entries were dated September 20, 2026; phases 20-26 were dated September 21, 2026; the bounded follow-up entries were dated September 25, 2026. The historical global suite often displayed `76/76 PASS`, meaning 67 passed, nine optional Wikipedia-model tests skipped, zero failed. Phase 20 `test_model_generic_rel` had 7/7 tested assertions; its results do not authorize a universal relationship or zero-failure claim. `ChatFactCount > 0` was observed on those fixtures. The optional local 251 MB ConceptNet 5.7.0 snapshot belongs to the separate Phase 27 note in AGENTS; a historical Phase 14 hypothetical 10M-triple/~305 MB number was not a measured shipped snapshot.

Other recorded paths and names: `src/chat.c`, `src/server_proto.c`, `src/symbols_server.c`, `tests/test_server_tool_calling.c`, `tools/test_opencode_user_cases.py`, `tools/test_opencode_copilot_e2e.py`; `ChatHandleToBuf`, `IsFolderOrGlobQuery`, `ServerIsCodingTask`, `ServerExtractQuery`, `ServerExtractLastToolResponse`, `FindFileForIssue` and the `strtok` extension correction. Tested natural-language directory variants included `lista las subcarpetas`, `subdirectorios`, `subfolders` and `listar`; file examples included `test.txt`, `main.c` and `config.json`. The status parser recognized text such as `No tests were found` and `Error: could not load cache`, not arbitrary shell failures. Neither a `400 Bad Request` fix nor a larger `Content-Length` buffer removes all HTTP size limits. The selected direct-code examples (`escribe en C la funcion de fibonacci`, `write a function to calculate factorial`, `invertir cadena`, `busqueda binaria`) are catalog cases, not proof that every user request is classified correctly.

## Exact fixture strings and protocol names

Quoted Spanish strings below are historical test inputs/output markers, not Spanish explanatory prose. They preserve how a reader can locate the matching test without implying open-ended language support.

- The technical-answer sample was `"Failing to free allocated memory causes memory leaks that exhaust available system resources."` in the tested C11 corpus, not proof that any technical question avoids anachronistic retrieval.
- Selected code-intent prompts included `escribe en C la funcion de fibonacci`, `write a C function to calculate factorial`, `write a function to calculate factorial`, `invertir cadena`, `busqueda binaria`, `funcion fibinacci en C`, `fibonaci`, `facturial`, `funcion ... en C`, `algoritmo de ordenamiento en C`, `busqueda binaria en c`, `ejemplo de punteros en C`. The Phase 25 matching shape was `[code noun] + [language specification]`; `haz` and `escribe` were examples of imperative words no longer required for that shape.
- Directory/file fixture strings included `dir *.*`, `lista las subcarpetas`, `subdirectorios`, `subfolders`, `listar`, `nuevo archivo config.json`, and `### Archivo Creado con Exito ('test.txt')` as a tested output marker. `filePath` named the selected client's argument key. They do not prove atomic file writes.
- The JSON payload variants included `"content": [{"type": "text", "text": "..."}]`, `"matches": [...]`, `"files": [...]`, `"error": "<mensaje>"`, and neutral `""`. Headers included `Content-Length:`, `content-length:` and `Content-length:`. `is_error = 1` was the erroneous flag that the tested neutral values stopped setting. The result heading `### Contenido del Directorio / Exploracion` was a historical string, not the recommended prose language for this English document.
- The follow-up relation test included the original Spanish kit-a inclusion question as a Spanish input marker, `que contiene el kit_pro`, `what includes the kit_a`, and `a que aplica el kit_b`. `/load <ruta>`, `ChatFactCount > 0`, `ch->tgraph`, `IngestTripleSource`, `binary_v2` and `corpus_text` were corresponding route names. The direct-generation branch returned C11 code in a Markdown block with `finish_reason: "stop"`; it was not checked for every possible overflow.

The original log's exact failure string `400 Bad Request: bad content length` identified the tested oversized-request bug, not every possible HTTP 400. A file-creation test returned a declared `write` tool call in a `{"role": "tool"}`-style conversation; shell tool mappings included `bash`. The nominal intention shape was a code noun plus a language specification, not a general natural-language classifier. Spanish kit-a inclusion was a test input, not the language of this documentation.
