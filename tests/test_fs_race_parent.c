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
   build later gained bounded liveness extension (L1b); its final guards are unchanged.
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
#include <sys/resource.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include "liveness_window.h"

#define ROOT "test_fs_race_parent_scratch"
#define OUTD "test_fs_race_parent_outside"
#define STOPF "test_fs_race_parent_stop"
#define BUDGET_MS 4000
#define LIVENESS_CAP_MS 30000
#ifndef MUTANT_CAP_MS
#define MUTANT_CAP_MS 30000
#endif
#define DIAG_BINS 8 /* 500 ms completion bins; final bin includes all >= 3500 ms. */
#define DIAG_STATUS 16
#define CNTF "test_fs_race_parent_count"
/* Non-mutant floor on successful calls per writer in the 4 s window: local minimum 224 over 27 runs, floor about a
   fifth of it. Not yet measured on the CI runner; if CI shows fewer, report before changing it. */
#ifndef RP_OK_FLOOR
#define RP_OK_FLOOR 50
#endif

static int mism;
static int progress_pipe[2][2];
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
/* Both builds. A replace, remove or batch cut off by the directory swap leaves its journal, commit marker and
   pins in ROOT, and recovery refuses to restore over the swapped directory, so every later writer call is
   DENIED for the rest of the run and no call ever meets a symlink window. Measured (local gcc sandbox, not
   the CI runner): mutant build, DENIED on 99.9 percent of the calls (m150); non-mutant build without this
   clearing, 0 to 4 OK calls per operation per writer in 4 s; with the clearing of every .fstxn.* file except
   the lock plus the .fsrp-, .fsrs-, .fst- and .fsp- files, 224 to 308 OK calls per writer in total (15 runs,
   unloaded) and 253 to 354 (12 runs, 6 busy loops on 2 cores). The narrower m150 set (replace journal, its
   commit marker, .fsrp- and .fsrs-) gave only 2 to 53 OK calls per writer in 12 runs. The swapper, which owns
   the fixture, clears those files so the writers keep making real calls. The lock file is left alone. */
