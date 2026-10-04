cmake_minimum_required(VERSION 3.10)
project(SymbolicLLM C)

# Measurement only (m130): what a Windows runner gives to `gcc` and to a missing command under cmd.exe.
# It prints "gcc-probe" lines in the configure log and never fails the configure.
function(gcc_probe)
    string(REPLACE ";" " " label "${ARGN}")
    execute_process(COMMAND cmd /c ${ARGN}
        RESULT_VARIABLE probe_rc OUTPUT_VARIABLE probe_out ERROR_VARIABLE probe_err TIMEOUT 30)
    string(REPLACE "\n" " | " probe_out "${probe_out}")
    string(REPLACE "\n" " | " probe_err "${probe_err}")
    message(STATUS "gcc-probe: cmd /c ${label}: rc=[${probe_rc}] out=[${probe_out}] err=[${probe_err}]")
endfunction()
if(WIN32)
    gcc_probe(where gcc)
    gcc_probe(gcc --version)
    gcc_probe(this_command_does_not_exist_probe)
endif()

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
    src/git_gate.c
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
    src/compile_repair.c
    src/shell_contract.c
    src/c_contract.c
    src/reflect.c
    src/git_ops.c
    src/text_lex.c
    src/meta_graph.c
    src/vsa.c
    src/ccg_realizer.c
    src/commonsense.c
    src/persona.c
    src/atomic_store.c
    src/fs_read.c
    src/fs_manifest.c
    src/fs_txn_plan.c
    src/engineering_episode.c
    src/attempt_capture.c
    src/episodic_memory.c
)

target_include_directories(symbolic PUBLIC include)
if(MSVC)
    # MSVC marks portable C runtime calls such as fopen, strcpy and tmpfile
    # as deprecated (C4996). Keep the shared C source portable rather than
    # replacing them with MSVC-only *_s variants.
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
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

