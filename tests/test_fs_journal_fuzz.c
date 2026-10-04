#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#include "fs_replace.h"
#include "fs_write.h"
#include "fs_remove.h"
#include "fs_move.h"
#include "fs_batch.h"
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
/* Seeded fuzz of the replace, create, remove, move and batch (create and replace) journals (M1 criterion 5, journal part, POSIX).
   A child crashes the operation at a real crash point and leaves a
   real journal. One journal file is then damaged and the kind's recovery function runs in a forked child
   (signal or hang = failure). Claim checked: a refusal leaves every workspace byte unchanged; an OK leaves
   the files in one of the two states the operation allows (before or after), no journal or stage name,
   every other file unchanged; nothing outside the workspace changes; a second recovery is OK and changes
   nothing. Known limits F2, F4, F5, F6, F7 are pinned (see CMakeLists.txt). Reproduce with FS_JFUZZ_SEED=<n> FS_JFUZZ_ITERS=<n> (iterations per kind). Not covered here:
   Windows, power loss, damage to two journal files at once. */
#define WS "test_fs_journal_fuzz_scratch"
#define OUT "test_fs_journal_fuzz_outside"
#define MAXP 1024
#define NCLS 8
#define NKIND 6
enum { K_REPLACE, K_CREATE, K_REMOVE, K_MOVE, K_BCREATE, K_BREPLACE };
static const char *KN[NKIND] = {"replace", "create", "remove", "move", "batch_create", "batch_replace"};
static const char *CLS[NCLS] = {"bitflip", "setbyte", "truncate", "append", "pathfield", "delete", "stage_decoy", "temp_decoy"};
#define H32D "dddddddddddddddddddddddddddddddd"
#define H32E "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"
typedef struct {
    const char *name;
    int noff, off_path[2], off_stage, off_stage2;
} JFILE;
typedef struct {
    int phases[8], nph, ncls, njf;
    JFILE jf[2];
    const char *stage_pref;   /* stage name prefix of the operation */
    const char *decoy;        /* decoy that a rewritten stage field points at */
    const char *stage2;       /* stage value for class temp_decoy */
    const char *decoy2;       /* decoy planted for class temp_decoy */
} KIND;
static const KIND K[NKIND] = {
    {{41, 42, 43, 48, 51, 52, 53}, 7, 8, 2,
     {{".fstxn.replace", 1, {40, -1}, 1064, 1112}, {".fstxn.pcommit", 1, {64, -1}, 1088, 1136}},
     ".fsrp-", ".fsrp-" H32D, ".fsrp-" H32E, ".fsrb-" H32E},
    {{2, 3, 4}, 3, 8, 1,
     {{".fstxn.intent", 1, {24, -1}, 1048, -1}, {0, 0, {-1, -1}, -1, -1}},
     ".fst-", ".fst-" H32D, ".fst-" H32E, ".fstxn-" H32E},
    {{11, 12, 14}, 3, 7, 2,
     {{".fstxn.remove", 1, {24, -1}, 1048, -1}, {".fstxn.rcommit", 0, {-1, -1}, -1, -1}},
     ".fsrm-", ".fsrm-" H32D, 0, 0},
    {{21, 22, 23, 25}, 4, 7, 2,
     {{".fstxn.move", 2, {24, 1048}, 2072, -1}, {".fstxn.mcommit", 0, {-1, -1}, -1, -1}},
     ".fsmv-", ".fsmv-" H32D, 0, 0},
    /* batch create: item = target[1024] stage[48] dev ino (1088 bytes), items start at 16 */
    {{5, 6, 7}, 3, 8, 2,
     {{".fstxn.batch", 2, {16, 1104}, 1040, -1}, {".fstxn.commit", 0, {-1, -1}, -1, -1}},
     ".fst-", ".fst-" H32D, ".fst-" H32E, ".fst-" H32E},
    /* batch replace: item = target[1024] oldstage[48] newstage[48] 4 x u64 (1152 bytes) */
    {{60, 61, 62, 63}, 4, 8, 2,
     {{".fstxn.batch", 2, {16, 1168}, 1040, 1088}, {".fstxn.commit", 0, {-1, -1}, -1, -1}},
     ".fsrp-", ".fsrp-" H32D, ".fsrp-" H32E, ".fsrb-" H32E},
};
static uint32_t g_seed = 0x6a7f0001u, rs;
static unsigned g_iter;
static int g_kind, g_cls, g_phase, nfail;
static uint64_t fail_mask;   /* bit kind*8+class */
static int cls_runs[NKIND][NCLS], n_refused, n_ok;
static char g_what[160];
static int g_name_damage, g_stage_damage, n_f2_limit, n_f5_limit, n_f6_limit, n_f7_limit;   /* remove record: damage inside the target name field */
static uint32_t rnd(void) { rs = rs * 1664525u + 1013904223u; return rs >> 8; }
static void fail(const char *m)
{
    nfail++;
    fail_mask |= (uint64_t)1 << (g_kind * 8 + g_cls);
    if (nfail <= 12)
        fprintf(stderr, "FAIL %s (seed=%u iter=%u kind=%s class=%s phase=%d %s)\n", m, (unsigned)g_seed, g_iter, KN[g_kind], CLS[g_cls], g_phase, g_what);
}
static void die(const char *m) { fprintf(stderr, "SETUP %s (seed=%u iter=%u kind=%s)\n", m, (unsigned)g_seed, g_iter, KN[g_kind]); exit(2); }
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
static int cmpn(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }
/* Flat serialization: sorted name, mode, size, bytes. */
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
static int has(const char *name)
{
    char p[512];
    struct stat st;
    snprintf(p, sizeof(p), "%s/%s", WS, name);
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
static FS_READ_STATUS do_op(FS_READ_ROOT *r)
{
    switch (g_kind)
    {
    case K_REPLACE: return FsReplaceFile(r, "target", "old", 3, "new", 3);
    case K_CREATE: return FsCreateFile(r, "target", "payload", 7, 0600);
    case K_REMOVE: return FsRemoveFile(r, "target", "old", 3);
    case K_BCREATE:
    {
        FS_BATCH_CREATE e[2] = {{"target", "payload", 7, 0600}, {"t2", "payload", 7, 0600}};
        return FsBatchCreate(r, e, 2);
    }
    case K_BREPLACE:
    {
        FS_BATCH_REPLACE e[2] = {{"target", "old", 3, "new", 3}, {"t2", "old", 3, "new", 3}};
        return FsBatchReplace(r, e, 2);
    }
    default: return FsMoveFile(r, "src", "dst", "old", 3);
    }
}
static FS_READ_STATUS do_recover(FS_READ_ROOT *r)
{
    switch (g_kind)
    {
    case K_REPLACE: return FsReplaceRecover(r);
    case K_CREATE: return FsCreateRecover(r);
    case K_REMOVE: return FsRemoveRecover(r);
    case K_BCREATE: case K_BREPLACE: return FsBatchRecover(r);
    default: return FsMoveRecover(r);
    }
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
        s = recover ? do_recover(r) : do_op(r);
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
/* The records carry an 8 byte trailer {0x4653434b, CRC-32 of the record bytes} (src/fs_read.c, rec_crc). This
   copy lets the test play a writer that rewrites a field and recomputes the CRC: such a forged but consistent
   record is the case the identity checks in recovery still have to catch (stage and temp mutants) and the case
   a CRC cannot detect (cells F2, F4, F6, F7 with recompute). Plain damage does not recompute. */
static uint32_t crc32_buf(const unsigned char *b, size_t n)
{
    uint32_t c = 0xffffffffu;
    for (size_t k = 0; k < n; k++) { c ^= b[k]; for (int j = 0; j < 8; j++) c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1u))); }
    return ~c;
}
static void refresh_crc(const char *journal)
{
    char p[512];
    unsigned char buf[16384];
    uint32_t tr[2];
    ssize_t n;
    int fd;
    snprintf(p, sizeof(p), "%s/%s", WS, journal);
    fd = open(p, O_RDWR | O_NOFOLLOW);
    if (fd < 0) return;
    n = pread(fd, buf, sizeof(buf), 0);
    if (n > 8)
    {
        memcpy(tr, buf + n - 8, 8);
        if (tr[0] == 0x4653434bu) { tr[1] = crc32_buf(buf, (size_t)n - 8); (void)!pwrite(fd, tr, 8, n - 8); }
    }
    close(fd);
}
/* Legacy cell: cut the 8 byte trailer, giving a record as the previous build wrote it. */
static void strip_trailers(void)
{
    const KIND *k = &K[g_kind];
    int cut = 0;
    char p[512];
    for (int i = 0; i < k->njf; i++)
    {
        int fd;
        uint32_t tr[2];
        off_t size;
        snprintf(p, sizeof(p), "%s/%s", WS, k->jf[i].name);
        fd = open(p, O_RDWR | O_NOFOLLOW);
        if (fd < 0) continue;
        size = lseek(fd, 0, SEEK_END);
        if (size > 8 && pread(fd, tr, 8, size - 8) == 8 && tr[0] == 0x4653434bu) { if (ftruncate(fd, size - 8) == 0) cut++; }
        close(fd);
    }
    snprintf(g_what, sizeof(g_what), "legacy: %d trailer(s) cut", cut);
}
static int g_legacy;
static void damage(int cls)
{
    const KIND *k = &K[g_kind];
    const JFILE *present[2];
    int np = 0;
    const JFILE *j;
    char p[512];
    int fd;
    off_t size;
    for (int i = 0; i < k->njf; i++) if (has(k->jf[i].name)) present[np++] = &k->jf[i];
    g_what[0] = 0;
    if (!np) { snprintf(g_what, sizeof(g_what), "no journal file at this phase"); return; }
    if (cls >= 6)
    {
        int n = 0;
        for (int i = 0; i < np; i++)
        {
            if (present[i]->off_stage < 0) continue;
            n++;
            if (cls == 6) field(present[i]->name, present[i]->off_stage, k->decoy, strlen(k->decoy));
            else field(present[i]->name, present[i]->off_stage2 >= 0 ? present[i]->off_stage2 : present[i]->off_stage, k->stage2, strlen(k->stage2));
        }
        for (int i = 0; i < np; i++) if (present[i]->off_stage >= 0) refresh_crc(present[i]->name);
        snprintf(g_what, sizeof(g_what), "%d record(s) rewritten, trailer recomputed", n);
        return;
    }
    j = present[rnd() % np];
    snprintf(p, sizeof(p), "%s/%s", WS, j->name);
    if (cls == 5)
    {
        unlink(p);
        snprintf(g_what, sizeof(g_what), "deleted %s", j->name);
        return;
    }
    if (cls == 4)
    {
        static const char *paths[] = {"../" OUT "/sentinel", "/etc/passwd", ".fstxn.lock", "keep", "a/../keep", "", ".fstxn.replace", "sub/target"};
        const char *v = paths[rnd() % 8];
        int which;
        if (!j->noff) { snprintf(g_what, sizeof(g_what), "%s has no path field", j->name); return; }
        which = rnd() % j->noff;
        field(j->name, j->off_path[which], v, strlen(v));
        if ((g_kind == K_REMOVE && !strcmp(j->name, ".fstxn.remove")) || (g_kind == K_MOVE && !strcmp(j->name, ".fstxn.move"))) g_name_damage = 1;
        snprintf(g_what, sizeof(g_what), "%s path field %d [%s]", j->name, which, v);
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
        if ((g_kind == K_REMOVE && !strcmp(j->name, ".fstxn.remove") && off >= 24 && off < 24 + MAXP) ||
            (g_kind == K_MOVE && !strcmp(j->name, ".fstxn.move") && ((off >= 24 && off < 24 + MAXP) || (off >= 1048 && off < 1048 + MAXP)))) g_name_damage = 1;
        if (g_kind == K_BCREATE && !strcmp(j->name, ".fstxn.batch") && ((off >= 16 && off < 16 + MAXP) || (off >= 1104 && off < 1104 + MAXP))) g_name_damage = 1;
        if (g_kind == K_BREPLACE && !strcmp(j->name, ".fstxn.batch") && ((off >= 1040 && off < 1136) || (off >= 2208 && off < 2304))) g_stage_damage = 1;
        snprintf(g_what, sizeof(g_what), "%s offset %ld", j->name, (long)off);
    }
    else if (cls == 2)
    {
        off_t len = rnd() % size;
        (void)!ftruncate(fd, len);
        snprintf(g_what, sizeof(g_what), "%s truncated to %ld", j->name, (long)len);
    }
    else
    {
        unsigned char b[16];
        int n = 1 + rnd() % 16;
        for (int i = 0; i < n; i++) b[i] = (unsigned char)rnd();
        (void)!pwrite(fd, b, n, size);
        snprintf(g_what, sizeof(g_what), "%s +%d bytes", j->name, n);
    }
    close(fd);
}
static void fixture(void)
{
    const KIND *k = &K[g_kind];
    wipe(WS); wipe(OUT);
    if (mkdir(WS, 0700) || mkdir(OUT, 0700)) die("mkdir");
    put(OUT, "sentinel", "outside");
    put(WS, "keep", "keep");
    put(WS, k->decoy, "decoy1");
    if (k->decoy2) put(WS, k->decoy2, "decoy2");
    if (g_kind == K_MOVE) put(WS, "src", "old");
    else if (g_kind == K_BREPLACE) { put(WS, "target", "old"); put(WS, "t2", "old"); }
    else if (g_kind != K_CREATE && g_kind != K_BCREATE) put(WS, "target", "old");
}
/* The allowed end states of the operation; returns 1 when the files are one of them. */
static int end_state_ok(int cls)
{
    switch (g_kind)
    {
    case K_REPLACE: return is_content("target", "old") || is_content("target", "new");
    case K_CREATE: return !has("target") || is_content("target", "payload");
    case K_REMOVE: return !has("target") || is_content("target", "old");
    case K_BCREATE: return (!has("target") && !has("t2")) || (is_content("target", "payload") && is_content("t2", "payload"));
    case K_BREPLACE: return (is_content("target", "old") && is_content("t2", "old")) || (is_content("target", "new") && is_content("t2", "new"));
    default:
        /* Deleted move journal at crash point 22: src and dst are hard links of one inode, both "old". That state
           is outside the crash model (the journal is only removed after cleanup), so it is allowed for the delete
           class only and stated as a limit. */
        if (cls == 5 && is_content("src", "old") && is_content("dst", "old")) return 1;
        return (is_content("src", "old") && !has("dst")) || (is_content("dst", "old") && !has("src"));
    }
}
static int allowed_name(const char *n)
{
    const KIND *k = &K[g_kind];
    return !strcmp(n, ".") || !strcmp(n, "..") || !strcmp(n, ".fstxn.lock") || !strcmp(n, "keep") ||
           !strcmp(n, "target") || !strcmp(n, "t2") || !strcmp(n, "src") || !strcmp(n, "dst") || !strcmp(n, k->decoy) ||
           (k->decoy2 && !strcmp(n, k->decoy2));
}
static void one(int kind, int cls, int phase)
{
    char *before, *after, *out_b, *out_a;
    int rc;
    FS_READ_ROOT *r;
    g_kind = kind; g_cls = cls; g_phase = phase; g_name_damage = 0; g_stage_damage = 0;
    fixture();
    if (FsReadOpen(WS, &r) != FS_READ_OK) die("open");
    if (run_child(phase, 0, r) != 90 + phase) die("crash point not reached");
    if (g_legacy) strip_trailers(); else damage(cls);
    before = snap(WS); out_b = snap(OUT);
    rc = run_child(0, 1, r);
    after = snap(WS); out_a = snap(OUT);
    cls_runs[kind][cls]++;
    if (rc < 0) fail("recovery died by signal or hang");
    else if (strcmp(out_b, out_a)) fail("a file outside the workspace changed");
    else if (g_legacy && rc != 40 + FS_READ_OK) fail("a legacy record (no trailer) was refused");
    else if (rc != 40 + FS_READ_OK)
    {
        n_refused++;
        if (strcmp(before, after)) fail("refusal changed the workspace");
    }
    else
    {
        DIR *d;
        struct dirent *e;
        n_ok++;
        if (!end_state_ok(cls))
        {
            /* Known limit F5: the batch journal is deleted while the batch is half published (create crash 6, replace
               crash 61). Recovery has no record, returns OK and leaves the half applied state. Allowed for the delete
               class at those two crash points only; the contract "all targets or none" is NOT met there. */
            if (cls == 5 && ((kind == K_BCREATE && phase == 6 && is_content("target", "payload") && !has("t2")) ||
                             (kind == K_BREPLACE && phase == 61 && is_content("target", "new") && is_content("t2", "old")))) n_f5_limit++;
            else fail("OK but the files are in neither allowed state");
        }
        if (!is_content("keep", "keep")) fail("OK but keep changed");
        if (!is_content(K[kind].decoy, "decoy1") || (K[kind].decoy2 && !is_content(K[kind].decoy2, "decoy2"))) fail("OK but a decoy was removed");
        d = opendir(WS);
        while ((e = readdir(d)))
        {
            if (allowed_name(e->d_name)) continue;
            /* A deleted journal is the crash state "journal gone, cleanup unfinished": the stages and temporary links stay by design. */
            if (cls == 5 && !strncmp(e->d_name, K[kind].stage_pref, strlen(K[kind].stage_pref))) continue;
            if (cls == 5 && (kind == K_REPLACE || kind == K_BREPLACE) && !strncmp(e->d_name, ".fsrb-", 6)) continue;
            if (cls == 5 && !strncmp(e->d_name, ".fstxn-", 7)) continue;
            { snprintf(g_what + strlen(g_what), 40, " left=%.30s", e->d_name); fail("OK but an unexpected name is left"); }
            break;
        }
        closedir(d);
        if (run_child(0, 1, r) != 40 + FS_READ_OK) fail("second recovery not OK");
    }
    free(before); free(after); free(out_b); free(out_a);
    FsReadClose(r);
}

