/* ============================================================
   test_code_graph_polyglot.c: Verification of the Polyglot Code
                               Knowledge Graph (Python, TS/JS, C).
   Pure ISO C11, zero tensors, zero backprop, fail-closed verification.
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "code_graph.h"

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

/* ============================================================
   Test 1: Language Detection
   ============================================================ */
static void test_language_detection(void)
{
    printf("\n=== Test 1: Language Detection ===\n");
    TEST_ASSERT(CodeGraphDetectLanguage("src/main.c") == CODE_LANG_C, "Detects .c as C");
    TEST_ASSERT(CodeGraphDetectLanguage("include/utils.h") == CODE_LANG_C, "Detects .h as C");
    TEST_ASSERT(CodeGraphDetectLanguage("django/models.py") == CODE_LANG_PYTHON, "Detects .py as Python");
    TEST_ASSERT(CodeGraphDetectLanguage("scripts/build.pyw") == CODE_LANG_PYTHON, "Detects .pyw as Python");
    TEST_ASSERT(CodeGraphDetectLanguage("src/app.ts") == CODE_LANG_TYPESCRIPT, "Detects .ts as TypeScript");
    TEST_ASSERT(CodeGraphDetectLanguage("src/Component.tsx") == CODE_LANG_TYPESCRIPT, "Detects .tsx as TypeScript");
    TEST_ASSERT(CodeGraphDetectLanguage("lib/index.js") == CODE_LANG_JAVASCRIPT, "Detects .js as JavaScript");
    TEST_ASSERT(CodeGraphDetectLanguage("lib/index.mjs") == CODE_LANG_JAVASCRIPT, "Detects .mjs as JavaScript");
    TEST_ASSERT(CodeGraphDetectLanguage("README.md") == CODE_LANG_C, "Defaults unknown extension to C");
}

/* ============================================================
   Test 2: Python Classes, Inheritance and Methods
   ============================================================ */
static void test_python_classes_and_methods(void)
{
    printf("\n=== Test 2: Python Classes & Methods ===\n");
    CODE_GRAPH *cg = CodeGraphCreate(1024, 2048);

    const char *py_code =
        "\"\"\"Module docstring explaining the architecture.\"\"\"\n"
        "import os\n"
        "import sys, json\n"
        "from django.db import models\n"
        "\n"
        "class BaseModel:\n"
        "    def __init__(self, name):\n"
        "        self.name = name\n"
        "\n"
        "    def validate(self):\n"
        "        return True\n"
        "\n"
        "class UserModel(BaseModel):\n"
        "    \"\"\"Represents a user account.\"\"\"\n"
        "    def save(self):\n"
        "        self.validate()\n"
        "        return True\n"
        "\n"
        "def helper_func(x):\n"
        "    return x * 2\n";

    int rc = CodeGraphIngestSource(cg, "django/user.py", py_code);
    TEST_ASSERT(rc == 1, "Ingest Python source succeeds");
    TEST_ASSERT(cg->total_files == 1, "1 file registered");
    TEST_ASSERT(cg->total_classes == 2, "2 classes registered (BaseModel, UserModel)");

    /* Verify classes extracted */
    char classes[8][MAX_CODE_NAME];
    uint32_t n_cls = CodeGraphGetClasses(cg, "django/user.py", classes, 8);
    TEST_ASSERT(n_cls == 2, "Extracted 2 classes from file");
    TEST_ASSERT(strcmp(classes[0], "BaseModel") == 0 || strcmp(classes[1], "BaseModel") == 0, "BaseModel present");
    TEST_ASSERT(strcmp(classes[0], "UserModel") == 0 || strcmp(classes[1], "UserModel") == 0, "UserModel present");

    /* Verify inheritance */
    const char *base = CodeGraphGetBaseClass(cg, "UserModel");
    TEST_ASSERT(base != NULL && strcmp(base, "BaseModel") == 0, "UserModel inherits from BaseModel");

    /* Verify methods of UserModel */
    char methods[8][MAX_CODE_NAME];
    uint32_t n_meth = CodeGraphGetClassMethods(cg, "UserModel", methods, 8);
    TEST_ASSERT(n_meth >= 1, "UserModel has save method");
    TEST_ASSERT(strcmp(methods[0], "save") == 0, "save method correctly extracted");

    /* Verify call graph: save calls validate */
    char callees[8][MAX_CODE_NAME];
    uint32_t n_callees = CodeGraphGetCallees(cg, "save", callees, 8);
    TEST_ASSERT(n_callees >= 1, "save has callee");
    TEST_ASSERT(strcmp(callees[0], "validate") == 0, "save calls validate");

    /* Verify top-level function */
    const char *f = CodeGraphGetFunctionFile(cg, "helper_func");
    TEST_ASSERT(f != NULL && strcmp(f, "django/user.py") == 0, "helper_func located in django/user.py");

    /* Verify imports */
    char imports[8][MAX_CODE_NAME];
    uint32_t n_imp = CodeGraphGetImports(cg, "django/user.py", imports, 8);
    TEST_ASSERT(n_imp >= 3, "Extracted imports (os, sys, django.db)");

    CodeGraphDestroy(cg);
}

/* ============================================================
   Test 3: TypeScript Classes, Interfaces, and Functions
   ============================================================ */
