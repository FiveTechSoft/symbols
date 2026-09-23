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


    /* Authentic OpenCode 1.18.32 continuation omits name on role=tool. */
    const char *oc_cont =
        "{\"messages\":["
        "{\"role\":\"user\",\"content\":\"uname -a\"},"
        "{\"role\":\"assistant\",\"content\":\"\",\"tool_calls\":[{"
        "\"id\":\"call_sym_real\",\"type\":\"function\",\"function\":{"
        "\"name\":\"bash\",\"arguments\":\"{\\\"command\\\":\\\"uname -a\\\"}\"}}]},"
        "{\"role\":\"tool\",\"tool_call_id\":\"call_sym_real\","
        "\"content\":\"Linux e2b.local GNU/Linux\\n\"}]}";
    OPENAI_TOOL_RESPONSE oc_resp;
    OPENAI_TOOL_CALL oc_call;
    TEST_ASSERT(ServerExtractLastToolResponse(oc_cont, &oc_resp) == 1,
                "extracts authentic nameless OpenCode tool response");
    TEST_ASSERT(oc_resp.name[0] == '\0', "authentic tool response has no name");
    TEST_ASSERT(ServerExtractPairedToolCall(oc_cont, oc_resp.tool_call_id, &oc_call) == 1,
                "recovers paired assistant tool call by id");
    TEST_ASSERT(strcmp(oc_call.name, "bash") == 0, "paired tool is bash");
    TEST_ASSERT(strstr(oc_call.arguments, "uname -a") != NULL,
                "paired call preserves exact command");
    TEST_ASSERT(ServerExtractPairedToolCall(oc_cont, "call_other", &oc_call) == 0,
                "mismatched tool id does not recover a call");

    const char *swap_cont =
        "{\"messages\":[{\"role\":\"assistant\",\"tool_calls\":[{"
        "\"id\":\"call_swap\",\"type\":\"function\",\"function\":{"
        "\"name\":\"edit\",\"arguments\":\"{\\\"filePath\\\":\\\"test.txt\\\","
        "\\\"oldString\\\":\\\"primera linea\\\\nsegunda linea\\\","
        "\\\"newString\\\":\\\"segunda linea\\\\nprimera linea\\\"}\"}}]},"
        "{\"role\":\"tool\",\"tool_call_id\":\"call_swap\","
        "\"content\":\"Edit applied successfully.\"}]}";
    TEST_ASSERT(ServerExtractLastToolResponse(swap_cont, &oc_resp) == 1,
                "extracts authentic nameless edit response");
    TEST_ASSERT(ServerExtractPairedToolCall(swap_cont, oc_resp.tool_call_id, &oc_call) == 1,
                "recovers paired edit call by exact id");
    TEST_ASSERT(strcmp(oc_call.name, "edit") == 0, "paired swap tool is edit");
    TEST_ASSERT(strstr(oc_call.arguments, "primera linea\\nsegunda linea") != NULL,
                "paired edit preserves old two-line span without final terminator");
    TEST_ASSERT(strstr(oc_call.arguments, "segunda linea\\nprimera linea") != NULL,
                "paired edit preserves new two-line span without final terminator");
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
    TEST_ASSERT(ServerIsCodingTask("lista las subcarpetas") == 1, "Detects 'lista las subcarpetas'");
    TEST_ASSERT(ServerIsInspectionTask("lista las subcarpetas") == 1, "Classifies 'lista las subcarpetas' as inspection");

    /* File creation tasks */
    TEST_ASSERT(ServerIsCodingTask("crea un fichero test.txt") == 1, "Detects 'crea un fichero test.txt'");
    TEST_ASSERT(ServerIsInspectionTask("crea un fichero test.txt") == 0, "File creation is not read-only inspection");
    TEST_ASSERT(ServerIsCodeSynthesisTask("crea un fichero test.txt") == 0, "File creation is not pure code synthesis");
    TEST_ASSERT(ServerIsFileCreationTask("crea un fichero test.txt") == 1, "crea un fichero is file creation");
    TEST_ASSERT(ServerIsFileCreationTask("create file config.json") == 1, "create file is file creation");
    TEST_ASSERT(ServerIsFileCreationTask("nuevo archivo notes.md") == 1, "nuevo archivo is file creation");
    TEST_ASSERT(ServerIsFileCreationTask("new feature in parser.c") == 0, "new feature in parser.c is not file creation");
    TEST_ASSERT(ServerIsFileCreationTask("make the makefile") == 0, "make makefile is not file creation");
    TEST_ASSERT(ServerIsFileCreationTask("crea una funcion en C") == 0, "crea una funcion is not file creation");
    {
        char path[260];
        TEST_ASSERT(ServerExtractCreatePath("crea un fichero test.txt", path, sizeof(path)) == 1,
                    "extract test.txt");
        TEST_ASSERT(strcmp(path, "test.txt") == 0, "create path is test.txt");
        TEST_ASSERT(ServerExtractCreatePath("crea un fichero notas", path, sizeof(path)) == 1,
                    "extract notas without extension");
        TEST_ASSERT(strcmp(path, "notas.txt") == 0, "bare name becomes notas.txt");
        TEST_ASSERT(ServerExtractCreatePath("crea un fichero chat", path, sizeof(path)) == 1,
                    "extract chat");
        TEST_ASSERT(strcmp(path, "chat.txt") == 0, "chat is chat.txt not src/chat.c");
        TEST_ASSERT(strstr(path, "CMakeLists") == NULL, "does not fall back to CMakeLists.txt");
        TEST_ASSERT(ServerExtractCreatePath("crea archivo config.json", path, sizeof(path)) == 1,
                    "extract config.json");
        TEST_ASSERT(strcmp(path, "config.json") == 0, "json name kept");
        TEST_ASSERT(ServerExtractCreatePath("crea un fichero django", path, sizeof(path)) == 1,
                    "extract django as file not folder");
        TEST_ASSERT(strcmp(path, "django.txt") == 0, "django.txt not django/");
        TEST_ASSERT(strchr(path, '/') == NULL && strchr(path, '\\') == NULL,
                    "create path is a file not a directory");
        TEST_ASSERT(ServerExtractCreatePath("crea un fichero", path, sizeof(path)) == 1,
                    "nameless create");
        TEST_ASSERT(strcmp(path, "nuevo.txt") == 0, "default nuevo.txt");
    }
    TEST_ASSERT(ServerIsCodeSynthesisTask("crea una funcion en C") == 1, "crea una funcion en C is synthesis");

    /* Code synthesis tasks */
    TEST_ASSERT(ServerIsCodingTask("escribe en C la funcion de fibonacci") == 1, "Detects 'escribe en C la funcion de fibonacci'");
    TEST_ASSERT(ServerIsCodeSynthesisTask("escribe en C la funcion de fibonacci") == 1, "Classifies as code synthesis");
    TEST_ASSERT(ServerIsInspectionTask("escribe en C la funcion de fibonacci") == 0, "Code synthesis is not inspection");
    TEST_ASSERT(ServerIsCodeSynthesisTask("write a function to calculate factorial") == 1, "Classifies 'write a function...' as code synthesis");
    TEST_ASSERT(ServerIsCodeSynthesisTask("lista las subcarpetas") == 0, "Inspection is not code synthesis");
    TEST_ASSERT(ServerIsCodeSynthesisTask("funcion fibinacci en C") == 1, "Classifies 'funcion fibinacci en C' with typo as code synthesis");
    TEST_ASSERT(ServerIsCodingTask("funcion fibinacci en C") == 1, "Detects 'funcion fibinacci en C' as coding task");
    TEST_ASSERT(ServerIsCodeSynthesisTask("funcion fibonacci en C") == 1, "Classifies 'funcion fibonacci en C' as code synthesis");
    TEST_ASSERT(ServerIsCodeSynthesisTask("funcion para ordenar un array en C") == 1, "Classifies 'funcion para ordenar un array en C' as code synthesis");
    TEST_ASSERT(ServerIsCodeSynthesisTask("busqueda binaria en c") == 1, "Classifies 'busqueda binaria en c' as code synthesis");
    TEST_ASSERT(ServerIsCodeSynthesisTask("quicksort en C") == 1, "Classifies 'quicksort en C' as code synthesis");
    TEST_ASSERT(ServerIsCodeSynthesisTask("lista enlazada en C") == 1, "Classifies 'lista enlazada en C' as code synthesis");
    TEST_ASSERT(ServerIsCodeSynthesisTask("array dinamico en C") == 1, "Classifies 'array dinamico en C' as code synthesis");
    TEST_ASSERT(ServerIsCodeSynthesisTask("leer archivo en C") == 1, "Classifies 'leer archivo en C' as code synthesis");
    TEST_ASSERT(ServerIsCodeSynthesisTask("pila en C") == 1, "Classifies 'pila en C' as code synthesis");
    TEST_ASSERT(ServerIsCodeSynthesisTask("ordenar con qsort en C") == 1, "Classifies 'ordenar con qsort en C' as code synthesis");
    TEST_ASSERT(ServerIsCodeSynthesisTask("implementa una lista en C") == 1, "Classifies 'implementa una lista en C' as code synthesis");
    TEST_ASSERT(ServerIsInspectionTask("implementa una lista en C") == 0, "List implementation is not workspace inspection");
    TEST_ASSERT(ServerIsCodeSynthesisTask("cola en C") == 1, "Classifies 'cola en C' as code synthesis");
    TEST_ASSERT(ServerIsCodeSynthesisTask("mergesort en C") == 1, "Classifies 'mergesort en C' as code synthesis");

    /* Workspace reads must not become canned fopen samples */
    TEST_ASSERT(ServerIsCodeSynthesisTask("read file src/chat.c") == 0, "'read file src/chat.c' is not synthesis");
    TEST_ASSERT(ServerIsInspectionTask("read file src/chat.c") == 1, "'read file src/chat.c' is inspection");
    TEST_ASSERT(ServerIsInspectionTask("cat src/chat.c") == 1, "'cat src/chat.c' is inspection");
    TEST_ASSERT(ServerIsInspectionTask("open src/main.c") == 1, "'open src/main.c' is inspection");
    TEST_ASSERT(ServerIsCodeSynthesisTask("cat src/chat.c") == 0, "'cat src/chat.c' is not synthesis");
    TEST_ASSERT(ServerIsCodeSynthesisTask("leer archivo src/chat.c") == 0, "'leer archivo src/chat.c' is not synthesis");
    TEST_ASSERT(ServerIsInspectionTask("leer archivo src/chat.c") == 1, "'leer archivo src/chat.c' is inspection");
    TEST_ASSERT(ServerIsCodeSynthesisTask("fiber in C") == 0, "'fiber' is not Fibonacci synthesis");
    TEST_ASSERT(ServerIsCodeSynthesisTask("What is a FILE in C") == 0, "Definitional FILE question is not synthesis");

    char synth[8192];
    ServerSynthesizeCode("ordenar con qsort en C", synth, sizeof(synth));
    TEST_ASSERT(strstr(synth, "qsort(") != NULL, "qsort prompt emits qsort(");
    TEST_ASSERT(strstr(synth, "Partition") == NULL, "qsort prompt does not emit Quicksort Partition");
    ServerSynthesizeCode("mergesort en C", synth, sizeof(synth));
    TEST_ASSERT(strstr(synth, "Merge") != NULL, "mergesort prompt emits Merge");
    TEST_ASSERT(strstr(synth, "Partition") == NULL, "mergesort prompt does not emit Quicksort Partition");
    ServerSynthesizeCode("cola en C", synth, sizeof(synth));
    TEST_ASSERT(strstr(synth, "Enqueue") != NULL, "cola prompt emits Enqueue");
    TEST_ASSERT(strstr(synth, "Dequeue") != NULL, "cola prompt emits Dequeue");
    TEST_ASSERT(strstr(synth, "% QUEUE_CAPACITY") != NULL, "cola prompt preserves modulo operators");
    TEST_ASSERT(strstr(synth, "StackPop") == NULL, "cola prompt does not emit a stack");
    ServerSynthesizeCode("funcion fibinacci en C", synth, sizeof(synth));
    TEST_ASSERT(strstr(synth, "fibonacci") != NULL, "fibinacci typo still emits fibonacci");
    ServerSynthesizeCode("leer archivo en C", synth, sizeof(synth));
    TEST_ASSERT(strstr(synth, "fopen") != NULL, "'leer archivo en C' emits fopen");
    TEST_ASSERT(strstr(synth, "fgets") != NULL, "'leer archivo en C' emits fgets");
    ServerSynthesizeCode("pila en C", synth, sizeof(synth));
    TEST_ASSERT(strstr(synth, "StackPush") != NULL, "pila prompt emits StackPush");
    TEST_ASSERT(strstr(synth, "Enqueue") == NULL, "pila prompt is not a queue");
    ServerSynthesizeCode("array dinamico en C", synth, sizeof(synth));
    TEST_ASSERT(strstr(synth, "VectorPush") != NULL, "vector prompt emits VectorPush");
    ServerSynthesizeCode("invertir cadena en C", synth, sizeof(synth));
    TEST_ASSERT(strstr(synth, "ReverseString") != NULL, "invertir prompt emits ReverseString");
    TEST_ASSERT(strstr(synth, "strlen") != NULL, "invertir prompt has a real body");

    /* Conversational greetings and identity queries */
    TEST_ASSERT(ServerIsGreeting("hola") == 1, "Detects 'hola' as greeting");
    TEST_ASSERT(ServerIsGreeting("¡Hola!") == 1, "Detects '¡Hola!' as greeting");
    TEST_ASSERT(ServerIsGreeting("hello") == 1, "Detects 'hello' as greeting");
    TEST_ASSERT(ServerIsGreeting("buenos dias") == 1, "Detects 'buenos dias' as greeting");
    TEST_ASSERT(ServerIsGreeting("quien eres") == 1, "Detects 'quien eres' as greeting/identity");
    TEST_ASSERT(ServerIsGreeting("who are you") == 1, "Detects 'who are you' as greeting/identity");
    TEST_ASSERT(ServerIsCodingTask("hola") == 0, "'hola' is not coding task");
    TEST_ASSERT(ServerIsInspectionTask("hola") == 0, "'hola' is not inspection task");
    TEST_ASSERT(ServerIsCodeSynthesisTask("hola") == 0, "'hola' is not code synthesis");

    char gbuf[512];
    TEST_ASSERT(ServerAnswerGreeting("hola", 0, gbuf, sizeof(gbuf)) == 1, "Answers 'hola'");
    TEST_ASSERT(strcmp(gbuf, "Hola. ¿Qué quieres hacer?") == 0,
                "'hola' response is direct, not a canned self-introduction");
    TEST_ASSERT(ServerAnswerGreeting("hello", 0, gbuf, sizeof(gbuf)) == 1, "Answers 'hello'");
    TEST_ASSERT(strcmp(gbuf, "Hello. What would you like to do?") == 0,
                "'hello' response is direct, not a canned self-introduction");

    /* Factual questions should NOT be detected as coding tasks */
    TEST_ASSERT(ServerIsCodingTask("Who is the father of Solomon?") == 0, "Factual query is not coding task");
    TEST_ASSERT(ServerIsCodeSynthesisTask("Who is the father of Solomon?") == 0, "Factual query is not code synthesis");
    TEST_ASSERT(ServerIsGreeting("Who is the father of Solomon?") == 0, "Factual query is not greeting");
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

    /* 6c: Tool returns plain text error (cmake cache error) */
    const char *cmake_err_payload =
        "{\"model\":\"symbols\",\"messages\":["
        "{\"role\":\"tool\",\"tool_call_id\":\"call_003b\",\"name\":\"bash\","
        "\"content\":\"Error: could not load cache\"}"
        "]}";
    OPENAI_TOOL_RESPONSE resp_cmake;
    int ok_cmake = ServerExtractLastToolResponse(cmake_err_payload, &resp_cmake);
    TEST_ASSERT(ok_cmake == 1, "Extracted cmake error response");
    TEST_ASSERT(resp_cmake.is_error == 1, "Flagged is_error == 1 for 'Error: could not load cache'");

    /* 6d: Tool returns 'No tests were found' */
    const char *ctest_err_payload =
        "{\"model\":\"symbols\",\"messages\":["
        "{\"role\":\"tool\",\"tool_call_id\":\"call_003c\",\"name\":\"bash\","
        "\"content\":\"Test project C:/tmp\\nNo tests were found!!!\"}"
        "]}";
    OPENAI_TOOL_RESPONSE resp_ctest;
    int ok_ctest = ServerExtractLastToolResponse(ctest_err_payload, &resp_ctest);
    TEST_ASSERT(ok_ctest == 1, "Extracted ctest error response");
    TEST_ASSERT(resp_ctest.is_error == 1, "Flagged is_error == 1 for 'No tests were found'");

    /* 6e: Tool returns clean success */
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

    /* 6f: Glob tool returns JSON with status: ok, error: null, matches: [...] */
    const char *glob_ok_payload =
        "{\"model\":\"symbols\",\"messages\":["
        "{\"role\":\"tool\",\"tool_call_id\":\"call_005\",\"name\":\"glob\","
        "\"content\":\"{\\\"status\\\":\\\"ok\\\",\\\"matches\\\":[\\\"src\\\",\\\"include\\\"],\\\"error\\\":null}\"}"
        "]}";
    OPENAI_TOOL_RESPONSE resp_glob;
    int ok_glob = ServerExtractLastToolResponse(glob_ok_payload, &resp_glob);
    TEST_ASSERT(ok_glob == 1, "Extracted glob response with 'error': null");
    TEST_ASSERT(resp_glob.is_error == 0, "is_error == 0 for glob with 'error': null");

    /* 6g: Glob tool output containing 'error:' or 'failed' inside filenames */
    const char *glob_errtext_payload =
        "{\"model\":\"symbols\",\"messages\":["
        "{\"role\":\"tool\",\"tool_call_id\":\"call_006\",\"name\":\"glob\","
        "\"content\":\"src/error_handler.c\\ntests/test_failed_cases.c\"}"
        "]}";
    OPENAI_TOOL_RESPONSE resp_glob2;
    int ok_glob2 = ServerExtractLastToolResponse(glob_errtext_payload, &resp_glob2);
    TEST_ASSERT(ok_glob2 == 1, "Extracted glob response with error words in filenames");
    TEST_ASSERT(resp_glob2.is_error == 0, "is_error == 0 because glob is an inspection tool");

    /* 6h: Tool returns multi-part array content [{"type": "text", "text": "..."}] */
    const char *array_payload =
        "{\"model\":\"symbols\",\"messages\":["
        "{\"role\":\"tool\",\"tool_call_id\":\"call_007\",\"name\":\"glob\","
        "\"content\":[{\"type\":\"text\",\"text\":\"build-gcc\\nsrc\\ninclude\"}]}"
        "]}";
    OPENAI_TOOL_RESPONSE resp_array;
    int ok_array = ServerExtractLastToolResponse(array_payload, &resp_array);
    TEST_ASSERT(ok_array == 1, "Extracted multi-part array tool content");
    TEST_ASSERT(strstr(resp_array.content, "build-gcc") != NULL, "Extracted text contains build-gcc");
    TEST_ASSERT(resp_array.is_error == 0, "is_error == 0 for multi-part array content");

    /* 6i: ServerFormatInspectionOutput cleanly formats JSON matches */
    char formatted[1024];
    const char *json_matches = "{\"status\":\"ok\",\"matches\":[\"build-gcc\",\"src\",\"include\",\"CMakeLists.txt\"],\"error\":null}";
    int fmt_ok = ServerFormatInspectionOutput(json_matches, formatted, sizeof(formatted));
    TEST_ASSERT(fmt_ok == 1, "ServerFormatInspectionOutput succeeded on JSON matches");
    TEST_ASSERT(strstr(formatted, "build-gcc\nsrc\ninclude\nCMakeLists.txt") != NULL,
                "Formatted output contains line-delimited files without JSON punctuation");
}

