#ifdef _WIN32
/* M1-3 / E1: AgentPatch on the Fs* API, Windows (NTFS). Port of the POSIX
   test_agent_patch_fs T1-T8. T9 (lock file mode 0644) is NOT ported: it is a
   POSIX permission-bit case with no NTFS equivalent. Crash and hook points are
   compiled in for this target alone (FS_CREATE_TEST_CRASH, AGENT_PATCH_TEST_HOOK). */
#include "agent_patch.h"
#include <windows.h>
#include <direct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void (*g_patch_test_before_replace)(void);

#define WS "test_agent_patch_fs_win_scratch"
#define OUTSIDE "test_agent_patch_fs_win_outside.c"
#define OLDC "int x = 1;\n"
#define NEWC "int x = 9;\n"

static int fails;
#ifdef AGENT_PATCH_APPLY_MUTANT
/* Mutant build (target test_agent_patch_fs_win_mc): replace_target drops its drift
   guard. Predicted kill set, written before the first run: exactly four cells,
   T1 "apply refuses", T1 "external bytes kept", T1 "diagnostic set, not applied"
   and T8 "rollback refused, user edit kept". Every other cell must still pass.
   Bits: 1, 2, 4, 8 in that order; anything else failing counts as "other". */
static int fail_mask, fail_other;
#endif
static void ck(int x, const char *m)
{
    printf("  [%s] %s\n", x ? "PASS" : "FAIL", m);
    if (!x)
    {
        fails++;
#ifdef AGENT_PATCH_APPLY_MUTANT
        if (!strcmp(m, "apply refuses")) fail_mask |= 1;
        else if (!strcmp(m, "external bytes kept")) fail_mask |= 2;
        else if (!strcmp(m, "diagnostic set, not applied")) fail_mask |= 4;
        else if (!strcmp(m, "rollback refused, user edit kept")) fail_mask |= 8;
        else fail_other++;
#endif
    }
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
    /* remove a junction as a link first, so the recursive delete never follows it */
    (void)system("if exist " WS "\\jn rmdir " WS "\\jn >NUL 2>NUL");
    (void)system("if exist " WS " rmdir /s /q " WS " >NUL 2>NUL");
    (void)system("if exist " OUTSIDE " del /f /q " OUTSIDE " >NUL 2>NUL");
}
static void reset(void)
{
    wipe();
    if (_mkdir(WS) != 0) { fprintf(stderr, "mkdir\n"); exit(2); }
    put(WS "\\a.c", OLDC);
}
static void plan_for(PATCH_PLAN *p, const char *target)
{
    PatchPlanInit(p, target);
    PatchPlanSetWorkspace(p, WS);
    PatchPlanAddHunk(p, 1, "", OLDC, NEWC, "");
}
static void hook_external(void) { put(WS "\\a.c", OLDC "// external\n"); }
static HANDLE g_foreign = INVALID_HANDLE_VALUE;
static void hook_foreign_handle(void)
{
    /* open without FILE_SHARE_DELETE: the rename-over of the target is refused */
    g_foreign = CreateFileA(WS "\\a.c", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
}

static int child_apply(void)
{
    PATCH_PLAN p;
    _putenv_s("FS_WIN_REPLACE_CRASH", "4");
    plan_for(&p, WS "\\a.c");
    (void)PatchApplyAtomic(&p);
    return 200; /* the crash point must have ended the process before this */
}

static int run_child(const char *exe)
{
    char cmd[1024];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    DWORD code = 0;
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);
    snprintf(cmd, sizeof(cmd), "\"%s\" child", exe);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) return -1;
    if (WaitForSingleObject(pi.hProcess, 30000) != WAIT_OBJECT_0) return -2;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (int)code;
}