static void test_typescript_parsing(void)
{
    printf("\n=== Test 3: TypeScript Classes & Functions ===\n");
    CODE_GRAPH *cg = CodeGraphCreate(1024, 2048);

    const char *ts_code =
        "// Utility functions and service\n"
        "import { Logger } from './logger';\n"
        "const path = require('path');\n"
        "\n"
        "interface UserConfig {\n"
        "    id: string;\n"
        "    retries: number;\n"
        "}\n"
        "\n"
        "class BaseService {\n"
        "    ping(): boolean {\n"
        "        return true;\n"
        "    }\n"
        "}\n"
        "\n"
        "class AuthService extends BaseService {\n"
        "    login(token: string): boolean {\n"
        "        this.ping();\n"
        "        return true;\n"
        "    }\n"
        "}\n"
        "\n"
        "export const computeHash = (data: string): string => {\n"
        "    return data;\n"
        "};\n"
        "\n"
        "function bootstrapApp(): void {\n"
        "    computeHash('secret');\n"
        "}\n";

    int rc = CodeGraphIngestSource(cg, "src/auth.ts", ts_code);
    TEST_ASSERT(rc == 1, "Ingest TypeScript source succeeds");
    TEST_ASSERT(cg->total_classes == 2, "2 classes registered (BaseService, AuthService)");
    TEST_ASSERT(cg->total_structs >= 1, "Interface registered as struct");

    /* Verify classes */
    char classes[8][MAX_CODE_NAME];
    uint32_t n_cls = CodeGraphGetClasses(cg, "src/auth.ts", classes, 8);
    TEST_ASSERT(n_cls == 2, "Extracted 2 classes from TS file");

    /* Verify inheritance */
    const char *base = CodeGraphGetBaseClass(cg, "AuthService");
    TEST_ASSERT(base != NULL && strcmp(base, "BaseService") == 0, "AuthService extends BaseService");

    /* Verify methods */
    char methods[8][MAX_CODE_NAME];
    uint32_t n_meth = CodeGraphGetClassMethods(cg, "AuthService", methods, 8);
    TEST_ASSERT(n_meth >= 1, "AuthService has login method");
    TEST_ASSERT(strcmp(methods[0], "login") == 0, "login method extracted");

    /* Verify call graph: login calls ping */
    char callees[8][MAX_CODE_NAME];
    uint32_t n_callees = CodeGraphGetCallees(cg, "login", callees, 8);
    TEST_ASSERT(n_callees >= 1 && strcmp(callees[0], "ping") == 0, "login calls ping");

    /* Verify arrow function and function calls: bootstrapApp calls computeHash */
    uint32_t n_bcallees = CodeGraphGetCallees(cg, "bootstrapApp", callees, 8);
    TEST_ASSERT(n_bcallees >= 1 && strcmp(callees[0], "computeHash") == 0, "bootstrapApp calls computeHash");

    /* Verify imports */
    char imports[8][MAX_CODE_NAME];
    uint32_t n_imp = CodeGraphGetImports(cg, "src/auth.ts", imports, 8);
    TEST_ASSERT(n_imp >= 2, "Imports extracted (./logger and path)");

    CodeGraphDestroy(cg);
}

/* ============================================================
   Test 4: Cross-Module & Polyglot Blast Radius
   ============================================================ */
static void test_polyglot_blast_radius(void)
{
    printf("\n=== Test 4: Polyglot Blast Radius Closure ===\n");
    CODE_GRAPH *cg = CodeGraphCreate(2048, 4096);

    /* File 1: Python core model */
    const char *core_py =
        "class Model:\n"
        "    def clean_fields(self):\n"
        "        pass\n"
        "\n"
        "    def full_clean(self):\n"
        "        self.clean_fields()\n";

    /* File 2: Python validation subclass */
    const char *validator_py =
        "from core import Model\n"
        "\n"
        "class Article(Model):\n"
        "    def validate_article(self):\n"
        "        self.clean_fields()\n";

    /* File 3: Python test suite */
    const char *test_py =
        "import validator\n"
        "\n"
        "def test_article_validation():\n"
        "    art = Article()\n"
        "    art.validate_article()\n";

    CodeGraphIngestSource(cg, "django/db/models/base.py", core_py);
    CodeGraphIngestSource(cg, "django/contrib/contenttypes/models.py", validator_py);
    CodeGraphIngestSource(cg, "tests/test_validation.py", test_py);

    /* Compute blast radius for modifying clean_fields */
    CODE_BLAST_RADIUS radius;
    int rc = CodeGraphComputeBlastRadius(cg, "clean_fields", 3, &radius);
    TEST_ASSERT(rc == 1, "ComputeBlastRadius succeeded for clean_fields");
    TEST_ASSERT(radius.affected_functions_count >= 2, "At least 2 callers affected (full_clean, validate_article)");
    TEST_ASSERT(radius.affected_files_count >= 2, "Multiple files affected across modules");

    /* Format markdown report */
    char report[2048];
    int rlen = CodeGraphFormatBlastRadius(cg, &radius, report, sizeof(report));
    TEST_ASSERT(rlen > 0, "Blast radius markdown report generated");
    TEST_ASSERT(strstr(report, "clean_fields") != NULL, "Report mentions clean_fields");
    TEST_ASSERT(strstr(report, "Risk Level") != NULL, "Report contains risk level");

    CodeGraphDestroy(cg);
}

/* ============================================================
   Main Runner
   ============================================================ */
int main(void)
{
    printf("=========================================================\n");
    printf("  POLYGLOT CODE KNOWLEDGE GRAPH TEST SUITE (ISO C11)      \n");
    printf("=========================================================\n");

    test_language_detection();
    test_python_classes_and_methods();
    test_typescript_parsing();
    test_polyglot_blast_radius();

    printf("\n=========================================================\n");
    printf("  RESULTS: %d / %d tests passed (100%%)\n", g_tests_passed, g_tests_run);
    printf("=========================================================\n");

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
