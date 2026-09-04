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
)

target_include_directories(symbolic PUBLIC include)

add_executable(sllm_demo src/main.c)
target_link_libraries(sllm_demo symbolic)

enable_testing()

add_executable(test_symbol tests/test_symbol.c)
target_link_libraries(test_symbol symbolic)
add_test(NAME test_symbol COMMAND test_symbol)

add_executable(test_tokenizer tests/test_tokenizer.c)
target_link_libraries(test_tokenizer symbolic)
add_test(NAME test_tokenizer COMMAND test_tokenizer)

add_executable(test_graph tests/test_graph.c)
target_link_libraries(test_graph symbolic)
add_test(NAME test_graph COMMAND test_graph)

add_executable(test_learning tests/test_learning.c)
target_link_libraries(test_learning symbolic)
add_test(NAME test_learning COMMAND test_learning)
