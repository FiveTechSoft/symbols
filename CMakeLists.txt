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
    src/prune.c
    src/shard.c
    src/stats.c
    src/transfer.c
    src/neuro_prolog.c
    src/export.c
    src/stem.c
    src/i18n.c
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
sllm_test(test_provenance     tests/test_provenance.c)
sllm_test(test_attention       test_attention.c)
sllm_test(test_concepts        test_concepts.c)
sllm_test(test_qa_debug        test_qa_debug.c)

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

add_executable(test_qa_hygiene tests/test_qa_hygiene.c)
target_link_libraries(test_qa_hygiene symbolic)
add_test(NAME test_qa_hygiene COMMAND test_qa_hygiene tests/qa_eval.tsv wiki_model.bin WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# test_wiki_ingest / test_wikidata_ingest write to their own scratch files
# (wiki_model_ingest_test.bin / wikidata_model_test.bin), never the tracked
# wiki_model.bin, so the golden model is never clobbered by a test.
sllm_test(test_wiki_ingest     tests/test_wiki_ingest.c)
sllm_test(test_wikidata_ingest tests/test_wikidata_ingest.c)
