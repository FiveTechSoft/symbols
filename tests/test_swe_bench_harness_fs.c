#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
/* M1-3 patch 2: the SWE-bench harness seeds and removes its benchmark file
   through Fs* (create-only, workspace-confined). POSIX only. */
#include "swe_bench_harness.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#define WS "test_swe_fs_scratch"
#define OUT "test_swe_fs_outside.py"

static int fails;
static void ck(int x, const char *m)
{
    printf("  [%s] %s\n", x ? "PASS" : "FAIL", m);
    if (!x) fails++;
}
static void put(const char *p, const char *v)
{
    FILE *f = fopen(p, "wb");
    if (!f || fwrite(v, 1, strlen(v), f) != strlen(v)) exit(2);
    fclose(f);
}
static int is(const char *p, const char *v)
{
    char b[512];
    FILE *f = fopen(p, "rb");
    size_t n;
    if (!f) return 0;
    n = fread(b, 1, sizeof(b) - 1, f);
    fclose(f);
    b[n] = 0;
    return strcmp(b, v) == 0;
}
static int exists(const char *p) { struct stat st; return stat(p, &st) == 0; }
static void reset(void)
{
    if (system("rm -rf " WS " " OUT) != 0) exit(2);
    mkdir(WS, 0700);
}
static void task(SWE_BENCH_TASK *t, const char *file)
{
    memset(t, 0, sizeof(*t));
    strcpy(t->task_id, "fs-001");
    strncpy(t->target_file, file, sizeof(t->target_file) - 1);
    strcpy(t->target_symbol, "f");
    t->target_line = 2;
    strcpy(t->context_before, "def f():\n");
    strcpy(t->buggy_snippet, "    return 1\n");
    strcpy(t->fixed_snippet, "    return 2\n");
    strcpy(t->context_after, "\n");
}
static int run(const char *file, SWE_BENCH_EVAL_SUMMARY *sum)
{
    SWE_BENCH_HARNESS *h = SweBenchHarnessCreate("fs", WS);
    SWE_BENCH_TASK t;
    int rc;
    task(&t, file);
    SweBenchHarnessAddTask(h, &t);
    rc = SweBenchHarnessRun(h, sum);
    SweBenchHarnessDestroy(h);
    return rc;
}

int main(void)
{
    SWE_BENCH_EVAL_SUMMARY *sum = SweBenchSummaryCreate();

    puts("T1 benchmark file is created and removed; nothing else is left");
    reset();
    run(WS "/pkg/mod.py", sum);
    ck(sum->total_tasks == 1 && sum->resolved_tasks + sum->failed_tasks == 1, "task accounted for");
    ck(!exists(WS "/pkg/mod.py"), "benchmark file removed after the run");
    {
        DIR *d = opendir(WS "/pkg");
        struct dirent *e;
        int extra = 0;
        while (d && (e = readdir(d)))
            if (strcmp(e->d_name, ".") && strcmp(e->d_name, "..")) extra++;
        if (d) closedir(d);
        ck(extra == 0, "no stage/intent leftovers beside the target");
    }

    puts("T2 an existing file is never overwritten or removed");
    reset();
    put(WS "/keep.py", "USER CODE\n");
    run(WS "/keep.py", sum);
    ck(sum->failed_tasks == 1 && sum->resolved_tasks == 0, "task counted as failed");
    ck(is(WS "/keep.py", "USER CODE\n"), "existing bytes intact");

    puts("T3 a target outside the workspace is rejected");
    reset();
    run(OUT, sum);
    ck(sum->failed_tasks == 1, "task counted as failed");
    ck(!exists(OUT), "no file created outside the workspace");
    run("../" OUT, sum);
    ck(sum->failed_tasks == 1 && !exists(OUT), "dotdot target rejected, nothing created");

    puts("T4 a symlinked target name is refused and left alone");
    reset();
    put(WS "/real.py", "REAL\n");
    symlink("real.py", WS "/link.py");
    run(WS "/link.py", sum);
    ck(sum->failed_tasks == 1 && is(WS "/real.py", "REAL\n"), "symlink target refused, real file intact");

    SweBenchSummaryDestroy(sum);
    if (system("rm -rf " WS " " OUT) != 0) return 2;
    printf("%s (%d failures)\n", fails ? "FAILED" : "ALL PASS", fails);
    return fails ? 1 : 0;
}
#else
int main(void) { return 0; }
#endif
