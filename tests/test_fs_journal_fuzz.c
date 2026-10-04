#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#include "fs_replace.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
/* Seeded fuzz of the replace journal (M1 criterion 5, journal part, POSIX, replace kind only).
   A child crashes FsReplaceFile at a real crash point and leaves a real journal. One journal file is
   then damaged and FsReplaceRecover runs in a forked child (signal or hang = failure). Claim checked:
   a refusal leaves every workspace byte unchanged; an OK leaves target "old" or "new", no journal or
   stage name, every other file unchanged; nothing outside the workspace changes; a second recovery is
   OK and changes nothing. Reproduce with FS_JFUZZ_SEED=<n> FS_JFUZZ_ITERS=<n>. Not covered here: the
   create, batch, remove and move journals, Windows, power loss. */
#define WS "test_fs_journal_fuzz_scratch"
#define OUT "test_fs_journal_fuzz_outside"
#define MAXP 1024
#define OFF_TARGET 40
#define OFF_OLD (OFF_TARGET + MAXP)
#define OFF_NEW (OFF_OLD + 48)
#define MARK_SHIFT 24
#define NCLS 8
static const char *CLS[NCLS] = {"bitflip", "setbyte", "truncate", "append", "pathfield", "delete", "stage_decoy", "temp_decoy"};
#define D1 ".fsrp-dddddddddddddddddddddddddddddddd"
#define D2 ".fsrb-eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"
#define S2 ".fsrp-eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"
static const int PHASES[] = {41, 42, 43, 48, 51, 52, 53};
#define NPH 7
static uint32_t g_seed = 0x6a7f0001u, rs;
static unsigned g_iter;
static int g_cls, g_phase, nfail, fail_mask, cls_runs[NCLS], n_refused, n_ok;
static char g_what[160];
static uint32_t rnd(void) { rs = rs * 1664525u + 1013904223u; return rs >> 8; }
static void fail(const char *m)
{
    nfail++;
    fail_mask |= 1 << g_cls;
    if (nfail <= 12)
        fprintf(stderr, "FAIL %s (seed=%u iter=%u class=%s phase=%d %s)\n", m, (unsigned)g_seed, g_iter, CLS[g_cls], g_phase, g_what);
}
static void die(const char *m) { fprintf(stderr, "SETUP %s (seed=%u iter=%u)\n", m, (unsigned)g_seed, g_iter); exit(2); }
static void put(const char *dir, const char *name, const char *v)
{
    char p[256];
    FILE *f;
    snprintf(p, sizeof(p), "%s/%s", dir, name);
    f = fopen(p, "wb");
    if (!f || fwrite(v, 1, strlen(v), f) != strlen(v) || fclose(f) != 0) die("put");
    if (chmod(p, 0600) != 0) die("chmod");
}
static void wipe(const char *dir)
{
    DIR *d = opendir(dir);
    struct dirent *e;
    char p[512];
    if (!d) return;
    while ((e = readdir(d)))
    {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        snprintf(p, sizeof(p), "%s/%s", dir, e->d_name);
        unlink(p);
    }
    closedir(d);
    rmdir(dir);
}
/* Flat serialization: sorted name, mode, size, bytes. */
static int cmpn(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }
static char *snap(const char *dir)
{
    DIR *d = opendir(dir);
    struct dirent *e;
    char *names[128], *out;
    size_t n = 0, cap = 4096, len = 0;
    if (!d) die("snap open");
    while ((e = readdir(d)) && n < 128)
        if (strcmp(e->d_name, ".") && strcmp(e->d_name, "..")) names[n++] = strdup(e->d_name);
    closedir(d);
    qsort(names, n, sizeof(char *), cmpn);
    out = malloc(cap);
    out[0] = 0;
    for (size_t i = 0; i < n; i++)
    {
        char p[512], buf[2048], hdr[1200];
        struct stat st;
        FILE *f;
        size_t got = 0;
        snprintf(p, sizeof(p), "%s/%s", dir, names[i]);
        if (lstat(p, &st) != 0) die("snap stat");
        f = fopen(p, "rb");
        if (f) { got = fread(buf, 1, sizeof(buf), f); fclose(f); }
        snprintf(hdr, sizeof(hdr), "[%s|%o|%ld|", names[i], (unsigned)st.st_mode, (long)st.st_size);
        if (len + strlen(hdr) + got + 8 > cap) { cap = (len + strlen(hdr) + got + 8) * 2; out = realloc(out, cap); }
        memcpy(out + len, hdr, strlen(hdr)); len += strlen(hdr);
        memcpy(out + len, buf, got); len += got;
        out[len++] = ']'; out[len] = 0;
        free(names[i]);
    }
    return out;
}
static int has(const char *dir, const char *name)
{
    char p[512];
    struct stat st;
    snprintf(p, sizeof(p), "%s/%s", dir, name);
    return lstat(p, &st) == 0;
}
static int is_content(const char *name, const char *v)
{
    char p[512], b[64];
    FILE *f;
    size_t n;
    snprintf(p, sizeof(p), "%s/%s", WS, name);
    f = fopen(p, "rb");
    if (!f) return 0;
    n = fread(b, 1, sizeof(b), f);
    fclose(f);
    return n == strlen(v) && !memcmp(b, v, n);
}
static int run_child(int point, int recover, FS_READ_ROOT *r)
{
    int status = 0;
    pid_t pid = fork();
    if (pid < 0) die("fork");
    if (!pid)
    {
        char v[16];
        FS_READ_STATUS s;
        alarm(20);
        if (point) { snprintf(v, sizeof(v), "%d", point); setenv("FS_CREATE_TEST_CRASH", v, 1); }
        s = recover ? FsReplaceRecover(r) : FsReplaceFile(r, "target", "old", 3, "new", 3);
        _exit(recover ? 40 + (int)s : 10);
    }
    if (waitpid(pid, &status, 0) != pid) die("waitpid");
    if (WIFSIGNALED(status)) return -WTERMSIG(status);
    return WEXITSTATUS(status);
}
static void field(const char *journal, int off, const char *val, size_t vlen)
{
    char p[512], buf[1100];
    int fd;
    snprintf(p, sizeof(p), "%s/%s", WS, journal);
    fd = open(p, O_RDWR | O_NOFOLLOW);
    if (fd < 0) return;
    memset(buf, 0, sizeof(buf));
    memcpy(buf, val, vlen);
    (void)!pwrite(fd, buf, vlen + 1, off);
    close(fd);
}
static void damage(int cls)
{
    const char *names[2] = {".fstxn.replace", ".fstxn.pcommit"};
    const char *present[2];
    int np = 0;
    const char *j;
    char p[512];
    int fd;
    off_t size;
    for (int i = 0; i < 2; i++) if (has(WS, names[i])) present[np++] = names[i];
    g_what[0] = 0;
    if (!np) { snprintf(g_what, sizeof(g_what), "no journal file at this phase"); return; }
    if (cls >= 6)
    {
        for (int i = 0; i < np; i++)
        {
            int shift = !strcmp(present[i], ".fstxn.pcommit") ? MARK_SHIFT : 0;
            if (cls == 6) field(present[i], OFF_OLD + shift, D1, strlen(D1));
            else field(present[i], OFF_NEW + shift, S2, strlen(S2));
        }
        snprintf(g_what, sizeof(g_what), "%d record(s) rewritten", np);
        return;
    }
    j = present[rnd() % np];
    snprintf(p, sizeof(p), "%s/%s", WS, j);
    if (cls == 5)
    {
        unlink(p);
        snprintf(g_what, sizeof(g_what), "deleted %s", j);
        return;
    }
    fd = open(p, O_RDWR | O_NOFOLLOW);
    if (fd < 0) die("open journal");
    size = lseek(fd, 0, SEEK_END);
    if (cls == 0 || cls == 1)
    {
        off_t off = rnd() % size;
        unsigned char b;
        (void)!pread(fd, &b, 1, off);
        if (cls == 0) b ^= (unsigned char)(1u << (rnd() % 8));
        else b = (rnd() & 1) ? 0x00 : 0xFF;
        (void)!pwrite(fd, &b, 1, off);
        snprintf(g_what, sizeof(g_what), "%s offset %ld", j, (long)off);
    }
    else if (cls == 2)
    {
        off_t len = rnd() % size;
        (void)!ftruncate(fd, len);
        snprintf(g_what, sizeof(g_what), "%s truncated to %ld", j, (long)len);
    }
    else if (cls == 3)
    {
        unsigned char b[16];
        int n = 1 + rnd() % 16;
        for (int i = 0; i < n; i++) b[i] = (unsigned char)rnd();
        (void)!pwrite(fd, b, n, size);
        snprintf(g_what, sizeof(g_what), "%s +%d bytes", j, n);
    }
    else
    {
        static const char *paths[] = {"../" OUT "/sentinel", "/etc/passwd", ".fstxn.lock", "keep", "a/../keep", "", ".fstxn.replace", "sub/target"};
        const char *v = paths[rnd() % 8];
        int shift = !strcmp(j, ".fstxn.pcommit") ? MARK_SHIFT : 0;
        close(fd);
        field(j, OFF_TARGET + shift, v, strlen(v));
        snprintf(g_what, sizeof(g_what), "%s target field [%s]", j, v);
        return;
    }
    close(fd);
}
static void one(int cls, int phase, FS_READ_ROOT **rp)
{
    char *before, *after, *out_b, *out_a;
    int rc;
    FS_READ_ROOT *r;
    g_cls = cls; g_phase = phase;
    wipe(WS); wipe(OUT);
    if (mkdir(WS, 0700) || mkdir(OUT, 0700)) die("mkdir");
    put(OUT, "sentinel", "outside");
    put(WS, "target", "old"); put(WS, "keep", "keep");
    put(WS, D1, "decoy1"); put(WS, D2, "decoy2");
    if (FsReadOpen(WS, &r) != FS_READ_OK) die("open");
    *rp = r;
    if (run_child(phase, 0, r) != 90 + phase) die("crash point not reached");
    damage(cls);
    before = snap(WS); out_b = snap(OUT);
    rc = run_child(0, 1, r);
    after = snap(WS); out_a = snap(OUT);
    cls_runs[cls]++;
    if (rc < 0) fail("recovery died by signal or hang");
    else if (strcmp(out_b, out_a)) fail("a file outside the workspace changed");
    else if (rc != 40 + FS_READ_OK)
    {
        n_refused++;
        if (strcmp(before, after)) fail("refusal changed the workspace");
    }
    else
    {
        n_ok++;
        if (!(is_content("target", "old") || is_content("target", "new"))) fail("OK but target is neither old nor new");
        if (!is_content("keep", "keep")) fail("OK but keep changed");
        if (!is_content(D1, "decoy1") || !is_content(D2, "decoy2")) fail("OK but a decoy was removed");
        if (has(WS, ".fstxn.replace") || has(WS, ".fstxn.pcommit")) fail("OK but a journal name is left");
        /* A deleted journal is the crash state "journal gone, cleanup unfinished": the old pin and
           stages stay by design (see fs_replace.h), so stage names are only checked for the other classes. */
        if (cls != 5)
        {
            DIR *d = opendir(WS);
            struct dirent *e;
            while ((e = readdir(d)))
                if (!strncmp(e->d_name, ".fsrp-", 6) || !strncmp(e->d_name, ".fsrb-", 6) || !strncmp(e->d_name, ".fstxn-", 7))
                    if (strcmp(e->d_name, D1) && strcmp(e->d_name, D2)) { fail("OK but a stage or temporary journal link is left"); break; }
            closedir(d);
        }
        if (run_child(0, 1, r) != 40 + FS_READ_OK) fail("second recovery not OK");
    }
    free(before); free(after); free(out_b); free(out_a);
    FsReadClose(r);
}
int main(void)
{
    const char *e;
    unsigned iters = 700;
    FS_READ_ROOT *r = NULL;
    if ((e = getenv("FS_JFUZZ_SEED"))) g_seed = (uint32_t)strtoul(e, NULL, 0);
    if ((e = getenv("FS_JFUZZ_ITERS"))) iters = (unsigned)strtoul(e, NULL, 0);
    rs = g_seed;
    for (g_iter = 0; g_iter < iters; g_iter++)
        one((int)(g_iter % NCLS), PHASES[(g_iter / NCLS) % NPH], &r);
    wipe(WS); wipe(OUT);
    for (int i = 0; i < NCLS; i++)
        if (!cls_runs[i]) { fprintf(stderr, "class %s never ran\n", CLS[i]); return 1; }
    printf("journal fuzz iterations: %u seed %u refused %d ok %d failures %d class mask %x\n", iters, (unsigned)g_seed, n_refused, n_ok, nfail, fail_mask);
#ifdef FS_JFUZZ_MUTANT_MASK
#ifndef FS_JFUZZ_MUTANT_IGNORE
#define FS_JFUZZ_MUTANT_IGNORE 0
#endif
    /* Exit 0 only when the failing classes, outside the ignored ones, are exactly the predicted one.
       FS_JFUZZ_MUTANT_IGNORE lists classes whose result under THIS mutant is not predicted (the random byte
       classes: a mutant that accepts a damaged identity number can add failures there, seed dependent). */
    {
        unsigned seen = (unsigned)fail_mask & ~(unsigned)FS_JFUZZ_MUTANT_IGNORE;
        if (seen == (unsigned)FS_JFUZZ_MUTANT_MASK)
        {
            printf("mutant killed, failing class mask exactly %x\n", seen);
            return 0;
        }
        printf("mutant SURVIVED or failed elsewhere: class mask %x (ignored %x), predicted %x\n", (unsigned)fail_mask, (unsigned)FS_JFUZZ_MUTANT_IGNORE, (unsigned)FS_JFUZZ_MUTANT_MASK);
        return 1;
    }
#endif
    return nfail ? 1 : 0;
}
#else
int main(void) { return 0; }
#endif
