/* build_ops.h: deterministic repair rules for build and CI files.
 *
 * Perceive: CMakeLists.txt, Makefiles, shell build scripts and GitHub
 * Actions workflows among the workspace text files.
 * Operate: one general rule, chosen only when the task states it and its
 * precondition holds:
 *   cmake_add_source     - add the C source to an empty add_executable(NAME)
 *   cmake_project        - add project(NAME C) after cmake_minimum_required
 *   cmake_missing_source - create a minimal main for the one missing source
 *   make_dep             - make target all depend on T (built from T.c)
 *   build_compile_step   - make a build script run the compiler, fail closed
 *   ci_ctest             - add a ctest step to the one workflow steps list
 * Verify (caller): really run cmake / make -n / the script in a temp copy;
 * ci_ctest is checked statically.
 * Abstain: 0 when no rule or more than one candidate applies.
 */
#ifndef BUILD_OPS_H
#define BUILD_OPS_H

#include <stddef.h>

typedef struct
{
    int   file;          /* index of the edited file, -1 = create rel */
    char  rel[256];      /* created file (file == -1) or edited file */
    char *text;          /* malloc'd new contents */
    char  rule[32];
    char  detail[160];
    char  expect[128];   /* make_dep: text make -n must show; build_compile_step: the C file */
} BUILD_EDIT;

int  BuildOpsPlan(const char *const *rels, const char *const *datas, int n, const char *task, BUILD_EDIT *out);
/* Evidence mode: the same cmake rules, chosen by cmake's own configure
 * output instead of the task ("No project() command is present", "No
 * SOURCES given to target", "Cannot find source file: F" naming the one
 * missing source). Task wording is not consulted. */
int  BuildOpsPlanEvidence(const char *const *rels, const char *const *datas, int n, const char *cmake_out, BUILD_EDIT *out);
/* static intent on the edited text */
int  BuildOpsIntent(const char *text, const BUILD_EDIT *e);

#endif
