#include "git_gate.h"
#include "agent_shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#define RMTREE(p) "if exist " p " rmdir /s /q " p
#define NOERR "2>nul"
#define DELFILE "del third.txt"
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir(p, 0755)
#define RMTREE(p) "rm -rf " p
#define NOERR "2>/dev/null"
#define DELFILE "rm third.txt"
#endif
/* M2: the workflow sequence end to end against a real bare remote, with the remote advancing
   after the first preflight and before the push (what apply-patch.yml does not guard in its
   own steps). Local clone L, other clone O, bare remote B. POSIX only, nothing mocked.
   Predictions, written before the first run:
     1 preflight --remote-sync ready: exit 0
     2 after the remote advances, preflight --remote-sync: exit 19 (remote_advanced)
     3 verify-staged and verify-head still pass (they do not look at the remote)
     4 git push origin HEAD:main is refused; the remote tip stays the other clone's commit
     5 local HEAD and the working tree are unchanged by the refused push
     6 preflight --remote-sync after the local commit: exit 19 (diverged)
   Control, same steps with the remote NOT advanced (predicted before the first run):
     7 preflight ready, verify-staged and verify-head pass, the push SUCCEEDS and the remote tip
       equals the local commit. This shows the refusal in 4 comes from the advance, not from a
       broken fixture or a push that can never work. */
#define S "test_git_gate_e2e_scratch"
#define B S "/b.git"
#define L S "/l"
#define O S "/o"
static int run_n, pass_n;
#define CHECK(x,m) do{run_n++;if(x){pass_n++;printf("  [PASS] %s\n",m);}else printf("  [FAIL] %s (line %d)\n",m,__LINE__);}while(0)
/* Cells tagged R are the retry cells. In the mutant build (AGENT_GIT_PATCH_STATE_MUTANT,
   patch-state never recognises an applied patch) the run is a kill when every non-R
   cell passes and at least one R cell fails: exit 0. A mutant that passes all R cells
   survives: exit 1. */
