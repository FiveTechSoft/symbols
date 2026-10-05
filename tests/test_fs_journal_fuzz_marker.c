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
/* Damage to the 8 byte commit marker alone (M1 criterion 5, journal part, POSIX: remove, move, batch create, batch
   replace). The marker is the journal's inode number (src/fs_read.c: batch markers at brepl_recover_locked and its
   create twin, remove and move markers in their recovery): recovery requires a regular file, one link, size exactly 8
   and the value of the live journal inode. A child is killed at every crash point of the kind that leaves a marker (found
   by looking, not assumed), the marker alone is damaged, then recovery runs in a forked child. Claim checked:
   every damaged marker is refused (DENIED), the workspace is byte for byte unchanged, nothing outside the workspace
   changes, the journal and the marker stay. The control cells are not damage: the marker rewritten with the journal's own
   inode number, and the marker replaced by a new file holding the same 8 bytes, recover OK in the committed state
   with no journal or marker left, and a second recovery is OK. This says nothing about a writer that writes the
   journal's inode number: that is the cooperating-writer model, the same limit as a recomputed CRC. Reproduce with
   FS_JMARK_SEED=<n>. Not covered: the replace marker (it embeds the record and has the CRC trailer), Windows, power loss,
   marker deleted (see test_fs_journal_fuzz delete class). */
#define WS "test_fs_journal_fuzz_marker_scratch"
#define OUT "test_fs_journal_fuzz_marker_outside"
enum { K_REMOVE, K_MOVE, K_BCREATE, K_BREPLACE, NKIND };
static const char *KN[NKIND] = {"remove", "move", "batch_create", "batch_replace"};
static const char *JN[NKIND] = {".fstxn.remove", ".fstxn.move", ".fstxn.batch", ".fstxn.batch"};
static const char *MN[NKIND] = {".fstxn.rcommit", ".fstxn.mcommit", ".fstxn.commit", ".fstxn.commit"};
static const int PH[NKIND][5] = {{11, 12, 14, 0, 0}, {21, 22, 23, 25, 0}, {5, 6, 7, 0, 0}, {60, 61, 62, 63, 0}};
static uint32_t g_seed = 0x6a7f0002u, rs;
static int g_kind, g_phase, nfail, ncells, nmarkerphase[NKIND], n_ctrl;
static char g_what[160];
static uint32_t rnd(void) { rs = rs * 1664525u + 1013904223u; return rs >> 8; }
static void fail(const char *m)
{
    nfail++;
    if (nfail <= 20) fprintf(stderr, "FAIL %s (seed=%u kind=%s phase=%d %s)\n", m, (unsigned)g_seed, KN[g_kind], g_phase, g_what);
}
static void die(const char *m) { fprintf(stderr, "SETUP %s (kind=%s phase=%d)\n", m, KN[g_kind], g_phase); exit(2); }
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
/* Flat serialization: sorted name, mode, size, link count, bytes (lstat, so a symlink shows as itself). */
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
        f = S_ISLNK(st.st_mode) ? NULL : fopen(p, "rb");
        if (f) { got = fread(buf, 1, sizeof(buf), f); fclose(f); }
        snprintf(hdr, sizeof(hdr), "[%s|%o|%ld|%ld|", names[i], (unsigned)st.st_mode, (long)st.st_size, (long)st.st_nlink);
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
static void fixture(void)
{
    wipe(WS); wipe(OUT);
    if (mkdir(WS, 0700) || mkdir(OUT, 0700)) die("mkdir");
    put(OUT, "sentinel", "outside");
    put(WS, "keep", "keep");
    if (g_kind == K_MOVE) put(WS, "src", "old");
    else if (g_kind == K_BREPLACE) { put(WS, "target", "old"); put(WS, "t2", "old"); }
    else if (g_kind == K_REMOVE) put(WS, "target", "old");
}
/* the state a committed operation leaves */
static int committed_ok(void)
{
    switch (g_kind)
    {
    case K_REMOVE: return !has("target");
    case K_MOVE: return is_content("dst", "old") && !has("src");
    case K_BCREATE: return is_content("target", "payload") && is_content("t2", "payload");
    default: return is_content("target", "new") && is_content("t2", "new");
    }
}
static void mpath(char *p, size_t n) { snprintf(p, n, "%s/%s", WS, MN[g_kind]); }
static uint64_t ino_of(const char *name)
{
    char p[512];
    struct stat st;
    snprintf(p, sizeof(p), "%s/%s", WS, name);
    if (lstat(p, &st) != 0) die("ino_of");
    return (uint64_t)st.st_ino;
}
static void write_marker_bytes(const void *b, size_t n)
{
    char p[512];
    int fd;
    mpath(p, sizeof(p));
    fd = open(p, O_WRONLY | O_TRUNC | O_NOFOLLOW);
    if (fd < 0 || (n && write(fd, b, n) != (ssize_t)n)) die("write marker");
    close(fd);
}
static void read_marker(unsigned char b[8])
{
    char p[512];
    int fd;
    mpath(p, sizeof(p));
    fd = open(p, O_RDONLY | O_NOFOLLOW);
    if (fd < 0 || read(fd, b, 8) != 8) die("read marker");
    close(fd);
}
/* One damaged-marker cell. Damage kinds: 0 bit flip (arg = bit 0..63), 1 set byte (arg = offset, random value),
   2 truncate (arg = length 0..7), 3 append (arg = bytes), 4 eight zero bytes, 5 inode of an unrelated file,
   6 inode of the marker file itself (a stale value), 7 symlink in place of the marker, 8 second hard link,
   9 control: the journal's own inode number written in place, 10 control: replaced by a new file with the same bytes. */
