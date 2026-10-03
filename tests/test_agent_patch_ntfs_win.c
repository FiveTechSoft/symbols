#ifdef _WIN32
/* M1-3 / E2: AgentPatch on the Fs* API, NTFS edge cases. CRLF and BOM round
   trips, multi-hunk all-or-nothing, a read-only target, and a foreign handle
   held by ANOTHER process. Where the exact Fs* status is not pinned by an
   earlier measurement the test asserts coherence (return code vs bytes on
   disk, no wedged state, retry works) and does not promise a status. */
#include "agent_patch.h"
#include <windows.h>
#include <direct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void (*g_patch_test_before_replace)(void);

#define WS "test_agent_patch_ntfs_win_scratch"
#define SYNC_READY WS "\\..\\test_agent_patch_ntfs_win.ready"
#define SYNC_GO WS "\\..\\test_agent_patch_ntfs_win.go"
#define OLDC "int x = 1;\n"
#define NEWC "int x = 9;\n"

static int fails;
static char g_exe[768];
static PROCESS_INFORMATION g_hold;
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
    (void)system("if exist " WS " attrib -r /s /d " WS "\\* >NUL 2>NUL");
    (void)system("if exist " WS " rmdir /s /q " WS " >NUL 2>NUL");
    (void)system("if exist " SYNC_READY " del /f /q " SYNC_READY " >NUL 2>NUL");
    (void)system("if exist " SYNC_GO " del /f /q " SYNC_GO " >NUL 2>NUL");
}
static void reset(void)
{
    wipe();
    if (_mkdir(WS) != 0) { fprintf(stderr, "mkdir\n"); exit(2); }
}
static void plan1(PATCH_PLAN *p, const char *target, const char *o, const char *n)
{
    PatchPlanInit(p, target);
    PatchPlanSetWorkspace(p, WS);
    PatchPlanAddHunk(p, 1, "", o, n, "");
}
static int only_expected_files(const char *name1)
{
    WIN32_FIND_DATAA d;
    HANDLE h = FindFirstFileA(WS "\\*", &d);
    int extra = 0;
    if (h == INVALID_HANDLE_VALUE) return 0;
    do
    {
        if (strcmp(d.cFileName, ".") && strcmp(d.cFileName, "..") &&
            strcmp(d.cFileName, name1) && strcmp(d.cFileName, ".fstxn.lock"))
        {
            printf("  (leftover: %s)\n", d.cFileName);
            extra++;
        }
    } while (FindNextFileA(h, &d));
    FindClose(h);
    return extra == 0;
}

