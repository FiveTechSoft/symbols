#ifdef _WIN32
/* Windows file symlink as the LEAF of a workspace operation (criterion 1, Windows).
   test_fs_create_windows and test_agent_patch_fs_win each have a symlink-leaf case that
   runs only if the runner can create symlinks, and a passing ctest does not say whether
   it ran. This target makes that visible: it exits 77 (ctest reports Skipped, as for
   test_fs_win_aliases_short) when CreateSymbolicLinkA fails, and otherwise runs the cells.
   Predictions, before the first run: windows-latest lets the account create the symlink
   (it runs as an administrator), so the target is Passed, not Skipped; create over the
   symlink leaf, replace, remove and copy-from are all refused; the target file's bytes
   stay "TARGET" and the symlink stays in place after each refusal. If a call is NOT
   refused the failure line prints the status and the observed bytes. Wrong predictions
   will be named in this header after the run. One runner, one account, one volume. */
#include "fs_write.h"
#include "fs_replace.h"
#include "fs_remove.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#define SC "test_fs_win_symleaf_scratch"
static int bad;
static void flag(const char *what, int status)
{
    printf("  [FAIL] %s (status %d)\n", what, status);
    bad++;
}
static void put(const char *path, const char *v)
{
    FILE *f = fopen(path, "wb");
    if (!f || fwrite(v, 1, strlen(v), f) != strlen(v) || fclose(f) != 0) { fprintf(stderr, "put failed\n"); exit(2); }
}
static int bytes_are(const char *path, const char *v)
{
    char b[64] = { 0 };
    FILE *f = fopen(path, "rb");
    size_t n = strlen(v);
    int ok;
    if (!f) return 0;
    ok = fread(b, 1, sizeof(b) - 1, f) == n && memcmp(b, v, n) == 0;
    fclose(f);
    return ok;
}
static int is_link(const char *path)
{
    DWORD a = GetFileAttributesA(path);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}
static void cleanup(void)
{
    DeleteFileA(SC "\\link.txt");
    DeleteFileA(SC "\\target.txt");
    DeleteFileA(SC "\\copied");
    RemoveDirectoryA(SC);
}
int main(void)
{
    FS_READ_ROOT *r;
    int s;
    cleanup();
    if (_mkdir(SC) != 0) { fprintf(stderr, "mkdir failed\n"); return 2; }
    put(SC "\\target.txt", "TARGET");
    if (!CreateSymbolicLinkA(SC "\\link.txt", "target.txt", 0x2 /* ALLOW_UNPRIVILEGED_CREATE */))
    {
        printf("SKIPPED: CreateSymbolicLinkA failed, error %lu\n", (unsigned long)GetLastError());
        cleanup();
        return 77;
    }
    if (FsReadOpen(SC, &r) != FS_READ_OK) { fprintf(stderr, "open root failed\n"); cleanup(); return 2; }

    s = (int)FsCreateFile(r, "link.txt", "bad", 3, 0666);
    if (s == (int)FS_READ_OK) flag("create over a symlink leaf was accepted", s);
    if (!bytes_are(SC "\\target.txt", "TARGET") || !is_link(SC "\\link.txt")) flag("after create: target or link changed", s);

    s = (int)FsReplaceFile(r, "link.txt", "TARGET", 6, "CHANGE", 6);
    if (s == (int)FS_READ_OK) flag("replace through a symlink leaf was accepted", s);
    if (!bytes_are(SC "\\target.txt", "TARGET") || !is_link(SC "\\link.txt")) flag("after replace: target or link changed", s);

    s = (int)FsRemoveFile(r, "link.txt", "TARGET", 6);
    if (s == (int)FS_READ_OK) flag("remove of a symlink leaf was accepted", s);
    if (!bytes_are(SC "\\target.txt", "TARGET") || !is_link(SC "\\link.txt")) flag("after remove: target or link changed", s);

    s = (int)FsCopyFile(r, "link.txt", "copied", "TARGET", 6);
    if (s == (int)FS_READ_OK) flag("copy from a symlink leaf was accepted", s);
    if (GetFileAttributesA(SC "\\copied") != INVALID_FILE_ATTRIBUTES) flag("after copy: a destination file exists", s);

    FsReadClose(r);
    cleanup();
    printf("%s (%d failures)\n", bad ? "FAILED" : "ALL PASS", bad);
    return bad ? 1 : 0;
}
#else
int main(void) { return 0; }
#endif