static void unwedge(void)
{
    DIR *d = opendir(ROOT);
    struct dirent *e;
    char p[512];
    if (!d) return;
    while ((e = readdir(d)))
    {
        if ((!strncmp(e->d_name, ".fstxn.", 7) && strcmp(e->d_name, ".fstxn.lock")) ||
            !strncmp(e->d_name, ".fsrp-", 6) || !strncmp(e->d_name, ".fsrs-", 6) ||
            !strncmp(e->d_name, ".fst-", 5) || !strncmp(e->d_name, ".fsp-", 5))
        {
            snprintf(p, sizeof p, ROOT "/%s", e->d_name);
            (void)unlink(p);
        }
    }
    closedir(d);
}
static int swapper(void)
{
    int verified = 0;
    while (!exists(STOPF))
    {
        clear_dir(ROOT "/D");
        unwedge();
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
static long diag_ms(struct timespec start)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec - start.tv_sec) * 1000 + (now.tv_nsec - start.tv_nsec) / 1000000;
}
static int writer(int idx)
{
    FS_READ_ROOT *r;
    long published_ms = 0;
    long ok = 0, calls = 0, op_calls[6] = {0}, op_ok[6] = {0}, status[DIAG_STATUS] = {0};
    long bin_calls[DIAG_BINS] = {0}, bin_ok[DIAG_BINS] = {0}, other_status = 0;
    long bin_status[DIAG_BINS][DIAG_STATUS] = {{0}}, max_batch_ms = 0;
    struct timespec start, cpu_start, cpu_end;
    struct rusage usage;
    clock_gettime(CLOCK_MONOTONIC, &start);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_start);
    if (FsReadOpen(ROOT, &r) != FS_READ_OK) return 5;
    while (!exists(STOPF))
    {
        long batch_start = diag_ms(start);
        FS_READ_STATUS s[6];
        FS_BATCH_CREATE b[2] = { { "D/b1", "x", 1, 0666 }, { "D/b2", "y", 1, 0666 } };
        s[0] = FsCreateFile(r, "D/w", "v1", 2, 0666);
        s[1] = FsReplaceFile(r, "D/w", "v1", 2, "v2", 2);
        s[2] = FsRemoveFile(r, "D/w", "v2", 2);
        s[3] = FsBatchCreate(r, b, 2);
        s[4] = FsRemoveFile(r, "D/b1", "x", 1);
        s[5] = FsRemoveFile(r, "D/b2", "y", 1);
        {
            long ms = diag_ms(start);
            int bin = (int)(ms / 500);
            if (ms - batch_start > max_batch_ms) max_batch_ms = ms - batch_start;
            if (bin >= DIAG_BINS) bin = DIAG_BINS - 1;
            for (int i = 0; i < 6; i++)
            {
                calls++; op_calls[i]++; bin_calls[bin]++;
                if ((unsigned)s[i] < DIAG_STATUS) { status[s[i]]++; bin_status[bin][s[i]]++; } else other_status++;
                if (s[i] == FS_READ_OK) { ok++; op_ok[i]++; bin_ok[bin]++; }
            }
        }
        /* One writer per nonblocking pipe; a long fits PIPE_BUF. A full pipe
           drops a snapshot. Stale counts may extend the window, never pass it. */
        if (diag_ms(start) - published_ms >= 500) {
            ssize_t n = write(progress_pipe[idx-1][1], &ok, sizeof(ok));
            if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) return 6;
            if (n >= 0 && n != (ssize_t)sizeof(ok)) return 6;
            published_ms = diag_ms(start);
        }
        nap(1);
    }
    FsReadClose(r);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_end);
    printf("RP diag writer %d: elapsed_ms %ld cpu_ms %ld calls %ld ok %ld other_status %ld\n", idx,
           diag_ms(start), (cpu_end.tv_sec - cpu_start.tv_sec) * 1000 +
           (cpu_end.tv_nsec - cpu_start.tv_nsec) / 1000000, calls, ok, other_status);
    if (getrusage(RUSAGE_SELF, &usage) == 0) printf("RP diag writer %d sched: voluntary %ld involuntary %ld inblock %ld outblock %ld\n", idx, usage.ru_nvcsw, usage.ru_nivcsw, usage.ru_inblock, usage.ru_oublock);
    for (int i = 0; i < 6; i++) printf("RP diag writer %d op %d: calls %ld ok %ld\n", idx, i, op_calls[i], op_ok[i]);
    for (int i = 0; i < DIAG_STATUS; i++) if (status[i]) printf("RP diag writer %d status %d: %ld\n", idx, i, status[i]);
    printf("RP diag writer %d max_batch_ms %ld\n", idx, max_batch_ms);
    for (int i = 0; i < DIAG_BINS; i++)
    {
        printf("RP diag writer %d bin %d: calls %ld ok %ld", idx, i, bin_calls[i], bin_ok[i]);
        for (int j = 0; j < DIAG_STATUS; j++) if (bin_status[i][j]) printf(" status%d=%ld", j, bin_status[i][j]);
        printf("\n");
    }
    fflush(stdout);
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
    long elapsed_ms = 0, live_ok[2] = {0, 0};
    struct timespec t0, t1;
    rmtree(ROOT); rmtree(OUTD); (void)unlink(STOPF);
    ck(mkdir(ROOT, 0700) == 0 && mkdir(OUTD, 0700) == 0 && mkdir(ROOT "/D", 0700) == 0, "mkdir");
    put(OUTD "/victim", "VICTIM");
    for (int k = 0; k < 2; k++) {
        ck(pipe(progress_pipe[k]) == 0, "progress pipe");
        for (int j = 0; j < 2; j++)
            ck(fcntl(progress_pipe[k][j], F_SETFL, O_NONBLOCK) == 0, "nonblocking progress pipe");
    }
    for (int k = 0; k < 3; k++)
    {
        pid[k] = fork();
        ck(pid[k] >= 0, "fork");
        if (pid[k] == 0) {
            for (int j = 0; j < 2; j++) {
                close(progress_pipe[j][0]);
                if (k != j+1) close(progress_pipe[j][1]);
            }
            _exit(k == 0 ? swapper() : writer(k));
        }
    }
    for (int k = 0; k < 2; k++) close(progress_pipe[k][1]);
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
        for (int k = 0; k < 2; k++) {
            long snapshot; ssize_t n;
            while ((n = read(progress_pipe[k][0], &snapshot, sizeof(snapshot))) > 0) {
                ck(n == (ssize_t)sizeof(snapshot), "whole progress snapshot");
                live_ok[k] = snapshot;
            }
            ck(n == 0 || errno == EAGAIN || errno == EWOULDBLOCK, "progress read");
        }
        if (!liveness_wait(ms, BUDGET_MS, LIVENESS_CAP_MS,
                           live_ok[0] >= RP_OK_FLOOR && live_ok[1] >= RP_OK_FLOOR)) break;
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
    for (int k = 0; k < 2; k++) close(progress_pipe[k][0]);
    outside_intact("after swap");
    printf("RP diag window: elapsed_ms %ld budget_ms %d floor %d outside_mismatches %d\n", elapsed_ms, BUDGET_MS, RP_OK_FLOOR, mism);
    printf("RP diag adaptive: cap_ms %d snapshots_ok %ld %ld extended %d\n",
           LIVENESS_CAP_MS, live_ok[0], live_ok[1], elapsed_ms > BUDGET_MS+100);
    {
        DIR *d = opendir(ROOT);
        struct dirent *e;
        int n = 0;
        printf("RP diag root dot names:");
        while (d && (e = readdir(d))) if (e->d_name[0] == '.' && strcmp(e->d_name, ".") && strcmp(e->d_name, "..")) { if (n < 20) printf(" %s", e->d_name); n++; }
        if (d) closedir(d);
        printf(" (count %d, first 20 shown)\n", n);
    }
    fflush(stdout);
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
    {
        long sw = loadcount(0), w1 = loadcount(1), w2 = loadcount(2);
        printf("  swaps %ld, writer calls OK %ld %ld (floor %d each)\n", sw, w1, w2, RP_OK_FLOOR);
        ck(w1 >= RP_OK_FLOOR && w2 >= RP_OK_FLOOR, "each writer made at least RP_OK_FLOOR successful calls");
    }
    printf("%s (%d outside mismatches)\n", mism ? "FAILED" : "ALL PASS", mism);
    return mism ? 1 : 0;
#endif
}
#else
int main(void) { return 0; }
#endif
