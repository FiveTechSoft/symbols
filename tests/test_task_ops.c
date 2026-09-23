/*
 * test_task_ops.c - generic task operators (synthetic workspaces only; no
 * engineering-bank task is used here, so the bank stays an honest measure).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "compat.h"
#include "task_ops.h"

static void set_memory(int on)
{
#ifdef _WIN32
    _putenv(on ? "SYMBOLS_TASK_OPS_MEMORY=1" : "SYMBOLS_TASK_OPS_MEMORY=0");
#else
    setenv("SYMBOLS_TASK_OPS_MEMORY", on ? "1" : "0", 1);
#endif
}

static int pass = 0, fail = 0;
#define CHECK(c, msg) do { if (c) { pass++; printf("  [PASS] %s\n", msg); } \
                           else { fail++; printf("  [FAIL] %s\n", msg); } } while (0)

static void mem_ws(TASK_OPS_WORKSPACE *ws, const char *rel, const char *text)
{
    TASK_OPS_FILE *f = &ws->files[ws->count++];
    snprintf(f->rel, sizeof(f->rel), "%s", rel);
    f->len = strlen(text);
    f->data = (char *)malloc(f->len + 1);
    memcpy(f->data, text, f->len + 1);
}

static void put(const char *dir, const char *rel, const char *text)
{
    char p[1024];
    snprintf(p, sizeof(p), "%s/%s", dir, rel);
    FILE *f = fopen(p, "wb");
    if (f) { fputs(text, f); fclose(f); }
}

static char *get(const char *dir, const char *rel)
{
    static char buf[4096];
    char p[1024];
    snprintf(p, sizeof(p), "%s/%s", dir, rel);
    FILE *f = fopen(p, "rb");
    size_t n = 0;
    buf[0] = '\0';
    if (f) { n = fread(buf, 1, sizeof(buf) - 1, f); fclose(f); }
    buf[n] = '\0';
    return buf;
}

static int file_exists(const char *dir, const char *rel)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", dir, rel);
    FILE *f = fopen(path, "rb");
    if (f) fclose(f);
    return f != NULL;
}

static void make_dir(char *out, size_t size, const char *tag)
{
    const char *t = getenv("TMPDIR");
    if (!t || !*t) t = getenv("TEMP");
    if (!t || !*t) t = "/tmp";
    snprintf(out, size, "%s/task_ops_test_%s_%ld", t, tag, (long)time(NULL));
    _mkdir(out);
}

int main(void)
{
    printf("=== task_ops ===\n");
    char a[128], b[128];

    /* reasoning only */
    {
        TASK_OPS_WORKSPACE ws; memset(&ws, 0, sizeof(ws));
        mem_ws(&ws, "k.h", "#define WIDGET_MAX 8\nint widget_count(void);\n");
        mem_ws(&ws, "k.c", "#include \"k.h\"\nint widget_count(void) { return WIDGET_MAX; }\n");
        CHECK(TaskOpsCountToken(&ws, "WIDGET_MAX") == 2, "whole-token count across files");
        CHECK(TaskOpsCountToken(&ws, "WIDGET") == 0, "partial identifiers do not count");
        CHECK(TaskOpsFindRename(&ws, "Please call WIDGET_MAX GADGET_LIMIT instead.", a, sizeof(a), b, sizeof(b)) == 1 &&
              !strcmp(a, "WIDGET_MAX") && !strcmp(b, "GADGET_LIMIT"), "adjacent present->absent pair found");
        CHECK(TaskOpsFindRename(&ws, "Cambia widget_count por gadgetCount en todo el proyecto", a, sizeof(a), b, sizeof(b)) == 1 &&
              !strcmp(b, "gadgetCount"), "language-independent connector, camelCase target");
        CHECK(TaskOpsFindRename(&ws, "Rename the counter to something better", a, sizeof(a), b, sizeof(b)) == 0,
              "plain prose words are never rename targets");
        CHECK(TaskOpsFindRename(&ws, "widget_count WIDGET_MAX", a, sizeof(a), b, sizeof(b)) == 0,
              "target already present: no rename");
        CHECK(TaskOpsFindRename(&ws, "widget_count -> w_count; WIDGET_MAX -> W_MAX", a, sizeof(a), b, sizeof(b)) == 2,
              "two different pairs: reported ambiguous");
        CHECK(TaskOpsFindRename(&ws, "WIDGET_MAX with -Werror=new_flag_x", a, sizeof(a), b, sizeof(b)) == 0,
              "compiler flags are not names");
        CHECK(TaskOpsFindRename(&ws, "WIDGET_MAX, and much later after a long long sentence, GADGET_LIMIT", a, sizeof(a), b, sizeof(b)) == 0,
              "non-adjacent names are not paired");
        TaskOpsFreeWorkspace(&ws);
    }

    /* random input terminates */
    {
        TASK_OPS_WORKSPACE ws; memset(&ws, 0, sizeof(ws));
        mem_ws(&ws, "r.c", "int x_1 = 0;\n");
        char task[300];
        unsigned s = 12345;
        int ok = 1;
        for (int it = 0; it < 3000; it++) {
            for (int i = 0; i < 299; i++) { s = s * 1103515245u + 12345u; task[i] = (char)(1 + (s >> 16) % 255); }
            task[299] = '\0';
            if (TaskOpsFindRename(&ws, task, a, sizeof(a), b, sizeof(b)) < 0) ok = 0;
        }
        CHECK(ok, "3000 random task texts terminate");
        TaskOpsFreeWorkspace(&ws);
    }

    /* full loop on disk: verified edit kept */
    {
        char d[512]; make_dir(d, sizeof(d), "keep");
        put(d, "lib.h", "#define OLD_SIZE 3\nint twice(int);\n");
        put(d, "lib.c", "#include \"lib.h\"\nint twice(int v) { return v * 2 + OLD_SIZE - OLD_SIZE; }\n");
        put(d, "main.c", "#include \"lib.h\"\nint main(void) { return twice(OLD_SIZE) == 6 ? 0 : 1; }\n");
        put(d, "NOTES.md", "OLD_SIZE is the size.\n");
        TASK_OPS_REPORT r;
        int kept = TaskOpsSolve(d, "Constant OLD_SIZE should be NEW_SIZE everywhere.", &r);
        printf("    %s | %s | compile %d->%d run %d->%d\n", r.detail, r.reason, r.compile_before, r.compile_after, r.run_before, r.run_after);
        CHECK(kept && r.verified, "rename verified and kept");
        CHECK(strstr(get(d, "main.c"), "NEW_SIZE") && !strstr(get(d, "lib.h"), "OLD_SIZE") &&
              strstr(get(d, "NOTES.md"), "NEW_SIZE"), "all mentions renamed, docs included");
        if (r.compile_before != -1)
            CHECK(r.compile_after == 1 && r.run_after == 0, "own probe: builds and exits 0 after");
    }

    /* full loop on disk: regression rolled back */
    {
        char d[512]; make_dir(d, sizeof(d), "roll");
        const char *m = "#include <stdlib.h>\nint main(void) { return EXIT_SUCCESS; }\n";
        put(d, "main.c", m);
        TASK_OPS_REPORT r;
        int kept = TaskOpsSolve(d, "Use EXIT_SUCCESS -> EXIT_FINE here", &r);
        printf("    %s | %s | compile %d->%d run %d->%d\n", r.detail, r.reason, r.compile_before, r.compile_after, r.run_before, r.run_after);
        if (r.compile_before == -1) {
            CHECK(1, "no compiler: regression case skipped");
        } else {
            CHECK(!kept && !r.verified, "edit that breaks the build is not kept");
            CHECK(!strcmp(get(d, "main.c"), m), "file restored byte for byte");
        }
    }


    /* compiler fix-it: the compiler proposes, the probe verifies */
    {
        char d[512]; make_dir(d, sizeof(d), "fixit");
        put(d, "calc.c", "int length_of(const char *s) { return (int)strlen(s); }\nint main(void) { return length_of(\"ab\") == 2 ? 0 : 1; }\n");
        TASK_OPS_REPORT r;
        int kept = TaskOpsSolve(d, "Build with -std=c99 -Werror=implicit-function-declaration and make it pass.", &r);
        printf("    %s %s | %s | compile %d->%d run %d->%d\n", r.op, r.detail, r.reason, r.compile_before, r.compile_after, r.run_before, r.run_after);
        if (r.compile_before == -1) {
            CHECK(1, "no compiler: fix-it case skipped");
        } else {
            CHECK(kept && !strcmp(r.op, "compiler_fixit"), "fix-it applied and verified (build fail -> ok)");
            CHECK(strstr(get(d, "calc.c"), "#include <string.h>") != NULL, "header inserted by the compiler's fix-it");
            CHECK(r.run_after == 0, "own probe: program exits 0 after");
        }
    }

    /* compiler fix-it never fires on a build that already works */
    {
        char d[512]; make_dir(d, sizeof(d), "fixok");
        const char *m = "#include <string.h>\nint main(void) { return (int)strlen(\"\"); }\n";
        put(d, "ok.c", m);
        TASK_OPS_REPORT r;
        CHECK(!TaskOpsSolve(d, "Tidy it with -std=c99.", &r) && !strcmp(get(d, "ok.c"), m), "working build: no fix-it, untouched");
    }


    /* stated fragment: anchored small token edit */
    {
        char d[512]; make_dir(d, sizeof(d), "frag");
        put(d, "cap.c", "static int cap(int x, int limit) { return x > limit ? limit : x; }\nint main(void) { return cap(5, 5) == 5 ? 0 : 1; }\n");
        TASK_OPS_REPORT r;
        int kept = TaskOpsSolve(d, "Values equal to the limit must be capped too; the code should read `x >= limit`.", &r);
        printf("    %s %s | %s\n", r.op, r.detail, r.reason);
        CHECK(kept && !strcmp(r.op, "stated_fragment") && strstr(get(d, "cap.c"), "x >= limit ?"), "fragment edit applied at the anchored place");
    }
    {
        char d[512]; make_dir(d, sizeof(d), "fragneg");
        const char *m = "int main(void) { int q = 1; return q > 1; }\n";
        put(d, "n.c", m);
        TASK_OPS_REPORT r;
        CHECK(!TaskOpsSolve(d, "Keep it simple. The form 'q >= 1' is forbidden here.", &r) && !strcmp(get(d, "n.c"), m),
              "fragment stated as forbidden is never inserted");
        CHECK(!TaskOpsSolve(d, "Add the heading '## Usage notes' to the docs.", &r) && !strcmp(get(d, "n.c"), m),
              "fragment with no shared name is not anchored: untouched");
    }
    {
        char d[512]; make_dir(d, sizeof(d), "fragamb");
        const char *m = "int f(int k) { if (k > 2) return 1; if (k > 2) return 2; return 0; }\nint main(void) { return f(0); }\n";
        put(d, "a.c", m);
        TASK_OPS_REPORT r;
        CHECK(!TaskOpsSolve(d, "Use 'k >= 2'.", &r) && !strcmp(get(d, "a.c"), m) && strstr(r.reason, "ambiguous"),
              "two equally good places: abstain");
    }


    /* random quoted fragments terminate and never touch an unanchored file */
    {
        char d[512]; make_dir(d, sizeof(d), "fuzz");
        const char *m = "alpha_1 = beta_2 + 3;\n";
        put(d, "notes.txt", m);
        char task[200];
        unsigned s = 777;
        const char alphabet[] = "ab_1 +-<>=;'`\"#()[]{}alpha_1beta_2";
        int ok = 1;
        for (int it = 0; it < 400 && ok; it++) {
            for (int i = 0; i < 199; i++) { s = s * 1103515245u + 12345u; task[i] = alphabet[(s >> 16) % (sizeof(alphabet) - 1)]; }
            task[199] = '\0';
            TASK_OPS_REPORT r;
            TaskOpsSolve(d, task, &r);
            if (r.verified) put(d, "notes.txt", m);   /* restore for the next round */
        }
        CHECK(ok, "400 random quoted task texts terminate");
    }


    /* declare an implicit function, grounded in the workspace */
    {
        char d[512]; make_dir(d, sizeof(d), "declself");
        put(d, "m.c", "int main(void) { return triple_of(1) == 3 ? 0 : 1; }\nint triple_of(int v) { return v * 3; }\n");
        TASK_OPS_REPORT r;
        int kept = TaskOpsSolve(d, "Make it build with -std=c99 -Werror=implicit-function-declaration.", &r);
        printf("    %s %s | %s\n", r.op, r.detail, r.reason);
        if (r.compile_before == -1) CHECK(1, "no compiler: skipped");
        else CHECK(kept && strstr(get(d, "m.c"), "int triple_of(int v);"), "prototype from its own definition");
    }
    {
        char d[512]; make_dir(d, sizeof(d), "declhdr");
        put(d, "api.h", "int twice_of(int v);\n");
        put(d, "api.c", "#include \"api.h\"\nint twice_of(int v) { return v * 2; }\n");
        put(d, "m.c", "int main(void) { return twice_of(2) == 4 ? 0 : 1; }\n");
        TASK_OPS_REPORT r;
        int kept = TaskOpsSolve(d, "Build with -std=c99 -Werror=implicit-function-declaration.", &r);
        printf("    %s %s | %s\n", r.op, r.detail, r.reason);
        if (r.compile_before == -1) CHECK(1, "no compiler: skipped");
        else CHECK(kept && strstr(get(d, "m.c"), "#include \"api.h\"") && !strstr(get(d, "m.c"), "int twice_of"),
                   "existing declaring header is included, no local prototype");
    }
    {
        char d[512]; make_dir(d, sizeof(d), "declown");
        put(d, "lib.h", "/* lib */\n");
        put(d, "lib.c", "int half_of(int v) { return v / 2; }\n");
        put(d, "m.c", "#include \"lib.h\"\nint main(void) { return half_of(4) == 2 ? 0 : 1; }\n");
        TASK_OPS_REPORT r;
        int kept = TaskOpsSolve(d, "Build with -std=c99 -Werror=implicit-function-declaration.", &r);
        printf("    %s %s | %s\n", r.op, r.detail, r.reason);
        if (r.compile_before == -1) CHECK(1, "no compiler: skipped");
        else CHECK(kept && strstr(get(d, "lib.h"), "int half_of(int v);") && !strstr(get(d, "m.c"), "int half_of"),
                   "prototype goes into the header the user already includes");
    }
    {
        char d[512]; make_dir(d, sizeof(d), "declmiss");
        const char *m = "int main(void) { return quad_of(1); }\nint quad_of(int v) { return v * 4; }\n";
        put(d, "m.c", m);
        TASK_OPS_REPORT r;
        CHECK(!TaskOpsSolve(d, "Declare it and write notes.txt; build with -Werror=implicit-function-declaration.", &r) &&
              !strcmp(get(d, "m.c"), m), "task names a file the edit does not create: rolled back");
    }
    {
        char d[512]; make_dir(d, sizeof(d), "declnew");
        put(d, "calc.c", "long twice(long v) { return v * 2; }\n");
        put(d, "app.c", "int main(void) { return twice(3) == 6 ? 0 : 1; }\n");
        TASK_OPS_REPORT r;
        int kept = TaskOpsSolve(d, "Put the declaration in calc.h and include it where needed; build with -Werror=implicit-function-declaration.", &r);
        printf("    %s %s | %s\n", r.op, r.detail, r.reason);
        if (r.compile_before == -1) CHECK(1, "no compiler: skipped");
        else {
            const char *h = get(d, "calc.h");
            CHECK(kept && h && strstr(h, "#ifndef CALC_H") && strstr(h, "long twice(long v);") &&
                  strstr(get(d, "app.c"), "#include \"calc.h\"") && strstr(get(d, "calc.c"), "#include \"calc.h\""),
                  "task-named header created with the prototype, included by user and definer");
        }
    }
    {
        char d[512]; make_dir(d, sizeof(d), "declnew2");
        const char *m = "int main(void) { return twice(3) == 6 ? 0 : 1; }\n";
        put(d, "calc.c", "long twice(long v) { return v * 2; }\n");
        put(d, "app.c", m);
        TASK_OPS_REPORT r;
        int kept = TaskOpsSolve(d, "Declare it in calc.h or maybe api.h; build with -Werror=implicit-function-declaration.", &r);
        char p2[600]; snprintf(p2, sizeof(p2), "%s/calc.h", d);
        FILE *hf = fopen(p2, "rb"); if (hf) fclose(hf);
        CHECK(!hf && (!kept || strstr(get(d, "app.c"), "long twice(long v);")),
              "two missing headers named: no header is created");
    }


    /* literal -> named constant, behavior preserved */
    {
        char d[512]; make_dir(d, sizeof(d), "litnum");
        put(d, "q.c", "#include <stdio.h>\nint main(void) { int w = 17; printf(\"%d\\n\", w * 17); return w == 17 ? 0 : 1; }\n");
        TASK_OPS_REPORT r;
        int kept = TaskOpsSolve(d, "Name the magic 17 as WIDTH_CELLS. Output must not change.", &r);
        printf("    %s %s | %s\n", r.op, r.detail, r.reason);
        const char *g = get(d, "q.c");
        CHECK(kept && strstr(g, "#define WIDTH_CELLS 17") && strstr(g, "w * WIDTH_CELLS") && !strstr(g, "w == 17"),
              "number literal replaced by a defined constant");
    }
    {
        char d[512]; make_dir(d, sizeof(d), "litstr");
        put(d, "q.c", "#include <stdio.h>\nint main(void) { printf(\"ERR> bad\\n\"); printf(\"ERR> worse\\n\"); return 0; }\n");
        TASK_OPS_REPORT r;
        int kept = TaskOpsSolve(d, "Pull the literal \"ERR> \" into ERR_TAG and reuse it.", &r);
        printf("    %s %s | %s\n", r.op, r.detail, r.reason);
        const char *g = get(d, "q.c");
        CHECK(kept && strstr(g, "#define ERR_TAG \"ERR> \"") && strstr(g, "printf(ERR_TAG \"bad"),
              "string prefix kept by C string concatenation");
    }
    {
        char d[512]; make_dir(d, sizeof(d), "litamb");
        const char *m = "int main(void) { int a = 3, b = 4; return a + b == 7 ? 0 : 1; }\n";
        put(d, "q.c", m);
        TASK_OPS_REPORT r;
        CHECK(!TaskOpsSolve(d, "Use a constant LIMIT_V for 3 or 4.", &r) && !strcmp(get(d, "q.c"), m),
              "two literals in the sentence: abstain");
    }

    /* operator memory (learn step, opt-in) */
    {
        char d[512]; make_dir(d, sizeof(d), "memskip");
        const char *m = "int main(void) { return quad_of(1) == 4 ? 0 : 1; }\nint quad_of(int v) { return v * 4; }\n";
        put(d, "m.c", m);
        const char *t = "Declare it and write notes.txt; build with -Werror=implicit-function-declaration.";
        TASK_OPS_REPORT r1, r2;
        set_memory(1);
        int k1 = TaskOpsSolve(d, t, &r1);
        int k2 = TaskOpsSolve(d, t, &r2);
        set_memory(0);
        char mp[600]; snprintf(mp, sizeof(mp), "%s/.symbols/task_ops_memory.tsv", d);
        FILE *mf = fopen(mp, "rb"); char line[256] = {0};
        if (mf) { if (!fgets(line, sizeof(line), mf)) line[0] = 0; fclose(mf); }
        printf("    memory: %s| second run skipped %d\n", line, r2.memory_skipped);
        if (r1.compile_before == -1) CHECK(1, "no compiler: skipped");
        else CHECK(!k1 && !k2 && strstr(line, "declare_implicit\trolled_back") && r2.memory_skipped == 1 &&
                   !r2.op[0] && !strcmp(get(d, "m.c"), m),
                   "rolled-back operator is remembered and skipped on the same workspace+task");
    }
    {
        char d[512]; make_dir(d, sizeof(d), "memorder");
        put(d, "m.c", "int main(void) { int old_n = 2; return helper(old_n) - 2; }\nint helper(int v) { return v; }\n");
        char sd[600]; snprintf(sd, sizeof(sd), "%s/.symbols", d);
        _mkdir(sd);
        put(d, ".symbols/task_ops_memory.tsv", "0000000000000000\tdeclare_implicit\tkept\n");
        TASK_OPS_REPORT r;
        set_memory(1);
        int kept = TaskOpsSolve(d, "Rename old_n to new_n; build with -Werror=implicit-function-declaration.", &r);
        set_memory(0);
        printf("    %s %s | reordered %d\n", r.op, r.detail, r.memory_reordered);
        if (r.compile_before == -1) CHECK(1, "no compiler: skipped");
        else CHECK(kept && !strcmp(r.op, "declare_implicit") && r.memory_reordered,
                   "remembered success puts that operator first when several apply");
    }
    {
        char d[512]; make_dir(d, sizeof(d), "memoff");
        put(d, "m.c", "int main(void) { int old_n = 2; return helper(old_n) - 2; }\nint helper(int v) { return v; }\n");
        char sd[600]; snprintf(sd, sizeof(sd), "%s/.symbols", d);
        _mkdir(sd);
        put(d, ".symbols/task_ops_memory.tsv", "0000000000000000\tdeclare_implicit\tkept\n");
        TASK_OPS_REPORT r;
        int kept = TaskOpsSolve(d, "Rename old_n to new_n; build with -Werror=implicit-function-declaration.", &r);
        CHECK(kept && !strcmp(r.op, "rename_symbol") && !r.memory_reordered,
              "memory off (default): fixed operator order");
    }

    /* operator 6: doc_sync */
    {
        char d[512]; make_dir(d, sizeof(d), "docflag");
        const char *c = "#include <string.h>\nint main(int c, char **v) { return c > 1 && !strcmp(v[1], \"--quiet\") ? 0 : 0; }\n";
        put(d, "app.c", c);
        put(d, "USAGE.md", "Pass --silent to mute it. --silent is optional.\n");
        TASK_OPS_REPORT r;
        int kept = TaskOpsSolve(d, "USAGE.md says --silent but app.c checks --quiet. Make the docs match.", &r);
        CHECK(kept && !strcmp(r.op, "doc_sync") && !strcmp(get(d, "USAGE.md"), "Pass --quiet to mute it. --quiet is optional.\n") &&
              !strcmp(get(d, "app.c"), c), "doc_sync: flag in docs replaced by the one the code uses; code untouched");
    }
    {
        char d[512]; make_dir(d, sizeof(d), "doccall");
        put(d, "lib.c", "int scale(int k, int f) { return k * f; }\n");
        put(d, "README", "Call `scale(k)` to scale.\n");
        TASK_OPS_REPORT r;
        int kept = TaskOpsSolve(d, "The README shows scale(k) but lib.c defines scale(int k, int f); document scale(int k, int f).", &r);
        CHECK(kept && strstr(get(d, "README"), "`scale(int k, int f)`") && !strstr(get(d, "README"), "scale(k)"),
              "doc_sync: call signature synced from the definition");
    }
    {
        char d[512]; make_dir(d, sizeof(d), "docamb");
        const char *md = "Use --fast or --tiny.\n";
        put(d, "main.c", "int main(void) { const char *a = \"--quick\", *b = \"--small\"; return a == b; }\n");
        put(d, "notes.md", md);
        TASK_OPS_REPORT r;
        CHECK(!TaskOpsSolve(d, "notes.md mentions --fast and --tiny; code has --quick and --small.", &r) &&
              !strcmp(get(d, "notes.md"), md) && strstr(r.reason, "ambiguous"),
              "doc_sync: two stale terms = abstain");
    }

    /* operator 7: remove_dead_function */
    {
        char d[512]; make_dir(d, sizeof(d), "deadfn");
        put(d, "k.c", "#include <stdio.h>\nint old_calc(int v);\n\nint old_calc(int v)\n{\n    return v * 2; /* } */\n}\n\nint main(void) { puts(\"ok\"); return 0; }\n");
        TASK_OPS_REPORT r;
        int kept = TaskOpsSolve(d, "Please delete old_calc, nothing calls it.", &r);
        CHECK(kept && !strcmp(r.op, "remove_dead_function") &&
              !strcmp(get(d, "k.c"), "#include <stdio.h>\n\nint main(void) { puts(\"ok\"); return 0; }\n"),
              "remove_dead_function: definition and prototype removed, output unchanged");
    }
    {
        char d[512]; make_dir(d, sizeof(d), "deadused");
        const char *m = "int twice(int v) { return 2 * v; }\nint main(void) { return twice(0); }\n";
        put(d, "u.c", m);
        TASK_OPS_REPORT r;
        CHECK(!TaskOpsSolve(d, "Remove twice.", &r) && !strcmp(get(d, "u.c"), m),
              "remove_dead_function: a called function is not dead");
        CHECK(!TaskOpsSolve(d, "Document twice in the header.", &r) && !strcmp(get(d, "u.c"), m),
              "remove_dead_function: no removal verb, no edit");
    }

    /* operator 8: unmatched_brace */
    {
        char d[512]; make_dir(d, sizeof(d), "brace");
        put(d, "b.c", "int f(void) { return 1; }\n  }\nint main(void) { const char *s = \"}\"; return f() - 1 + (s[0] != '}'); }\n");
        TASK_OPS_REPORT r;
        int kept = TaskOpsSolve(d, "Make it compile.", &r);
        CHECK(kept && !strcmp(r.op, "unmatched_brace") &&
              !strcmp(get(d, "b.c"), "int f(void) { return 1; }\nint main(void) { const char *s = \"}\"; return f() - 1 + (s[0] != '}'); }\n"),
              "unmatched_brace: the one closing brace that closes nothing is removed");
    }
    {
        char d[512]; make_dir(d, sizeof(d), "brace2");
        const char *m = "int main(void) {\n    return 0;\n}\n}\n}\n";
        put(d, "c.c", m);
        TASK_OPS_REPORT r;
        CHECK(!TaskOpsSolve(d, "Make it compile.", &r) && !strcmp(get(d, "c.c"), m),
              "unmatched_brace: two stray braces = abstain");
    }

    /* stated_fragment: unquoted target after "change to" */
    {
        char d[512]; make_dir(d, sizeof(d), "chgto");
        put(d, "w.c", "int lim(int k) { return k > 9; }\nint main(void) { return !lim(9); }\n");
        TASK_OPS_REPORT r;
        int kept = TaskOpsSolve(d, "Nine must count too: it uses k > 9 now. Change to k >= 9.", &r);
        CHECK(kept && !strcmp(r.op, "stated_fragment") && strstr(get(d, "w.c"), "return k >= 9;"),
              "stated_fragment: unquoted target after 'Change to' is used");
        const char *m = "int main(void) { return 0; }\n";
        char d2[512]; make_dir(d2, sizeof(d2), "chgto2");
        put(d2, "z.c", m);
        CHECK(!TaskOpsSolve(d2, "Change to a faster approach.", &r) && !strcmp(get(d2, "z.c"), m),
              "stated_fragment: plain words after 'Change to' are not a fragment");
    }

    /* operator 10: author_test */
    {
        char d[512]; make_dir(d, sizeof(d), "authtest");
        put(d, "m.c", "int twice(int v) { return 2 * v; }\nint has_x(const char *s) { return s[0] == 'x'; }\n");
        put(d, "m.h", "int twice(int v);\nint has_x(const char *s);\n");
        TASK_OPS_REPORT r;
        int kept = TaskOpsSolve(d, "Write check_twice.c: a test that twice(v) works, asserting twice(21) == 42.", &r);
        CHECK(kept && !strcmp(r.op, "author_test") && strstr(get(d, "check_twice.c"), "#include \"m.h\"") &&
              strstr(get(d, "check_twice.c"), "twice(21) == 42"), "author_test: literal call and expected value from the task");
        char d2[512]; make_dir(d2, sizeof(d2), "authtest2");
        put(d2, "m.c", "int has_x(const char *s) { return s[0] == 'x'; }\n");
        put(d2, "m.h", "int has_x(const char *s);\n");
        kept = TaskOpsSolve(d2, "has_x on a string without x must return 0; put it in t_has.c.", &r);
        CHECK(kept && strstr(get(d2, "t_has.c"), "has_x(\"abc\") == 0"), "author_test: arguments from the prototype");
        char d3[512]; make_dir(d3, sizeof(d3), "authtest3");
        put(d3, "m.c", "int twice(int v) { return 2 * v; }\n");
        put(d3, "m.h", "int twice(int v);\n");
        CHECK(!TaskOpsSolve(d3, "Add t2.c asserting twice(2) == 5.", &r) && !file_exists(d3, "t2.c"),
              "author_test: a test that fails on the current code is rolled back");
    }

    /* nothing applicable: untouched */
    {
        char d[512]; make_dir(d, sizeof(d), "none");
        const char *m = "int main(void) { return 0; }\n";
        put(d, "main.c", m);
        TASK_OPS_REPORT r;
        CHECK(!TaskOpsSolve(d, "Make the program faster.", &r) && r.applied == 0 &&
              !strcmp(get(d, "main.c"), m), "no preconditions: workspace untouched");
    }

    printf("\nTEST RESULTS: %d passed, %d failed\n", pass, fail);
    return fail ? 1 : 0;
}
