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
    src/select_report.c
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
    # Windows replace journal byte fuzz (m171): one kind, phases 3 to 6, two seeds. Every cell asserts the
    # outcome predicted before the first run; a miss names phase, class, iteration and seed.
    add_executable(test_fs_win_journal_fuzz tests/test_fs_win_journal_fuzz.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_win_journal_fuzz PRIVATE include)
    target_compile_definitions(test_fs_win_journal_fuzz PRIVATE FS_CREATE_TEST_CRASH=1)
    foreach(wjf_seed 1786707969 12648430)
        add_test(NAME test_fs_win_journal_fuzz_seed_${wjf_seed} COMMAND test_fs_win_journal_fuzz
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
        set_tests_properties(test_fs_win_journal_fuzz_seed_${wjf_seed} PROPERTIES
            ENVIRONMENT "FS_WJFUZZ_SEED=${wjf_seed}" TIMEOUT 900)
    endforeach()
    # Windows create journal byte fuzz (m176): second kind, points 3 to 6, two seeds, strict oracle with
    # predictions written before the first run.
    add_executable(test_fs_win_journal_fuzz_create tests/test_fs_win_journal_fuzz_create.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_win_journal_fuzz_create PRIVATE include)
    target_compile_definitions(test_fs_win_journal_fuzz_create PRIVATE FS_CREATE_TEST_CRASH=1)
    foreach(wjc_seed 1786707969 12648430)
        add_test(NAME test_fs_win_journal_fuzz_create_seed_${wjc_seed} COMMAND test_fs_win_journal_fuzz_create
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
        set_tests_properties(test_fs_win_journal_fuzz_create_seed_${wjc_seed} PROPERTIES
            ENVIRONMENT "FS_WJFUZZ_SEED=${wjc_seed}" TIMEOUT 900)
    endforeach()
    # Windows remove and move journal byte fuzz (m178): kinds 1 and 2, points 1 to 4, two seeds, strict oracle,
    # predictions written before the first run.
    add_executable(test_fs_win_journal_fuzz_ops tests/test_fs_win_journal_fuzz_ops.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_win_journal_fuzz_ops PRIVATE include)
    target_compile_definitions(test_fs_win_journal_fuzz_ops PRIVATE FS_CREATE_TEST_CRASH=1)
    foreach(wjo_seed 1786707969 12648430)
        add_test(NAME test_fs_win_journal_fuzz_ops_seed_${wjo_seed} COMMAND test_fs_win_journal_fuzz_ops
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
        set_tests_properties(test_fs_win_journal_fuzz_ops_seed_${wjo_seed} PROPERTIES
            ENVIRONMENT "FS_WJFUZZ_SEED=${wjo_seed}" TIMEOUT 900)
    endforeach()
    # Windows batch journal byte fuzz (m180): kinds 1 (create) and 2 (replace), points 3 to 6, two seeds, strict
    # oracle, predictions written before the first run.
    add_executable(test_fs_win_journal_fuzz_batch tests/test_fs_win_journal_fuzz_batch.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_win_journal_fuzz_batch PRIVATE include)
    target_compile_definitions(test_fs_win_journal_fuzz_batch PRIVATE FS_CREATE_TEST_CRASH=1)
    foreach(wjb_seed 1786707969 12648430)
        add_test(NAME test_fs_win_journal_fuzz_batch_seed_${wjb_seed} COMMAND test_fs_win_journal_fuzz_batch
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
        set_tests_properties(test_fs_win_journal_fuzz_batch_seed_${wjb_seed} PROPERTIES
            ENVIRONMENT "FS_WJFUZZ_SEED=${wjb_seed}" TIMEOUT 900)
    endforeach()
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
    # Two real processes on the workspace lock. The marker is printed only at zero failures.
    add_executable(test_agent_patch_contend_win tests/test_agent_patch_contend_win.c src/agent_patch.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_agent_patch_contend_win PRIVATE include)
    target_compile_definitions(test_agent_patch_contend_win PRIVATE FS_CREATE_TEST_CRASH=1 AGENT_PATCH_TEST_HOOK=1)
    add_test(NAME test_agent_patch_contend_win COMMAND test_agent_patch_contend_win
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_agent_patch_contend_win PROPERTIES PASS_REGULAR_EXPRESSION "agentpatch contention cells ran: 2")
    # Mutant 12 again: the drift guard is dropped. The same test must fail exactly one cell.
    add_executable(test_agent_patch_contend_win_mc tests/test_agent_patch_contend_win.c src/agent_patch.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_agent_patch_contend_win_mc PRIVATE include)
    target_compile_definitions(test_agent_patch_contend_win_mc PRIVATE FS_CREATE_TEST_CRASH=1 AGENT_PATCH_TEST_HOOK=1 AGENT_PATCH_APPLY_MUTANT=1)
    add_test(NAME test_agent_patch_contend_win_mc COMMAND test_agent_patch_contend_win_mc
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_agent_patch_contend_win_mc PROPERTIES PASS_REGULAR_EXPRESSION "mutant killed by 1 cell")
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
    # m194/m195: the test enables 8.3 names on its volume (restored at exit) and prints a table, one fresh fixture per cell.
    # The regex holds the m195 predictions (not yet measured): a wrong one goes red and prints the measured table.
    set_tests_properties(test_fs_win_aliases_short PROPERTIES
        PASS_REGULAR_EXPRESSION "fs win short cells: lock: create=R3 copy_target=R3 copy_source=R3 move_target=R3 move_source=R3 replace=R3 remove=R3 batch_create=R3 batch_replace=R3 ; ordinary_alias: create=3 copy_target=3 copy_source=0 move_target=3 move_source=0 replace=0 remove=0 batch_create=3 batch_replace=0 ; empty_long: copy_source=0 move_source=0 replace=0 remove=0 batch_replace=0")
    # m193: probe of 8.3 short names on the workspace volume. The regex holds my predictions: no short name before,
    # `fsutil 8dot3name set` returns 0 and a new file then has one, the setting is restored. A wrong one goes red and prints the line.
    add_executable(test_fs_win_8dot3_probe tests/test_fs_win_8dot3_probe.c)
    add_test(NAME test_fs_win_8dot3_probe COMMAND test_fs_win_8dot3_probe WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_fs_win_8dot3_probe PROPERTIES
        PASS_REGULAR_EXPRESSION "8dot3 probe: drive [A-Z] before short=no enable rc=0 after short=yes restore rc=0")
    # m197: probe of the per-directory case flag. The regex holds my predictions: a default directory cannot hold "x.txt" and
    # "X.txt", `fsutil file setCaseSensitiveInfo` returns 0, the query says enabled, and the flagged directory holds both.
    # A wrong one goes red and prints the measured line.
    add_executable(test_fs_win_case_probe tests/test_fs_win_case_probe.c)
    add_test(NAME test_fs_win_case_probe COMMAND test_fs_win_case_probe WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_fs_win_case_probe PROPERTIES
        PASS_REGULAR_EXPRESSION "case probe: default both=no set rc=0 query enabled=yes flagged both=yes")
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
    add_executable(test_fs_win_race_swap tests/test_fs_win_race_swap.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_win_race_swap PRIVATE include)
    add_test(NAME test_fs_win_race_swap COMMAND test_fs_win_race_swap
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_fs_win_race_swap PROPERTIES
        PASS_REGULAR_EXPRESSION "fs win race swap ok: parent swap invariant held")
    add_executable(test_fs_win_case tests/test_fs_win_case.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_win_case PRIVATE include)
    add_test(NAME test_fs_win_case COMMAND test_fs_win_case
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    # m198: the library on a case-sensitive directory (per-directory flag), with entries x.txt/X.txt, z.txt/Z.txt (equal bytes).
    # The regex is the line measured by m198 (msvc and asan): "d/X.txt" and "d/Z.txt" act on the exact-case entry only; create, copy
    # target and move target onto "d/Y.txt" next to y.txt succeed and create Y.txt (m198 predicted a refusal: wrong).
    add_executable(test_fs_win_case_flag tests/test_fs_win_case_flag.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_win_case_flag PRIVATE include)
    add_test(NAME test_fs_win_case_flag COMMAND test_fs_win_case_flag
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_fs_win_case_flag PROPERTIES
        PASS_REGULAR_EXPRESSION "case flag: read_xX=0U replace_xX=0on remove_xX=0o- copy_source_xX=0oo move_source_xX=0o- replace_zZ=0on remove_zZ=0o- copy_source_zZ=0oo move_source_zZ=0o- create_yY=0on copy_target_yY=0on move_target_yY=0on")
    # m200: the case flag on the workspace root. The regex holds my predictions: a foreign ".FSTXN.LOCK" does not stop the library
    # from taking its own empty ".fstxn.lock" and stays untouched; the reserved name in another case is refused by name for
    # create and remove; replace and remove of "A.txt" next to "a.txt" act on A.txt only. A wrong one goes red and prints the line.
    add_executable(test_fs_win_case_root tests/test_fs_win_case_root.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_win_case_root PRIVATE include)
    add_test(NAME test_fs_win_case_root COMMAND test_fs_win_case_root
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_fs_win_case_root PROPERTIES
        PASS_REGULAR_EXPRESSION "root flag: foreign_lock=0ooo reserved_create=3- reserved_remove=3o root_replace_aA=0on root_remove_aA=0o-")
    # m201: batch operations in a case-sensitive directory. The regex holds my predictions: a batch naming x.txt and X.txt is refused whole (status 1, nothing touched, the batch code folds names); a batch naming X.txt or Z.txt plus k.txt acts on the exact-case entry only; a create batch of n.txt and N.txt is refused; a create batch of Y.txt and k.txt next to y.txt succeeds. A wrong one goes red and prints the line.
    add_executable(test_fs_win_case_batch tests/test_fs_win_case_batch.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_win_case_batch PRIVATE include)
    add_test(NAME test_fs_win_case_batch COMMAND test_fs_win_case_batch
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_fs_win_case_batch PROPERTIES
        PASS_REGULAR_EXPRESSION "batch flag: rep_dup_xX=1ooo rep_X_xX=0onn rep_Z_zZ=0onn cre_dup_nN=1--- cre_Y_yY=0onn")
    # m204 note: m203 measured foreign_alias accepted (copy_source=0e move_source=0- replace=0n remove=0- batch_replace=0n, runs 37294782982); the regex below is now my prediction after the wc_open final-name check.
    # m203: why is the 8.3 alias of .fstxn.lock refused? Three groups of the five byte-checking ops on an empty target. The regex holds my predictions: a byte-range lock held on an ordinary empty file makes all five refuse (hypothesis H1); a foreign reserved-prefix file is refused by name through its long name; its 8.3 alias is accepted (50 percent; refused would mean a check that does not depend on the lock). A wrong one goes red and prints the line.
    add_executable(test_fs_win_lock_alias tests/test_fs_win_lock_alias.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_win_lock_alias PRIVATE include)
    add_test(NAME test_fs_win_lock_alias COMMAND test_fs_win_lock_alias
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_fs_win_lock_alias PROPERTIES
        PASS_REGULAR_EXPRESSION "lock alias cells: held_long: copy_source=3e move_source=3e replace=3e remove=3e batch_replace=3e ; foreign_long: copy_source=3e move_source=3e replace=3e remove=3e batch_replace=3e ; foreign_alias: copy_source=3e move_source=3e replace=3e remove=3e batch_replace=3e")
    # m207 note: m206 measured dir=3o0n (runs 37303780972); the regex now holds my prediction after the wc_parent segment check (dir=3o3o) and an over-refusal control (dirord=0n0n).
    # m206: the other eight reserved prefixes and a reserved directory as a path component, through the 8.3 name. The regex holds my predictions: every prefix refused through the long name and through the alias (m204 check), the file unchanged; the directory case refused by its long path and ACCEPTED through the alias of the directory (not covered by m204, 60 percent). A wrong one goes red and prints the line.
    add_executable(test_fs_win_reserved_alias tests/test_fs_win_reserved_alias.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_win_reserved_alias PRIVATE include)
    add_test(NAME test_fs_win_reserved_alias COMMAND test_fs_win_reserved_alias
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_fs_win_reserved_alias PROPERTIES
        PASS_REGULAR_EXPRESSION "reserved alias cells: .fstxn.=3e3e .fst-=3e3e .fsp-=3e3e .fsj-=3e3e .fsrp-=3e3e .fsrs-=3e3e .fsrb-=3e3e .fsrm-=3e3e .fsmv-=3e3e dir=3o3o dirord=0n0n")
    # m213: the orphan sweep against a FOREIGN file of the exact shape of a control name. The regex holds my predictions (.fst- .fsp- .fsrs- deleted, one-link .fsrp- and the four other prefixes kept, a two-link .fsrp- loses the name only); a wrong one goes red and prints the measured line.
    add_executable(test_fs_win_sweep_foreign tests/test_fs_win_sweep_foreign.c
        src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_win_sweep_foreign PRIVATE include)
    add_test(NAME test_fs_win_sweep_foreign COMMAND test_fs_win_sweep_foreign
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_fs_win_sweep_foreign PROPERTIES
        PASS_REGULAR_EXPRESSION "sweep foreign cells: batch: .fst-=- .fsp-=- .fsrp-=o .fsrs-=- .fsj-=o .fsrb-=o .fsrm-=o .fsmv-=o twolink=-o; replace: .fst-=- .fsp-=- .fsrp-=o .fsrs-=- .fsj-=o .fsrb-=o .fsrm-=o .fsmv-=o twolink=-o")
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
    # The line is printed only after every cell and mutant passed (m186); it asserts the orphan counts of the killed sweep (9 before, 8 after: 3 names per item, read on msvc and asan in the m188 run) and, since m211, the chained kills: the counts 9 8 7 6 5 4 3 3 are a PREDICTION pinned before the first run, not a measurement.
    set_tests_properties(test_fs_win_batch_replace PROPERTIES
        PASS_REGULAR_EXPRESSION "sweep chain: kills 6 counts 9 8 7 6 5 4 3 3; test_fs_win_batch_replace ok, sweep crash cells 3, orphans before/after mode1 9/8 mode2 9/8 mode0 9/8")
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
# Windows only: the default seed of the same fuzz at 400 iterations (its own default there is 150; the POSIX default is already 400).
if(WIN32)
    add_test(NAME test_fs_fuzz_ops_default_seed_i400 COMMAND test_fs_fuzz_ops
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_fs_fuzz_ops_default_seed_i400 PROPERTIES
        ENVIRONMENT "FS_FUZZ_ITERS=400${fz_req}" TIMEOUT 300)
endif()
# Windows write path with generated file contents (m218). The regex pins the last printed line.
if(WIN32)
    sllm_test(test_fs_win_content_fuzz tests/test_fs_win_content_fuzz.c)
    set_tests_properties(test_fs_win_content_fuzz PROPERTIES TIMEOUT 300
        PASS_REGULAR_EXPRESSION "content fuzz done: seeds=4 iterations=400 ops_ok=1200")
endif()
# AgentPatch on Windows with generated files and hunks (m220). The regex pins the last printed line.
if(WIN32)
    sllm_test(test_fs_win_agentpatch_fuzz tests/test_fs_win_agentpatch_fuzz.c)
    set_tests_properties(test_fs_win_agentpatch_fuzz PROPERTIES TIMEOUT 300
        PASS_REGULAR_EXPRESSION "agentpatch fuzz done: seeds=4 rounds=400 ok=400")
endif()
sllm_test(test_fs_c4_posix tests/test_fs_c4_posix.c)
sllm_test(test_fs_race tests/test_fs_race.c)
if(NOT WIN32)
    add_executable(test_fs_replace tests/test_fs_replace.c src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_replace PRIVATE include)
    target_compile_definitions(test_fs_replace PRIVATE FS_CREATE_TEST_CRASH=1)
    add_test(NAME test_fs_replace COMMAND test_fs_replace WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    # Journal fuzz of the replace, create, remove and move journals (M1 criterion 5, POSIX; not batch, not Windows). Seeds in the test names; sequential
    # (shared scratch directories). The two _mc targets drop one identity check in src/fs_read.c
    # (FS_JREC_MUTANT_STAGE and FS_JREC_MUTANT_TEMP) and pass only when exactly the predicted class fails;
    # _mc_tmp drops the removal of the .fstxn- temporary links in FsReplaceRecover (FS_JREC_MUTANT_TMP).
    # Seed 1786707969 is the test default (no FS_JFUZZ_SEED): iteration 7, kind move, bitflip at phase 22 first
    # showed the damaged-name limit F4; seeds 0, 1, 7 and 12648430 did not hit it, so the default is run as well.
    # Known limits F2 (remove) and F4 (move) are pinned cells in the test.
    # Integrity trailer (record + 8 bytes: magic, CRC-32). Plain damage is now refused; the pinned cells for F2, F4, F6 and
    # F7 run twice (plain: refused; CRC recomputed: the old outcome, a CRC is not authentication). The stage decoy classes
    # recompute the CRC on purpose, otherwise every stage mutant would be killed by the CRC instead of by the identity
    # check it drops. Legacy cells cut the trailer of every journal and require recovery OK. The tmp mutant now fails
    # replace legacy cells (bit 0); class setbyte (bit 1) is ignored because it only fails when a random setbyte writes
    # the value that is already there (the record is then intact), which is seed dependent.
    # Batch kinds (batch_create, batch_replace): no mutant, none was both cheap and killable. Known limits counted as
    # known-limit-hits and pinned as asserted cells: F5 (journal deleted while half published, delete class at create
    # crash 6 and replace crash 61 only), F6 (damaged create batch item name: half applied, recovery OK), F7 (damaged
    # replace batch stage name: orphan stage left). F5 and F6 do NOT meet "all targets or none".
    # No create mutant: the create stage identity check and the record temp link check cover each other for the
    # stage_decoy and temp_decoy classes (a rewritten stage field changes the derived temp link name), so a
    # create-stage and a create-temp mutant both SURVIVED on all 4 seeds; the check is not independently
    # exercised by the current classes.
    add_executable(test_fs_journal_fuzz tests/test_fs_journal_fuzz.c src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_journal_fuzz PRIVATE include)
    target_compile_definitions(test_fs_journal_fuzz PRIVATE FS_CREATE_TEST_CRASH=1)
    foreach(jf_seed 0 1 7 12648430 1786707969)
        add_test(NAME test_fs_journal_fuzz_seed_${jf_seed} COMMAND test_fs_journal_fuzz WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
        set_tests_properties(test_fs_journal_fuzz_seed_${jf_seed} PROPERTIES
            ENVIRONMENT "FS_JFUZZ_SEED=${jf_seed}" TIMEOUT 300
            PASS_REGULAR_EXPRESSION "journal fuzz iterations: 350 per kind, 6 kinds, seed ${jf_seed} refused [0-9]+ ok [0-9]+ known-limit-hits [0-9]+ failures 0 class mask 0")
    endforeach()
    # Damage to the 8 byte commit marker alone (remove, move, batch create, batch replace), m183: 89 damage cells and
    # 2 control cells per kind, every damaged marker refused, the controls recover OK. The printed line is matched so a pass is not hidden.
    add_executable(test_fs_journal_fuzz_marker tests/test_fs_journal_fuzz_marker.c src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_journal_fuzz_marker PRIVATE include)
    target_compile_definitions(test_fs_journal_fuzz_marker PRIVATE FS_CREATE_TEST_CRASH=1)
    foreach(jm_seed 1786707970 12648430 305419896)
        add_test(NAME test_fs_journal_fuzz_marker_seed_${jm_seed} COMMAND test_fs_journal_fuzz_marker WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
        set_tests_properties(test_fs_journal_fuzz_marker_seed_${jm_seed} PROPERTIES
            ENVIRONMENT "FS_JMARK_SEED=${jm_seed}" TIMEOUT 300
            PASS_REGULAR_EXPRESSION "journal marker fuzz: 364 cells, seed ${jm_seed}, marker phases per kind 1 1 1 1, control cells 8, failures 0")
    endforeach()
    add_executable(test_fs_journal_fuzz_mc_stage tests/test_fs_journal_fuzz.c src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_journal_fuzz_mc_stage PRIVATE include)
    target_compile_definitions(test_fs_journal_fuzz_mc_stage PRIVATE FS_CREATE_TEST_CRASH=1 FS_JREC_MUTANT_STAGE=1 FS_JFUZZ_MUTANT_MASK=0x40 FS_JFUZZ_MUTANT_IGNORE=0x3f)
    add_test(NAME test_fs_journal_fuzz_mc_stage COMMAND test_fs_journal_fuzz_mc_stage WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_fs_journal_fuzz_mc_stage PROPERTIES TIMEOUT 300
        PASS_REGULAR_EXPRESSION "mutant killed, failing class mask exactly 40")
    add_executable(test_fs_journal_fuzz_mc_temp tests/test_fs_journal_fuzz.c src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_journal_fuzz_mc_temp PRIVATE include)
    target_compile_definitions(test_fs_journal_fuzz_mc_temp PRIVATE FS_CREATE_TEST_CRASH=1 FS_JREC_MUTANT_TEMP=1 FS_JFUZZ_MUTANT_MASK=0x80 FS_JFUZZ_MUTANT_IGNORE=0x3f)
    add_test(NAME test_fs_journal_fuzz_mc_temp COMMAND test_fs_journal_fuzz_mc_temp WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_fs_journal_fuzz_mc_temp PROPERTIES TIMEOUT 300
        PASS_REGULAR_EXPRESSION "mutant killed, failing class mask exactly 80")
    add_executable(test_fs_journal_fuzz_mc_tmp tests/test_fs_journal_fuzz.c src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_journal_fuzz_mc_tmp PRIVATE include)
    target_compile_definitions(test_fs_journal_fuzz_mc_tmp PRIVATE FS_CREATE_TEST_CRASH=1 FS_JREC_MUTANT_TMP=1 FS_JFUZZ_MUTANT_MASK=0x1 FS_JFUZZ_MUTANT_IGNORE=0x2)
    add_test(NAME test_fs_journal_fuzz_mc_tmp COMMAND test_fs_journal_fuzz_mc_tmp WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_fs_journal_fuzz_mc_tmp PROPERTIES TIMEOUT 300
        PASS_REGULAR_EXPRESSION "mutant killed, failing class mask exactly 1")
    # remove and move: dropping the stage identity check (FS_JREC_MUTANT_RSTAGE, FS_JREC_MUTANT_MSTAGE) must
    # fail exactly the stage_decoy class of that kind (the other classes of the kind are ignored: seed dependent).
    add_executable(test_fs_journal_fuzz_mc_rstage tests/test_fs_journal_fuzz.c src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_journal_fuzz_mc_rstage PRIVATE include)
    target_compile_definitions(test_fs_journal_fuzz_mc_rstage PRIVATE FS_CREATE_TEST_CRASH=1 FS_JREC_MUTANT_RSTAGE=1 FS_JFUZZ_MUTANT_MASK=0x400000 FS_JFUZZ_MUTANT_IGNORE=0x3f0000)
    add_test(NAME test_fs_journal_fuzz_mc_rstage COMMAND test_fs_journal_fuzz_mc_rstage WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_fs_journal_fuzz_mc_rstage PROPERTIES TIMEOUT 300
        PASS_REGULAR_EXPRESSION "mutant killed, failing class mask exactly 400000")
    add_executable(test_fs_journal_fuzz_mc_mstage tests/test_fs_journal_fuzz.c src/fs_read.c src/fs_manifest.c)
    target_include_directories(test_fs_journal_fuzz_mc_mstage PRIVATE include)
    target_compile_definitions(test_fs_journal_fuzz_mc_mstage PRIVATE FS_CREATE_TEST_CRASH=1 FS_JREC_MUTANT_MSTAGE=1 FS_JFUZZ_MUTANT_MASK=0x40000000 FS_JFUZZ_MUTANT_IGNORE=0x3f000000)
    add_test(NAME test_fs_journal_fuzz_mc_mstage COMMAND test_fs_journal_fuzz_mc_mstage WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_fs_journal_fuzz_mc_mstage PROPERTIES TIMEOUT 300
        PASS_REGULAR_EXPRESSION "mutant killed, failing class mask exactly 40000000")
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
    # Wording-robustness probe (m229): the declared rewrites, and the harness accepts --rewrite (noop agent: nothing passes).
    add_test(NAME test_wording_rewrites
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/wording_rewrites.py --self-test
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_wording_rewrites PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "wording_rewrites self-test ok")
    add_test(NAME test_wording_rewrite_harness
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/bank_harness.py --agent noop --rewrite R3 --counts-only
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_wording_rewrite_harness PROPERTIES TIMEOUT 120 PASS_REGULAR_EXPRESSION "\"tasks_passed\": 0")
    add_test(NAME test_build_graph_repo
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_build_graph_repo.py ${CMAKE_BINARY_DIR}
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_build_graph_repo PROPERTIES TIMEOUT 120 SKIP_RETURN_CODE 77 RESOURCE_LOCK build_graph_repo_reply)
    add_test(NAME test_build_graph_repo_mc_drop
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_build_graph_repo.py ${CMAKE_BINARY_DIR} --mutant drop_decl
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_build_graph_repo_mc_drop PROPERTIES TIMEOUT 120 SKIP_RETURN_CODE 77 RESOURCE_LOCK build_graph_repo_reply)
    add_test(NAME test_build_graph_repo_mc_glob
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_build_graph_repo.py ${CMAKE_BINARY_DIR} --mutant stale_glob
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_build_graph_repo_mc_glob PROPERTIES TIMEOUT 120 SKIP_RETURN_CODE 77 RESOURCE_LOCK build_graph_repo_reply)
    add_test(NAME test_select_report
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_select_report.py $<TARGET_FILE:symbols-agent> ${CMAKE_BINARY_DIR}
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_select_report PROPERTIES TIMEOUT 120 SKIP_RETURN_CODE 77 RESOURCE_LOCK build_graph_repo_reply)
    add_test(NAME test_select_hook
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_select_hook.py $<TARGET_FILE:symbols-agent>
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_select_hook PROPERTIES TIMEOUT 120 SKIP_RETURN_CODE 77)
    add_test(NAME test_ctest_report
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_ctest_report.py ${CMAKE_COMMAND} ${CMAKE_CTEST_COMMAND}
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_ctest_report PROPERTIES TIMEOUT 300 SKIP_RETURN_CODE 77)
    add_test(NAME test_build_graph
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_build_graph.py
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_build_graph PROPERTIES TIMEOUT 300 SKIP_RETURN_CODE 77)
    add_test(NAME test_build_graph_mc
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_build_graph.py --mutant no_closure
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_build_graph_mc PROPERTIES TIMEOUT 300 SKIP_RETURN_CODE 77)
    add_test(NAME test_build_graph_mc_scan
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_build_graph.py --mutant scan_off
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_build_graph_mc_scan PROPERTIES TIMEOUT 300 SKIP_RETURN_CODE 77)
    add_test(NAME test_build_graph_mc_sidecar
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_build_graph.py --mutant sidecar_off
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_build_graph_mc_sidecar PROPERTIES TIMEOUT 300 SKIP_RETURN_CODE 77)
    add_test(NAME test_build_graph_mc_decl
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_build_graph.py --mutant decl_targets_off
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_build_graph_mc_decl PROPERTIES TIMEOUT 300 SKIP_RETURN_CODE 77)
    add_test(NAME test_build_graph_mc_always
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_build_graph.py --mutant always_off
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_build_graph_mc_always PROPERTIES TIMEOUT 300 SKIP_RETURN_CODE 77)
    add_test(NAME test_build_graph_dynamic
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_build_graph_dynamic.py
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_build_graph_dynamic PROPERTIES TIMEOUT 120 SKIP_RETURN_CODE 77)
    add_test(NAME test_build_graph_dynamic_mc
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_build_graph_dynamic.py --mutant
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_build_graph_dynamic_mc PROPERTIES TIMEOUT 120 SKIP_RETURN_CODE 77)
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
# Refactor guards (m230): rename_symbol and literal_to_constant need a build probe, a rename may not use a build-file word,
# a literal sentence may not be about a preprocessor directive. The mutant (isolated test library) switches the guards off and
# must fail exactly cells A B C.
sllm_test(test_task_ops_refactor_guard tests/test_task_ops_refactor_guard.c)
set_tests_properties(test_task_ops_refactor_guard PROPERTIES PASS_REGULAR_EXPRESSION "ALL PASS \\(0 cells wrong\\)")
add_executable(test_task_ops_refactor_guard_mc tests/test_task_ops_refactor_guard.c)
target_compile_definitions(test_task_ops_refactor_guard_mc PRIVATE REFACTOR_GUARD_MUTANT=1 TASK_OPS_TEST_FAULTS)
target_link_libraries(test_task_ops_refactor_guard_mc symbolic_rollback_test)
add_test(NAME test_task_ops_refactor_guard_mc COMMAND test_task_ops_refactor_guard_mc WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_task_ops_refactor_guard_mc PROPERTIES PASS_REGULAR_EXPRESSION "MUTANT killed by exactly A B C")

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
# A NUL byte in the target is refused (m221); the mutant build removes the check and must fail exactly cells A B D F G.
add_executable(test_agent_patch_nul tests/test_agent_patch_nul.c src/agent_patch.c
    src/fs_read.c src/fs_manifest.c)
target_include_directories(test_agent_patch_nul PRIVATE include)
add_test(NAME test_agent_patch_nul COMMAND test_agent_patch_nul WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_agent_patch_nul PROPERTIES PASS_REGULAR_EXPRESSION "ALL PASS \\(0 cells wrong\\)")
add_executable(test_agent_patch_nul_mc tests/test_agent_patch_nul.c src/agent_patch.c
    src/fs_read.c src/fs_manifest.c)
target_include_directories(test_agent_patch_nul_mc PRIVATE include)
target_compile_definitions(test_agent_patch_nul_mc PRIVATE AGENT_PATCH_NUL_MUTANT=1)
add_test(NAME test_agent_patch_nul_mc COMMAND test_agent_patch_nul_mc WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_agent_patch_nul_mc PROPERTIES PASS_REGULAR_EXPRESSION "MUTANT killed by exactly A B D F G")
# Multi-hunk plans (m224): verify and apply share one routine, each hunk must occur exactly once at its own step;
# the mutant lets later hunks take the first match (the old behaviour) and must fail exactly cells C and D.
add_executable(test_agent_patch_multi tests/test_agent_patch_multi.c src/agent_patch.c
    src/fs_read.c src/fs_manifest.c)
target_include_directories(test_agent_patch_multi PRIVATE include)
add_test(NAME test_agent_patch_multi COMMAND test_agent_patch_multi WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_agent_patch_multi PROPERTIES PASS_REGULAR_EXPRESSION "ALL PASS \\(0 cells wrong\\)")
add_executable(test_agent_patch_multi_mc tests/test_agent_patch_multi.c src/agent_patch.c
    src/fs_read.c src/fs_manifest.c)
target_include_directories(test_agent_patch_multi_mc PRIVATE include)
target_compile_definitions(test_agent_patch_multi_mc PRIVATE AGENT_PATCH_UNIQ_MUTANT=1)
add_test(NAME test_agent_patch_multi_mc COMMAND test_agent_patch_multi_mc WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_agent_patch_multi_mc PROPERTIES PASS_REGULAR_EXPRESSION "MUTANT killed by exactly C D")
# Multi-hunk and multi-line AgentPatch fuzz (m225): 4 seeds x 100 rounds against a line model; refused plans leave the file unchanged.
add_executable(test_agent_patch_multi_fuzz tests/test_agent_patch_multi_fuzz.c src/agent_patch.c
    src/fs_read.c src/fs_manifest.c)
target_include_directories(test_agent_patch_multi_fuzz PRIVATE include)
add_test(NAME test_agent_patch_multi_fuzz COMMAND test_agent_patch_multi_fuzz WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_agent_patch_multi_fuzz PROPERTIES TIMEOUT 300
    PASS_REGULAR_EXPRESSION "multi fuzz done: seeds=4 rounds=400 ok=400")
# AgentPatch context-string and expected_line fuzz: repeated lines, 0 to 2 context lines each side, count-based oracle.
add_executable(test_agent_patch_ctx_fuzz tests/test_agent_patch_ctx_fuzz.c src/agent_patch.c
    src/fs_read.c src/fs_manifest.c)
target_include_directories(test_agent_patch_ctx_fuzz PRIVATE include)
add_test(NAME test_agent_patch_ctx_fuzz COMMAND test_agent_patch_ctx_fuzz WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_agent_patch_ctx_fuzz PROPERTIES TIMEOUT 300
    PASS_REGULAR_EXPRESSION "ctx fuzz done: seeds=4 rounds=400 ok=400")
# Overlapping matches (m227): the occurrence count advances one byte per match; the mutant restores the old advance and must fail exactly cells A B C F.
add_executable(test_agent_patch_overlap tests/test_agent_patch_overlap.c src/agent_patch.c
    src/fs_read.c src/fs_manifest.c)
target_include_directories(test_agent_patch_overlap PRIVATE include)
add_test(NAME test_agent_patch_overlap COMMAND test_agent_patch_overlap WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_agent_patch_overlap PROPERTIES PASS_REGULAR_EXPRESSION "ALL PASS \\(0 cells wrong\\)")
add_executable(test_agent_patch_overlap_mc tests/test_agent_patch_overlap.c src/agent_patch.c
    src/fs_read.c src/fs_manifest.c)
target_include_directories(test_agent_patch_overlap_mc PRIVATE include)
target_compile_definitions(test_agent_patch_overlap_mc PRIVATE AGENT_PATCH_OVERLAP_MUTANT=1)
add_test(NAME test_agent_patch_overlap_mc COMMAND test_agent_patch_overlap_mc WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_agent_patch_overlap_mc PROPERTIES PASS_REGULAR_EXPRESSION "MUTANT killed by exactly A B C F")
# AgentPatch fuzz of multi-hunk plans with context strings and files without a final newline (m228); substring-count oracle, overlaps counted.
add_executable(test_agent_patch_ctxm_fuzz tests/test_agent_patch_ctxm_fuzz.c src/agent_patch.c
    src/fs_read.c src/fs_manifest.c)
target_include_directories(test_agent_patch_ctxm_fuzz PRIVATE include)
add_test(NAME test_agent_patch_ctxm_fuzz COMMAND test_agent_patch_ctxm_fuzz WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_agent_patch_ctxm_fuzz PROPERTIES TIMEOUT 300
    PASS_REGULAR_EXPRESSION "ctxm fuzz done: seeds=4 rounds=400 ok=400")
