/* test_server_taskops.c: operators run on a scratch copy; the client's
   workspace is untouched and the verified edit comes back as hunks. */
#include "server_taskops.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define mk(p) _mkdir(p)
#define pid() _getpid()
#else
#include <sys/stat.h>
#include <unistd.h>
#define mk(p) mkdir(p, 0755)
#define pid() getpid()
#endif

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static void put(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");
    if (f) { fputs(text, f); fclose(f); }
}

static int same(const char *path, const char *text)
{
    char buf[4096];
    FILE *f = fopen(path, "rb");
    size_t n;
    if (!f) return 0;
    n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';
    return !strcmp(buf, text);
}

int main(void)
{
    char *o, *n;
    /* minimal hunk: one changed line, widened until unique */
    CHECK(StoMinimalHunk("a\nb\nc\n", "a\nB\nc\n", &o, &n) && !strcmp(o, "b\n") && !strcmp(n, "B\n"));
    free(o); free(n);
    CHECK(StoMinimalHunk("x\nfi\ny\nfi\n", "x\nfi\ny\nfi\nz\n", &o, &n) && strstr(o, "y\n") && !strcmp(n + strlen(n) - 2, "z\n"));
    free(o); free(n);
    CHECK(!StoMinimalHunk("same\n", "same\n", &o, &n));

    char dir[256], path[320];
    const char *t = getenv("TMPDIR");
    if (!t || !*t) t = getenv("TEMP");
    if (!t || !*t) t = "/tmp";
    snprintf(dir, sizeof(dir), "%s/sto_test_%d_%ld", t, (int)pid(), (long)time(NULL));
    mk(dir);
    snprintf(path, sizeof(path), "%s/run.sh", dir);
    put(path, "#!/bin/sh\nexit 0\n");

    StoPlan p;
    int ok = StoPlanTask(dir, "run.sh should exit with status 3 when the argument is 'fail'.", 3000, &p);
    CHECK(ok && p.nhunks == 1 && !p.hunks[0].created && !strcmp(p.hunks[0].rel, "run.sh"));
    CHECK(ok && !strcmp(p.op, "shell_harden"));
    CHECK(ok && p.hunks[0].old_text && strstr(p.hunks[0].new_text, "if [ \"$1\" = \"fail\" ]"));
    CHECK(same(path, "#!/bin/sh\nexit 0\n"));                /* workspace untouched */
    StoPlanFree(&p);

    ok = StoPlanTask(dir, "\"run.sh should exit with status 3 when the argument is 'fail'.\"", 3000, &p);  /* quoted request */
    CHECK(ok && p.nhunks == 1);
    StoPlanFree(&p);

    ok = StoPlanTask(dir, "Tidy things up.", 3000, &p);    /* nothing stated: abstain */
    CHECK(!ok && p.nhunks == 0 && p.reason[0]);
    CHECK(same(path, "#!/bin/sh\nexit 0\n"));
    StoPlanFree(&p);

    ok = StoPlanTask(dir, "run.sh should exit with status 3 when the argument is 'fail'.", 8, &p);  /* too big */
    CHECK(!ok && strstr(p.reason, "too large"));
    StoPlanFree(&p);

    remove(path);
#ifdef _WIN32
    _rmdir(dir);
#else
    rmdir(dir);
#endif
    printf("test_server_taskops: %s\n", fails ? "FAILED" : "ALL PASSED");
    return fails ? 1 : 0;
}