/* 7. Test Binary Model Detection in Chat Layer */
static void test_observed_repair_gaps(void)
{
    char old_text[512], new_text[512];
    const char *semi_src = "int main(void)\n{\n    int total = 3\n    return total;\n}\n";
    TEST_ASSERT(ServerPlanObservedCRepair(semi_src,
                    "inventario.c:4:5: error: expected ';' before 'return'",
                    old_text, sizeof(old_text), new_text, sizeof(new_text)) == 1 &&
                strstr(new_text, "int total = 3;") != NULL &&
                strstr(old_text, "int total = 3") != NULL,
                "missing ';' (ASCII quotes) appends ';' to the previous line");
    TEST_ASSERT(ServerPlanObservedCRepair(semi_src,
                    "inventario.c:4:5: error: expected \xe2\x80\x98;\xe2\x80\x99 before \xe2\x80\x98return\xe2\x80\x99",
                    old_text, sizeof(old_text), new_text, sizeof(new_text)) == 1 &&
                strstr(new_text, "int total = 3;") != NULL,
                "missing ';' (gcc UTF-8 quotes) appends ';' to the previous line");
    const char *inc_src = "#include <stdio.h>\nint main(void)\n{\n    return (int)strlen(\"ab\");\n}\n";
    TEST_ASSERT(ServerPlanObservedCRepair(inc_src,
                    "nombre.c:4:17: warning: implicit declaration of function 'strlen'\n"
                    "nombre.c:2:1: note: include '<string.h>' or provide a declaration of 'strlen'",
                    old_text, sizeof(old_text), new_text, sizeof(new_text)) == 1 &&
                strstr(new_text, "#include <string.h>") != NULL,
                "gcc include note yields the missing #include");
    OPENAI_TOOL_RESPONSE resp;
    const char *brace_payload =
        "{\"tools\":[{\"type\":\"function\",\"function\":{\"name\":\"bash\",\"parameters\":{\"properties\":{\"content\":{\"type\":\"string\"}}}}}],"
        "\"messages\":[{\"role\":\"assistant\",\"tool_calls\":[{\"id\":\"c1\",\"type\":\"function\",\"function\":{\"name\":\"bash\",\"arguments\":\"{}\"}}]},"
        "{\"role\":\"tool\",\"tool_call_id\":\"c1\",\"content\":\"a.c: In function 'f': { } warning: unused\"}]}";
    TEST_ASSERT(ServerExtractLastToolResponse(brace_payload, &resp) == 1 &&
                strstr(resp.content, "warning: unused") != NULL,
                "tool output containing braces keeps its content");
    TEST_ASSERT(ServerCheckOutputFails("x.c:3:1: warning: implicit declaration") == 1 &&
                ServerCheckOutputFails("x.c:3:1: error: expected") == 1 &&
                ServerCheckOutputFails("build ok\n") == 0,
                "warnings-only output is not a pass");
}

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

