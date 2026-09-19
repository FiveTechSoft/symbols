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

    /* Factual questions should NOT be detected as coding tasks */
    TEST_ASSERT(ServerIsCodingTask("Who is the father of Solomon?") == 0, "Factual query is not coding task");
    TEST_ASSERT(ServerIsCodingTask("Tell me about wisdom and proverbs") == 0, "Topical query is not coding task");
    TEST_ASSERT(ServerIsCodingTask("What areas do you know?") == 0, "Introspection query is not coding task");
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

    printf("\n======================================================================\n");
    printf("  TEST RESULTS: %d passed, %d failed\n", g_tests_passed, g_tests_run - g_tests_passed);
    printf("======================================================================\n");

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