static const char *DN[] = {"bitflip", "setbyte", "truncate", "append", "zeros", "unrelated_inode", "stale_inode",
                           "symlink", "second_link", "ctrl_same_value", "ctrl_new_file"};
static void cell(int dk, int arg)
{
    char *before, *after, *ob, *oa, p[512];
    unsigned char b[32];
    int rc, ctrl = dk >= 9;
    FS_READ_ROOT *r;
    uint64_t v;
    g_what[0] = 0;
    fixture();
    if (FsReadOpen(WS, &r) != FS_READ_OK) die("open");
    if (run_child(g_phase, 0, r) != 90 + g_phase) die("crash point not reached");
    snprintf(g_what, sizeof(g_what), "%s arg=%d", DN[dk], arg);
    read_marker(b);
    switch (dk)
    {
    case 0: b[arg / 8] ^= (unsigned char)(1u << (arg % 8)); write_marker_bytes(b, 8); break;
    case 1: b[arg] ^= (unsigned char)(1 + rnd() % 255); write_marker_bytes(b, 8); break;
    case 2: write_marker_bytes(b, (size_t)arg); break;
    case 3: for (int k = 0; k < arg; k++) b[8 + k] = (unsigned char)rnd(); write_marker_bytes(b, 8 + (size_t)arg); break;
    case 4: memset(b, 0, 8); write_marker_bytes(b, 8); break;
    case 5: v = ino_of("keep"); write_marker_bytes(&v, 8); break;
    case 6: v = ino_of(MN[g_kind]); write_marker_bytes(&v, 8); break;
    case 7: mpath(p, sizeof(p)); unlink(p); if (symlink("keep", p) != 0) die("symlink"); break;
    case 8:
    {
        char q[512];
        mpath(p, sizeof(p));
        snprintf(q, sizeof(q), "%s/.fsx-second-link", WS);
        if (link(p, q) != 0) die("link");
        break;
    }
    case 9: v = ino_of(JN[g_kind]); write_marker_bytes(&v, 8); break;
    default:
    {
        int fd;
        mpath(p, sizeof(p));
        unlink(p);
        fd = open(p, O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (fd < 0 || write(fd, b, 8) != 8) die("new marker");
        close(fd);
        break;
    }
    }
    before = snap(WS); ob = snap(OUT);
    rc = run_child(0, 1, r);
    after = snap(WS); oa = snap(OUT);
    ncells++;
    if (rc < 0) fail("recovery died by signal or hang");
    else if (strcmp(ob, oa)) fail("a file outside the workspace changed");
    else if (!ctrl)
    {
        if (rc != 40 + FS_READ_DENIED) fail("a damaged marker was not refused");
        else
        {
            if (strcmp(before, after)) fail("refusal changed the workspace");
            if (!has(JN[g_kind]) || !has(MN[g_kind])) fail("refusal removed the journal or the marker");
        }
    }
    else
    {
        n_ctrl++;
        if (dk == 10)
        {
            /* the marker is a new file with the same 8 bytes: its value is the journal inode, so it must pass */
        }
        if (rc != 40 + FS_READ_OK) fail("a control marker (the journal's own inode number) was refused");
        else
        {
            if (!committed_ok()) fail("control: OK but the operation is not in the committed state");
            if (has(JN[g_kind]) || has(MN[g_kind])) fail("control: OK but the journal or the marker is left");
            if (!is_content("keep", "keep")) fail("control: keep changed");
            if (run_child(0, 1, r) != 40 + FS_READ_OK) fail("control: second recovery not OK");
        }
    }
    free(before); free(after); free(ob); free(oa);
    FsReadClose(r);
}
int main(void)
{
    const char *e;
    if ((e = getenv("FS_JMARK_SEED"))) g_seed = (uint32_t)strtoul(e, NULL, 0);
    rs = g_seed;
    for (g_kind = 0; g_kind < NKIND; g_kind++)
        for (int pi = 0; PH[g_kind][pi]; pi++)
        {
            FS_READ_ROOT *r;
            g_phase = PH[g_kind][pi];
            fixture();
            if (FsReadOpen(WS, &r) != FS_READ_OK) die("open");
            if (run_child(g_phase, 0, r) != 90 + g_phase) die("crash point not reached");
            if (!has(MN[g_kind])) { FsReadClose(r); continue; }
            FsReadClose(r);
            nmarkerphase[g_kind]++;
            for (int bit = 0; bit < 64; bit++) cell(0, bit);
            for (int off = 0; off < 8; off++) cell(1, off);
            for (int len = 0; len < 8; len++) cell(2, len);
            { static const int ap[4] = {1, 8, 9, 16}; for (int k = 0; k < 4; k++) cell(3, ap[k]); }
            for (int dk = 4; dk <= 10; dk++) cell(dk, 0);
        }
    wipe(WS); wipe(OUT);
    for (int k = 0; k < NKIND; k++)
        if (!nmarkerphase[k]) { fprintf(stderr, "no crash point of %s leaves a marker\n", KN[k]); return 1; }
    printf("journal marker fuzz: %d cells, seed %u, marker phases per kind %d %d %d %d, control cells %d, failures %d\n",
           ncells, (unsigned)g_seed, nmarkerphase[0], nmarkerphase[1], nmarkerphase[2], nmarkerphase[3], n_ctrl, nfail);
    return nfail ? 1 : 0;
}
#else
int main(void) { return 0; }
#endif