/* Shell commands: engine picks bash/execute_command; harness runs them. */
static void test_shell_tool_dispatch(void)
{
    printf("\n=== Test 10: Shell tool dispatch (engine chooses, harness executes) ===\n");
    const char *cmds[] = {
        "cmake --build .",
        "ctest --output-on-failure",
        "gcc -Wall -Wextra main.c -o main",
        "git status",
        "git diff --stat",
        "ls -la",
        "pwd",
        "uname -a",
        "echo hello",
        "mkdir tmp_shell_test",
        "python --version",
        "powershell Get-ChildItem",
        "run ctest",
        "ejecuta cmake --build build-gcc"
    };
    char bash_only[][64] = { "bash", "read", "write" };
    char exec_only[][64] = { "execute_command", "read" };
    char glob_bash[][64] = { "glob", "bash", "read" };
    size_t i;

    for (i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++)
    {
        OPENAI_TOOL_CALL tc;
        char msg[192];
        TEST_ASSERT(ServerIsShellTask(cmds[i]) == 1, cmds[i]);
        TEST_ASSERT(ServerIsCodeSynthesisTask(cmds[i]) == 0, "shell is not code synthesis");
        TEST_ASSERT(ServerMapShellToolCall(cmds[i], bash_only, 3, &tc) == 1,
                    "maps with bash declared");
        TEST_ASSERT(strcmp(tc.name, "bash") == 0, "prefers bash when declared");
        TEST_ASSERT(strstr(tc.arguments, "\"command\"") != NULL, "arguments have command");
        TEST_ASSERT(strstr(tc.arguments, cmds[i]) != NULL ||
                    strstr(tc.arguments, "ctest") != NULL ||
                    strstr(tc.arguments, "cmake") != NULL,
                    "command text is in arguments");
        TEST_ASSERT(strstr(tc.arguments, "PASSED") == NULL, "engine did not run the command");
        TEST_ASSERT(ServerMapShellToolCall(cmds[i], exec_only, 2, &tc) == 1,
                    "maps with execute_command");
        TEST_ASSERT(strcmp(tc.name, "execute_command") == 0, "falls back to execute_command");
        snprintf(msg, sizeof(msg), "shell task: %s", cmds[i]);
        (void)msg;
    }

    TEST_ASSERT(ServerIsShellTask("dir *.*") == 1, "dir *.* is a shell-shaped listing");
    TEST_ASSERT(ServerIsShellTask("uname -a") == 1,
                "authentic OpenCode TUI uname request is shell-shaped");
    TEST_ASSERT(ServerIsShellTask("uname is a Unix utility") == 0,
                "ordinary prose beginning with uname is not executed");
    TEST_ASSERT(ServerIsShellTask("uname explained") == 0,
                "uname plus a non-option argument is not executed");
    {
        OPENAI_TOOL_CALL tc;
        TEST_ASSERT(ServerMapShellToolCall("dir *.*", glob_bash, 3, &tc) == 1,
                    "dir *.* still mappable to bash");
        TEST_ASSERT(strcmp(tc.name, "bash") == 0, "mapper prefers bash over glob");
    }

    {
        static char big[12000], out[4096];
        memset(big, 'x', sizeof(big) - 1); big[sizeof(big) - 1] = '\0';
        strcpy(out, "STALE");
        TEST_ASSERT(ServerBuildStreamResponse("m", 1, 1, big, out, sizeof(out)) == 0 && out[0] == '\0',
                    "failed stream encode never leaves a stale response");
        strcpy(out, "STALE");
        TEST_ASSERT(ServerBuildResponse("m", 1, 1, big, "q", out, sizeof(out)) == 0 && out[0] == '\0',
                    "failed encode never leaves a stale response");
    }
    /* procedural memory: the second identical request must not repeat the probe */
    {
        EPISODIC_STORE st, st2;
        OPENAI_TOOL_CALL tc;
        SERVER_PROC_TRACE tr;
        char learned[256], scope[32], shown[256];
        const char *path = "/tmp/symbols_test_procedural.tsv";
        remove(path);
        ServerProcScope("<env>\n  Platform: linux\n</env>", scope, sizeof(scope));
        TEST_ASSERT(strcmp(scope, "linux") == 0, "scope comes from the client env block");
        TEST_ASSERT(EpisodicStoreInit(&st, path), "procedural store opens");
        /* run 1: nothing known -> probe both readings */
        TEST_ASSERT(ServerMapShellToolCallMem("ejecuta ps aux", bash_only, 3, &st, scope, &tr, &tc) == 1 &&
                    tr.decision == SERVER_PROC_PROBED && strstr(tc.arguments, "command -v ejecuta") != NULL,
                    "first run probes");
        /* environment answer for run 1 */
        TEST_ASSERT(ServerProcLearnFromOutput(&st, scope,
                    "symbols-probe:not-a-command:ejecuta\nsymbols-probe:is-command:ps\nUSER PID\n",
                    learned, sizeof(learned)) == 2, "learns both outcomes");
        ServerStripProbeLines("symbols-probe:is-command:ps\nUSER PID\n", shown, sizeof(shown));
        TEST_ASSERT(strcmp(shown, "USER PID\n") == 0, "probe lines never reach the user");
        /* run 2: same request goes direct */
        TEST_ASSERT(ServerMapShellToolCallMem("ejecuta ps aux", bash_only, 3, &st, scope, &tr, &tc) == 1 &&
                    tr.decision == SERVER_PROC_DIRECT && strstr(tc.arguments, "\"command\":\"ps aux\"") != NULL,
                    "second run goes direct with no probe");
        /* transfer: a new request with the learned request word only probes the unknown part */
        TEST_ASSERT(ServerMapShellToolCallMem("ejecuta df -h", bash_only, 3, &st, scope, &tr, &tc) == 1 &&
                    tr.decision == SERVER_PROC_DIRECT && strstr(tc.arguments, "\"command\":\"df -h\"") != NULL,
                    "learned request word transfers to new commands");
        TEST_ASSERT(ServerMapShellToolCallMem("ejecuta htop", bash_only, 3, &st, scope, &tr, &tc) == 1 &&
                    tr.decision == SERVER_PROC_PARTIAL && strstr(tc.arguments, "command -v ejecuta") == NULL &&
                    strstr(tc.arguments, "command -v htop") != NULL, "only the unknown program is probed");
        /* all-negative line: answered from memory, no tool */
        ServerProcLearnFromOutput(&st, scope, "symbols-probe:not-a-command:hola que\n", learned, sizeof(learned));
        TEST_ASSERT(ServerShellRouteMem("hola que tal", &st, scope, &tr) == 2 && tr.decision == SERVER_PROC_FROM_MEMORY,
                    "known non-commands skip the tool round-trip");
        TEST_ASSERT(ServerShellRouteMem("hola que tal", &st, "windows", &tr) == 1, "memory is scoped per platform");
        {
            uint32_t i;
            for (i = 0; i < st.count; i++)
                if (strcmp(st.records[i].subject, "que") == 0) st.records[i].timestamp -= SERVER_PROC_NEG_TTL + 10;
            TEST_ASSERT(ServerShellRouteMem("hola que tal", &st, scope, &tr) == 1,
                        "an old missing-program memory is re-checked, not trusted forever");
            ServerProcLearnFromOutput(&st, scope, "symbols-probe:not-a-command:hola que\n", learned, sizeof(learned));
            TEST_ASSERT(ServerShellRouteMem("hola que tal", &st, scope, &tr) == 2, "re-verification refreshes the memory");
        }
        /* restart: memory survives on disk */
        TEST_ASSERT(EpisodicStoreInit(&st2, path) && EpisodicStoreLoad(&st2) >= 4, "memory reloads after restart");
        TEST_ASSERT(ServerProcRecall(&st2, scope, "ps") == 1 && ServerProcRecall(&st2, scope, "ejecuta") == -1,
                    "reloaded memory keeps outcomes");
        /* correct: a remembered command that is now missing is forgotten */
        TEST_ASSERT(ServerProcCorrectFromOutput(&st2, scope, "ps aux", "/bin/bash: line 1: ps: command not found\n") == 1 &&
                    ServerProcRecall(&st2, scope, "ps") == 0, "stale positive memory is corrected");
        /* ingest gate: markers are learned only when OUR call carried the probe */
        TEST_ASSERT(ServerProcArgumentsCarryProbe("{\"command\":\"ls\"}") == 0,
                    "plain command arguments fail the ingest gate");
        TEST_ASSERT(ServerProcArgumentsCarryProbe(
                        "{\"command\":\"if command -v ls; then echo symbols-probe:is-command:ls; fi\"}") == 1,
                    "probe markers in arguments pass the ingest gate");
        TEST_ASSERT(ServerProcArgumentsCarryProbe(NULL) == 0, "NULL arguments fail the ingest gate");
        /* risk 8: strong shape + remembered negative re-probes instead of DIRECT */
        TEST_ASSERT(ServerProcLearnFromOutput(&st, scope,
                    "symbols-probe:not-a-command:htop\n", learned, sizeof(learned)) >= 1,
                    "learn htop as not-a-command");
        TEST_ASSERT(ServerMapShellToolCallMem("htop -d 1", bash_only, 3, &st, scope, &tr, &tc) == 1 &&
                    tr.decision == SERVER_PROC_PROBED &&
                    strstr(tc.arguments, "command -v htop") != NULL,
                    "strong shape with remembered negative re-probes");
        /* bare single-token negative re-probes; multi-word reading stays from memory */
        TEST_ASSERT(ServerShellRouteMem("htop", &st, scope, &tr) == 1,
                    "bare remembered non-command re-probes");
        TEST_ASSERT(ServerShellRouteMem("hola que tal", &st, scope, &tr) == 2 &&
                    tr.decision == SERVER_PROC_FROM_MEMORY,
                    "multi-word known negative stays answered from memory");
        /* risk 7: Windows not-recognized corrects a stale positive */
        ServerProcLearnFromOutput(&st, scope, "symbols-probe:is-command:htop\n", learned, sizeof(learned));
        TEST_ASSERT(ServerProcRecall(&st, scope, "htop") == 1, "htop remembered as command");
        TEST_ASSERT(ServerProcCorrectFromOutput(&st, scope, "htop -d 1",
                    "'htop' is not recognized as an internal or external command,\r\n") == 1 &&
                    ServerProcRecall(&st, scope, "htop") == 0,
                    "Windows not-recognized corrects stale positive memory");
        EpisodicStoreDestroy(&st);
        EpisodicStoreDestroy(&st2);
        remove(path);
    }
    /* shape-based routing: no program whitelist */
    TEST_ASSERT(ServerIsShellTask("ejecuta ps aux") == 1, "ejecuta + unlisted program is shell");
    TEST_ASSERT(ServerIsShellTask("ejecuta ollama list") == 1, "ejecuta + unknown program is shell");
    TEST_ASSERT(ServerIsShellTask("ps aux") == 1, "terse bare command line is shell-shaped");
    TEST_ASSERT(ServerIsShellTask("kubectl get pods -n web") == 1, "options make a bare line strong");
    TEST_ASSERT(ServerIsShellTask("ejecuta ls y dime cuantos ficheros hay") == 0, "verb + sentence is not a command");
    TEST_ASSERT(ServerIsShellTask("que diria un pirata del siglo XVIII acerca de la mecanica cuantica ?") == 0, "question is not a command");
    {
        OPENAI_TOOL_CALL tc;
        TEST_ASSERT(ServerMapShellToolCall("ps aux", bash_only, 3, &tc) == 1, "bare weak line maps");
        TEST_ASSERT(strstr(tc.arguments, "command -v ps") != NULL && strstr(tc.arguments, "ps aux") != NULL,
                    "weak bare line is probed before running");
        TEST_ASSERT(ServerMapShellToolCall("ejecuta ollama list", bash_only, 3, &tc) == 1 &&
                    strstr(tc.arguments, "command -v ejecuta") != NULL &&
                    strstr(tc.arguments, "elif command -v ollama") != NULL &&
                    strstr(tc.arguments, "is-command:ollama; ollama list;") != NULL,
                    "leading word is tested, not looked up in a verb list");
        TEST_ASSERT(ServerMapShellToolCall("lanza git status", bash_only, 3, &tc) == 1 &&
                    strstr(tc.arguments, "is-command:git; git status;") != NULL, "any leading word works, no verb list");
        TEST_ASSERT(ServerMapShellToolCall("ejecuta ls -la", bash_only, 3, &tc) == 1 &&
                    strstr(tc.arguments, "else ejecuta ls -la; fi") != NULL,
                    "strong line falls back to running it for the real error");
        TEST_ASSERT(ServerIsShellTask("fix the leak in parser.c") == 0, "prose after a leading word is not a command");
    }
    TEST_ASSERT(ServerIsShellTask("quien es el padre de David?") == 0,
                "factual QA is not a shell task");
    TEST_ASSERT(ServerIsShellTask("escribe en C la funcion de fibonacci") == 0,
                "code synthesis is not a shell task");
    TEST_ASSERT(ServerIsShellTask("crea un fichero test.txt") == 0,
                "file creation is not a shell task");
    TEST_ASSERT(ServerIsShellTask("fix the leak in parser.c") == 0,
                "repair task is not a one-shot shell command");
    TEST_ASSERT(ServerIsShellTask("what happens if a glass falls") == 0,
                "commonsense QA is not a shell task");
    {
        char none[][64] = { "read", "write", "glob" };
        OPENAI_TOOL_CALL tc;
        TEST_ASSERT(ServerMapShellToolCall("git status", none, 3, &tc) == 0,
                    "no mapping without bash/execute_command");
    }
}

