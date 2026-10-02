#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
/* M1-3: AgentPatch on the Fs* API. POSIX only; crash and hook points are
   compiled in for this target alone (FS_CREATE_TEST_CRASH, AGENT_PATCH_TEST_HOOK). */
#include "agent_patch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>

extern void (*g_patch_test_before_replace)(void);

#define WS "test_agent_patch_fs_scratch"
#define OUT "test_agent_patch_fs_outside.c"
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
static void reset(void)
{
    if (system("rm -rf " WS " " OUT) != 0) exit(2);
    mkdir(WS, 0700);
    put(WS "/a.c", OLDC);
}
static void plan_for(PATCH_PLAN *p, const char *target)
{
    PatchPlanInit(p, target);
    PatchPlanSetWorkspace(p, WS);
    PatchPlanAddHunk(p, 1, "", OLDC, NEWC, "");
}
static void hook_external(void) { put(WS "/a.c", OLDC "// external\n"); }

int main(void)
{
    PATCH_PLAN p;
    PATCH_VERIFY_REPORT rep;
    char cwd[512], abs[1024];
    struct stat st;
    if (!getcwd(cwd, sizeof(cwd))) return 2;

    puts("T1 drift between read and replace is DENIED, not overwritten");
    reset();
    plan_for(&p, WS "/a.c");
    g_patch_test_before_replace = hook_external;
    ck(PatchApplyAtomic(&p) == 0, "apply refuses");
    g_patch_test_before_replace = NULL;
    ck(is(WS "/a.c", OLDC "// external\n"), "external bytes kept");
    ck(p.io_diag[0] != 0 && !p.is_applied, "diagnostic set, not applied");
    PatchPlanFree(&p);

    puts("T2 success leaves only .fstxn.lock as control artifact");
    reset();
    plan_for(&p, WS "/a.c");
    ck(PatchApplyAtomic(&p) == 1 && is(WS "/a.c", NEWC), "apply ok");
    {
        DIR *d = opendir(WS);
        struct dirent *e;
        int extra = 0;
        while ((e = readdir(d)))
            if (strcmp(e->d_name, ".") && strcmp(e->d_name, "..") &&
                strcmp(e->d_name, "a.c") && strcmp(e->d_name, ".fstxn.lock"))
                extra++;
        closedir(d);
        ck(extra == 0, "no stage/intent leftovers");
    }
    PatchPlanFree(&p);

    puts("T3 rollback restores bytes and mode");
    reset();
    chmod(WS "/a.c", 0640);
    plan_for(&p, WS "/a.c");
    ck(PatchApplyAtomic(&p) == 1 && stat(WS "/a.c", &st) == 0 && (st.st_mode & 0777) == 0640, "mode kept on apply");
    ck(PatchRollback(&p) == 1 && is(WS "/a.c", OLDC) &&
       stat(WS "/a.c", &st) == 0 && (st.st_mode & 0777) == 0640, "rollback restores bytes and mode");
    PatchPlanFree(&p);

    puts("T4 paths outside the workspace are rejected");
    reset();
    put(OUT, OLDC);
    plan_for(&p, WS "/../" OUT);
    ck(PatchVerifyPlan(&p, &rep) == 0 && rep.status == PATCH_CHECK_IO_ERROR, "dotdot verify rejected");
    ck(PatchApplyAtomic(&p) == 0 && is(OUT, OLDC), "dotdot apply rejected, file untouched");
    PatchPlanFree(&p);
    snprintf(abs, sizeof(abs), "%s/%s", cwd, OUT);
    plan_for(&p, abs);
    ck(PatchApplyAtomic(&p) == 0 && is(OUT, OLDC), "absolute path outside rejected");
    PatchPlanFree(&p);
    plan_for(&p, OUT);
    ck(PatchApplyAtomic(&p) == 0 && is(OUT, OLDC), "relative path outside rejected");
    PatchPlanFree(&p);
    snprintf(abs, sizeof(abs), "%s/%s/a.c", cwd, WS);
    plan_for(&p, abs);
    ck(PatchApplyAtomic(&p) == 1 && is(WS "/a.c", NEWC), "absolute path inside workspace works");
    PatchPlanFree(&p);

    puts("T5 files over 1 MiB are refused explicitly");
    reset();
    {
        size_t n = 2u * 1024 * 1024;
        char *big = malloc(n + 1);
        FILE *f;
        memset(big, 'a', n);
        big[n] = 0;
        f = fopen(WS "/big.c", "wb");
        fwrite(big, 1, n, f);
        fclose(f);
        free(big);
    }
    plan_for(&p, WS "/big.c");
    ck(PatchVerifyPlan(&p, &rep) == 0 && rep.status == PATCH_CHECK_IO_ERROR, "verify reports IO error");
    ck(PatchApplyAtomic(&p) == 0 && stat(WS "/big.c", &st) == 0 && st.st_size == 2 * 1024 * 1024, "apply refused, size unchanged");
    PatchPlanFree(&p);

    puts("T6 crash then recovery, and PENDING is never reported as success");
    reset();
    {
        int status;
        pid_t pid = fork();
        if (!pid)
        {
            setenv("FS_CREATE_TEST_CRASH", "43", 1);
            plan_for(&p, WS "/a.c");
            (void)PatchApplyAtomic(&p);
            _exit(10);
        }
        waitpid(pid, &status, 0);
        ck(WIFEXITED(status) && WEXITSTATUS(status) == 133, "crash point 43 reached");
    }
    plan_for(&p, WS "/a.c");
    ck(is(WS "/a.c", NEWC) || is(WS "/a.c", OLDC), "after crash: old or new, never mixed");
    ck(PatchApplyAtomic(&p) == 0, "pending transaction blocks further apply");
    ck(PatchRecover(&p) == 1, "recovery completes");
    ck(is(WS "/a.c", NEWC) || is(WS "/a.c", OLDC), "after recovery: old or new, never mixed");
    PatchPlanFree(&p);
    reset();
    setenv("FS_REPLACE_TEST_FAIL_SYNC", "1", 1);
    plan_for(&p, WS "/a.c");
    {
        int rc = PatchApplyAtomic(&p);
        ck(rc == (is(WS "/a.c", NEWC) ? 1 : 0), "PENDING result matches on-disk bytes");
        ck((rc == 1) == p.is_applied, "is_applied matches result");
        printf("  (measured: PENDING apply returned %d)\n", rc);
    }
    unsetenv("FS_REPLACE_TEST_FAIL_SYNC");
    PatchPlanFree(&p);

    puts("T7 symlink and hard-linked targets are refused");
    reset();
    symlink("a.c", WS "/link.c");
    plan_for(&p, WS "/link.c");
    ck(PatchApplyAtomic(&p) == 0 && is(WS "/a.c", OLDC), "symlink target refused, file untouched");
    PatchPlanFree(&p);
    reset();
    link(WS "/a.c", WS "/h2.c");
    plan_for(&p, WS "/a.c");
    ck(PatchApplyAtomic(&p) == 0 && is(WS "/a.c", OLDC) && is(WS "/h2.c", OLDC), "hard-linked target refused, both untouched");
    PatchPlanFree(&p);

    puts("T8 rollback refuses to overwrite drift");
    reset();
    plan_for(&p, WS "/a.c");
    ck(PatchApplyAtomic(&p) == 1, "apply ok");
    put(WS "/a.c", "user edit\n");
    ck(PatchRollback(&p) == 0 && is(WS "/a.c", "user edit\n") && p.is_applied, "rollback refused, user edit kept");
    PatchPlanFree(&p);

    puts("T9 pre-existing .fstxn.lock with unsafe mode fails closed");
    reset();
    put(WS "/.fstxn.lock", "");
    chmod(WS "/.fstxn.lock", 0644);
    plan_for(&p, WS "/a.c");
    ck(PatchApplyAtomic(&p) == 0 && is(WS "/a.c", OLDC), "0644 lock: apply refused, file untouched");
    ck(p.io_diag[0] != 0 && !p.is_applied, "diagnostic set, not applied");
    chmod(WS "/.fstxn.lock", 0600);
    ck(PatchApplyAtomic(&p) == 1 && is(WS "/a.c", NEWC), "same lock at 0600: apply works");
    PatchPlanFree(&p);

    if (system("rm -rf " WS " " OUT) != 0) return 2;
    printf("%s (%d failures)\n", fails ? "FAILED" : "ALL PASS", fails);
    return fails ? 1 : 0;
}
#else
int main(void) { return 0; }
#endif
