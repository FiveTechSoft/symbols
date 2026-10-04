#ifdef _WIN32
/* M1 criterion 3, Windows reading of POSIX T9. T9 plants a .fstxn.lock with an
   unsafe mode (0644) and expects AgentPatch to fail closed. Windows has no mode
   bits, so this test asks the nearest question: what does PatchApplyAtomic do when
   .fstxn.lock is not a plain empty file it may lock. Cells: a directory, a file
   symlink (if the runner can create one), a non-empty file, a file held open
   without sharing by another handle, and a plain empty file as the control. A
   hard-linked empty lock is run and printed but not asserted (no claim is made
   about it).
   Predictions (written before the first run): directory, symlink, non-empty and
   held-open all give rc 0 with a.c still OLD and the lock object unchanged, and
   the same plan applies once the lock is back to a plain empty file (held-open:
   once the holder closes); the control applies (rc 1); the hard-linked lock is
   accepted (rc 1) because wc_lock checks only kind and size. */
#include "agent_patch.h"
#include <windows.h>
#include <direct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define WS "test_agent_patch_lock_win_scratch"
#define LK WS "\\.fstxn.lock"
#define AUX "test_agent_patch_lock_win_aux" /* outside WS, so the leftover check stays strict */
#define OLDC "int x = 1;\n"
#define NEWC "int x = 9;\n"
static int fails;
static void ck(int x, const char *m)
{
    printf("  [%s] %s\n", x ? "PASS" : "FAIL", m);
    if (!x) fails++;
}
static void put(const char *p, const char *v)
{
    FILE *f = fopen(p, "wb");
    if (!f || fwrite(v, 1, strlen(v), f) != strlen(v)) { fprintf(stderr, "put %s\n", p); exit(2); }
    fclose(f);
}
static int is(const char *p, const char *v)
{
    char buf[4096];
    FILE *f = fopen(p, "rb");
    size_t n;
    if (!f) return 0;
    n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = 0;
    return strcmp(buf, v) == 0;
}
static void wipe(void)
{
    (void)system("if exist " LK "\\NUL rmdir " LK " >NUL 2>NUL");
    (void)system("if exist " WS " rmdir /s /q " WS " >NUL 2>NUL");
    (void)system("if exist " AUX " rmdir /s /q " AUX " >NUL 2>NUL");
}
static void reset(void)
{
    wipe();
    if (_mkdir(WS) != 0) { fprintf(stderr, "mkdir\n"); exit(2); }
    put(WS "\\a.c", OLDC);
}
static void plan_for(PATCH_PLAN *p)
{
    PatchPlanInit(p, WS "\\a.c");
    PatchPlanSetWorkspace(p, WS);
    PatchPlanAddHunk(p, 1, "", OLDC, NEWC, "");
}
/* Names other than the target and the lock itself. */
static int leftovers(void)
{
    WIN32_FIND_DATAA d;
    HANDLE h = FindFirstFileA(WS "\\*", &d);
    int n = 0;
    if (h == INVALID_HANDLE_VALUE) return -1;
    do {
        if (!strcmp(d.cFileName, ".") || !strcmp(d.cFileName, "..") ||
            !strcmp(d.cFileName, "a.c") || !strcmp(d.cFileName, ".fstxn.lock")) continue;
        printf("  (leftover: %s)\n", d.cFileName);
        n++;
    } while (FindNextFileA(h, &d));
    FindClose(h);
    return n;
}
/* Refused: rc 0, target OLD, nothing else left behind. */
static void refused(const char *what)
{
    PATCH_PLAN p;
    int rc;
    plan_for(&p);
    rc = PatchApplyAtomic(&p);
    printf("  (measured: %s: apply rc %d, a.c %s, io_diag [%s])\n", what, rc,
           is(WS "\\a.c", OLDC) ? "OLD" : is(WS "\\a.c", NEWC) ? "NEW" : "OTHER", p.io_diag);
    ck(rc == 0 && is(WS "\\a.c", OLDC), "apply refused, target untouched");
    ck(leftovers() == 0, "no leftover names");
    PatchPlanFree(&p);
}
static void applies(const char *m)
{
    PATCH_PLAN p;
    plan_for(&p);
    ck(PatchApplyAtomic(&p) == 1 && is(WS "\\a.c", NEWC), m);
    PatchPlanFree(&p);
}
int main(void)
{
    int ran = 0, symlink_ran = 0;
    HANDLE h;

    puts("L1 lock is a directory");
    reset();
    ck(_mkdir(LK) == 0, "directory fixture");
    refused("directory");
    ck(GetFileAttributesA(LK) != INVALID_FILE_ATTRIBUTES && (GetFileAttributesA(LK) & FILE_ATTRIBUTE_DIRECTORY),
       "the directory is still there");
    ck(_rmdir(LK) == 0, "remove the directory");
    applies("the same plan applies once the lock is gone");
    ran++;

    puts("L2 lock is a file symlink");
    reset();
    if (_mkdir(AUX) != 0) { fprintf(stderr, "mkdir aux\n"); exit(2); }
    put(AUX "\\other", "");
    if (CreateSymbolicLinkA(LK, "..\\" AUX "\\other", 0x2 /* ALLOW_UNPRIVILEGED_CREATE */))
    {
        symlink_ran = 1;
        refused("symlink");
        ck(DeleteFileA(LK) != 0, "remove the symlink");
        applies("the same plan applies once the lock is gone");
    }
    printf("  (symlink lock case: %s)\n", symlink_ran ? "RAN" : "SKIPPED, runner cannot create symlinks");
    ran++;

    puts("L3 lock is a non-empty file");
    reset();
    put(LK, "x");
    refused("non-empty file");
    ck(is(LK, "x"), "the lock file is untouched");
    ck(DeleteFileA(LK) != 0, "remove the lock file");
    applies("the same plan applies once the lock is gone");
    ran++;

    puts("L4 lock held open without sharing by another handle");
    reset();
    put(LK, "");
    h = CreateFileA(LK, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    ck(h != INVALID_HANDLE_VALUE, "holder opened the lock without sharing");
    refused("held open");
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    applies("the same plan applies once the holder is gone");
    ran++;

    puts("L5 control: plain empty lock file");
    reset();
    put(LK, "");
    applies("a plain empty lock is accepted");
    ran++;

    puts("L6 hard-linked empty lock (measured only, no assertion)");
    reset();
    if (_mkdir(AUX) != 0) { fprintf(stderr, "mkdir aux\n"); exit(2); }
    put(AUX "\\other", "");
    if (CreateHardLinkA(LK, AUX "\\other", NULL))
    {
        PATCH_PLAN p;
        int rc;
        plan_for(&p);
        rc = PatchApplyAtomic(&p);
        printf("  (measured: hard-linked lock: apply rc %d, a.c %s, io_diag [%s])\n", rc,
               is(WS "\\a.c", OLDC) ? "OLD" : is(WS "\\a.c", NEWC) ? "NEW" : "OTHER", p.io_diag);
        PatchPlanFree(&p);
    }
    else puts("  (hard link fixture failed)");

    wipe();
    if (!fails) printf("agentpatch lock cells ran: %d\n", ran);
    printf("%s (%d failures)\n", fails ? "FAILED" : "ALL PASS", fails);
    return fails ? 1 : 0;
}
#else
int main(void) { return 0; }
#endif
