/* ============================================================
   test_server_tool_calling.c: Unit test suite for OpenAI Tool Calling
                               Wire Protocol in server_proto.
   Pure ISO C11, zero tensors, zero backprop, fail-closed verification.
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "server_proto.h"

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define TEST_ASSERT(expr, msg) do { \
    g_tests_run++; \
    if (expr) { \
        g_tests_passed++; \
        printf("  [PASS] %s\n", msg); \
    } else { \
        printf("  [FAIL] %s (line %d)\n", msg, __LINE__); \
    } \
} while(0)

/* 1. Test Tools Declared Extraction */
static void test_tools_declared_extraction(void)
{
    printf("\n=== Test 1: Tools Declared Extraction ===\n");
    const char *payload = 
        "{\"model\":\"symbols\",\"messages\":[{\"role\":\"user\",\"content\":\"Fix main.c\"}],"
        "\"tools\":["
        "{\"type\":\"function\",\"function\":{\"name\":\"apply_patch\",\"description\":\"Apply diff\"}},"
        "{\"type\":\"function\",\"function\":{\"name\":\"execute_command\",\"description\":\"Run bash\"}},"
        "{\"type\":\"function\",\"function\":{\"name\":\"inspect_code\",\"description\":\"Read file\"}}"
        "]}";

    char tool_names[8][64];
    int count = ServerExtractToolsDeclared(payload, tool_names, 8);
    TEST_ASSERT(count == 3, "Extracted exactly 3 declared tools");
    TEST_ASSERT(strcmp(tool_names[0], "apply_patch") == 0, "Tool 0 is apply_patch");
    TEST_ASSERT(strcmp(tool_names[1], "execute_command") == 0, "Tool 1 is execute_command");
    TEST_ASSERT(strcmp(tool_names[2], "inspect_code") == 0, "Tool 2 is inspect_code");
}

/* 2. Test Tool Response Extraction */
static void test_tool_response_extraction(void)
{
    printf("\n=== Test 2: Tool Response Extraction ===\n");
    const char *payload = 
        "{\"model\":\"symbols\",\"messages\":["
        "{\"role\":\"user\",\"content\":\"Fix main.c\"},"
        "{\"role\":\"assistant\",\"tool_calls\":[{\"id\":\"call_001\",\"type\":\"function\","
        "\"function\":{\"name\":\"apply_patch\",\"arguments\":\"{\\\"file\\\":\\\"main.c\\\"}\"}}]},"
        "{\"role\":\"tool\",\"tool_call_id\":\"call_001\",\"name\":\"apply_patch\",\"content\":\"{\\\"status\\\":\\\"applied\\\",\\\"hunks\\\":1}\"}"
        "]}";

    OPENAI_TOOL_RESPONSE resp;
    int ok = ServerExtractLastToolResponse(payload, &resp);
    TEST_ASSERT(ok == 1, "Successfully extracted tool response");
    TEST_ASSERT(resp.has_response == 1, "Response flag is set");
    TEST_ASSERT(strcmp(resp.tool_call_id, "call_001") == 0, "tool_call_id is call_001");
    TEST_ASSERT(strcmp(resp.name, "apply_patch") == 0, "tool name is apply_patch");
    TEST_ASSERT(strstr(resp.content, "applied") != NULL, "content contains 'applied'");
}

