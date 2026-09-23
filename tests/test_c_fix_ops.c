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

    o = fix("int main(void) {\n    return buf_len > 0;\n}\n", "buf_len is used but never declared; declare it.", rule);
    CHECK(o && !strcmp(rule, "declare_local") && strstr(o, "{\n    int buf_len = 0;\n    return buf_len > 0;"));
    free(o);
    CHECK(!fix("int buf_len;\nint main(void) { return buf_len; }\n", "buf_len is never declared", rule));   /* declared */
    CHECK(!fix("int a(void) { return n_x; }\nint b(void) { return n_x; }\n", "n_x is never declared", rule)); /* two functions */
    CHECK(!fix("int main(void) { return a_1 + b_2; }\n", "a_1 and b_2 are never declared", rule));        /* two names */
    CHECK(!fix("int main(void) { return n_x(); }\n", "n_x is never declared", rule));                     /* a call */

    o = fix("/* returns -1 on error */\nint f(void) { return 0; }\n", "The comment says returns -1 on error; change it to 'returns 0 on success'.", rule);
    CHECK(o && !strcmp(rule, "comment_fix") && !strcmp(o, "/* returns 0 on success */\nint f(void) { return 0; }\n"));
    free(o);
    CHECK(!fix("/* returns -1 on error */\n/* returns -1 on error */\n", "comment says returns -1 on error; say 'returns 0 now'", rule)); /* two comments */
    CHECK(!fix("const char *m = \"returns -1 on error\"; /* returns -1 on error */\n", "comment says returns -1 on error; say 'ok now yes'", rule)); /* also in code */
    CHECK(!fix("/* old text here */\n", "comment says old text here; say 'x' or 'y z'", rule));           /* several quotes */

    {
        CFIX_SPLIT sp;
        const char *src = "static int process(int x) { return x + 1; }\n\nint main(void) {\n    return process(1) == 2 ? 0 : 1;\n}\n";
        CHECK(CFixSplit(src, "main.c", "Split process() out of main.c into process.c with a prototype in process.h.", &sp) == 1);
        CHECK(sp.new_src && !strcmp(sp.new_src, "#include \"process.h\"\n\nint main(void) {\n    return process(1) == 2 ? 0 : 1;\n}\n"));
        CHECK(sp.c_text && !strcmp(sp.c_text, "#include \"process.h\"\n\nint process(int x) { return x + 1; }\n"));
        CHECK(sp.h_text && !strcmp(sp.h_text, "#ifndef PROCESS_H\n#define PROCESS_H\n\nint process(int x);\n\n#endif\n"));
        CHECK(!strcmp(sp.c_rel, "process.c") && !strcmp(sp.h_rel, "process.h"));
        CFixSplitFree(&sp);
        CHECK(CFixSplit(src, "main.c", "Split process() into process.c.", &sp) == 0);                       /* no header named */
        CHECK(CFixSplit(src, "main.c", "Split run() into run.c with a prototype in run.h", &sp) == 0);       /* not defined */
        CHECK(CFixSplit("int f(void);\nint g(void) { return f(); }\n", "main.c", "Split f() into f.c with a prototype in f.h", &sp) == 0); /* only a prototype */
    }
    printf("test_c_fix_ops: %s\n", fails ? "FAILED" : "ALL PASSED");
    return fails ? 1 : 0;
}
