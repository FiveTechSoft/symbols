/* test_compile_repair.c: compiler errors map to the expected single edits,
   and the full loop keeps only a verified repair. */
#include "compile_repair.h"
#include "task_ops.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static TASK_OPS_WORKSPACE ws;

static void set(int i, const char *rel, const char *data)
{
    snprintf(ws.files[i].rel, sizeof(ws.files[i].rel), "%s", rel);
    ws.files[i].data = (char *)data;
    ws.files[i].len = strlen(data);
    if (ws.count <= i) ws.count = i + 1;
}

static int has(CR_CAND *c, int n, const char *rule, const char *needle)
{
    for (int i = 0; i < n; i++)
        if (!strcmp(c[i].rule, rule) &&
            (!needle || (c[i].text && strstr(c[i].text, needle)) || !strcmp(c[i].to, needle)))
            return 1;
    return 0;
}

#ifndef _WIN32
static int solve(const char *main_c, const char *extra_rel, const char *extra, TASK_OPS_REPORT *rep, char *out, size_t osz)
{
    char dir[] = "/tmp/crtestXXXXXX", p[600];
    if (!mkdtemp(dir)) return -1;
    snprintf(p, sizeof(p), "%s/main.c", dir);
    FILE *f = fopen(p, "w"); fputs(main_c, f); fclose(f);
    if (extra_rel) { snprintf(p, sizeof(p), "%s/%s", dir, extra_rel); f = fopen(p, "w"); fputs(extra, f); fclose(f); }
    int kept = TaskOpsSolve(dir, "Make it compile.", rep);
    snprintf(p, sizeof(p), "%s/main.c", dir);
    f = fopen(p, "r"); size_t n = fread(out, 1, osz - 1, f); out[n] = 0; fclose(f);
    char cmd[700]; snprintf(cmd, sizeof(cmd), "rm -rf %s", dir); if (system(cmd)) {}
    return kept;
}
#endif