/* 3. Test Tool Call Response Building */
static void test_tool_call_response_building(void)
{
    printf("\n=== Test 3: Tool Call Response Building ===\n");
    OPENAI_TOOL_CALLS tc;
    memset(&tc, 0, sizeof(tc));
    tc.count = 2;

    strncpy(tc.calls[0].id, "call_abc123", sizeof(tc.calls[0].id) - 1);
    strncpy(tc.calls[0].name, "locate_symbol", sizeof(tc.calls[0].name) - 1);
    strncpy(tc.calls[0].arguments, "{\"symbol\":\"CalculateTotal\"}", sizeof(tc.calls[0].arguments) - 1);

    strncpy(tc.calls[1].id, "call_abc124", sizeof(tc.calls[1].id) - 1);
    strncpy(tc.calls[1].name, "inspect_code", sizeof(tc.calls[1].name) - 1);
    strncpy(tc.calls[1].arguments, "{\"file\":\"src/calc.c\",\"start_line\":1,\"end_line\":50}", sizeof(tc.calls[1].arguments) - 1);

    char out[8192];
    int ok = ServerBuildToolCallResponse("symbols", 1742410000L, 42UL, &tc,
                                         "I need to locate the symbol and inspect its definition.",
                                         out, sizeof(out));

    TEST_ASSERT(ok == 1, "ServerBuildToolCallResponse succeeds");
    TEST_ASSERT(strstr(out, "\"finish_reason\":\"tool_calls\"") != NULL, "finish_reason is tool_calls");
    TEST_ASSERT(strstr(out, "\"name\":\"locate_symbol\"") != NULL, "Includes first tool locate_symbol");
    TEST_ASSERT(strstr(out, "\"name\":\"inspect_code\"") != NULL, "Includes second tool inspect_code");
    TEST_ASSERT(strstr(out, "call_abc123") != NULL, "Includes id call_abc123");
    TEST_ASSERT(strstr(out, "call_abc124") != NULL, "Includes id call_abc124");
    TEST_ASSERT(strstr(out, "\"role\":\"assistant\"") != NULL, "Role is assistant");
    TEST_ASSERT(strstr(out, "I need to locate the symbol") != NULL, "Includes thought content");
}

/* 4. Test Tool Call Streaming Response */
static void test_tool_call_streaming_response(void)
{
    printf("\n=== Test 4: Tool Call Streaming Response (SSE) ===\n");
    OPENAI_TOOL_CALLS tc;
    memset(&tc, 0, sizeof(tc));
    tc.count = 1;

    strncpy(tc.calls[0].id, "call_stream_1", sizeof(tc.calls[0].id) - 1);
    strncpy(tc.calls[0].name, "execute_command", sizeof(tc.calls[0].name) - 1);
    strncpy(tc.calls[0].arguments, "{\"command\":\"gcc -Wall main.c -o main\"}", sizeof(tc.calls[0].arguments) - 1);

    char out[8192];
    int ok = ServerBuildToolCallStreamResponse("symbols", 1742410000L, 99UL, &tc, out, sizeof(out));

    TEST_ASSERT(ok == 1, "ServerBuildToolCallStreamResponse succeeds");
    TEST_ASSERT(strstr(out, "data: ") != NULL, "Contains SSE data: prefix");
    TEST_ASSERT(strstr(out, "\"delta\":{\"role\":\"assistant\"") != NULL, "Contains assistant delta");
    TEST_ASSERT(strstr(out, "execute_command") != NULL, "Contains function name execute_command");
    TEST_ASSERT(strstr(out, "\"finish_reason\":\"tool_calls\"") != NULL, "Contains finish_reason: tool_calls chunk");
    TEST_ASSERT(strstr(out, "data: [DONE]") != NULL, "Ends with data: [DONE]");
}