int main(int argc, char **argv)
{
    PATCH_PLAN p;
    PATCH_VERIFY_REPORT rep;
    char cwd[512], abs[1024], exe[768];
    int symlink_ran = 0;
    DWORD got;
    if (argc == 2 && !strcmp(argv[1], "child")) return child_apply();
    if (!_getcwd(cwd, sizeof(cwd))) return 2;
    got = GetModuleFileNameA(NULL, exe, sizeof(exe));
    if (!got || got >= sizeof(exe)) return 2;

    puts("T1 drift between read and replace is DENIED, not overwritten");
    reset();
    plan_for(&p, WS "\\a.c");
    g_patch_test_before_replace = hook_external;
    ck(PatchApplyAtomic(&p) == 0, "apply refuses");
    g_patch_test_before_replace = NULL;
    ck(is(WS "\\a.c", OLDC "// external\n"), "external bytes kept");
    ck(p.io_diag[0] != 0 && !p.is_applied, "diagnostic set, not applied");
    PatchPlanFree(&p);

    puts("T2 success leaves only .fstxn.lock as control artifact");
    reset();
    plan_for(&p, WS "\\a.c");
    ck(PatchApplyAtomic(&p) == 1 && is(WS "\\a.c", NEWC), "apply ok");
    {
        WIN32_FIND_DATAA d;
        HANDLE h = FindFirstFileA(WS "\\*", &d);
        int extra = 0, found = 0;
        if (h != INVALID_HANDLE_VALUE)
        {
            do
            {
                found++;
                if (strcmp(d.cFileName, ".") && strcmp(d.cFileName, "..") &&
                    strcmp(d.cFileName, "a.c") && strcmp(d.cFileName, ".fstxn.lock"))
                {
                    printf("  (leftover: %s)\n", d.cFileName);
                    extra++;
                }
            } while (FindNextFileA(h, &d));
            FindClose(h);
        }
        ck(found > 0 && extra == 0, "no stage/intent leftovers");
    }
    PatchPlanFree(&p);

    puts("T3 rollback restores bytes (file attributes: see E2, not claimed here)");
    reset();
    plan_for(&p, WS "\\a.c");
    ck(PatchApplyAtomic(&p) == 1 && is(WS "\\a.c", NEWC), "apply ok");
    ck(PatchRollback(&p) == 1 && is(WS "\\a.c", OLDC), "rollback restores bytes");
    PatchPlanFree(&p);

    puts("T4 paths outside the workspace are rejected");
    reset();
    put(OUTSIDE, OLDC);
    plan_for(&p, WS "\\..\\" OUTSIDE);
    ck(PatchVerifyPlan(&p, &rep) == 0 && rep.status == PATCH_CHECK_IO_ERROR, "dotdot verify rejected");
    ck(PatchApplyAtomic(&p) == 0 && is(OUTSIDE, OLDC), "dotdot apply rejected, file untouched");
    PatchPlanFree(&p);
    snprintf(abs, sizeof(abs), "%s\\%s", cwd, OUTSIDE);
    plan_for(&p, abs);
    ck(PatchApplyAtomic(&p) == 0 && is(OUTSIDE, OLDC), "absolute path outside rejected");
    PatchPlanFree(&p);
    plan_for(&p, OUTSIDE);
    ck(PatchApplyAtomic(&p) == 0 && is(OUTSIDE, OLDC), "relative path outside rejected");
    PatchPlanFree(&p);
    snprintf(abs, sizeof(abs), "%s\\%s\\a.c", cwd, WS);
    plan_for(&p, abs);
    ck(PatchApplyAtomic(&p) == 1 && is(WS "\\a.c", NEWC), "absolute path inside workspace works");
    PatchPlanFree(&p);

    puts("T5 files over 1 MiB are refused explicitly");
    reset();
    {
        size_t n = 2u * 1024 * 1024;
        char *big = (char *)malloc(n + 1);
        FILE *f;
        WIN32_FILE_ATTRIBUTE_DATA ad;
        memset(big, 'a', n);
        big[n] = 0;
        f = fopen(WS "\\big.c", "wb");
        fwrite(big, 1, n, f);
        fclose(f);
        free(big);
        plan_for(&p, WS "\\big.c");
        ck(PatchVerifyPlan(&p, &rep) == 0 && rep.status == PATCH_CHECK_IO_ERROR, "verify reports IO error");
        ck(PatchApplyAtomic(&p) == 0 && GetFileAttributesExA(WS "\\big.c", GetFileExInfoStandard, &ad) &&
           ad.nFileSizeHigh == 0 && ad.nFileSizeLow == 2u * 1024 * 1024, "apply refused, size unchanged");
        PatchPlanFree(&p);
    }

    puts("T6 crash then recovery, and PENDING is never reported as success");
    reset();
    ck(run_child(exe) == 144, "crash point (phase 4: published, not committed) reached");
    plan_for(&p, WS "\\a.c");
    ck(is(WS "\\a.c", NEWC) || is(WS "\\a.c", OLDC), "after crash: old or new, never mixed");
    ck(PatchApplyAtomic(&p) == 0, "pending transaction blocks further apply");
    ck(PatchRecover(&p) == 1, "recovery completes");
    ck(is(WS "\\a.c", NEWC) || is(WS "\\a.c", OLDC), "after recovery: old or new, never mixed");
    printf("  (measured: after phase-4 crash and recovery the file holds %s)\n",
           is(WS "\\a.c", OLDC) ? "OLD" : is(WS "\\a.c", NEWC) ? "NEW" : "OTHER");
    PatchPlanFree(&p);
    reset();
    plan_for(&p, WS "\\a.c");
    g_patch_test_before_replace = hook_foreign_handle;
    {
        int rc = PatchApplyAtomic(&p);
        g_patch_test_before_replace = NULL;
        if (g_foreign != INVALID_HANDLE_VALUE) { CloseHandle(g_foreign); g_foreign = INVALID_HANDLE_VALUE; }
        ck(rc == (is(WS "\\a.c", NEWC) ? 1 : 0), "PENDING result matches on-disk bytes");
        ck((rc == 1) == (p.is_applied != 0), "is_applied matches result");
        printf("  (measured: foreign-handle apply returned %d, file holds %s)\n", rc,
               is(WS "\\a.c", OLDC) ? "OLD" : is(WS "\\a.c", NEWC) ? "NEW" : "OTHER");
    }
    PatchPlanFree(&p);

    puts("T7 hard-linked, reparse-parent and symlink targets are refused");
    reset();
    ck(CreateHardLinkA(WS "\\h2.c", WS "\\a.c", NULL) != 0, "hard link fixture");
    plan_for(&p, WS "\\a.c");
    ck(PatchApplyAtomic(&p) == 0 && is(WS "\\a.c", OLDC) && is(WS "\\h2.c", OLDC),
       "hard-linked target refused, both untouched");
    PatchPlanFree(&p);
    reset();
    {
        char absreal[MAX_PATH], cmd[2 * MAX_PATH + 128];
        int mk = _mkdir(WS "\\real") == 0;
        DWORD n = GetFullPathNameA(WS "\\real", MAX_PATH, absreal, NULL);
        put(WS "\\real\\a.c", OLDC);
        snprintf(cmd, sizeof(cmd), "cmd /D /C mklink /J \"%s\\jn\" \"%s\" >NUL", WS, absreal);
        ck(mk && n > 0 && n < MAX_PATH && system(cmd) == 0, "junction fixture");
        plan_for(&p, WS "\\jn\\a.c");
        ck(PatchApplyAtomic(&p) == 0 && is(WS "\\real\\a.c", OLDC), "junction parent refused, real file untouched");
        PatchPlanFree(&p);
        (void)system("rmdir " WS "\\jn >NUL 2>NUL");
    }
    reset();
    if (CreateSymbolicLinkA(WS "\\link.c", "a.c", 0x2 /* ALLOW_UNPRIVILEGED_CREATE */))
    {
        symlink_ran = 1;
        plan_for(&p, WS "\\link.c");
        ck(PatchApplyAtomic(&p) == 0 && is(WS "\\a.c", OLDC), "symlink target refused, file untouched");
        PatchPlanFree(&p);
    }
    printf("  (symlink leaf case: %s)\n", symlink_ran ? "RAN" : "SKIPPED, runner cannot create symlinks");

    puts("T8 rollback refuses to overwrite drift");
    reset();
    plan_for(&p, WS "\\a.c");
    ck(PatchApplyAtomic(&p) == 1, "apply ok");
    put(WS "\\a.c", "user edit\n");
    ck(PatchRollback(&p) == 0 && is(WS "\\a.c", "user edit\n") && p.is_applied, "rollback refused, user edit kept");
    PatchPlanFree(&p);

    puts("T9 not ported: .fstxn.lock mode 0644 is a POSIX permission case");

    wipe();
#ifdef AGENT_PATCH_APPLY_MUTANT
    /* Exit 0 only when the failing cells are exactly the predicted four. */
    if (fail_mask == 0xF && fail_other == 0 && fails == 4)
    {
        printf("mutant killed by %d cell(s)\n", fails);
        return 0;
    }
    printf("mutant SURVIVED or failed elsewhere: mask %x, other %d, fails %d\n", fail_mask, fail_other, fails);
    return 1;
#endif
    printf("%s (%d failures)\n", fails ? "FAILED" : "ALL PASS", fails);
    return fails ? 1 : 0;
}
#else
int main(void) { return 0; }
#endif