int main(void)
{
    CR_CAND c[32];
    int n;

    memset(&ws, 0, sizeof(ws));
    set(0, "main.c", "int twice(int x){ return 2*x; }\nint main(void){ return twcie(0); }\n");
    n = CompileRepairCandidates(&ws, "main.c:2:24: error: implicit declaration of function 'twcie'; did you mean 'twice'?\n", c, 32);
    CHECK(has(c, n, "ident_near", "return twice(0)"));
    CompileRepairFree(c, n);

    memset(&ws, 0, sizeof(ws));
    set(0, "main.c", "#include <stdio.h>\nint main(void){ int x = 4\n    printf(\"%d\", x); return 0; }\n");
    n = CompileRepairCandidates(&ws, "main.c:3:5: error: expected ',' or ';' before 'printf'\n", c, 32);
    CHECK(has(c, n, "semicolon", "int x = 4;"));
    CompileRepairFree(c, n);

    memset(&ws, 0, sizeof(ws));
    set(0, "main.c", "#include <stdio.h>\nint main(void){ printf(\"%d\", BUF_SIZE); return 0; }\n");
    set(1, "cfg.h", "#define BUF_SIZE 64\n");
    n = CompileRepairCandidates(&ws, "main.c:2:31: error: 'BUF_SIZE' undeclared (first use in this function)\n", c, 32);
    CHECK(has(c, n, "include_def", "#include \"cfg.h\""));
    CompileRepairFree(c, n);

    memset(&ws, 0, sizeof(ws));
    set(0, "main.c", "int half(int x);\nint main(void){ return half(4) > 1; }\ndouble half(int x){ return x / 2.0; }\n");
    n = CompileRepairCandidates(&ws, "main.c:3:8: error: conflicting types for 'half'; have 'double(int)'\n", c, 32);
    CHECK(has(c, n, "proto_sync", "double half(int x);"));
    CompileRepairFree(c, n);

    memset(&ws, 0, sizeof(ws));
    set(0, "main.c", "int square(int);\nint main(void){ return sqare(3) != 9; }\n");
    set(1, "sq.c", "int square(int x){ return x*x; }\n");
    n = CompileRepairCandidates(&ws, "/usr/bin/ld: /tmp/cc.o: in function `main':\nmain.c:(.text+0x9): undefined reference to `sqare'\n", c, 32);
    CHECK(has(c, n, "ident_near", "square"));
    CompileRepairFree(c, n);

    /* implicit call, definition in another file, no header: no local copy */
    memset(&ws, 0, sizeof(ws));
    set(0, "main.c", "#include <stdio.h>\nint main(void){ return add3(1,2,3) != 6; }\n");
    set(1, "util.c", "int add3(int a, int b, int c){ return a+b+c; }\n");
    n = CompileRepairCandidates(&ws, "main.c:2:24: error: implicit declaration of function 'add3' [-Werror=implicit-function-declaration]\n", c, 32);
    CHECK(!has(c, n, "proto_add", NULL));
    CompileRepairFree(c, n);

    /* ... and in the one header both files include, when there is one (tier 1) */
    memset(&ws, 0, sizeof(ws));
    set(0, "src/main.c", "#include \"util.h\"\nint main(void){ return twice(4) != 8; }\n");
    set(1, "src/util.c", "#include \"util.h\"\nint twice(int x){ return 2*x; }\n");
    set(2, "src/util.h", "#ifndef UTIL_H\n#define UTIL_H\nint half(int x);\n#endif\n");
    n = CompileRepairCandidates(&ws, "src/main.c:2:24: error: implicit declaration of function 'twice'\n", c, 32);
    CHECK(has(c, n, "proto_add", "int half(int x);\nint twice(int x);\n#endif"));
    CompileRepairFree(c, n);

    /* static in another file, or defined twice: no prototype candidate */
    memset(&ws, 0, sizeof(ws));
    set(0, "main.c", "int main(void){ return hid(); }\n");
    set(1, "a.c", "static int hid(void){ return 0; }\n");
    n = CompileRepairCandidates(&ws, "main.c:1:24: error: implicit declaration of function 'hid'\n", c, 32);
    CHECK(!has(c, n, "proto_add", NULL));
    CompileRepairFree(c, n);
    set(1, "a.c", "int hid(void){ return 0; }\n");
    set(2, "b.c", "int hid(void){ return 1; }\n");
    n = CompileRepairCandidates(&ws, "main.c:1:24: error: implicit declaration of function 'hid'\n", c, 32);
    CHECK(!has(c, n, "proto_add", NULL));
    CompileRepairFree(c, n);

    /* nothing near: no candidate */
    memset(&ws, 0, sizeof(ws));
    set(0, "main.c", "int main(void){ return zzqq; }\n");
    n = CompileRepairCandidates(&ws, "main.c:1:24: error: 'zzqq' undeclared (first use in this function)\n", c, 32);
    CHECK(n == 0);

#ifndef _WIN32   /* end-to-end loop: POSIX temp dirs */
    if (system("gcc --version >/dev/null 2>&1") == 0) {
        TASK_OPS_REPORT rep;
        char out[4096];
        memset(&rep, 0, sizeof(rep));
        int kept = solve("int main(void){\n    int s = 0;\n    for (i = 0; i < 3; i++) s += i;\n    return s != 3;\n}\n", NULL, NULL, &rep, out, sizeof(out));
        CHECK(kept == 1 && !strcmp(rep.op, "compile_repair") && strstr(out, "for (int i = 0;"));
        /* two equally near names: compile_repair abstains (a later
           operator may still act on the compiler's own suggestion) */
        memset(&rep, 0, sizeof(rep));
        kept = solve("int cat1 = 1, cat2 = 2;\nint main(void){ return cat3 - 1; }\n", NULL, NULL, &rep, out, sizeof(out));
        CHECK(strcmp(rep.op, "compile_repair") != 0);
    }
#endif
    printf("%s\n", fails ? "FAILED" : "OK");
    return fails != 0;
}