static void test_edit_and_diff_dispatch(void)
{
    printf("\n=== Test 11: Edit file and show diff (harness executes) ===\n");
    char bash_edit[][64] = { "bash", "edit", "read", "glob" };
    char edit_only[][64] = { "edit", "read" };
    OPENAI_TOOL_CALL tc;
    char file[260], old_s[64], new_s[64];
    int has_rep = 0;

    TEST_ASSERT(ServerIsDiffTask("muestra el diff") == 1, "muestra el diff");
    TEST_ASSERT(ServerIsDiffTask("show the diff") == 1, "show the diff");
    TEST_ASSERT(ServerIsDiffTask("ver los cambios") == 1, "ver los cambios");
    TEST_ASSERT(ServerIsDiffTask("git diff") == 1, "git diff");
    TEST_ASSERT(ServerIsDiffTask("apply this patch to main.c") == 0, "apply patch is not show-diff");
    TEST_ASSERT(ServerIsDiffTask("quien es el padre de David?") == 0, "QA is not a diff task");
    TEST_ASSERT(ServerMapDiffToolCall("muestra el diff", bash_edit, 4, &tc) == 1,
                "maps diff to a shell tool");
    TEST_ASSERT(strcmp(tc.name, "bash") == 0, "diff uses bash");
    TEST_ASSERT(strstr(tc.arguments, "git diff") != NULL, "command is git diff");
    TEST_ASSERT(strstr(tc.arguments, "buggy line") == NULL, "diff is not a fake hunk");

    TEST_ASSERT(ServerIsEditTask("cambia foo por bar en main.c") == 1,
                "cambia X por Y en file");
    TEST_ASSERT(ServerIsEditTask("replace foo with bar in notes.txt") == 1,
                "replace X with Y in file");
    TEST_ASSERT(ServerIsEditTask("modifica README.md") == 1, "modifica FILE");
    TEST_ASSERT(ServerIsEditTask("crea un fichero notas.txt") == 0, "create is not edit");
    TEST_ASSERT(ServerIsEditTask("fix the leak in parser.c") == 0, "fix stays STRIPS");
    TEST_ASSERT(ServerIsSwapLinesTask("intercambia las líneas") == 1,
                "authentic Spanish swap intent");
    TEST_ASSERT(ServerIsSwapLinesTask("swap the lines") == 1,
                "English swap intent");
    TEST_ASSERT(ServerIsSwapLinesTask("intercambia columnas") == 0,
                "unbounded interchange is not a line swap");

    TEST_ASSERT(ServerExtractEditSpec("cambia foo por bar en main.c",
                                      file, sizeof(file), old_s, sizeof(old_s),
                                      new_s, sizeof(new_s), &has_rep) == 1,
                "extract edit spec");
    TEST_ASSERT(strcmp(file, "main.c") == 0, "edit file is main.c");
    TEST_ASSERT(has_rep == 1 && strcmp(old_s, "foo") == 0 && strcmp(new_s, "bar") == 0,
                "old=foo new=bar");

    TEST_ASSERT(ServerMapEditToolCall("cambia foo por bar en main.c",
                                      edit_only, 2, &tc) == 1, "maps explicit replace");
    TEST_ASSERT(strcmp(tc.name, "edit") == 0, "uses edit tool");
    TEST_ASSERT(strstr(tc.arguments, "main.c") != NULL, "edit targets main.c");
    TEST_ASSERT(strstr(tc.arguments, "oldString") != NULL &&
                strstr(tc.arguments, "foo") != NULL, "oldString foo");
    TEST_ASSERT(strstr(tc.arguments, "newString") != NULL &&
                strstr(tc.arguments, "bar") != NULL, "newString bar");
    TEST_ASSERT(strstr(tc.arguments, "buggy line") == NULL, "no dummy hunk");

    TEST_ASSERT(ServerMapEditToolCall("modifica README.md", edit_only, 2, &tc) == 1,
                "modifica FILE without replacement");
    TEST_ASSERT(strcmp(tc.name, "read") == 0, "without old/new, ask harness to read");
    TEST_ASSERT(strstr(tc.arguments, "README.md") != NULL, "read README.md");
}