static int retry_fail_n;
#define CHECKR(x,m) do{run_n++;if(x){pass_n++;printf("  [PASS] %s\n",m);}else{retry_fail_n++;printf("  [FAIL] %s (line %d)\n",m,__LINE__);}}while(0)
static int Run(const char *cwd, const char *cmd, int want)
{
    SHELL_EXEC_RESULT r;
    if (!AgentShellExec(cmd, cwd, 20000, &r)) return 0;
    return !r.execution_failed && !r.timed_out && r.exit_code == want;
}
static int Out(const char *cwd, const char *cmd, char *buf, size_t n)
{
    SHELL_EXEC_RESULT r;
    size_t k;
    if (!AgentShellExec(cmd, cwd, 20000, &r) || r.execution_failed || r.exit_code != 0) return 0;
    snprintf(buf, n, "%s", r.stdout_buf);
    k = strlen(buf);
    while (k && (buf[k - 1] == '\n' || buf[k - 1] == '\r')) buf[--k] = 0;
    return 1;
}
static int Put(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");
    int ok = f && fputs(text, f) >= 0;
    if (f && fclose(f) != 0) ok = 0;
    return ok;
}
static char gout[2048], gerr[2048];
static int Gate(const char *a1, const char *a2, const char *a3, const char *a4, const char *a5, const char *a6, const char *a7, const char *a8)
{
    char *argv[11];
    int argc = 0, rc;
    FILE *o, *e;
    size_t n;
    argv[argc++] = "gate";
    if (a1) argv[argc++] = (char *)a1;
    if (a2) argv[argc++] = (char *)a2;
    if (a3) argv[argc++] = (char *)a3;
    if (a4) argv[argc++] = (char *)a4;
    if (a5) argv[argc++] = (char *)a5;
    if (a6) argv[argc++] = (char *)a6;
    if (a7) argv[argc++] = (char *)a7;
    if (a8) argv[argc++] = (char *)a8;
#ifdef _WIN32
    /* tmpfile() writes to the drive root on Windows and can be refused; use files in the working directory. */
    o = fopen("test_git_gate_e2e_out.tmp", "w+b");
    e = fopen("test_git_gate_e2e_err.tmp", "w+b");
    if (o == NULL || e == NULL) return 99;
#else
    o = tmpfile();
    e = tmpfile();
#endif
    rc = GitGateRun(argc, argv, o, e);
    rewind(o);
    rewind(e);
    n = fread(gout, 1, sizeof gout - 1, o);
    gout[n] = 0;
    n = fread(gerr, 1, sizeof gerr - 1, e);
    gerr[n] = 0;
    fclose(o);
    fclose(e);
#ifdef _WIN32
    (void)remove("test_git_gate_e2e_out.tmp");
    (void)remove("test_git_gate_e2e_err.tmp");
#endif
    return rc;
}
int main(void)
{
    char head[80] = "", tip0[80] = "", tip_other[80] = "", tip_after[80] = "", local_commit[80] = "",
         local_after[80] = "", porcelain[256] = "x";
    int rc;
    printf("=== gate sequence with the remote advancing before the push ===\n");
    (void)system(RMTREE(S));
    CHECK(MKDIR(S) == 0 && Run(S, "git init -q --bare -b main b.git", 0) && Run(S, "git clone -q b.git l " NOERR, 0) &&
          Run(S, "git clone -q b.git o " NOERR, 0), "bare remote and two clones");
    CHECK(Run(L, "git config user.name L && git config user.email l@example.invalid && git config core.autocrlf false && git checkout -q -b main", 0) &&
          Run(O, "git config user.name O && git config user.email o@example.invalid && git config core.autocrlf false && git checkout -q -b main", 0), "identities, branch main");
    CHECK(Put(L "/a.txt", "one\ntwo\nthree\n") && Run(L, "git add -A && git commit -q -m base && git push -q -u origin main " NOERR, 0), "base pushed with upstream");
    CHECK(Out(L, "git rev-parse HEAD", head, sizeof head) && Out(S "/b.git", "git rev-parse main", tip0, sizeof tip0) && !strcmp(head, tip0),
          "local head equals the remote tip");

    CHECK(Put(L "/a.txt", "one\nTWO\nthree\n") && Run(L, "git diff --binary > .git/change.patch", 0) && Run(L, "git checkout -q -- a.txt", 0), "patch made, tree back to base");

    rc = Gate("preflight", "--expected-head", head, "--branch", "main", "--remote-sync", "--dir", L);
    CHECK(rc == 0 && strstr(gout, "ready"), "1: preflight --remote-sync is ready before the advance");

    CHECK(Run(O, "git pull -q origin main " NOERR, 0) && Put(O "/other.txt", "o\n") &&
          Run(O, "git add -A && git commit -q -m other && git push -q origin main " NOERR, 0) &&
          Out(O, "git rev-parse HEAD", tip_other, sizeof tip_other) && strcmp(tip_other, head) != 0, "the other clone advances the remote");

    rc = Gate("preflight", "--expected-head", head, "--branch", "main", "--remote-sync", "--dir", L);
    if (rc != 19) printf("    got exit %d out=%s err=%s\n", rc, gout, gerr);
    CHECK(rc == 10 + 9 && strstr(gout, "remote_advanced"), "2: the same preflight now reports remote_advanced, exit 19");

    /* The workflow steps that do not consult the remote. */
    CHECK(Run(L, "git apply .git/change.patch && git add -A", 0), "git apply and stage");
    rc = Gate("verify-staged", ".git/change.patch", "--dir", L, NULL, NULL, NULL, NULL);
    CHECK(rc == 0, "3: verify-staged passes (it does not look at the remote)");
    CHECK(Run(L, "git commit -q -m change", 0), "commit");
    rc = Gate("verify-head", ".git/change.patch", "--dir", L, NULL, NULL, NULL, NULL);
    CHECK(rc == 0, "3: verify-head passes");
    CHECK(Out(L, "git rev-parse HEAD", local_commit, sizeof local_commit) && strcmp(local_commit, head) != 0, "local commit recorded");

    CHECK(Run(L, "git push origin HEAD:main " NOERR, 1), "4: git push origin HEAD:main is refused (non-fast-forward)");
    CHECK(Out(S "/b.git", "git rev-parse main", tip_after, sizeof tip_after) && !strcmp(tip_after, tip_other),
          "4: the remote tip is still the other clone's commit");
    CHECK(Out(L, "git rev-parse HEAD", local_after, sizeof local_after) && !strcmp(local_after, local_commit),
          "5: local HEAD unchanged by the refused push");
    CHECK(Out(L, "git status --porcelain", porcelain, sizeof porcelain) && porcelain[0] == 0, "5: working tree clean after the refused push");

    rc = Gate("preflight", "--expected-head", local_commit, "--branch", "main", "--remote-sync", "--dir", L);
    CHECK(rc == 10 + 9 && strstr(gout, "remote_advanced"), "6: preflight --remote-sync on the diverged state reports remote_advanced");

    /* Control: sync L to the remote tip (explicit fixture reset), then run the same steps with no advance. */
    CHECK(Run(L, "git fetch -q origin 2>/dev/null && git reset -q --hard origin/main", 0) &&
          Out(L, "git rev-parse HEAD", head, sizeof head) && !strcmp(head, tip_other), "control: local reset to the remote tip");
    CHECK(Put(L "/other.txt", "o2\n") && Run(L, "git diff --binary > .git/change2.patch", 0) && Run(L, "git checkout -q -- other.txt", 0),
          "control: second patch made, tree clean");
    rc = Gate("preflight", "--expected-head", head, "--branch", "main", "--remote-sync", "--dir", L);
    CHECK(rc == 0 && strstr(gout, "ready"), "7: control preflight --remote-sync ready");
    CHECK(Run(L, "git apply .git/change2.patch && git add -A", 0) &&
          Gate("verify-staged", ".git/change2.patch", "--dir", L, NULL, NULL, NULL, NULL) == 0 &&
          Run(L, "git commit -q -m change2", 0) &&
          Gate("verify-head", ".git/change2.patch", "--dir", L, NULL, NULL, NULL, NULL) == 0, "7: control apply, stage, verify, commit, verify");
    CHECK(Run(L, "git push origin HEAD:main " NOERR, 0), "7: control push succeeds when the remote did not advance");
    CHECK(Out(L, "git rev-parse HEAD", local_commit, sizeof local_commit) && Out(S "/b.git", "git rev-parse main", tip_after, sizeof tip_after) &&
          !strcmp(local_commit, tip_after) && strcmp(tip_after, tip_other) != 0, "7: control remote tip equals the local commit");

    /* Interrupted run: the sequence is applied, staged, verified and committed, and the process
       dies before the push. Retrying must report the completed work and finish once.
       Predictions, written before the first run:
         R1 patch-state on the committed tree: already applied, exit 3
         R2 verify-head still passes on the committed tree (exit 0)
         R3 a naive second `git apply` of the patch is refused (git exit 1), HEAD and tree unchanged
         R4 the push then succeeds once: the remote tip equals the local commit and the remote
            has exactly one more commit than before
         R5 a second push is a no-op (exit 0): tip unchanged, commit count unchanged
         R6 patch-state after the push still reports already applied, exit 3 */
    {
        char before[80] = "", count0[32] = "", count1[32] = "", count2[32] = "", c3[80] = "", tip3[80] = "", head3[80] = "";
        CHECK(Out(L, "git rev-parse HEAD", before, sizeof before) && Out(S "/b.git", "git rev-list --count main", count0, sizeof count0) &&
              Put(L "/third.txt", "t\n") && Run(L, "git add -A -N && git diff --binary > .git/change3.patch && git reset -q && " DELFILE, 0) &&
              Run(L, "git apply .git/change3.patch && git add -A", 0) &&
              Gate("verify-staged", ".git/change3.patch", "--dir", L, NULL, NULL, NULL, NULL) == 0 &&
              Run(L, "git commit -q -m change3", 0) && Out(L, "git rev-parse HEAD", c3, sizeof c3) && strcmp(c3, before) != 0,
              "interrupted run: apply, stage, verify, commit done, no push");
        rc = Gate("patch-state", ".git/change3.patch", "--dir", L, NULL, NULL, NULL, NULL);
        if (rc != 3) printf("    got exit %d out=%s err=%s\n", rc, gout, gerr);
        CHECKR(rc == 3 && strstr(gout, "already applied"), "R1: patch-state on the committed tree reports already applied, exit 3");
        CHECK(Gate("verify-head", ".git/change3.patch", "--dir", L, NULL, NULL, NULL, NULL) == 0, "R2: verify-head passes on the committed tree");
        CHECK(Run(L, "git apply .git/change3.patch " NOERR, 1) && Out(L, "git rev-parse HEAD", head3, sizeof head3) && !strcmp(head3, c3) &&
              Out(L, "git status --porcelain", porcelain, sizeof porcelain) && porcelain[0] == 0, "R3: a naive second git apply is refused and changes nothing");
        CHECK(Run(L, "git push origin HEAD:main " NOERR, 0) && Out(S "/b.git", "git rev-parse main", tip3, sizeof tip3) && !strcmp(tip3, c3) &&
              Out(S "/b.git", "git rev-list --count main", count1, sizeof count1) && atoi(count1) == atoi(count0) + 1, "R4: the push finishes once, remote has exactly one more commit");
        CHECK(Run(L, "git push origin HEAD:main " NOERR, 0) && Out(S "/b.git", "git rev-parse main", tip3, sizeof tip3) && !strcmp(tip3, c3) &&
              Out(S "/b.git", "git rev-list --count main", count2, sizeof count2) && !strcmp(count1, count2), "R5: a second push is a no-op, tip and commit count unchanged");
        rc = Gate("patch-state", ".git/change3.patch", "--dir", L, NULL, NULL, NULL, NULL);
        CHECKR(rc == 3 && strstr(gout, "already applied"), "R6: patch-state after the push still reports already applied, exit 3");
    }

    printf("\n%d/%d passed\n", pass_n, run_n);
    (void)system(RMTREE(S));
#ifdef AGENT_GIT_PATCH_STATE_MUTANT
    if (retry_fail_n > 0 && pass_n + retry_fail_n == run_n) { printf("mutant killed by %d retry cell(s)\n", retry_fail_n); return 0; }
    printf("MUTANT SURVIVED or a non-retry cell failed\n");
    return 1;
#else
    return pass_n == run_n ? 0 : 1;
#endif
}
