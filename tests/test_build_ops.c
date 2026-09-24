/* test_build_ops.c: build/CI rules plan, and abstain when unsure. */
#include "build_ops.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static int plan(int n, const char **r, const char **d, const char *task, BUILD_EDIT *e)
{
    return BuildOpsPlan(r, d, n, task, e);
}

int main(void)
{
    BUILD_EDIT e;

    const char *r1[] = {"CMakeLists.txt", "main.c"};
    const char *d1[] = {"cmake_minimum_required(VERSION 3.10)\nproject(x C)\nadd_executable(x)\n", "int main(void){return 0;}\n"};
    CHECK(plan(2, r1, d1, "add_executable is empty; add main.c", &e) && !strcmp(e.rule, "cmake_add_source") &&
          strstr(e.text, "add_executable(x main.c)"));
    free(e.text);
    CHECK(!plan(2, r1, d1, "Make the build faster.", &e));                   /* rule not stated */

    const char *r2[] = {"CMakeLists.txt", "a.c", "b.c"};
    const char *d2[] = {d1[0], "int a;", "int b;"};
    CHECK(!plan(3, r2, d2, "add_executable is empty", &e));                  /* which source? abstain */

    const char *r3[] = {"CMakeLists.txt"};
    const char *d3[] = {"cmake_minimum_required(VERSION 3.10)\n"};
    CHECK(plan(1, r3, d3, "Add a project(<name> C) line.", &e) && !strcmp(e.rule, "cmake_project") &&
          !strcmp(e.text, "cmake_minimum_required(VERSION 3.10)\nproject(app C)\n") && BuildOpsIntent(e.text, &e));
    free(e.text);

    const char *d4[] = {"cmake_minimum_required(VERSION 3.10)\nproject(d C)\nadd_executable(d main.c)\n"};
    CHECK(plan(1, r3, d4, "main.c is missing; recreate it.", &e) && e.file == -1 && !strcmp(e.rel, "main.c"));
    free(e.text);
    CHECK(!plan(1, r3, d4, "the source is missing", &e));                    /* file not named */

    const char *r5[] = {"Makefile", "app.c"};
    const char *d5[] = {"all:\n\t@echo done\n", "int main(void){return 0;}\n"};
    CHECK(plan(2, r5, d5, "all does not depend on app.", &e) && !strcmp(e.rule, "make_dep") &&
          !strcmp(e.text, "all: app\n\napp: app.c\n\t$(CC) -o app app.c\n"));
    free(e.text);
    const char *d5b[] = {"all: lib\n", "int main(void){return 0;}\n"};
    CHECK(!plan(2, r5, d5b, "all should depend on app", &e));               /* has prerequisites */
    CHECK(!plan(2, r5, d5, "all should depend on tool", &e));               /* no tool.c */

    const char *r6[] = {"build.sh", "main.c"};
    const char *d6[] = {"#!/bin/sh\necho compile\nexit 0\n", "int main(void){return 0;}\n"};
    CHECK(plan(2, r6, d6, "If the compiler fails the script must fail.", &e) && !strcmp(e.rule, "build_compile_step") &&
          !strcmp(e.text, "#!/bin/sh\nset -e\necho compile\ncc -fsyntax-only main.c\n") && BuildOpsIntent(e.text, &e));
    free(e.text);
    CHECK(plan(2, r6, d6, "Run gcc -std=c11 -Wall on main.c.", &e) && strstr(e.text, "gcc -std=c11 -Wall main.c\n"));
    free(e.text);
    const char *d6b[] = {"#!/bin/sh\ngcc main.c\n", "int main;"};
    CHECK(!plan(2, r6, d6b, "the compiler must fail the script", &e));      /* already compiles */

    const char *r7[] = {"ci.yml"};
    const char *d7[] = {"jobs:\n  build:\n    steps:\n      - uses: actions/checkout@v4\n        with:\n          x: 1\n"};
    CHECK(plan(1, r7, d7, "add a ctest step", &e) && !strcmp(e.rule, "ci_ctest") &&
          strstr(e.text, "          x: 1\n      - run: ctest --test-dir build\n"));
    free(e.text);
    const char *d7b[] = {"jobs:\n  a:\n    steps:\n      - run: x\n  b:\n    steps:\n      - run: y\n"};
    CHECK(!plan(1, r7, d7b, "add a ctest step", &e));                       /* two steps lists */


    {   /* evidence mode: cmake's own output picks the rule, task wording is not read */
        const char *er[] = {"CMakeLists.txt", "main.c"};
        const char *ed1[] = {"cmake_minimum_required(VERSION 3.10)\nproject(demo C)\nadd_executable(demo)\n", "int main(void) { return 0; }\n"};
        CHECK(BuildOpsPlanEvidence(er, ed1, 2, "CMake Error at CMakeLists.txt:3 (add_executable):\n  No SOURCES given to target: demo\n", &e) == 1 &&
              !strcmp(e.rule, "cmake_add_source") && strstr(e.text, "add_executable(demo main.c)"));
        free(e.text);
        CHECK(!BuildOpsPlanEvidence(er, ed1, 2, "-- Configuring done\n", &e));                         /* no diagnostic */
        const char *er3[] = {"CMakeLists.txt", "a.c", "b.c"};
        const char *ed3[] = {ed1[0], "int main(void) { return 0; }\n", "int f(void) { return 0; }\n"};
        CHECK(!BuildOpsPlanEvidence(er3, ed3, 3, "No SOURCES given to target: demo\n", &e));          /* two C files */
        const char *ed2[] = {"cmake_minimum_required(VERSION 3.10)\n", "int main(void) { return 0; }\n"};
        CHECK(BuildOpsPlanEvidence(er, ed2, 2, "CMake Warning (dev) in CMakeLists.txt:\n  No project() command is present.\n", &e) == 1 &&
              !strcmp(e.rule, "cmake_project") && strstr(e.text, "project(app C)"));
        free(e.text);
        const char *er4[] = {"CMakeLists.txt"};
        const char *ed4[] = {"cmake_minimum_required(VERSION 3.10)\nproject(demo C)\nadd_executable(demo main.c)\n"};
        CHECK(BuildOpsPlanEvidence(er4, ed4, 1, "CMake Error at CMakeLists.txt:3 (add_executable):\n  Cannot find source file:\n\n    main.c\n", &e) == 1 &&
              !strcmp(e.rule, "cmake_missing_source") && e.file == -1 && !strcmp(e.rel, "main.c"));
        free(e.text);
        CHECK(!BuildOpsPlanEvidence(er4, ed4, 1, "Cannot find source file:\n\n    other.c\n", &e));    /* names a different file */
        CHECK(!BuildOpsPlanEvidence(er4, ed4, 1, NULL, &e));
    }
    printf("test_build_ops: %s\n", fails ? "FAILED" : "ALL PASSED");
    return fails ? 1 : 0;
}