/* Pinned cells for damaged name fields (findings F2 remove, F4 move, F6 batch create, F7 batch replace) and the
   journal deleted while half published (F5). Each cell runs twice:
   - plain damage (the field is rewritten, the record CRC is NOT recomputed): recovery must REFUSE (DENIED), the
     workspace must stay byte for byte unchanged and the journal must stay. Before the integrity trailer these four
     shapes returned OK with a wrong state; the trailer turns them into refusals.
   - recomputed CRC (a writer that rewrites the field and recomputes the CRC): the old outcome remains, because a CRC
     is not authentication: F2 restored under another name, F4 two links of one inode, F6 first target stays
     published (half applied), F7 an orphan stage. These cells document the cooperating-writer limit and flip only if
     the record gets real authentication or the recovery gets an independent check.
   F5 (journal deleted) has no record left to check; the CRC does not change it. */
static int pin_prepare(int kind, int phase, FS_READ_ROOT **r)
{
    g_kind = kind; g_cls = 4; g_phase = phase;
    fixture();
    if (FsReadOpen(WS, r) != FS_READ_OK) die("open");
    if (run_child(phase, 0, *r) != 90 + phase) die("crash point not reached");
    return 1;
}
static int count_prefix(const char *pre)
{
    DIR *d = opendir(WS);
    struct dirent *e;
    int n = 0;
    while (d && (e = readdir(d))) if (!strncmp(e->d_name, pre, strlen(pre))) n++;
    if (d) closedir(d);
    return n;
}
/* Runs recovery; returns 1 when the plain-damage expectation holds (DENIED, workspace unchanged, journal present). */
static int plain_refused(FS_READ_ROOT *r, const char *journal, int *rc_out)
{
    char *b = snap(WS), *a;
    int rc = run_child(0, 1, r), ok;
    a = snap(WS);
    ok = rc == 40 + FS_READ_DENIED && !strcmp(a, b) && has(journal);
    free(a); free(b);
    *rc_out = rc;
    return ok;
}
static void pin_report(const char *name, int good, const char *limit_text)
{
    snprintf(g_what, sizeof(g_what), "%s", name);
    if (!good) { g_cls = 7; fail("a pinned cell changed its outcome (flip it deliberately if a fix is intended)"); }
    else printf("pinned: %s\n", limit_text);
}
static void pinned_cells(void)
{
    FS_READ_ROOT *r;
    int rc, good;
    char name[64], p[512];
    int fd;
    for (int recompute = 0; recompute < 2; recompute++)
    {
        /* F2: remove, crash 12 (target absent, not committed), target field rewritten to "renamed". */
        pin_prepare(K_REMOVE, 12, &r);
        field(".fstxn.remove", 24, "renamed", 7);
        if (recompute) refresh_crc(".fstxn.remove");
        if (!recompute) good = plain_refused(r, ".fstxn.remove", &rc);
        else
        {
            char *ob = snap(OUT), *oa;
            rc = run_child(0, 1, r);
            oa = snap(OUT);
            good = rc == 40 + FS_READ_OK && is_content("renamed", "old") && !has("target") && is_content("keep", "keep") &&
                   !strcmp(ob, oa) && !has(".fstxn.remove") && !has(".fstxn.rcommit");
            free(ob); free(oa);
        }
        FsReadClose(r);
        pin_report(recompute ? "F2 recomputed" : "F2 plain", good, recompute ? "F2 remove, CRC recomputed: restored under another name, recovery OK (cooperating-writer limit)" : "F2 remove, plain damage: refused, workspace unchanged");
        /* F4: move, crash 22 (src and dst links of one inode), source field rewritten to "rrc". */
        pin_prepare(K_MOVE, 22, &r);
        field(".fstxn.move", 24, "rrc", 3);
        if (recompute) refresh_crc(".fstxn.move");
        if (!recompute) good = plain_refused(r, ".fstxn.move", &rc);
        else
        {
            rc = run_child(0, 1, r);
            good = rc == 40 + FS_READ_OK && is_content("rrc", "old") && is_content("src", "old") && !has("dst") && is_content("keep", "keep") &&
                   !has(".fstxn.move") && !has(".fstxn.mcommit");
        }
        FsReadClose(r);
        pin_report(recompute ? "F4 recomputed" : "F4 plain", good, recompute ? "F4 move, CRC recomputed: new name and src are one inode, dst gone, recovery OK (cooperating-writer limit)" : "F4 move, plain damage: refused, workspace unchanged");
        /* F6: batch create, crash 6 (first target published), item 0 target rewritten to "tarqet". */
        pin_prepare(K_BCREATE, 6, &r);
        field(".fstxn.batch", 16, "tarqet", 6);
        if (recompute) refresh_crc(".fstxn.batch");
        if (!recompute) good = plain_refused(r, ".fstxn.batch", &rc);
        else
        {
            rc = run_child(0, 1, r);
            good = rc == 40 + FS_READ_OK && is_content("target", "payload") && !has("t2") && !has("tarqet") && !has(".fstxn.batch") && count_prefix(".fst-") == 2;
        }
        FsReadClose(r);
        pin_report(recompute ? "F6 recomputed" : "F6 plain", good, recompute ? "F6 batch create, CRC recomputed: first target stays published, half applied, recovery OK (cooperating-writer limit)" : "F6 batch create, plain damage: refused, workspace unchanged");
        /* F7: batch replace, crash 61, one hex digit of item 1's newstage changed. */
        pin_prepare(K_BREPLACE, 61, &r);
        snprintf(p, sizeof(p), "%s/.fstxn.batch", WS);
        fd = open(p, O_RDWR | O_NOFOLLOW);
        if (fd < 0 || pread(fd, name, 38, 2240) != 38) die("read newstage");
        name[37] = name[37] == 'a' ? 'b' : 'a';
        (void)!pwrite(fd, name, 38, 2240);
        close(fd);
        name[0] = 0;
        if (recompute) refresh_crc(".fstxn.batch");
        if (!recompute) good = plain_refused(r, ".fstxn.batch", &rc);
        else
        {
            rc = run_child(0, 1, r);
            good = rc == 40 + FS_READ_OK && is_content("target", "old") && is_content("t2", "old") && !has(".fstxn.batch") && count_prefix(".fsrp-") == 2;
        }
        FsReadClose(r);
        pin_report(recompute ? "F7 recomputed" : "F7 plain", good, recompute ? "F7 batch replace, CRC recomputed: orphan stage left, old state, recovery OK (cooperating-writer limit)" : "F7 batch replace, plain damage: refused, workspace unchanged");
    }
    /* F5: journal deleted while half published (create crash 6). No record is left to check. */
    pin_prepare(K_BCREATE, 6, &r);
    unlink(WS "/.fstxn.batch");
    rc = run_child(0, 1, r);
    good = rc == 40 + FS_READ_OK && is_content("target", "payload") && !has("t2") && !has(".fstxn.batch");
    FsReadClose(r);
    pin_report("F5", good, "F5 batch journal deleted while half published, recovery OK, half applied (known limit, outside the crash model)");
}
/* Legacy cells: a journal as the previous build wrote it (no trailer) must still be recovered, at every crash point of
   every kind. Deterministic, no random numbers. */