add_executable(symbols_git_gate tools/symbols_git_gate.c)
target_link_libraries(symbols_git_gate symbolic)

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
# Compile the index implementation into a private fault-injection test target.
add_executable(test_relation_subject_index tests/test_relation_subject_index.c)
target_include_directories(test_relation_subject_index PRIVATE include)
add_test(NAME test_relation_subject_index COMMAND test_relation_subject_index)
sllm_test(test_fs_read tests/test_fs_read.c)
sllm_test(test_fs_manifest tests/test_fs_manifest.c)
sllm_test(test_fs_create tests/test_fs_create.c)
if(WIN32)
    add_executable(test_fs_create_windows tests/test_fs_create_windows.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_create_windows PRIVATE include)
    target_compile_definitions(test_fs_create_windows PRIVATE FS_CREATE_TEST_CRASH=1)
    add_test(NAME test_fs_create_windows COMMAND test_fs_create_windows
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_executable(test_fs_win_symlink_leaf tests/test_fs_win_symlink_leaf.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_win_symlink_leaf PRIVATE include)
    add_test(NAME test_fs_win_symlink_leaf COMMAND test_fs_win_symlink_leaf
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_fs_win_symlink_leaf PROPERTIES SKIP_RETURN_CODE 77)
    add_executable(test_fs_win_journal tests/test_fs_win_journal.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_win_journal PRIVATE include)
    target_compile_definitions(test_fs_win_journal PRIVATE FS_CREATE_TEST_CRASH=1)
    add_test(NAME test_fs_win_journal COMMAND test_fs_win_journal
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_executable(test_fs_win_ops tests/test_fs_win_ops.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_win_ops PRIVATE include)
    target_compile_definitions(test_fs_win_ops PRIVATE FS_CREATE_TEST_CRASH=1)
    add_test(NAME test_fs_win_ops COMMAND test_fs_win_ops
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_executable(test_fs_win_replace tests/test_fs_win_replace.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_win_replace PRIVATE include)
    target_compile_definitions(test_fs_win_replace PRIVATE FS_CREATE_TEST_CRASH=1)
    add_test(NAME test_fs_win_replace COMMAND test_fs_win_replace
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_executable(test_agent_patch_fs_win tests/test_agent_patch_fs_win.c src/agent_patch.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_agent_patch_fs_win PRIVATE include)
    target_compile_definitions(test_agent_patch_fs_win PRIVATE FS_CREATE_TEST_CRASH=1 AGENT_PATCH_TEST_HOOK=1)
    add_test(NAME test_agent_patch_fs_win COMMAND test_agent_patch_fs_win
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    # Mutant 12: the drift guard of replace_target is dropped. The same test must fail exactly four cells.
    add_executable(test_agent_patch_fs_win_mc tests/test_agent_patch_fs_win.c src/agent_patch.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_agent_patch_fs_win_mc PRIVATE include)
    target_compile_definitions(test_agent_patch_fs_win_mc PRIVATE FS_CREATE_TEST_CRASH=1 AGENT_PATCH_TEST_HOOK=1 AGENT_PATCH_APPLY_MUTANT=1)
    add_test(NAME test_agent_patch_fs_win_mc COMMAND test_agent_patch_fs_win_mc
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_agent_patch_fs_win_mc PROPERTIES PASS_REGULAR_EXPRESSION "mutant killed by 4 cell")
    add_executable(test_agent_patch_crash_win tests/test_agent_patch_crash_win.c src/agent_patch.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_agent_patch_crash_win PRIVATE include)
    target_compile_definitions(test_agent_patch_crash_win PRIVATE FS_CREATE_TEST_CRASH=1 AGENT_PATCH_TEST_HOOK=1)
    add_test(NAME test_agent_patch_crash_win COMMAND test_agent_patch_crash_win
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    # PASS_REGULAR_EXPRESSION replaces the exit code check; the line is printed only at zero failures.
    set_tests_properties(test_agent_patch_crash_win PROPERTIES PASS_REGULAR_EXPRESSION "agentpatch crash phases ran: 7")
    # Mutant: FS_WIN_BATCH_MUTANT=10 skips the sweep; the same test must then fail.
    add_test(NAME test_agent_patch_crash_win_mc10 COMMAND test_agent_patch_crash_win
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_agent_patch_crash_win_mc10 PROPERTIES
        ENVIRONMENT "FS_WIN_BATCH_MUTANT=10" PASS_REGULAR_EXPRESSION "mutant killed by 6 cell")
    # Mutant 11: the sweep keeps an unshared .fsrs- stage; only the phase 1 leftover cell may fail.
    add_test(NAME test_agent_patch_crash_win_mc11 COMMAND test_agent_patch_crash_win
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_agent_patch_crash_win_mc11 PROPERTIES
        ENVIRONMENT "FS_WIN_BATCH_MUTANT=11" PASS_REGULAR_EXPRESSION "mutant killed by 1 cell")
    add_executable(test_agent_patch_lock_win tests/test_agent_patch_lock_win.c src/agent_patch.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_agent_patch_lock_win PRIVATE include)
    target_compile_definitions(test_agent_patch_lock_win PRIVATE FS_CREATE_TEST_CRASH=1 AGENT_PATCH_TEST_HOOK=1)
    add_test(NAME test_agent_patch_lock_win COMMAND test_agent_patch_lock_win
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    # The marker is printed only at zero failures; PASS_REGULAR_EXPRESSION replaces the exit code check.
    set_tests_properties(test_agent_patch_lock_win PROPERTIES PASS_REGULAR_EXPRESSION "agentpatch lock cells ran: 6")
    add_executable(test_agent_patch_ntfs_win tests/test_agent_patch_ntfs_win.c src/agent_patch.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_agent_patch_ntfs_win PRIVATE include)
    target_compile_definitions(test_agent_patch_ntfs_win PRIVATE FS_CREATE_TEST_CRASH=1 AGENT_PATCH_TEST_HOOK=1)
    add_test(NAME test_agent_patch_ntfs_win COMMAND test_agent_patch_ntfs_win
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_executable(test_fs_win_reparse tests/test_fs_win_reparse.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_win_reparse PRIVATE include)
    add_test(NAME test_fs_win_reparse COMMAND test_fs_win_reparse
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_executable(test_fs_win_reparse_mc1 tests/test_fs_win_reparse.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_win_reparse_mc1 PRIVATE include)
    target_compile_definitions(test_fs_win_reparse_mc1 PRIVATE FS_REPARSE_MUTANT=1)
    add_test(NAME test_fs_win_reparse_mc1 COMMAND test_fs_win_reparse_mc1
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_executable(test_fs_win_aliases tests/test_fs_win_aliases.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_win_aliases PRIVATE include)
    add_test(NAME test_fs_win_aliases COMMAND test_fs_win_aliases
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_executable(test_fs_win_aliases_short tests/test_fs_win_aliases.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_win_aliases_short PRIVATE include)
    target_compile_definitions(test_fs_win_aliases_short PRIVATE ALIAS_SHORT=1)
    add_test(NAME test_fs_win_aliases_short COMMAND test_fs_win_aliases_short
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_fs_win_aliases_short PROPERTIES SKIP_RETURN_CODE 77)
    add_executable(test_fs_win_aliases_mc2 tests/test_fs_win_aliases.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_win_aliases_mc2 PRIVATE include)
    target_compile_definitions(test_fs_win_aliases_mc2 PRIVATE FS_ALIAS_MUTANT=2)
    add_test(NAME test_fs_win_aliases_mc2 COMMAND test_fs_win_aliases_mc2
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_executable(test_fs_win_aliases_mc3 tests/test_fs_win_aliases.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_win_aliases_mc3 PRIVATE include)
    target_compile_definitions(test_fs_win_aliases_mc3 PRIVATE FS_ALIAS_MUTANT=3)
    add_test(NAME test_fs_win_aliases_mc3 COMMAND test_fs_win_aliases_mc3
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_executable(test_fs_win_race tests/test_fs_win_race.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_win_race PRIVATE include)
    add_test(NAME test_fs_win_race COMMAND test_fs_win_race
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_executable(test_fs_win_case tests/test_fs_win_case.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_win_case PRIVATE include)
    add_test(NAME test_fs_win_case COMMAND test_fs_win_case
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_executable(test_fs_win_longpath tests/test_fs_win_longpath.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_win_longpath PRIVATE include)
    add_test(NAME test_fs_win_longpath COMMAND test_fs_win_longpath
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_executable(test_fs_win_locked tests/test_fs_win_locked.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_win_locked PRIVATE include)
    add_test(NAME test_fs_win_locked COMMAND test_fs_win_locked
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_fs_win_locked PROPERTIES TIMEOUT 120)
    add_executable(test_fs_win_newline tests/test_fs_win_newline.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_win_newline PRIVATE include)
    add_test(NAME test_fs_win_newline COMMAND test_fs_win_newline
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_fs_win_newline PROPERTIES TIMEOUT 120)
    add_executable(test_fs_win_devext tests/test_fs_win_devext.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_win_devext PRIVATE include)
    add_test(NAME test_fs_win_devext COMMAND test_fs_win_devext
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_executable(test_fs_win_fileparent tests/test_fs_win_fileparent.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_win_fileparent PRIVATE include)
    add_test(NAME test_fs_win_fileparent COMMAND test_fs_win_fileparent
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_executable(test_fs_win_fileparent_mc tests/test_fs_win_fileparent.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_win_fileparent_mc PRIVATE include)
    target_compile_definitions(test_fs_win_fileparent_mc PRIVATE FS_PARENT_MUTANT=1)
    add_test(NAME test_fs_win_fileparent_mc COMMAND test_fs_win_fileparent_mc
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_executable(test_fs_win_perm tests/test_fs_win_perm.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_win_perm PRIVATE include)
    add_test(NAME test_fs_win_perm COMMAND test_fs_win_perm
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_executable(test_fs_win_perm_mc4 tests/test_fs_win_perm.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_win_perm_mc4 PRIVATE include)
    target_compile_definitions(test_fs_win_perm_mc4 PRIVATE FS_PIN_MUTANT=1)
    add_test(NAME test_fs_win_perm_mc4 COMMAND test_fs_win_perm_mc4
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_executable(test_fs_win_perm_acl tests/test_fs_win_perm.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_win_perm_acl PRIVATE include)
    target_compile_definitions(test_fs_win_perm_acl PRIVATE ACL_ONLY=1)
    add_test(NAME test_fs_win_perm_acl COMMAND test_fs_win_perm_acl
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_fs_win_perm_acl PROPERTIES SKIP_RETURN_CODE 77)
    add_executable(test_fs_win_device_names tests/test_fs_win_device_names.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_win_device_names PRIVATE include)
    add_test(NAME test_fs_win_device_names COMMAND test_fs_win_device_names
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_executable(test_fs_win_batch_validate tests/test_fs_win_batch_validate.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_win_batch_validate PRIVATE include)
    target_compile_definitions(test_fs_win_batch_validate PRIVATE FS_CREATE_TEST_CRASH=1)
    add_test(NAME test_fs_win_batch_validate COMMAND test_fs_win_batch_validate
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_executable(test_fs_win_batch_create tests/test_fs_win_batch_create.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_win_batch_create PRIVATE include)
    target_compile_definitions(test_fs_win_batch_create PRIVATE FS_CREATE_TEST_CRASH=1)
    add_test(NAME test_fs_win_batch_create COMMAND test_fs_win_batch_create
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_executable(test_fs_win_batch_replace tests/test_fs_win_batch_replace.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_win_batch_replace PRIVATE include)
    target_compile_definitions(test_fs_win_batch_replace PRIVATE FS_CREATE_TEST_CRASH=1)
    add_test(NAME test_fs_win_batch_replace COMMAND test_fs_win_batch_replace
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_executable(test_fs_win_probe tests/test_fs_win_probe.c)
    add_test(NAME test_fs_win_probe COMMAND test_fs_win_probe
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
else()
    sllm_test(test_fs_create_windows tests/test_fs_create_windows.c)
endif()
sllm_test(test_fs_copy tests/test_fs_copy.c)
sllm_test(test_fs_adversarial tests/test_fs_adversarial.c)
sllm_test(test_fs_fuzz_ops tests/test_fs_fuzz_ops.c)
# On Windows the fuzz has symlink-leaf cases that need CreateSymbolicLinkA.
# With this variable a runner that cannot create the symlink makes the test
# fail instead of silently skipping those cases, so Passed means they ran.
set(fz_req "")
if(WIN32)
    set(fz_req ";FS_FUZZ_REQUIRE_SYMLINK=1")
    set_tests_properties(test_fs_fuzz_ops PROPERTIES
        ENVIRONMENT "FS_FUZZ_REQUIRE_SYMLINK=1")
endif()
# More fixed seeds of the same confinement fuzz (M1 criterion 5). The seed and
# the iteration count are in the test name, so a CI log names what ran. The
# test is sequential in CI (no ctest -j); every run uses the same scratch
# directories, so do not run these in parallel.
foreach(fz_seed 1 7 12648430)
    add_test(NAME test_fs_fuzz_ops_seed_${fz_seed}_i400 COMMAND test_fs_fuzz_ops
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_fs_fuzz_ops_seed_${fz_seed}_i400 PROPERTIES
        ENVIRONMENT "FS_FUZZ_SEED=${fz_seed};FS_FUZZ_ITERS=400${fz_req}" TIMEOUT 300)
endforeach()
sllm_test(test_fs_c4_posix tests/test_fs_c4_posix.c)
sllm_test(test_fs_race tests/test_fs_race.c)
if(NOT WIN32)
    add_executable(test_fs_replace tests/test_fs_replace.c src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_replace PRIVATE include)
    target_compile_definitions(test_fs_replace PRIVATE FS_CREATE_TEST_CRASH=1)
    add_test(NAME test_fs_replace COMMAND test_fs_replace WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_executable(test_fs_race_parent tests/test_fs_race_parent.c src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_race_parent PRIVATE include)
    target_compile_definitions(test_fs_race_parent PRIVATE FS_CREATE_TEST_CRASH=1)
    add_test(NAME test_fs_race_parent COMMAND test_fs_race_parent WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_executable(test_fs_race_parent_mc tests/test_fs_race_parent.c src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_race_parent_mc PRIVATE include)
    target_compile_definitions(test_fs_race_parent_mc PRIVATE FS_CREATE_TEST_CRASH=1 FS_PARENT_FOLLOW_MUTANT=1)
    add_test(NAME test_fs_race_parent_mc COMMAND test_fs_race_parent_mc WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_executable(test_agent_patch_fs tests/test_agent_patch_fs.c src/agent_patch.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_agent_patch_fs PRIVATE include)
    target_compile_definitions(test_agent_patch_fs PRIVATE FS_CREATE_TEST_CRASH=1 AGENT_PATCH_TEST_HOOK=1)
    add_test(NAME test_agent_patch_fs COMMAND test_agent_patch_fs WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_executable(test_fs_move tests/test_fs_move.c src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_move PRIVATE include)
    target_compile_definitions(test_fs_move PRIVATE FS_CREATE_TEST_CRASH=1)
    add_test(NAME test_fs_move COMMAND test_fs_move WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()
if(NOT WIN32)
    add_executable(test_fs_remove tests/test_fs_remove.c src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_remove PRIVATE include)
    target_compile_definitions(test_fs_remove PRIVATE FS_CREATE_TEST_CRASH=1)
    add_test(NAME test_fs_remove COMMAND test_fs_remove WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()
sllm_test(test_fs_txn_plan tests/test_fs_txn_plan.c)
if(NOT WIN32)
    add_executable(test_fs_batch tests/test_fs_batch.c src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_batch PRIVATE include)
    target_compile_definitions(test_fs_batch PRIVATE FS_CREATE_TEST_CRASH=1)
    add_test(NAME test_fs_batch COMMAND test_fs_batch WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_executable(test_fs_batch_replace tests/test_fs_batch_replace.c src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_batch_replace PRIVATE include)
    target_compile_definitions(test_fs_batch_replace PRIVATE FS_CREATE_TEST_CRASH=1)
    add_test(NAME test_fs_batch_replace COMMAND test_fs_batch_replace WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()
if(NOT WIN32)
    add_executable(test_fs_create_recover tests/test_fs_create_recover.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_create_recover PRIVATE include)
    target_compile_definitions(test_fs_create_recover PRIVATE FS_CREATE_TEST_CRASH=1)
    add_test(NAME test_fs_create_recover COMMAND test_fs_create_recover
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()
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
if(NOT WIN32)
    sllm_test(test_swe_bench_harness_fs tests/test_swe_bench_harness_fs.c)
endif()
sllm_test(test_agent_planner tests/test_agent_planner.c)
sllm_test(test_agent_runner tests/test_agent_runner.c)
sllm_test(test_agent_runner_heldout tests/test_agent_runner_heldout.c)
sllm_test(test_agent_runner_external tests/test_agent_runner_external.c)
sllm_test(test_agent_runner_learning tests/test_agent_runner_learning.c)
# Per-commit bank report: runs external+heldout+learning, appends one JSONL row.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(NAME test_typed_c_schema
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_typed_c_schema.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_test(NAME test_qemu_collection
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_qemu_collection.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_test(NAME test_qemu_trace
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_qemu_trace.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_test(NAME test_qemu_closure
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_qemu_closure.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_test(NAME test_minimal_initramfs
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_minimal_initramfs.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_test(NAME test_builder_rootfs_policy
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_builder_rootfs_policy.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_test(NAME test_typed_c_verify_lock
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_typed_c_verify_lock.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_test(NAME test_typed_c_lock_wire
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_typed_c_lock_wire.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_test(NAME test_typed_c_framing
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_typed_c_framing.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_test(NAME test_typed_c_verify
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_typed_c_verify.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_test(NAME test_typed_c_preview
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_typed_c_preview.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_test(NAME test_typed_c_binary_preview
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_typed_c_binary_preview.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()
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
sllm_test(test_compile_repair tests/test_compile_repair.c)
sllm_test(test_shell_contract tests/test_shell_contract.c)
sllm_test(test_c_contract tests/test_c_contract.c)
sllm_test(test_c_contract_static tests/test_c_contract_static.c)
# Trusted static source transformer for the typed, non-executing preview.
add_executable(c_contract_static src/c_contract_static_main.c)
target_link_libraries(c_contract_static symbolic)
sllm_test(test_reflect tests/test_reflect.c)
sllm_test(test_git_ops tests/test_git_ops.c)
if(WIN32)
    # The Windows revert cells need gcc and skip quietly without it. This turns a skip into a red:
    # the test prints the line only when nothing failed and all three revert fixtures ran.
    # PASS_REGULAR_EXPRESSION replaces the exit code check, which is why the line is printed only at zero failures.
    set_tests_properties(test_git_ops PROPERTIES PASS_REGULAR_EXPRESSION "revert cells ran: 3")
endif()
sllm_test(test_agent_git tests/test_agent_git.c)
sllm_test(test_agent_git_remote tests/test_agent_git_remote.c)
sllm_test(test_agent_git_contract tests/test_agent_git_contract.c)
sllm_test(test_agent_git_submodule tests/test_agent_git_submodule.c)
sllm_test(test_git_destructive_audit tests/test_git_destructive_audit.c)
sllm_test(test_git_gate_e2e tests/test_git_gate_e2e.c)
# Mutant: patch-state never recognises an applied patch. Kill = exit 0 (a retry cell failed
# and every other cell passed). POSIX only, like the test.
if(NOT WIN32)
    add_executable(test_git_gate_e2e_mc tests/test_git_gate_e2e.c src/git_gate.c src/agent_git.c src/agent_shell.c src/command_policy.c)
    target_include_directories(test_git_gate_e2e_mc PRIVATE include)
    target_compile_definitions(test_git_gate_e2e_mc PRIVATE AGENT_GIT_PATCH_STATE_MUTANT=1)
    add_test(NAME test_git_gate_e2e_mc COMMAND test_git_gate_e2e_mc WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()
sllm_test(test_git_gate tests/test_git_gate.c)
add_dependencies(test_git_gate symbols_git_gate)
target_compile_definitions(test_git_gate PRIVATE GATE_EXE="$<TARGET_FILE:symbols_git_gate>")
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
sllm_test(test_chat_text_cleanup tests/test_chat_text_cleanup.c)

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


add_executable(lexical_impact tools/ast_inspect/lexical_impact.c)
target_link_libraries(lexical_impact symbolic)
if(Python3_Interpreter_FOUND)
    add_test(NAME test_impact_compare
             COMMAND ${CMAKE_COMMAND} -E env
                     LEXICAL_IMPACT_BIN=$<TARGET_FILE:lexical_impact>
                     ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_impact_compare.py
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

# Opt-in, read-only libclang consumer. The C core and default CI build never
# download or link LLVM. Install the pinned package to run the corpus check.
if(Python3_Interpreter_FOUND)
    add_test(NAME test_ast_inspect
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_ast_inspect.py
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_test(NAME test_c_source_qa
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_c_source_qa.py
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

sllm_test(test_atomic_store tests/test_atomic_store.c)
sllm_test(test_engineering_episode tests/test_engineering_episode.c)
if(Python3_Interpreter_FOUND)
    add_test(NAME test_episode_replay_contract
             COMMAND ${CMAKE_COMMAND} -E env
                     ATTEMPT_CAPTURE_REPORT_BIN=$<TARGET_FILE:attempt_capture_report>
                     SYMBOLS_AGENT_BIN=$<TARGET_FILE:symbols-agent>
                     ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_episode_replay.py
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()
sllm_test(test_episode_shadow tests/test_episode_shadow.c)
# Isolated copy of the library: only this test sees rollback fault hooks.
# The production symbolic archive is compiled without TASK_OPS_TEST_FAULTS.
get_target_property(symbolic_sources symbolic SOURCES)
add_library(symbolic_rollback_test STATIC ${symbolic_sources})
target_include_directories(symbolic_rollback_test PUBLIC include)
target_compile_definitions(symbolic_rollback_test PRIVATE TASK_OPS_TEST_FAULTS)
if(MSVC)
    target_compile_definitions(symbolic_rollback_test PUBLIC strcasecmp=_stricmp strncasecmp=_strnicmp)
    target_link_options(symbolic_rollback_test INTERFACE "/STACK:8388608")
else()
    target_link_libraries(symbolic_rollback_test PUBLIC m)
endif()
if(WIN32)
    target_link_libraries(symbolic_rollback_test PUBLIC psapi)
endif()
add_executable(test_task_ops_rollback tests/test_task_ops_rollback.c)
target_compile_definitions(test_task_ops_rollback PRIVATE TASK_OPS_TEST_FAULTS)
target_link_libraries(test_task_ops_rollback symbolic_rollback_test)
add_test(NAME test_task_ops_rollback COMMAND test_task_ops_rollback WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

sllm_test(test_runner_episode_shadow tests/test_runner_episode_shadow.c)
add_library(engineering_episode_report_lib STATIC tools/engineering_episode_report.c)
target_link_libraries(engineering_episode_report_lib symbolic)
target_compile_definitions(engineering_episode_report_lib PRIVATE EPISODE_REPORT_NO_MAIN)
target_link_libraries(test_engineering_episode engineering_episode_report_lib)
add_executable(engineering_episode_report tools/engineering_episode_report.c)
target_link_libraries(engineering_episode_report symbolic)

add_executable(attempt_capture_report tools/attempt_capture_report.c src/attempt_capture.c)
target_include_directories(attempt_capture_report PRIVATE include)

add_executable(test_attempt_capture tests/test_attempt_capture.c src/attempt_capture.c)
target_compile_definitions(test_attempt_capture PRIVATE ATTEMPT_CAPTURE_TEST_CRASH=1)
target_include_directories(test_attempt_capture PRIVATE include)
add_test(NAME test_attempt_capture COMMAND test_attempt_capture WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(test_agent_patch_eol tests/test_agent_patch_eol.c src/agent_patch.c
    src/fs_read.c src/fs_manifest.c)
target_include_directories(test_agent_patch_eol PRIVATE include)
add_test(NAME test_agent_patch_eol COMMAND test_agent_patch_eol WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_executable(test_agent_patch_eol_mc tests/test_agent_patch_eol.c src/agent_patch.c
    src/fs_read.c src/fs_manifest.c)
target_include_directories(test_agent_patch_eol_mc PRIVATE include)
target_compile_definitions(test_agent_patch_eol_mc PRIVATE AGENT_PATCH_EOL_MUTANT=1)
add_test(NAME test_agent_patch_eol_mc COMMAND test_agent_patch_eol_mc WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_executable(test_agent_patch_eol_crmc tests/test_agent_patch_eol.c src/agent_patch.c
    src/fs_read.c src/fs_manifest.c)
target_include_directories(test_agent_patch_eol_crmc PRIVATE include)
target_compile_definitions(test_agent_patch_eol_crmc PRIVATE AGENT_PATCH_CR_MUTANT=1)
add_test(NAME test_agent_patch_eol_crmc COMMAND test_agent_patch_eol_crmc WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
