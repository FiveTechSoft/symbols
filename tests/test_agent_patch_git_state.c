#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
/* S3(c), M2 scope note (docs/core-m2-exit-reading.md, m227): the in-process patch path
   (PatchApplyAtomic, the call agent_runner.c makes) has no Git preflight. This test MEASURES what it does
   in a real Git repository with a dirty tree and a moved HEAD. It is a characterization, not a claim that
   the behavior is wanted. Predictions, written before the first run:
     G1 unrelated dirty files (modified tracked, untracked): apply SUCCEEDS, the unrelated files keep their bytes
     G2 uncommitted user edit that makes the hunk no longer match: apply is REFUSED, the file keeps the user's bytes
     G3 uncommitted user edit elsewhere in the target, hunk still matches: apply SUCCEEDS, the user's edit survives
     G4 HEAD moved by a commit that touches another file after the plan was made: apply SUCCEEDS (no HEAD check)
     G5 HEAD moved by a commit that changes the hunk site: apply is REFUSED, the file keeps the committed bytes
     G6 after a successful apply: HEAD is unchanged and the target shows as modified (nothing is committed) */
#include "agent_patch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#define WS "test_agent_patch_git_state_scratch"
static int fails;
static void ck(int x, const char *m) { printf("  [%s] %s\n", x ? "PASS" : "FAIL", m); if (!x) fails++; }
static void sh(const char *c) { if (system(c) != 0) { fprintf(stderr, "shell failed: %s\n", c); exit(2); } }
static void put(const char *p, const char *v)
{
    FILE *f = fopen(p, "wb");
    if (!f || fwrite(v, 1, strlen(v), f) != strlen(v)) { fprintf(stderr, "put %s\n", p); exit(2); }
    fclose(f);
}
static int is(const char *p, const char *v)
{
    char b[4096]; size_t n; FILE *f = fopen(p, "rb");
    if (!f) return 0;
    n = fread(b, 1, sizeof b - 1, f); fclose(f); b[n] = 0;
    return !strcmp(b, v);
}
static int out_is(const char *cmd, const char *want)
{
    char b[512]; size_t n; FILE *f = popen(cmd, "r");
    if (!f) return 0;
    n = fread(b, 1, sizeof b - 1, f); pclose(f); b[n] = 0;
    while (n && (b[n - 1] == '\n')) b[--n] = 0;
    return !strcmp(b, want);
}
static char head0[64];
static void repo(void)
{
    FILE *f;
    sh("rm -rf " WS);
    mkdir(WS, 0700);
    put(WS "/a.c", "line1\nint x = 1;\nline3\nline4\n");
    put(WS "/other.txt", "o\n");
    sh("cd " WS " && git init -q -b main . && git config user.name t && git config user.email t@example.invalid && git config core.autocrlf false && git add -A && git commit -q -m base");
    f = popen("cd " WS " && git rev-parse HEAD", "r");
    if (!f || !fgets(head0, sizeof head0, f)) exit(2);
    pclose(f);
    head0[strcspn(head0, "\n")] = 0;
}
static void plan(PATCH_PLAN *p)
{
    PatchPlanInit(p, WS "/a.c");
    PatchPlanSetWorkspace(p, WS);
    PatchPlanAddHunk(p, 2, "", "int x = 1;\n", "int x = 9;\n", "");
}
int main(void)
{
    PATCH_PLAN p;
    char cmd[256];
    puts("G1 unrelated dirty files");
    repo();
    put(WS "/other.txt", "o dirty\n"); put(WS "/new_untracked.txt", "u\n");
    plan(&p);
    ck(PatchApplyAtomic(&p) == 1, "G1: apply succeeds with unrelated dirty files");
    ck(is(WS "/a.c", "line1\nint x = 9;\nline3\nline4\n"), "G1: target has the new bytes");
    ck(is(WS "/other.txt", "o dirty\n") && is(WS "/new_untracked.txt", "u\n"), "G1: unrelated dirty files untouched");
    snprintf(cmd, sizeof cmd, "cd " WS " && git rev-parse HEAD");
    ck(out_is(cmd, head0), "G6: HEAD unchanged after the apply");
    ck(out_is("cd " WS " && git status --porcelain a.c", " M a.c"), "G6: target shows as modified, nothing committed");
    PatchPlanFree(&p);

    puts("G2 user edit breaks the hunk");
    repo();
    put(WS "/a.c", "line1\nint x = 2;\nline3\nline4\n");
    plan(&p);
    ck(PatchApplyAtomic(&p) == 0, "G2: apply refused");
    ck(is(WS "/a.c", "line1\nint x = 2;\nline3\nline4\n"), "G2: the user's bytes are kept");
    PatchPlanFree(&p);

    puts("G3 user edit elsewhere, hunk still matches");
    repo();
    put(WS "/a.c", "line1\nint x = 1;\nline3\nline4 USER\n");
    plan(&p);
    ck(PatchApplyAtomic(&p) == 1, "G3: apply succeeds");
    ck(is(WS "/a.c", "line1\nint x = 9;\nline3\nline4 USER\n"), "G3: the user's edit survives next to the patch");
    PatchPlanFree(&p);

    puts("G4 HEAD moves by an unrelated commit after the plan");
    repo();
    plan(&p);
    put(WS "/other.txt", "o2\n");
    sh("cd " WS " && git add -A && git commit -q -m moved");
    ck(PatchApplyAtomic(&p) == 1, "G4: apply succeeds on a HEAD that moved (no HEAD check)");
    ck(is(WS "/a.c", "line1\nint x = 9;\nline3\nline4\n"), "G4: target has the new bytes");
    PatchPlanFree(&p);

    puts("G5 HEAD moves by a commit that changes the hunk site");
    repo();
    plan(&p);
    put(WS "/a.c", "line1\nint x = 5;\nline3\nline4\n");
    sh("cd " WS " && git add -A && git commit -q -m moved-target");
    ck(PatchApplyAtomic(&p) == 0, "G5: apply refused");
    ck(is(WS "/a.c", "line1\nint x = 5;\nline3\nline4\n"), "G5: the committed bytes are kept");
    PatchPlanFree(&p);

    sh("rm -rf " WS);
    printf("\ngit state cells done, %d failed\n", fails);
    return fails ? 1 : 0;
}
#else
int main(void) { return 0; }
#endif
