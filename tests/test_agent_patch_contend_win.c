#ifdef _WIN32
/* M1 criterion 3, Windows: contention on the workspace lock with SEPARATE PROCESSES. The lock cells
   (test_agent_patch_lock_win) and the T1 to T8 cells run in one process. This test starts the
   test executable again as a child that calls PatchApplyAtomic, so the second writer is a real process.
   W1 a foreign handle (this process) holds an exclusive LockFileEx on byte 0 of .fstxn.lock for 1500 ms
      while a child applies a one-hunk patch. W2 two children apply DIFFERENT one-hunk patches (x = 9 and
      x = 7) to the same file at the same time. A hook compiled into the test target makes each child read
      the file, then sleep 1500 ms before the replace, so both have read the OLD bytes before either
      replaces. The lock then decides the order and the drift guard of the conditional replace must
      refuse the second one.
   Predictions (written before the first run, none measured yet):
   W1: the child is still running after 1500 ms and a.c is still OLD (the writer waits, it does not
       fail); after the release the child exits with code 10 (applied) and a.c holds x = 9; nothing is
       left in the workspace besides a.c and .fstxn.lock.
   W2: exactly one child exits 10 (applied), the other 11 (refused, rc 0); a.c holds the text of the
       child that applied; nothing else is left. A reading of the source: PatchApplyAtomic verifies and reads
       before it takes the lock, FsReplaceFile takes the lock, and replace_target passes the bytes it read
       as the expected bytes, so the loser sees drift.
   Mutant 12 (AGENT_PATCH_APPLY_MUTANT, target test_agent_patch_contend_win_mc) drops that drift guard.
   Predicted kill set: exactly one cell, W2 "exactly one applier won". Then both children exit 10 and the
   later one overwrites the earlier. W1 and the other W2 cells pass in the mutant build.
   Limits: one runner, one account, two writers; the waiting child is not killed, power loss is not
   tried, a waiter that is killed while it holds the lock is not tried; the 1500 ms timings assume the
   children start and read within that window (if one did not, W2 can pass in the mutant build, and the
   mutant target then reports SURVIVED, a red, not a silent pass). */
#include "agent_patch.h"
#include <windows.h>
#include <direct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void (*g_patch_test_before_replace)(void);

#define WS "test_agent_patch_contend_win_scratch"
#define LK WS "\\.fstxn.lock"
#define OLDC "int x = 1;\n"
#define NEW9 "int x = 9;\n"
#define NEW7 "int x = 7;\n"
#define EXIT_APPLIED 10
#define EXIT_REFUSED 11
#define EXIT_OTHER 12

