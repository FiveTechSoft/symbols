/* test_c_fix_ops.c: C fault repairs apply exactly and abstain when unsure. */
#include "c_fix_ops.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static char *fix(const char *src, const char *task, char *rule)
{
    char detail[128];
    return CFixApply(src, task, rule, 32, detail, sizeof(detail));
}

int main(void)
{
    char rule[32];
    char *o;
    o = fix("for (int i = 0; i <= n; i++) x[i] = 0;\n", "Off-by-one in the loop bound.", rule);
    CHECK(o && !strcmp(rule, "loop_bound") && strstr(o, "i < n;"));
    free(o);
    o = fix("for (i = 0; i < n; i++) f();\n", "fix the loop bound", rule);
    CHECK(o && strstr(o, "i <= n;"));
    free(o);
    CHECK(!fix("for (i=0;i<n;i++) a();\nfor (j=0;j<m;j++) b();\n", "loop bound off-by-one", rule));   /* two loops */
    CHECK(!fix("for (i=0;i<n && i!=k;i++) a();\n", "loop bound", rule));                              /* compound condition */
    CHECK(!fix("for (i=0;i<=n;i++) a();\n", "make it faster", rule));                                 /* not stated */

    o = fix("char buf[4] = \"abcdef\";\n", "The buffer is too small.", rule);
    CHECK(o && !strcmp(rule, "array_fit") && strstr(o, "char buf[7] = \"abcdef\";"));
    free(o);
    CHECK(!fix("char buf[8] = \"abc\";\n", "buffer too small", rule));                                  /* already fits */
    CHECK(!fix("char a[2] = \"xyz\";\nchar b[2] = \"xyz\";\n", "buffer size too small", rule));        /* two candidates */
    CHECK(!fix("char a[2] = \"\\x41\\x42\";\n", "buffer too small", rule));                           /* escapes: abstain */

    o = fix("int f(void) {\n    if (!ok) goto fail;\n    return 0;\nfail:\n    return 1;\n}\n", "Replace goto; no goto.", rule);
    CHECK(o && !strcmp(rule, "goto_return") && !strstr(o, "goto") && !strstr(o, "fail:") && strstr(o, "if (!ok) {\n        return 1;\n    }"));
    free(o);
    CHECK(!fix("int f(void) {\n  if (a) goto e;\n  if (b) goto e;\n  return 0;\ne:\n  return 1;\n}\n", "remove goto", rule));  /* two gotos */
    CHECK(!fix("int f(void) {\n  if (a) goto e;\n  return 0;\ne:\n  cleanup();\n  return 1;\n}\n", "remove goto", rule));    /* label does more */
    printf("test_c_fix_ops: %s\n", fails ? "FAILED" : "ALL PASSED");
    return fails ? 1 : 0;
}
