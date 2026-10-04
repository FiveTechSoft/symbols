#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
/* Criterion 1, POSIX: a parent directory is swapped between a real directory
   and a symlink to an OUTSIDE directory while writers create, replace, remove
   and batch-create through it. This is the POSIX counterpart of
   test_fs_win_race.c cell E (junction swap).

   Prediction written before the first measurement: the invariant holds. Every
   POSIX writer resolves the parent once with openat and O_NOFOLLOW per
   component (src/fs_read.c posix_open) and then works through the held
   directory descriptor, so a later swap of the path cannot redirect a write.
   The outside directory never gains, loses or changes a file.
   Mutant (FS_PARENT_FOLLOW_MUTANT, one macro in src/fs_read.c: the per-component
   O_NOFOLLOW removed): with it defined this target exits 0 only if the outside
   directory was touched (killed). v2 (m100): the kill is a race outcome, and in CI run
   37181356800 (build-test-linux) the 4 s fixed window ended with zero touches, so the
   old target printed SURVIVED by luck (local kill rate before the change: 14 of 14; CI:
   1 survival in about 11 executions). The mutant build now keeps running until the first
   outside touch (killed, exit 0) or MUTANT_CAP_MS (30 s). No touch within the cap is
   INCONCLUSIVE, exit 7, with the swapper and writer counts printed; it is never reported
   as a surviving mutant, and CI treats any non-zero exit as a failure. The non-mutant
   build is unchanged (4 s window, same guards).
   Predictions before the v2 runs: kill in 30 of 30 local runs, typically well under the
   old 4 s; INCONCLUSIVE never locally.
   Guards, so the test cannot pass vacuously: the swapper verifies the symlink
   state with lstat at least 5 times, and at least one writer process has a
   successful call. Measured while writing this: in 1 of 6 local runs one of
   the two writers had no successful call in 4 s (the other did), so the guard
   is "at least one writer", not "each writer".
   Cooperating writers only; one runner; no claim about other interleavings. */
#include "fs_write.h"
#include "fs_remove.h"
#include "fs_replace.h"
#include "fs_batch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>

#define ROOT "test_fs_race_parent_scratch"
#define OUTD "test_fs_race_parent_outside"
#define STOPF "test_fs_race_parent_stop"
#define BUDGET_MS 4000
#ifndef MUTANT_CAP_MS
#define MUTANT_CAP_MS 30000
#endif
#define CNTF "test_fs_race_parent_count"

