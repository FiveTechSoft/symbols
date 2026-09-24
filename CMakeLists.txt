cmake_minimum_required(VERSION 3.10)
project(SymbolicLLM C)

set(CMAKE_C_STANDARD 11)

add_library(symbolic STATIC
    src/symbol.c
    src/tokenizer.c
    src/relation.c
    src/graph.c
    src/learning.c
    src/model.c
    src/embedding.c
    src/parser.c
    src/context.c
    src/dialog.c
    src/generator.c
    src/ingest.c
    src/nlg.c
    src/deep_nlg.c
    src/graph_reasoning.c
    src/cognitive_learning.c
    src/stochastic_nlg.c
    src/metacognition.c
    src/passage_nlg.c
    src/agent_core.c
    src/code_graph.c
    src/agent_patch.c
    src/agent_planner.c
    src/agent_runner.c
    src/agent_diagnose.c
    src/agent_shell.c
    src/task_ops.c
    src/agent_git.c
    src/swe_bench_harness.c
    src/numeric.c
    src/prune.c
    src/shard.c
    src/stats.c
    src/neuro_prolog.c
    src/export.c
    src/stem.c
    src/i18n.c
    src/neuro_rules.c
    src/tagged_term.c
    src/schema.c
    src/metaschema.c
    src/transfer.c
    src/transfer_legacy.c
    src/learn.c
    src/chat.c
    src/chat_clarify.c
    src/tool_executor.c
    src/tool_config.c
    src/agent_action.c
    src/c_rules.c
    src/qa_layer.c
    src/dict.c
    src/server_proto.c
    src/server_subagent.c
    src/output_contract.c
    src/c_edit_ops.c
    src/shell_ops.c
    src/server_taskops.c
    src/build_ops.c
    src/command_policy.c
    src/c_fix_ops.c
    src/git_ops.c
    src/text_lex.c
    src/meta_graph.c
    src/vsa.c
    src/ccg_realizer.c
    src/commonsense.c
    src/persona.c
    src/episodic_memory.c
)

target_include_directories(symbolic PUBLIC include)
if(MSVC)
    target_compile_definitions(symbolic PUBLIC
        strcasecmp=_stricmp
        strncasecmp=_strnicmp
    )
    # Core value types intentionally carry bounded arenas (CHAT, PATCH_PLAN,
    # SWE_BENCH_RESULT). Propagate an explicit stack reserve to every MSVC
    # executable instead of relying on the 1 MiB linker default.
    target_link_options(symbolic INTERFACE "/STACK:8388608")
endif()
if(NOT MSVC)
    target_link_libraries(symbolic PUBLIC m)
endif()
if(WIN32)
    target_link_libraries(symbolic PUBLIC psapi)
endif()

add_executable(sllm_cli src/main_cli.c)
target_link_libraries(sllm_cli symbolic)

add_executable(train_shard src/train_shard.c)
target_link_libraries(train_shard symbolic)

add_executable(train_massive src/train_massive.c)
target_link_libraries(train_massive symbolic)

add_executable(bench_1m tests/bench_1m_relations.c)
target_link_libraries(bench_1m symbolic)

add_executable(bench_stress tests/bench_stress_50k.c)
target_link_libraries(bench_stress symbolic)

enable_testing()