static int fails;
#ifdef AGENT_PATCH_APPLY_MUTANT
static int fail_mask, fail_other;
#endif
static void ck(int x, const char *m)
{
    printf("  [%s] %s\n", x ? "PASS" : "FAIL", m);
    if (!x)
    {
        fails++;
#ifdef AGENT_PATCH_APPLY_MUTANT
        if (!strcmp(m, "exactly one applier won, the other was refused")) fail_mask |= 1;
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
    (void)system("if exist " WS " rmdir /s /q " WS " >NUL 2>NUL");
}
static void reset(void)
{
    wipe();
    if (_mkdir(WS) != 0) { fprintf(stderr, "mkdir\n"); exit(2); }
    put(WS "\\a.c", OLDC);
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

/* ---- child ---- */
static void hook_sleep(void) { Sleep(1500); }
static int child_main(const char *which, int delay)
{
    PATCH_PLAN p;
    int rc;
    const char *repl = !strcmp(which, "9") ? NEW9 : NEW7;
    PatchPlanInit(&p, WS "\\a.c");
    PatchPlanSetWorkspace(&p, WS);
    PatchPlanAddHunk(&p, 1, "", OLDC, repl, "");
    if (delay) g_patch_test_before_replace = hook_sleep;
    rc = PatchApplyAtomic(&p);
    g_patch_test_before_replace = NULL;
    PatchPlanFree(&p);
    return rc == 1 ? EXIT_APPLIED : rc == 0 ? EXIT_REFUSED : EXIT_OTHER;
}

/* ---- parent ---- */
static int spawn(const char *which, int delay, PROCESS_INFORMATION *pi)
{
    char exe[MAX_PATH], cmd[MAX_PATH + 64];
    STARTUPINFOA si;
    DWORD n = GetModuleFileNameA(NULL, exe, sizeof(exe));
    if (n == 0 || n >= sizeof(exe)) return 0;
    snprintf(cmd, sizeof(cmd), "\"%s\" child %s%s", exe, which, delay ? " delay" : "");
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    memset(pi, 0, sizeof(*pi));
    return CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, pi) != 0;
}
static int still_running(HANDLE h)
{
    DWORD code = 0;
    return GetExitCodeProcess(h, &code) && code == STILL_ACTIVE;
}
/* Waits up to 30 s; a child that does not return is a hang: kill it and stop the whole test. */
static DWORD finish(PROCESS_INFORMATION *pi, const char *name)
{
    DWORD code = 0;
    if (WaitForSingleObject(pi->hProcess, 30000) != WAIT_OBJECT_0)
    {
        fprintf(stderr, "FAIL %s did not return within 30 s\n", name);
        TerminateProcess(pi->hProcess, 99);
        exit(1);
    }
    GetExitCodeProcess(pi->hProcess, &code);
    CloseHandle(pi->hProcess);
    CloseHandle(pi->hThread);
    return code;
}

int main(int argc, char **argv)
{
    int ran = 0;
    if (argc >= 3 && !strcmp(argv[1], "child"))
        return child_main(argv[2], argc >= 4 && !strcmp(argv[3], "delay"));

    puts("W1 a child waits behind a foreign exclusive lock, then applies");
    {
        PROCESS_INFORMATION pi;
        OVERLAPPED o;
        HANDLE lk;
        DWORD code;
        reset();
        lk = CreateFileA(LK, GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, NULL);
        ck(lk != INVALID_HANDLE_VALUE, "holder opened the lock file");
        memset(&o, 0, sizeof(o));
        ck(lk != INVALID_HANDLE_VALUE && LockFileEx(lk, LOCKFILE_EXCLUSIVE_LOCK, 0, 1, 0, &o) != 0,
           "holder took the exclusive lock");
        ck(spawn("9", 0, &pi), "child started");
        Sleep(1500);
        ck(still_running(pi.hProcess), "the child is still waiting after 1500 ms");
        ck(is(WS "\\a.c", OLDC), "a.c is still OLD while the lock is held");
        UnlockFileEx(lk, 0, 1, 0, &o);
        CloseHandle(lk);
        code = finish(&pi, "W1 child");
        printf("  (measured: W1 child exit %lu, a.c %s)\n", (unsigned long)code,
               is(WS "\\a.c", NEW9) ? "x = 9" : is(WS "\\a.c", OLDC) ? "OLD" : "OTHER");
        ck(code == EXIT_APPLIED, "after the release the child applied");
        ck(is(WS "\\a.c", NEW9), "a.c holds the child's text");
        ck(leftovers() == 0, "no leftover names");
        ran++;
    }

    puts("W2 two children apply different patches at the same time");
    {
        PROCESS_INFORMATION p9, p7;
        DWORD c9, c7;
        int applied, refused, winner9;
        reset();
        ck(spawn("9", 1, &p9), "child 9 started");
        ck(spawn("7", 1, &p7), "child 7 started");
        c9 = finish(&p9, "W2 child 9");
        c7 = finish(&p7, "W2 child 7");
        applied = (c9 == EXIT_APPLIED) + (c7 == EXIT_APPLIED);
        refused = (c9 == EXIT_REFUSED) + (c7 == EXIT_REFUSED);
        winner9 = (c9 == EXIT_APPLIED);
        printf("  (measured: child 9 exit %lu, child 7 exit %lu, a.c %s)\n", (unsigned long)c9, (unsigned long)c7,
               is(WS "\\a.c", NEW9) ? "x = 9" : is(WS "\\a.c", NEW7) ? "x = 7" : is(WS "\\a.c", OLDC) ? "OLD" : "OTHER");
        ck(applied == 1 && refused == 1, "exactly one applier won, the other was refused");
        ck(is(WS "\\a.c", NEW9) || is(WS "\\a.c", NEW7), "a.c holds exactly one applier's text");
        ck(applied != 1 || is(WS "\\a.c", winner9 ? NEW9 : NEW7), "a.c holds the text of the child that applied");
        ck(leftovers() == 0, "no leftover names");
        ran++;
    }

    wipe();
#ifdef AGENT_PATCH_APPLY_MUTANT
    /* Exit 0 only when the failing cells are exactly the predicted one. */
    if (fail_mask == 1 && fail_other == 0 && fails == 1)
    {
        printf("mutant killed by %d cell(s)\n", fails);
        return 0;
    }
    printf("mutant SURVIVED or failed elsewhere: mask %x, other %d, fails %d\n", fail_mask, fail_other, fails);
    return 1;
#endif
    if (!fails) printf("agentpatch contention cells ran: %d\n", ran);
    printf("%s (%d failures)\n", fails ? "FAILED" : "ALL PASS", fails);
    return fails ? 1 : 0;
}
#else
int main(void) { return 0; }
#endif