static int mism;
static void ck(int x, const char *m)
{
    if (!x) { fprintf(stderr, "FAIL %s\n", m); exit(1); }
}
static void nap(int ms) { struct timespec t = { ms / 1000, (ms % 1000) * 1000000L }; nanosleep(&t, NULL); }
static int exists(const char *p) { struct stat st; return lstat(p, &st) == 0; }
static void put(const char *p, const char *v)
{
    FILE *f = fopen(p, "wb");
    ck(f && fwrite(v, 1, strlen(v), f) == strlen(v) && fclose(f) == 0, "fixture");
}
static void rmtree(const char *dir)
{
    char cmd[256];
    snprintf(cmd, sizeof cmd, "rm -rf %s", dir);
    if (system(cmd) != 0) exit(2);
}
/* Only the swapper changes D, so this never races with a flip. Never follows a symlink. */
static void clear_dir(const char *dir)
{
    struct stat st;
    DIR *d;
    struct dirent *e;
    char p[512];
    if (lstat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) return;
    d = opendir(dir);
    if (!d) return;
    while ((e = readdir(d)))
    {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
        (void)unlink(p);
    }
    closedir(d);
}
static void savecount(int k, long n)
{
    char p[64];
    FILE *f;
    snprintf(p, sizeof p, CNTF "%d", k);
    f = fopen(p, "wb");
    if (f) { fprintf(f, "%ld", n); fclose(f); }
}
static long loadcount(int k)
{
    char p[64];
    long n = -1;
    FILE *f;
    snprintf(p, sizeof p, CNTF "%d", k);
    f = fopen(p, "rb");
    if (f) { if (fscanf(f, "%ld", &n) != 1) n = -1; fclose(f); }
    (void)unlink(p);
    return n;
}
static int swapper(void)
{
    int verified = 0;
    while (!exists(STOPF))
    {
        clear_dir(ROOT "/D");
        if (rmdir(ROOT "/D") == 0)
        {
            struct stat st;
            if (symlink("../" OUTD, ROOT "/D") == 0 && lstat(ROOT "/D", &st) == 0 && S_ISLNK(st.st_mode))
                verified++;
            nap(15);
            (void)unlink(ROOT "/D");
            (void)mkdir(ROOT "/D", 0700);
            nap(30);
        }
        else nap(5);
    }
    if (!exists(ROOT "/D")) (void)mkdir(ROOT "/D", 0700);
    savecount(0, verified);
    return verified >= 5 ? 0 : 3;
}
static int writer(int idx)
{
    FS_READ_ROOT *r;
    long ok = 0;
    if (FsReadOpen(ROOT, &r) != FS_READ_OK) return 5;
    while (!exists(STOPF))
    {
        FS_READ_STATUS s[6];
        FS_BATCH_CREATE b[2] = { { "D/b1", "x", 1, 0666 }, { "D/b2", "y", 1, 0666 } };
        s[0] = FsCreateFile(r, "D/w", "v1", 2, 0666);
        s[1] = FsReplaceFile(r, "D/w", "v1", 2, "v2", 2);
        s[2] = FsRemoveFile(r, "D/w", "v2", 2);
        s[3] = FsBatchCreate(r, b, 2);
        s[4] = FsRemoveFile(r, "D/b1", "x", 1);
        s[5] = FsRemoveFile(r, "D/b2", "y", 1);
        for (int i = 0; i < 6; i++) if (s[i] == FS_READ_OK) ok++;
        nap(1);
    }
    FsReadClose(r);
    savecount(idx, ok);
    return ok > 0 ? 0 : 4;
}
static void outside_intact(const char *when)
{
    DIR *d = opendir(OUTD);
    struct dirent *e;
    int n = 0;
    char b[16] = { 0 };
    FILE *f;
    ck(d != NULL, "list outside");
    while ((e = readdir(d))) if (strcmp(e->d_name, ".") && strcmp(e->d_name, "..")) n++;
    closedir(d);
    f = fopen(OUTD "/victim", "rb");
    if (f) { size_t k = fread(b, 1, 6, f); b[k] = 0; fclose(f); }
    if (n != 1 || strcmp(b, "VICTIM") != 0)
    {
        mism++;
        printf("  [FAIL] %s: outside has %d entries, victim bytes \"%s\"\n", when, n, b);
        {
            DIR *d2 = opendir(OUTD);
            while (d2 && (e = readdir(d2))) if (strcmp(e->d_name, ".") && strcmp(e->d_name, "..")) printf("    outside entry: %s\n", e->d_name);
            if (d2) closedir(d2);
        }
    }
}
int main(void)
{
    pid_t pid[3];
    int st[3] = { 0, 0, 0 };
    long elapsed_ms = 0;
    struct timespec t0, t1;
    rmtree(ROOT); rmtree(OUTD); (void)unlink(STOPF);
    ck(mkdir(ROOT, 0700) == 0 && mkdir(OUTD, 0700) == 0 && mkdir(ROOT "/D", 0700) == 0, "mkdir");
    put(OUTD "/victim", "VICTIM");
    for (int k = 0; k < 3; k++)
    {
        pid[k] = fork();
        ck(pid[k] >= 0, "fork");
        if (pid[k] == 0) _exit(k == 0 ? swapper() : writer(k));
    }
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (;;)
    {
        long ms;
        outside_intact("during swap");
#ifdef FS_PARENT_FOLLOW_MUTANT
        if (mism) break; /* first outside touch: the mutant is killed, no need to wait */
#endif
        nap(50);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        ms = (t1.tv_sec - t0.tv_sec) * 1000 + (t1.tv_nsec - t0.tv_nsec) / 1000000;
#ifdef FS_PARENT_FOLLOW_MUTANT
        if (ms >= MUTANT_CAP_MS) break;
#else
        if (ms >= BUDGET_MS) break;
#endif
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    elapsed_ms = (t1.tv_sec - t0.tv_sec) * 1000 + (t1.tv_nsec - t0.tv_nsec) / 1000000;
    put(STOPF, "stop");
    for (int k = 0; k < 3; k++)
    {
        int status = 0;
        ck(waitpid(pid[k], &status, 0) == pid[k] && WIFEXITED(status), "wait");
        st[k] = WEXITSTATUS(status);
    }
    outside_intact("after swap");
    printf("  swapper exit %d (0 = symlink state verified >= 5 times), writers exit %d %d (0 = that writer had a call OK)\n", st[0], st[1], st[2]);
    rmtree(ROOT); rmtree(OUTD); (void)unlink(STOPF);
#ifdef FS_PARENT_FOLLOW_MUTANT
    {
        long sw = loadcount(0), w1 = loadcount(1), w2 = loadcount(2);
        if (mism) { printf("FS_PARENT_FOLLOW_MUTANT killed after %ld ms: outside touched %d times (swaps %ld, writer calls OK %ld %ld)\n", elapsed_ms, mism, sw, w1, w2); return 0; }
        printf("FS_PARENT_FOLLOW_MUTANT INCONCLUSIVE, not a surviving mutant: no outside touch in %ld ms (swaps %ld, writer calls OK %ld %ld, swapper exit %d)\n", elapsed_ms, sw, w1, w2, st[0]);
        return 7;
    }
#else
    ck(st[0] == 0, "swapper verified the symlink state at least 5 times");
    ck(st[1] == 0 || st[2] == 0, "at least one writer had a successful call");
    (void)loadcount(0); (void)loadcount(1); (void)loadcount(2);
    printf("%s (%d outside mismatches)\n", mism ? "FAILED" : "ALL PASS", mism);
    return mism ? 1 : 0;
#endif
}
#else
int main(void) { return 0; }
#endif