/* 5. Test Coding Task Intent Classification */
static void test_coding_task_intent(void)
{
    printf("\n=== Test 5: Coding Task Intent Classification ===\n");
    TEST_ASSERT(ServerIsCodingTask("Fix the memory leak in parser.c") == 1, "Detects 'parser.c'");
    TEST_ASSERT(ServerIsCodingTask("Refactor the login function") == 1, "Detects 'refactor' & 'function'");
    TEST_ASSERT(ServerIsCodingTask("Why does gcc fail with exit code 1?") == 1, "Detects 'gcc' & 'fail'");
    TEST_ASSERT(ServerIsCodingTask("Run tests and build the project") == 1, "Detects 'tests' & 'build'");
    TEST_ASSERT(ServerIsCodingTask("apply this patch to main.h") == 1, "Detects 'patch' & 'main.h'");

    /* Spanish and English repository/inspection tasks */
    TEST_ASSERT(ServerIsCodingTask("revisa esta carpeta") == 1, "Detects 'revisa' & 'carpeta'");
    TEST_ASSERT(ServerIsInspectionTask("revisa esta carpeta") == 1, "Classifies 'revisa esta carpeta' as inspection");
    TEST_ASSERT(ServerIsCodingTask("review this folder") == 1, "Detects 'review' & 'folder'");
    TEST_ASSERT(ServerIsInspectionTask("review this folder") == 1, "Classifies 'review this folder' as inspection");
    TEST_ASSERT(ServerIsCodingTask("inspecciona el directorio src") == 1, "Detects 'inspecciona' & 'directorio'");
    TEST_ASSERT(ServerIsInspectionTask("inspecciona el directorio src") == 1, "Classifies 'inspecciona el directorio src' as inspection");
    TEST_ASSERT(ServerIsInspectionTask("Fix the memory leak in parser.c") == 0, "Fix task is not read-only inspection");

    /* Shell and wildcard inspection tasks */
    TEST_ASSERT(ServerIsCodingTask("dir *.*") == 1, "Detects 'dir *.*'");
    TEST_ASSERT(ServerIsInspectionTask("dir *.*") == 1, "Classifies 'dir *.*' as inspection");
    TEST_ASSERT(ServerIsCodingTask("dir") == 1, "Detects 'dir'");
    TEST_ASSERT(ServerIsInspectionTask("dir") == 1, "Classifies 'dir' as inspection");
    TEST_ASSERT(ServerIsCodingTask("ls -la") == 1, "Detects 'ls -la'");
    TEST_ASSERT(ServerIsInspectionTask("ls -la") == 1, "Classifies 'ls -la' as inspection");
    TEST_ASSERT(ServerIsCodingTask("*.c") == 1, "Detects '*.c'");
    TEST_ASSERT(ServerIsInspectionTask("*.c") == 1, "Classifies '*.c' as inspection");

    /* Factual questions should NOT be detected as coding tasks */
    TEST_ASSERT(ServerIsCodingTask("Who is the father of Solomon?") == 0, "Factual query is not coding task");
    TEST_ASSERT(ServerIsCodingTask("Tell me about wisdom and proverbs") == 0, "Topical query is not coding task");
    TEST_ASSERT(ServerIsCodingTask("What areas do you know?") == 0, "Introspection query is not coding task");
    TEST_ASSERT(ServerIsInspectionTask("Who is the father of Solomon?") == 0, "Factual query is not inspection");
}

/* 6. Test Tool Error and Diagnostic Validation */
static void test_tool_error_validation(void)
{
    printf("\n=== Test 6: Tool Error and Exit Code Validation ===\n");
    
    /* 6a: Tool returns non-zero exit code in JSON */
    const char *err_payload = 
        "{\"model\":\"symbols\",\"messages\":["
        "{\"role\":\"tool\",\"tool_call_id\":\"call_002\",\"name\":\"execute_command\","
        "\"content\":\"{\\\"exit_code\\\":1,\\\"stderr\\\":\\\"Build failed\\\"}\"}"
        "]}";
    OPENAI_TOOL_RESPONSE resp1;
    int ok1 = ServerExtractLastToolResponse(err_payload, &resp1);
    TEST_ASSERT(ok1 == 1, "Extracted tool response with exit code 1");
    TEST_ASSERT(resp1.has_exit_code == 1, "Detected has_exit_code flag");
    TEST_ASSERT(resp1.exit_code == 1, "Extracted exit_code == 1");
    TEST_ASSERT(resp1.is_error == 1, "Flagged is_error == 1 due to exit_code != 0");

    /* 6b: Tool returns compiler error diagnostics */
    const char *diag_payload = 
        "{\"model\":\"symbols\",\"messages\":["
        "{\"role\":\"tool\",\"tool_call_id\":\"call_003\",\"name\":\"execute_command\","
        "\"content\":\"src/main.c:42:10: error: 'undefined_var' undeclared\\nmake: *** Error 1\"}"
        "]}";
    OPENAI_TOOL_RESPONSE resp2;
    int ok2 = ServerExtractLastToolResponse(diag_payload, &resp2);
    TEST_ASSERT(ok2 == 1, "Extracted compiler error diagnostic");
    TEST_ASSERT(resp2.is_error == 1, "Flagged is_error == 1 due to compiler diagnostic");

    /* 6c: Tool returns clean success */
    const char *ok_payload = 
        "{\"model\":\"symbols\",\"messages\":["
        "{\"role\":\"tool\",\"tool_call_id\":\"call_004\",\"name\":\"execute_command\","
        "\"content\":\"{\\\"exit_code\\\":0,\\\"status\\\":\\\"ok\\\"}\"}"
        "]}";
    OPENAI_TOOL_RESPONSE resp3;
    int ok3 = ServerExtractLastToolResponse(ok_payload, &resp3);
    TEST_ASSERT(ok3 == 1, "Extracted success response");
    TEST_ASSERT(resp3.has_exit_code == 1, "Detected has_exit_code == 1");
    TEST_ASSERT(resp3.exit_code == 0, "Extracted exit_code == 0");
    TEST_ASSERT(resp3.is_error == 0, "is_error == 0 for clean exit");
}

