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
    src/text_lex.c
    src/meta_graph.c
)

target_include_directories(symbolic PUBLIC include)
if(NOT MSVC)
    target_link_libraries(symbolic PUBLIC m)
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
sllm_test(test_code_graph tests/test_code_graph.c)
sllm_test(test_agent_patch tests/test_agent_patch.c)
sllm_test(test_agent_planner tests/test_agent_planner.c)
sllm_test(test_agent_runner tests/test_agent_runner.c)
sllm_test(test_agent_hard_tasks tests/test_agent_hard_tasks.c)
sllm_test(test_tps_benchmark tests/test_tps_benchmark.c)
sllm_test(test_server_tool_calling tests/test_server_tool_calling.c)
sllm_test(test_agent_diagnose tests/test_agent_diagnose.c)

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

# relprobe: graph relation inspector (diagnostic CLI, no es test)
add_executable(relprobe tools/relprobe.c)
target_link_libraries(relprobe symbolic)

# symbolic-learn: primer bucle de aprendizaje simbolico end-to-end (no es test)
add_executable(symbolic-learn src/symbolic_learn_cli.c)
target_link_libraries(symbolic-learn symbolic)

# Read-only queries against the tracked golden wiki_model.bin
add_executable(test_wiki_inference tests/test_wiki_inference.c)
target_link_libraries(test_wiki_inference symbolic)
add_test(NAME test_wiki_inference COMMAND test_wiki_inference wiki_model.bin FRANCIA CAPITAL WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(test_eval_qa tests/test_eval_qa.c)
target_link_libraries(test_eval_qa symbolic)
add_test(NAME test_eval_qa COMMAND test_eval_qa tests/qa_eval.tsv wiki_model.bin WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME test_eval_count COMMAND test_eval_qa tests/qa_eval_count.tsv wiki_model.bin WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME test_eval_negation COMMAND test_eval_qa tests/qa_eval_negation.tsv wiki_model.bin WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME test_eval_default COMMAND test_eval_qa tests/qa_eval_default.tsv wiki_model.bin WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME test_eval_reverse COMMAND test_eval_qa tests/qa_eval_reverse.tsv wiki_model.bin WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME test_eval_conjunctive COMMAND test_eval_qa tests/qa_eval_conjunctive.tsv wiki_model.bin WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME test_eval_multihop COMMAND test_eval_qa tests/qa_eval_multihop.tsv wiki_model.bin WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(test_qa_hygiene tests/test_qa_hygiene.c)
target_link_libraries(test_qa_hygiene symbolic)
add_test(NAME test_qa_hygiene COMMAND test_qa_hygiene tests/qa_eval.tsv wiki_model.bin WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(test_qa_layer tests/test_qa_layer.c)
target_link_libraries(test_qa_layer symbolic)
add_test(NAME test_qa_layer COMMAND test_qa_layer wiki_model.bin WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# test_wiki_ingest / test_wikidata_ingest write to their own scratch files
# (wiki_model_ingest_test.bin / wikidata_model_test.bin), never the tracked
# wiki_model.bin, so the golden model is never clobbered by a test.
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
add_executable(test_multi_hop tests/test_multi_hop.c)
target_link_libraries(test_multi_hop symbolic)