function(sllm_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} symbolic)
    add_test(NAME ${name} COMMAND ${name} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endfunction()

sllm_test(test_symbol          tests/test_symbol.c)
sllm_test(test_tokenizer       tests/test_tokenizer.c)
sllm_test(test_graph           tests/test_graph.c)
sllm_test(test_learning        tests/test_learning.c)
sllm_test(test_model           tests/test_model.c)
sllm_test(test_embedding       tests/test_embedding.c)
sllm_test(test_model_embeddings tests/test_model_embeddings.c)
sllm_test(test_graph_fuzzy     tests/test_graph_fuzzy.c)
sllm_test(test_context         tests/test_context.c)
sllm_test(test_conversation    tests/test_conversation.c)
sllm_test(test_generator       tests/test_generator.c)
sllm_test(test_ingest          tests/test_ingest.c)
sllm_test(test_contradictions  tests/test_contradictions.c)
sllm_test(test_stats           tests/test_stats.c)
sllm_test(test_analogical      tests/test_analogical.c)
sllm_test(test_stem           tests/test_stem.c)
sllm_test(test_i18n           tests/test_i18n.c)
sllm_test(test_self_reference tests/test_self_reference.c)
sllm_test(test_neuro_prolog  tests/test_neuro_prolog.c)
sllm_test(test_neuro_prolog_rules tests/test_neuro_prolog_rules.c)
sllm_test(test_provenance     tests/test_provenance.c)
sllm_test(test_numeric        tests/test_numeric.c)
sllm_test(test_attention       test_attention.c)
sllm_test(test_concepts        test_concepts.c)
sllm_test(test_qa_debug        test_qa_debug.c)

sllm_test(test_tagged_term   tests/test_tagged_term.c)
sllm_test(test_schema        tests/test_schema.c)
sllm_test(test_metaschema    tests/test_metaschema.c)
sllm_test(test_family2        tests/test_family2.c)
# Removed: test_bible_kinship, test_bible_conclusions, test_bible_chat,
# test_bible_relfam, test_bible_chat_lex (depend on bible_relations.tsv)
sllm_test(test_queryplan     tests/test_queryplan.c)
sllm_test(test_composite      tests/test_composite.c)
sllm_test(test_clarify        tests/test_clarify.c)
# Removed: test_toolplan, test_toolexec (depend on fixtures.tsv)
sllm_test(test_action          tests/test_action.c)
sllm_test(test_toolconfig      tests/test_toolconfig.c)
sllm_test(test_fixtures        tests/test_fixtures.c)
# Removed: test_self (depends on self.tsv), test_crules (depends on languagec.tsv),
# test_ingiere (tests loading .tsv files)
sllm_test(test_textlex         tests/test_textlex.c)
# Removed: test_server_proto, test_chain, test_bible_bfs, test_compose
# (depend on bible_relations.tsv)
sllm_test(test_learn          tests/test_learn.c)
sllm_test(test_deep_nlg        tests/test_deep_nlg.c)
sllm_test(test_graph_reasoning tests/test_graph_reasoning.c)
sllm_test(test_verify_3_points tests/test_verify_3_points.c)
sllm_test(test_cognitive_learning tests/test_cognitive_learning.c)
sllm_test(test_stochastic_nlg tests/test_stochastic_nlg.c)
sllm_test(test_metacognition tests/test_metacognition.c)
sllm_test(test_passage_nlg tests/test_passage_nlg.c)
sllm_test(test_agent_core tests/test_agent_core.c)
sllm_test(test_server_session tests/test_server_session.c)
if(UNIX)
    add_test(NAME test_session_isolation_e2e
             COMMAND bash ${CMAKE_SOURCE_DIR}/tests/test_session_isolation.sh
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_session_isolation_e2e PROPERTIES SKIP_RETURN_CODE 77)
    add_test(NAME test_shell_routing_e2e
             COMMAND bash ${CMAKE_SOURCE_DIR}/tests/test_shell_routing.sh
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_shell_routing_e2e PROPERTIES SKIP_RETURN_CODE 77)
    add_test(NAME test_opencode_repository_stream_e2e
             COMMAND bash ${CMAKE_SOURCE_DIR}/tests/test_opencode_repository_stream.sh
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_opencode_repository_stream_e2e PROPERTIES SKIP_RETURN_CODE 77)
    add_test(NAME test_opencode_subagent_e2e
             COMMAND bash ${CMAKE_SOURCE_DIR}/tests/test_opencode_subagent.sh
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_opencode_subagent_e2e PROPERTIES SKIP_RETURN_CODE 77)
    add_test(NAME test_repository_c_repair_e2e
             COMMAND bash ${CMAKE_SOURCE_DIR}/tests/test_repository_c_repair.sh
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_repository_c_repair_e2e PROPERTIES SKIP_RETURN_CODE 77)
    add_test(NAME test_git_inquiry_e2e
             COMMAND bash ${CMAKE_SOURCE_DIR}/tests/test_git_inquiry_e2e.sh
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_git_inquiry_e2e PROPERTIES SKIP_RETURN_CODE 77)
    add_test(NAME test_episodic_memory_e2e
             COMMAND bash ${CMAKE_SOURCE_DIR}/tests/test_episodic_memory_e2e.sh
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_episodic_memory_e2e PROPERTIES SKIP_RETURN_CODE 77)
endif()
sllm_test(test_code_graph tests/test_code_graph.c)
sllm_test(test_agent_patch tests/test_agent_patch.c)
sllm_test(test_agent_planner tests/test_agent_planner.c)
sllm_test(test_agent_runner tests/test_agent_runner.c)
sllm_test(test_agent_runner_heldout tests/test_agent_runner_heldout.c)
sllm_test(test_agent_runner_external tests/test_agent_runner_external.c)
sllm_test(test_agent_runner_learning tests/test_agent_runner_learning.c)
# Per-commit bank report: runs external+heldout+learning, appends one JSONL row.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_custom_target(bank_report
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/bank_report.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS test_agent_runner_external test_agent_runner_heldout
                test_agent_runner_learning
        COMMENT "Agent-runner task bank report (JSONL)"
        VERBATIM)
    # Engineering bank self-test: every check.py fails on before/ and passes on after/.
    # TIMEOUT 300: measured 101s alone, 148s under full-suite load (flaked at default 180s).
    add_test(NAME test_engineering_bank
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/test_engineering_bank.py
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_engineering_bank PROPERTIES TIMEOUT 300)
    # Bank harness per-task setup.py / golden.py (git fixtures); skips (77) without git.
    add_test(NAME test_bank_harness_setup
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_bank_harness_setup.py
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_bank_harness_setup PROPERTIES TIMEOUT 120 SKIP_RETURN_CODE 77)
endif()
sllm_test(test_agent_hard_tasks tests/test_agent_hard_tasks.c)
sllm_test(test_tps_benchmark tests/test_tps_benchmark.c)
sllm_test(test_server_tool_calling tests/test_server_tool_calling.c)
sllm_test(test_opencode_subagent tests/test_opencode_subagent.c)
sllm_test(test_opencode_parent tests/test_opencode_parent.c)
sllm_test(test_c_edit_ops tests/test_c_edit_ops.c)
sllm_test(test_agent_diagnose tests/test_agent_diagnose.c)
sllm_test(test_code_graph_polyglot tests/test_code_graph_polyglot.c)
sllm_test(test_swe_bench_harness tests/test_swe_bench_harness.c)
sllm_test(test_code_graph_indexer tests/test_code_graph_indexer.c)
sllm_test(test_agent_shell tests/test_agent_shell.c)
sllm_test(test_task_ops tests/test_task_ops.c)
sllm_test(test_shell_ops tests/test_shell_ops.c)
sllm_test(test_server_taskops tests/test_server_taskops.c)
sllm_test(test_build_ops tests/test_build_ops.c)
sllm_test(test_command_policy tests/test_command_policy.c)
sllm_test(test_core_fuzz tests/test_core_fuzz.c)
sllm_test(test_c_fix_ops tests/test_c_fix_ops.c)
sllm_test(test_git_ops tests/test_git_ops.c)
sllm_test(test_agent_git tests/test_agent_git.c)
sllm_test(test_server_git_inquiry tests/test_server_git_inquiry.c)
sllm_test(test_c_synthesis tests/test_c_synthesis.c)

