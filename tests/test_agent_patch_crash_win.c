#ifdef _WIN32
/* M1 criterion 3, Windows parity of the single-replace crash matrix through
   AgentPatch. POSIX test_agent_patch_fs T6 kills at one point; the Windows
   test_agent_patch_fs_win T6 kills at phase 4 only. This test kills the writer
   at every FsReplaceFile phase 0..6 (FS_WIN_REPLACE_CRASH) through
   PatchApplyAtomic, then PatchRecover, and checks bytes and leftover names.
   Predictions (written before the first run): bytes before recovery are new
   from phase 4 on and old before; after recovery new only at phase 6; phases
   0..2 may leave unjournaled orphan names (the single-file path does not
   sweep), so the leftover cells are expected to be the ones that fail there. */
#include "agent_patch.h"
#include <windows.h>
#include <direct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define WS "test_agent_patch_crash_win_scratch"
#define OLDC "int x = 1;\n"
#define NEWC "int x = 9;\n"
static int fails;
/* Which cells failed, for the mutant mode: bit k of a mask is phase k. */
static int cur_phase = -1, fail_leftover, fail_apply, fail_other;
static void ck(int x, const char *m)
{
    printf("  [%s] %s\n", x ? "PASS" : "FAIL", m);
    if (!x)
    {
        fails++;
        if (cur_phase >= 0 && strstr(m, "leftover")) fail_leftover |= 1 << cur_phase;
        else if (cur_phase >= 0 && strstr(m, "applies afterwards")) fail_apply |= 1 << cur_phase;
        else fail_other++;
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
    (void)system("if exist " WS " rmdir /s /q " WS " >NUL 2>NUL");
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
static int child_apply(int phase)
{
    PATCH_PLAN p;
    char v[16];
    snprintf(v, sizeof(v), "%d", phase);
    _putenv_s("FS_WIN_REPLACE_CRASH", v);
    plan_for(&p);
    (void)PatchApplyAtomic(&p);
    return 200;
}
static int run_child(const char *exe, int phase)
{
    char cmd[1024];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    DWORD code = 0;
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);
    snprintf(cmd, sizeof(cmd), "\"%s\" child %d", exe, phase);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) return -1;
    if (WaitForSingleObject(pi.hProcess, 30000) != WAIT_OBJECT_0) return -2;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (int)code;
}
/* Diagnostic: link count, size and which known content a leftover name holds. */
static void describe(const char *name)
{
    char path[MAX_PATH], buf[256];
    BY_HANDLE_FILE_INFORMATION bi;
    DWORD got = 0;
    const char *cls = "NEITHER";
    HANDLE h;
    snprintf(path, sizeof(path), WS "\\%s", name);
    h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { printf("  (leftover detail: cannot open, error %lu)\n", GetLastError()); return; }
    memset(&bi, 0, sizeof(bi));
    if (!GetFileInformationByHandle(h, &bi)) { printf("  (leftover detail: no info)\n"); CloseHandle(h); return; }
    if (ReadFile(h, buf, sizeof(buf) - 1, &got, NULL)) buf[got] = 0; else got = 0, buf[0] = 0;
    if (got == strlen(NEWC) && !memcmp(buf, NEWC, got)) cls = "NEW";
    else if (got == strlen(OLDC) && !memcmp(buf, OLDC, got)) cls = "OLD";
    else if (got == 0) cls = "EMPTY";
    printf("  (leftover detail: links %lu, size %lu, bytes %s)\n", (unsigned long)bi.nNumberOfLinks,
           (unsigned long)bi.nFileSizeLow, cls);
    CloseHandle(h);
}
/* Names in the workspace other than the target and the persistent lock. */
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
        describe(d.cFileName);
        n++;
    } while (FindNextFileA(h, &d));
    FindClose(h);
    return n;
}
static unsigned long links(void)
{
    BY_HANDLE_FILE_INFORMATION bi;
    HANDLE h = CreateFileA(WS "\\a.c", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    unsigned long n = 0;
    if (h == INVALID_HANDLE_VALUE) return 0;
    if (GetFileInformationByHandle(h, &bi)) n = bi.nNumberOfLinks;
    CloseHandle(h);
    return n;
}
static const char *what(void)
{
    return is(WS "\\a.c", OLDC) ? "OLD" : is(WS "\\a.c", NEWC) ? "NEW" : "OTHER";
}
int main(int argc, char **argv)
{
    char exe[768];
    DWORD got;
    int ran = 0;
    if (argc == 3 && !strcmp(argv[1], "child")) return child_apply(atoi(argv[2]));
    got = GetModuleFileNameA(NULL, exe, sizeof(exe));
    if (!got || got >= sizeof(exe)) return 2;
    for (int phase = 0; phase <= 6; phase++)
    {
        PATCH_PLAN p;
        int rec1, rec2, left;
        printf("phase %d\n", phase);
        cur_phase = phase;
        reset();
        ck(run_child(exe, phase) == 140 + phase, "writer killed at the phase");
        printf("  (measured: before recovery %s)\n", what());
        ck(is(WS "\\a.c", phase >= 4 ? NEWC : OLDC), "bytes before recovery match the phase");
        plan_for(&p);
        rec1 = PatchRecover(&p);
        rec2 = PatchRecover(&p);
        printf("  (measured: PatchRecover %d then %d, file %s)\n", rec1, rec2, what());
        ck(is(WS "\\a.c", phase == 6 ? NEWC : OLDC), "bytes after recovery match the phase");
        printf("  (measured: links of a.c after recovery %lu)\n", links());
        left = leftovers();
        ck(left == 0, "no leftover names after recovery");
        PatchPlanFree(&p);
        if (is(WS "\\a.c", OLDC))
        {
            int arc;
            plan_for(&p);
            arc = PatchApplyAtomic(&p);
            printf("  (measured: re-apply rc %d, links of a.c %lu, io_diag [%s])\n", arc, links(), p.io_diag);
            ck(arc == 1 && is(WS "\\a.c", NEWC), "the same plan applies afterwards");
            PatchPlanFree(&p);
        }
        ran++;
    }
    wipe();
    cur_phase = -1;
    {
        /* Mutant mode (FS_WIN_BATCH_MUTANT set): exit 0 only when the failing cells
           are exactly the expected ones, so a survivor or a failure elsewhere is
           not counted as a kill. Mutant 10 (sweep skipped): leftover and re-apply
           cells at phases 0, 1, 2 (the six cells measured in m137). Mutant 11
           (unshared .fsrs- kept): the leftover cell at phase 1 only. */
        const char *mv = getenv("FS_WIN_BATCH_MUTANT");
        if (mv && *mv)
        {
            int m = atoi(mv), el = 0, ea = 0, n;
            if (m == 10) { el = 0x7; ea = 0x7; }
            else if (m == 11) { el = 0x2; ea = 0; }
            else { printf("mutant %d not handled here\n", m); return 1; }
            n = fails;
            if (fail_leftover == el && fail_apply == ea && fail_other == 0 && n > 0)
            {
                printf("mutant killed by %d cell(s)\n", n);
                return 0;
            }
            printf("mutant %d SURVIVED or failed elsewhere: leftover mask %x, apply mask %x, other %d\n",
                   m, fail_leftover, fail_apply, fail_other);
            return 1;
        }
    }
    if (!fails) printf("agentpatch crash phases ran: %d\n", ran);
    printf("%s (%d failures)\n", fails ? "FAILED" : "ALL PASS", fails);
    return fails ? 1 : 0;
}
#else
int main(void) { return 0; }
#endif