/* 7. Test Binary Model Detection in Chat Layer */
static void test_binary_model_support(void)
{
    printf("\n=== Test 7: Binary Model Detection in Chat Layer ===\n");
    TEST_ASSERT(ChatIsBinaryModel("wiki_model.bin") == 1, "Detects 'wiki_model.bin' as binary model");
    TEST_ASSERT(ChatIsBinaryModel("data/texts/bible.txt") == 0, "'bible.txt' is not binary model");
    TEST_ASSERT(ChatIsBinaryModel("data/corpus.tsv") == 0, "'corpus.tsv' is not binary model");
    TEST_ASSERT(ChatIsBinaryModel(NULL) == 0, "NULL path safely returns 0");
}

/* 8. Test Last Role Extraction for Conversation Turns */
static void test_last_role_extraction(void)
{
    printf("\n=== Test 8: Last Role Extraction ===\n");
    const char *payload_user = 
        "{\"model\":\"symbols\",\"messages\":["
        "{\"role\":\"user\",\"content\":\"Fix main.c\"},"
        "{\"role\":\"assistant\",\"tool_calls\":[{\"id\":\"call_1\",\"type\":\"function\",\"function\":{\"name\":\"read\"}}]},"
        "{\"role\":\"tool\",\"content\":\"code\"},"
        "{\"role\":\"user\",\"content\":\"¿Qué pasa si se calienta el hielo?\"}"
        "]}";
    char role[32];
    int ok = ServerExtractLastRole(payload_user, role, sizeof(role));
    TEST_ASSERT(ok == 1, "Extracted last role from multi-turn history");
    TEST_ASSERT(strcmp(role, "user") == 0, "Last role is 'user' despite previous tool message");

    const char *payload_tool = 
        "{\"model\":\"symbols\",\"messages\":["
        "{\"role\":\"user\",\"content\":\"Fix main.c\"},"
        "{\"role\":\"assistant\",\"tool_calls\":[{\"id\":\"call_1\",\"type\":\"function\",\"function\":{\"name\":\"read\"}}]},"
        "{\"role\":\"tool\",\"content\":\"file contents\"}"
        "]}";
    ok = ServerExtractLastRole(payload_tool, role, sizeof(role));
    TEST_ASSERT(ok == 1, "Extracted last role from tool resumption");
    TEST_ASSERT(strcmp(role, "tool") == 0, "Last role is 'tool' for active tool response");
}