static void TestWorkspacePlanningEvidence(void)
{
    char file[260], build[256], test[256];
    const char *make_listing = "temp_conv.c\nMakefile\ntests/test_temp.c\n";
    const char *cmake_listing = "src/main.c\nCMakeLists.txt\ntests/unit.c\n";

    TEST_ASSERT(ServerSelectWorkspaceFile("corrige temp_conv.c", make_listing,
                                          file, sizeof(file)) == 1,
                "named source selected only from workspace listing");
    TEST_ASSERT(strcmp(file, "temp_conv.c") == 0,
                "selected named source is exact");
    TEST_ASSERT(ServerSelectWorkspaceFile("corrige missing.c", make_listing,
                                          file, sizeof(file)) == 0,
                "missing named source fails closed");
    TEST_ASSERT(file[0] == '\0',
                "missing named source is never substituted silently");
    TEST_ASSERT(ServerSelectWorkspaceFile("corrige el fallo", "Makefile\nREADME.md\n",
                                          file, sizeof(file)) == 0,
                "no invented source when listing has none");

    TEST_ASSERT(ServerInferWorkspaceCommands(make_listing, build, sizeof(build),
                                              test, sizeof(test)) == 1,
                "Makefile build inferred from evidence");
    TEST_ASSERT(strcmp(build, "make") == 0, "Makefile uses make, not CMake");
    TEST_ASSERT(strcmp(test, "make test") == 0, "observed tests enable make test");
    TEST_ASSERT(ServerInferWorkspaceCommands(cmake_listing, build, sizeof(build),
                                              test, sizeof(test)) == 1,
                "CMake commands inferred from CMake marker");
    TEST_ASSERT(strstr(build, "cmake -S . -B build") != NULL,
                "CMake configures explicit build directory");
    TEST_ASSERT(ServerInferWorkspaceCommands("main.c\nREADME.md\n", build, sizeof(build),
                                              test, sizeof(test)) == 0,
                "no build command invented without build marker");
    TEST_ASSERT(build[0] == '\0' && test[0] == '\0',
                "unknown build system fails closed");

    {
        char command[512], old_text[256], new_text[256];
        const char *typo_source =
            "int main(void){double fahrenheit=32;printf(\"%f\",fahrenhiet);}";
        const char *gcc_diag =
            "main.c:1: error: ‘fahrenhiet’ undeclared; did you mean ‘fahrenheit’?";
        const char *heap_source =
            "double *datos=0; size_t cap=4; datos=realloc(datos,cap);";
        TEST_ASSERT(ServerDeriveSingleCCommand("no compila", "src/main.c",
                                               command, sizeof(command)) == 1 &&
                    strstr(command, "gcc -Wall -Wextra -std=c11 src/main.c") != NULL,
                    "single observed C source gets bounded compiler command");
        TEST_ASSERT(ServerDeriveSingleCCommand("fallo de memoria", "acum.c",
                                               command, sizeof(command)) == 1 &&
                    strstr(command, "-fsanitize=address") != NULL &&
                    strstr(command, "1 2 3 4 5 6 7 8") != NULL,
                    "memory task gets bounded ASan command with several arguments");
        TEST_ASSERT(ServerDeriveSingleCCommand("fix", "main.c;rm", command,
                                               sizeof(command)) == 0,
                    "unsafe path never becomes a command");
        TEST_ASSERT(ServerIssueRequestsSanitizer("AddressSanitizer falla al crecer") == 1,
                    "explicit sanitizer intent wins over a plain compile");
        TEST_ASSERT(ServerIssueRequestsTests("implementa para que los tests pasen") == 1,
                    "explicit test intent selects an evidenced test target");
        TEST_ASSERT(ServerSelectWorkspaceTestFile("texto.c\ntest_texto.c\nMakefile\n", "texto.c", file, sizeof(file)) == 1 &&
                    strcmp(file, "test_texto.c") == 0,
                    "failing test route inspects observed test source");
        TEST_ASSERT(ServerPlanTestObservedCRepair(
                        "char *repeat(const char *s,int n){(void)s;(void)n;return 0;}",
                        "int main(){char *x=repeat(\"ab\",3);assert(x);free(x);}",
                        old_text, sizeof(old_text), new_text, sizeof(new_text)) == 1 &&
                    strstr(new_text, "malloc") != NULL && strstr(new_text, "for(int i=0;i<veces;i++)") != NULL,
                    "source and inspected tests can establish a bounded repeat implementation");
        TEST_ASSERT(ServerPlanObservedCRepair(typo_source, gcc_diag,
                                              old_text, sizeof(old_text),
                                              new_text, sizeof(new_text)) == 1 &&
                    strcmp(old_text, "fahrenhiet") == 0 &&
                    strcmp(new_text, "fahrenheit") == 0,
                    "compiler suggestion yields exact identifier edit");
        TEST_ASSERT(ServerPlanObservedCRepair(
                        heap_source,
                        "ERROR: AddressSanitizer: heap-buffer-overflow",
                        old_text, sizeof(old_text), new_text, sizeof(new_text)) == 1 &&
                    strcmp(old_text, "realloc(datos,cap)") == 0 &&
                    strcmp(new_text, "realloc(datos,cap*sizeof *datos)") == 0,
                    "ASan overflow plus realloc count yields allocation-size edit");
        TEST_ASSERT(ServerPlanObservedCRepair(
                        "datos=realloc(datos,cap*sizeof *datos);",
                        "ERROR: AddressSanitizer: heap-buffer-overflow",
                        old_text, sizeof(old_text), new_text, sizeof(new_text)) == 0,
                    "already byte-sized allocation is not edited blindly");
    }

    TEST_ASSERT(ServerIsAmbiguousCodingTask("Optimiza este programa") == 1,
                "consequentially ambiguous optimization requires clarification");
    TEST_ASSERT(ServerIsAmbiguousCodingTask("Optimiza parser.c para reducir memoria") == 0,
                "named target and metric are actionable");
    TEST_ASSERT(ServerIsAmbiguousCodingTask("Arregla este proyecto en C para que haga lo que necesito.") == 1,
                "missing requested behavior requires clarification even when compilation passes");
    {
        char a[2048], b[2048], c[4096];
        TEST_ASSERT(ServerIsExplicitStockTotalFeature("Añade un campo stock, calcula precio por stock, total y actualiza main") == 1,
                    "explicit field, formula, and call-site request enables coherent feature route");
        TEST_ASSERT(ServerSelectFeatureFiles("item.h\nitem.c\nmain.c\nMakefile\n", "item.h", file, sizeof(file), a, sizeof(a)) == 1 && strcmp(file,"item.c")==0 && strcmp(a,"main.c")==0,
                    "feature route discovers implementation and call site from listing");
        TEST_ASSERT(ServerPlanStockHeader("typedef struct {const char *nombre; double precio;} Producto;", b, sizeof(b)) == 1 && strstr(b,"int stock") && strstr(b,"valor_total_inventario"),
                    "header edit derives field and declaration from observed type");
        TEST_ASSERT(ServerPlanStockImplementation("static Producto items[4]; static int n;", b, sizeof(b)) == 1 && strstr(b,"precio*items[i].stock"),
                    "implementation follows explicit price-times-stock formula");
        TEST_ASSERT(ServerPlanStockMain("stock 3 para el alpha y 5 para el beta; actualiza main", "int main(){add((Producto){\"alpha\",2});add((Producto){\"beta\",4});return 0;}", c, sizeof(c)) == 1 && strstr(c,",3}") && strstr(c,",5}") && strstr(c,"valor_total_inventario"),
                    "call-site constants come from request, not fixture routing");
    }

    /* Authentic five-case envelopes: assert grounding/abstention protocol,
       never encode the repair. */
    TEST_ASSERT(ServerSelectWorkspaceFile(
                    "Este programa en C no compila. Arréglalo para que compile y funcione. No cambies lo que calcula.",
                    "temp_conv.c\n", file, sizeof(file)) == 1 &&
                strcmp(file, "temp_conv.c") == 0,
                "case 1 discovers its only observed source");
    TEST_ASSERT(ServerSelectWorkspaceFile(
                    "En este proyecto hay tests que fallan porque la función de texto.c está sin implementar. Implementa la función para que los tests pasen. No modifiques los tests.",
                    "texto.c\ntexto.h\ntest_texto.c\nMakefile\n", file, sizeof(file)) == 1 &&
                strcmp(file, "texto.c") == 0,
                "case 2 binds the named implementation, not tests");
    TEST_ASSERT(ServerSelectWorkspaceFile(
                    "Al compilarlo con -fsanitize=address y ejecutarlo con varios números como argumentos, falla. Encuentra y corrige el fallo de memoria. El programa debe seguir calculando lo mismo.",
                    "acumulador.c\n", file, sizeof(file)) == 1 &&
                strcmp(file, "acumulador.c") == 0,
                "case 3 discovers the ASan fixture without a leaked solution");
    TEST_ASSERT(ServerSelectWorkspaceFile(
                    "Añade un campo stock al producto, con stock 4 para el teclado, 10 para el ratón y 2 para el monitor, y una función que devuelva el valor total del inventario (precio multiplicado por stock de cada producto). Actualiza el main para que muestre ese total.",
                    "inventario.h\ninventario.c\nmain.c\nMakefile\n", file, sizeof(file)) == 1,
                "case 4 starts from an observed project source");
    TEST_ASSERT(ServerIsAmbiguousCodingTask(
                    "Los usuarios dicen que este programa tarda demasiado. Optimízalo.") == 1,
                "case 5 clarifies or abstains instead of synthesizing a template");
    TEST_ASSERT(ServerIsRepositoryTask(
                    "Este programa en C no compila. Arréglalo para que compile y funcione. No cambies lo que calcula."),
                "case 1 enters repository route");
    TEST_ASSERT(ServerIsRepositoryTask(
                    "En este proyecto hay tests que fallan porque la función de texto.c está sin implementar. Implementa la función para que los tests pasen. No modifiques los tests."),
                "case 2 enters repository route before synthesis");
    TEST_ASSERT(ServerIsRepositoryTask(
                    "Al compilarlo con -fsanitize=address y ejecutarlo con varios números como argumentos, falla. Encuentra y corrige el fallo de memoria. El programa debe seguir calculando lo mismo."),
                "case 3 enters repository route");
    TEST_ASSERT(ServerIsRepositoryTask(
                    "Añade un campo stock al producto, con stock 4 para el teclado, 10 para el ratón y 2 para el monitor, y una función que devuelva el valor total del inventario (precio multiplicado por stock de cada producto). Actualiza el main para que muestre ese total."),
                "case 4 enters repository route");
    TEST_ASSERT(ServerIsRepositoryTask(
                    "Los usuarios dicen que este programa tarda demasiado. Optimízalo."),
                "case 5 enters repository route for clarification");
}

int main(void)
{
    TestWorkspacePlanningEvidence();
    printf("======================================================================\n");
    printf("  TEST SUITE: OPENAI TOOL CALLING WIRE PROTOCOL (OPENCODE INTEGRATION)\n");
    printf("======================================================================\n");

    test_tools_declared_extraction();
    test_tool_response_extraction();
    test_tool_call_response_building();
    test_tool_call_streaming_response();
    test_coding_task_intent();
    test_tool_error_validation();
    test_observed_repair_gaps();
    test_binary_model_support();
    test_last_role_extraction();
    test_discrimination_battery();
    test_shell_tool_dispatch();
    test_edit_and_diff_dispatch();

    printf("\n======================================================================\n");
    printf("  TEST RESULTS: %d passed, %d failed\n", g_tests_passed, g_tests_run - g_tests_passed);
    printf("======================================================================\n");

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}