# chat REPL: CLI conversacional (no es test)
add_executable(chat_main src/chat_main.c)
target_link_libraries(chat_main symbolic)

add_executable(demo_conversation tests/demo_conversation.c)
target_link_libraries(demo_conversation symbolic)

# symbols-server: OpenAI-compatible model endpoint (harness, no es test)
add_executable(symbols-server src/symbols_server.c)
if(WIN32)
    target_link_libraries(symbols-server symbolic ws2_32)
else()
    target_link_libraries(symbols-server symbolic)
endif()

# symbols-agent: Standalone autonomous AI coding agent CLI
add_executable(symbols-agent src/agent_cli_main.c)
target_link_libraries(symbols-agent symbolic)

# relprobe: graph relation inspector (diagnostic CLI, no es test)
add_executable(relprobe tools/relprobe.c)
target_link_libraries(relprobe symbolic)

# symbolic-learn: primer bucle de aprendizaje simbolico end-to-end (no es test)
add_executable(symbolic-learn src/symbolic_learn_cli.c)
target_link_libraries(symbolic-learn symbolic)

# Wikidata QA tests (Ask 5 gate still parked; golden now tracked). Golden model
# lives at data/wiki_model.bin (recovered from f4579eb^, Antonio 2026-09-24;
# committed as 8cb275d). Without the file these ten tests exit 77 (Skipped).
# They carry the ctest label qa_model; exclude with `ctest -LE qa_model`.
# Tests are read-only and never write the model.
add_executable(test_wiki_inference tests/test_wiki_inference.c)
target_link_libraries(test_wiki_inference symbolic)
add_test(NAME test_wiki_inference COMMAND test_wiki_inference data/wiki_model.bin FRANCIA CAPITAL WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(test_eval_qa tests/test_eval_qa.c)
target_link_libraries(test_eval_qa symbolic)
add_test(NAME test_eval_qa COMMAND test_eval_qa tests/qa_eval.tsv data/wiki_model.bin WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME test_eval_count COMMAND test_eval_qa tests/qa_eval_count.tsv data/wiki_model.bin WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME test_eval_negation COMMAND test_eval_qa tests/qa_eval_negation.tsv data/wiki_model.bin WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME test_eval_default COMMAND test_eval_qa tests/qa_eval_default.tsv data/wiki_model.bin WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME test_eval_reverse COMMAND test_eval_qa tests/qa_eval_reverse.tsv data/wiki_model.bin WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME test_eval_conjunctive COMMAND test_eval_qa tests/qa_eval_conjunctive.tsv data/wiki_model.bin WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME test_eval_multihop COMMAND test_eval_qa tests/qa_eval_multihop.tsv data/wiki_model.bin WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(test_qa_hygiene tests/test_qa_hygiene.c)
target_link_libraries(test_qa_hygiene symbolic)
add_test(NAME test_qa_hygiene COMMAND test_qa_hygiene tests/qa_eval.tsv data/wiki_model.bin WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# Hidden QA split: measure-only runner (never a development threshold).
# Prints HIDDEN_QA pass= total= rate=; exit 0 on well-formed set, 77 if model missing.
add_executable(test_output_contract tests/test_output_contract.c)
target_link_libraries(test_output_contract symbolic)
add_test(NAME test_output_contract COMMAND test_output_contract)

add_executable(test_qa_hidden tests/test_qa_hidden.c)
target_link_libraries(test_qa_hidden symbolic)
add_test(NAME test_qa_hidden COMMAND test_qa_hidden tests/qa_hidden.tsv data/wiki_model.bin WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_qa_hidden PROPERTIES SKIP_RETURN_CODE 77)

add_executable(test_qa_layer tests/test_qa_layer.c)
target_link_libraries(test_qa_layer symbolic)
add_test(NAME test_qa_layer COMMAND test_qa_layer data/wiki_model.bin WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

set_tests_properties(
    test_wiki_inference
    test_eval_qa
    test_eval_count
    test_eval_negation
    test_eval_default
    test_eval_reverse
    test_eval_conjunctive
    test_eval_multihop
    test_qa_hygiene
    PROPERTIES SKIP_RETURN_CODE 77
)
set_tests_properties(
    test_wiki_inference test_eval_qa test_eval_count test_eval_negation
    test_eval_default test_eval_reverse test_eval_conjunctive test_eval_multihop
    test_qa_hygiene test_qa_hidden
    PROPERTIES LABELS qa_model
)

# test_wiki_ingest / test_wikidata_ingest write to their own scratch files
# (wiki_model_ingest_test.bin / wikidata_model_test.bin), never the golden
# data/wiki_model.bin, so the model is never clobbered by a test.
sllm_test(test_wiki_ingest     tests/test_wiki_ingest.c)
sllm_test(test_wikidata_ingest tests/test_wikidata_ingest.c)

# test_jung_battery: 20-question QA battery over Jung corpus
add_executable(test_jung_battery tests/test_jung_battery.c)
target_link_libraries(test_jung_battery symbolic)

# test_battery50: 50-question cross-corpus battery (Jung + Bible)
add_executable(test_battery50 tests/test_battery50.c)
target_link_libraries(test_battery50 symbolic)

# test_battery100: 100-question multi-domain battery (Jung + Bible + Wikipedia)
add_executable(test_battery100 tests/test_battery100.c)
target_link_libraries(test_battery100 symbolic)

# test_multi_hop: reasoning depth (1-hop, 2-hop, negative, cross-domain)
sllm_test(test_multi_hop tests/test_multi_hop.c)

# L3: graph_reasoning on the chat hot path (deductive closure)
sllm_test(test_chat_reasoning tests/test_chat_reasoning.c)

# Open-domain QA: entity-grounded retrieval (no Lev false friends)
sllm_test(test_open_qa tests/test_open_qa.c)

# Pillar 1: Vector Symbolic Architecture (VSA / HDC)
sllm_test(test_vsa tests/test_vsa.c)

# Pillar 2: Combinatory Categorial Grammar (CCG) & Surface Realization
sllm_test(test_ccg_realizer tests/test_ccg_realizer.c)

# Pillar 3: Large-Scale Commonsense & World Knowledge Ingestion
sllm_test(test_commonsense tests/test_commonsense.c)
sllm_test(test_cs_battery tests/test_cs_battery.c)
# Throughput assertion flakes under parallel ctest; keep the check but serialize.
set_tests_properties(test_commonsense PROPERTIES RUN_SERIAL TRUE)

# Pillar 4: Pragmatic Conditioning, Epistemic Perspectives & Persona Filters
sllm_test(test_persona tests/test_persona.c)

# Continuous Episodic Memory Persistence
sllm_test(test_episodic_memory tests/test_episodic_memory.c)
sllm_test(test_episodic_session tests/test_episodic_session.c)

# Universal Generic Relation Ingestion & Binary Model QA
sllm_test(test_model_generic_rel tests/test_model_generic_rel.c)