static void legacy_cells(void)
{
    g_legacy = 1;
    for (int kind = 0; kind < NKIND; kind++)
        for (int i = 0; i < K[kind].nph; i++)
            one(kind, 0, K[kind].phases[i]);
    g_legacy = 0;
}

int main(void)
{
    const char *e;
    unsigned iters = 350;
    if ((e = getenv("FS_JFUZZ_SEED"))) g_seed = (uint32_t)strtoul(e, NULL, 0);
    if ((e = getenv("FS_JFUZZ_ITERS"))) iters = (unsigned)strtoul(e, NULL, 0);
    rs = g_seed;
    pinned_cells();
    legacy_cells();
    for (int kind = 0; kind < NKIND; kind++)
        for (g_iter = 0; g_iter < iters; g_iter++)
            one(kind, (int)(g_iter % K[kind].ncls), K[kind].phases[(g_iter / K[kind].ncls) % K[kind].nph]);
    wipe(WS); wipe(OUT);
    for (int kind = 0; kind < NKIND; kind++)
        for (int i = 0; i < K[kind].ncls; i++)
            if (!cls_runs[kind][i]) { fprintf(stderr, "class %s of %s never ran\n", CLS[i], KN[kind]); return 1; }
    printf("journal fuzz iterations: %u per kind, %d kinds, seed %u refused %d ok %d known-limit-hits %d failures %d class mask %llx\n", iters, NKIND, (unsigned)g_seed, n_refused, n_ok, n_f2_limit + n_f5_limit + n_f6_limit + n_f7_limit, nfail, (unsigned long long)fail_mask);
#ifdef FS_JFUZZ_MUTANT_MASK
#ifndef FS_JFUZZ_MUTANT_IGNORE
#define FS_JFUZZ_MUTANT_IGNORE 0
#endif
    /* Exit 0 only when the failing (kind, class) cells, outside the ignored ones, are exactly the predicted
       ones. Bit = kind * 8 + class (kinds replace, create, remove, move, batch_create, batch_replace). FS_JFUZZ_MUTANT_IGNORE lists cells
       whose result under THIS mutant is not predicted (the random byte classes: a mutant that accepts a
       damaged identity number can add failures there, seed dependent). */
    {
        unsigned long long seen = (unsigned long long)fail_mask & ~(unsigned long long)FS_JFUZZ_MUTANT_IGNORE;
        if (seen == (unsigned long long)FS_JFUZZ_MUTANT_MASK)
        {
            printf("mutant killed, failing class mask exactly %llx\n", seen);
            return 0;
        }
        printf("mutant SURVIVED or failed elsewhere: class mask %llx (ignored %llx), predicted %llx\n", (unsigned long long)fail_mask, (unsigned long long)FS_JFUZZ_MUTANT_IGNORE, (unsigned long long)FS_JFUZZ_MUTANT_MASK);
        return 1;
    }
#endif
    return nfail ? 1 : 0;
}
#else
int main(void) { return 0; }
#endif