/* 9. Comprehensive Intent Discrimination Battery */
static void test_discrimination_battery(void)
{
    printf("\n=== Test 9: Comprehensive Intent Discrimination Battery ===\n");

    /* Group 1: Shell & Wildcard Exploration Commands (Coding=1, Inspection=1) */
    const char *shell_cmds[] = {
        "dir", "dir *.*", "dir /w", "dir *.h", "ls", "ls -l", "ls -la", "ls *.py",
        "pwd", "tree", "*.*", "*.c", "*.h", "*.py", "*.json", "*.md"
    };
    for (size_t i = 0; i < sizeof(shell_cmds) / sizeof(shell_cmds[0]); i++)
    {
        TEST_ASSERT(ServerIsCodingTask(shell_cmds[i]) == 1, "Shell/wildcard detected as coding task");
        TEST_ASSERT(ServerIsInspectionTask(shell_cmds[i]) == 1, "Shell/wildcard detected as inspection task");
    }

    /* Group 2: Repository and Workspace Inspection (Coding=1, Inspection=1) */
    const char *workspace_queries[] = {
        "revisa esta carpeta", "revisar el directorio actual", "inspecciona este proyecto",
        "inspeccionar archivos", "archivos del workspace", "qué archivos hay",
        "que es lo que hay en esta carpeta ?", "lista los ficheros", "explora el repositorio",
        "review this directory", "inspect the codebase", "list repository files",
        "show files in workspace", "explore the project structure"
    };
    for (size_t i = 0; i < sizeof(workspace_queries) / sizeof(workspace_queries[0]); i++)
    {
        TEST_ASSERT(ServerIsCodingTask(workspace_queries[i]) == 1, "Workspace query detected as coding task");
        TEST_ASSERT(ServerIsInspectionTask(workspace_queries[i]) == 1, "Workspace query detected as inspection task");
    }

    /* Group 3: Coding Mutation, Build, Test, and Repair Tasks (Coding=1, Inspection=0) */
    const char *mutation_tasks[] = {
        "fix the memory leak in parser.c", "arregla el fallo de segmentacion",
        "corrige la funcion login", "refactor the database layer",
        "apply this patch to server.h", "aplica el parche en main.c",
        "run tests and build", "compila el proyecto con cmake",
        "cmake --build .", "ctest --output-on-failure",
        "gcc -Wall -Wextra main.c", "git commit -m fix"
    };
    for (size_t i = 0; i < sizeof(mutation_tasks) / sizeof(mutation_tasks[0]); i++)
    {
        TEST_ASSERT(ServerIsCodingTask(mutation_tasks[i]) == 1, "Mutation/build detected as coding task");
        TEST_ASSERT(ServerIsInspectionTask(mutation_tasks[i]) == 0, "Mutation/build is NOT read-only inspection");
    }

    /* Group 4: Factual and Domain Knowledge Questions (Coding=0, Inspection=0) */
    const char *factual_queries[] = {
        "Who is the father of Solomon?", "¿Quién es el padre de Salomón?",
        "Tell me about wisdom and proverbs", "Háblame de la sabiduría",
        "Where was Jonah sent?", "¿A dónde fue enviado Jonás?",
        "What is the archetype of the shadow?", "¿Qué es el inconsciente colectivo?",
        "What areas do you know?", "¿Qué áreas de conocimiento tienes?"
    };
    for (size_t i = 0; i < sizeof(factual_queries) / sizeof(factual_queries[0]); i++)
    {
        TEST_ASSERT(ServerIsCodingTask(factual_queries[i]) == 0, "Factual query is NOT coding task");
        TEST_ASSERT(ServerIsInspectionTask(factual_queries[i]) == 0, "Factual query is NOT inspection task");
    }

    /* Group 5: Physical Causality and Affordance Questions (Coding=0, Inspection=0) */
    const char *causality_queries[] = {
        "¿Qué pasa si se cae un vaso de cristal al suelo?", "What happens if a glass falls to the floor?",
        "¿Qué pasa si se calienta el hielo?", "What happens if you heat ice?",
        "¿Para qué sirve un martillo?", "What is a hammer used for?",
        "¿Qué pasa si se suelta una piedra en el aire?"
    };
    for (size_t i = 0; i < sizeof(causality_queries) / sizeof(causality_queries[0]); i++)
    {
        TEST_ASSERT(ServerIsCodingTask(causality_queries[i]) == 0, "Causality query is NOT coding task");
        TEST_ASSERT(ServerIsInspectionTask(causality_queries[i]) == 0, "Causality query is NOT inspection task");
    }
}

int main(void)
{
    printf("======================================================================\n");
    printf("  TEST SUITE: OPENAI TOOL CALLING WIRE PROTOCOL (OPENCODE INTEGRATION)\n");
    printf("======================================================================\n");

    test_tools_declared_extraction();
    test_tool_response_extraction();
    test_tool_call_response_building();
    test_tool_call_streaming_response();
    test_coding_task_intent();
    test_tool_error_validation();
    test_binary_model_support();
    test_last_role_extraction();
    test_discrimination_battery();

    printf("\n======================================================================\n");
    printf("  TEST RESULTS: %d passed, %d failed\n", g_tests_passed, g_tests_run - g_tests_passed);
    printf("======================================================================\n");

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}