/* ---- holder process: opens the target without delete sharing, then waits ---- */
static int holder_main(const char *target)
{
    int i;
    HANDLE h = CreateFileA(target, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    HANDLE g;
    if (h == INVALID_HANDLE_VALUE) return 3;
    g = CreateFileA(SYNC_READY, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    if (g == INVALID_HANDLE_VALUE) return 4;
    CloseHandle(g);
    for (i = 0; i < 1500 && GetFileAttributesA(SYNC_GO) == INVALID_FILE_ATTRIBUTES; i++) Sleep(20);
    CloseHandle(h);
    return 0;
}
static int spawn_holder(const char *target)
{
    char cmd[1200];
    STARTUPINFOA si;
    int i;
    memset(&si, 0, sizeof(si));
    memset(&g_hold, 0, sizeof(g_hold));
    si.cb = sizeof(si);
    snprintf(cmd, sizeof(cmd), "\"%s\" hold \"%s\"", g_exe, target);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &g_hold)) return 0;
    for (i = 0; i < 1000 && GetFileAttributesA(SYNC_READY) == INVALID_FILE_ATTRIBUTES; i++) Sleep(20);
    return GetFileAttributesA(SYNC_READY) != INVALID_FILE_ATTRIBUTES;
}
static int release_holder(void)
{
    DWORD code = 99;
    HANDLE g = CreateFileA(SYNC_GO, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (g != INVALID_HANDLE_VALUE) CloseHandle(g);
    if (WaitForSingleObject(g_hold.hProcess, 30000) != WAIT_OBJECT_0) return 0;
    GetExitCodeProcess(g_hold.hProcess, &code);
    CloseHandle(g_hold.hThread);
    CloseHandle(g_hold.hProcess);
    return code == 0;
}
static void hook_spawn_holder(void) { (void)spawn_holder(WS "\\a.c"); }

int main(int argc, char **argv)
{
    PATCH_PLAN p;
    DWORD got;
    if (argc == 3 && !strcmp(argv[1], "hold")) return holder_main(argv[2]);
    got = GetModuleFileNameA(NULL, g_exe, sizeof(g_exe));
    if (!got || got >= sizeof(g_exe)) return 2;

    puts("N1 CRLF file: hunk written with LF, CRLF convention kept, rollback byte-exact");
    reset();
    put(WS "\\a.c", "// head\r\nint x = 1;\r\n// tail\r\n");
    plan1(&p, WS "\\a.c", OLDC, NEWC);
    ck(PatchApplyAtomic(&p) == 1 && is(WS "\\a.c", "// head\r\nint x = 9;\r\n// tail\r\n"),
       "apply keeps every CRLF");
    ck(PatchRollback(&p) == 1 && is(WS "\\a.c", "// head\r\nint x = 1;\r\n// tail\r\n"),
       "rollback restores the CRLF bytes exactly");
    PatchPlanFree(&p);

    puts("N2 UTF-8 BOM file: BOM kept through apply and rollback");
    reset();
    put(WS "\\a.c", "\xEF\xBB\xBF" OLDC);
    plan1(&p, WS "\\a.c", OLDC, NEWC);
    ck(PatchApplyAtomic(&p) == 1 && is(WS "\\a.c", "\xEF\xBB\xBF" NEWC), "apply keeps the BOM");
    ck(PatchRollback(&p) == 1 && is(WS "\\a.c", "\xEF\xBB\xBF" OLDC), "rollback restores BOM and bytes");
    PatchPlanFree(&p);

    puts("N3 multi-hunk is all or nothing");
    reset();
    put(WS "\\a.c", "int a = 1;\nint b = 2;\nint c = 3;\n");
    PatchPlanInit(&p, WS "\\a.c");
    PatchPlanSetWorkspace(&p, WS);
    PatchPlanAddHunk(&p, 1, "", "int a = 1;\n", "int a = 10;\n", "");
    PatchPlanAddHunk(&p, 2, "", "int b = 2;\n", "int b = 20;\n", "");
    ck(PatchApplyAtomic(&p) == 1 && is(WS "\\a.c", "int a = 10;\nint b = 20;\nint c = 3;\n"),
       "two hunks applied in one replace");
    ck(PatchRollback(&p) == 1 && is(WS "\\a.c", "int a = 1;\nint b = 2;\nint c = 3;\n"),
       "rollback restores all hunks");
    PatchPlanFree(&p);
    reset();
    put(WS "\\a.c", "int a = 1;\nint b = 2;\nint c = 3;\n");
    PatchPlanInit(&p, WS "\\a.c");
    PatchPlanSetWorkspace(&p, WS);
    PatchPlanAddHunk(&p, 1, "", "int a = 1;\n", "int a = 10;\n", "");
    PatchPlanAddHunk(&p, 2, "", "int zz = 0;\n", "int zz = 1;\n", "");
    ck(PatchApplyAtomic(&p) == 0 && !p.is_applied, "second hunk does not match: apply refused");
    ck(is(WS "\\a.c", "int a = 1;\nint b = 2;\nint c = 3;\n"), "first hunk was not written either");
    ck(only_expected_files("a.c"), "no stage/intent leftovers");
    PatchPlanFree(&p);

    puts("N4 read-only target: coherent result, never mixed, not wedged");
    reset();
    put(WS "\\a.c", OLDC);
    ck(SetFileAttributesA(WS "\\a.c", FILE_ATTRIBUTE_READONLY) != 0, "read-only fixture");
    plan1(&p, WS "\\a.c", OLDC, NEWC);
    {
        int rc = PatchApplyAtomic(&p);
        ck(rc == (is(WS "\\a.c", NEWC) ? 1 : 0), "return code matches the bytes on disk");
        ck(is(WS "\\a.c", NEWC) || is(WS "\\a.c", OLDC), "old or new, never mixed");
        ck((rc == 1) == (p.is_applied != 0), "is_applied matches result");
        if (rc == 0)
        {
            ck(p.io_diag[0] != 0, "refusal carries a diagnostic");
            SetFileAttributesA(WS "\\a.c", FILE_ATTRIBUTE_NORMAL);
            ck(PatchApplyAtomic(&p) == 1 && is(WS "\\a.c", NEWC), "after clearing read-only the same plan applies (not wedged)");
        }
        else
            printf("  (read-only target was replaced; retry case not needed)\n");
    }
    PatchPlanFree(&p);
    SetFileAttributesA(WS "\\a.c", FILE_ATTRIBUTE_NORMAL);

    puts("N5 handle held by another process after our read: refused, not wedged");
    reset();
    put(WS "\\a.c", OLDC);
    plan1(&p, WS "\\a.c", OLDC, NEWC);
    g_patch_test_before_replace = hook_spawn_holder;
    {
        int rc = PatchApplyAtomic(&p);
        g_patch_test_before_replace = NULL;
        ck(GetFileAttributesA(SYNC_READY) != INVALID_FILE_ATTRIBUTES, "holder process held the target");
        printf("  (diag: %s)\n", p.io_diag);
        ck(rc == 0 && !p.is_applied, "apply not reported as success");
        ck(is(WS "\\a.c", OLDC), "old bytes on disk");
        ck(p.io_diag[0] != 0, "diagnostic set");
    }
    ck(release_holder(), "holder released and exited cleanly");
    ck(PatchApplyAtomic(&p) == 1 && is(WS "\\a.c", NEWC), "same plan applies once the holder is gone");
    PatchPlanFree(&p);

    puts("N6 handle held by another process BEFORE apply: refused, not wedged");
    reset();
    put(WS "\\a.c", OLDC);
    ck(spawn_holder(WS "\\a.c"), "holder process started");
    plan1(&p, WS "\\a.c", OLDC, NEWC);
    {
        int rc = PatchApplyAtomic(&p);
        printf("  (diag: %s)\n", p.io_diag);
        ck(rc == 0 && !p.is_applied, "apply not reported as success");
        ck(is(WS "\\a.c", OLDC), "old bytes on disk");
    }
    ck(release_holder(), "holder released and exited cleanly");
    ck(PatchApplyAtomic(&p) == 1 && is(WS "\\a.c", NEWC), "same plan applies once the holder is gone");
    PatchPlanFree(&p);

    wipe();
    printf("%s (%d failures)\n", fails ? "FAILED" : "ALL PASS", fails);
    return fails ? 1 : 0;
}
#else
int main(void) { return 0; }
#endif
