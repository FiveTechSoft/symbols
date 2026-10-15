/* Native Windows test only. No stub registration on other platforms. */
#include "agent_shell.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>

enum { BAD_STATUS=1, BAD_OUT_TOTAL=2, BAD_ERR_TOTAL=4, BAD_CAPTURE=8,
       BAD_OUT_PURITY=16, BAD_ERR_PURITY=32, BAD_FILES=64 };

static unsigned observe(const char *helper, const char *mode, const char *dir)
{
    char command[4096], pattern[MAX_PATH];
    SHELL_EXEC_RESULT r;
    unsigned mask = 0;
    size_t i;
    WIN32_FIND_DATAA fd;
    HANDLE find;
    int n = snprintf(command, sizeof(command), "\"%s\" %s", helper, mode);
    int ok;
    if (n < 0 || (size_t)n >= sizeof(command)) return BAD_STATUS;
    ok = AgentShellExec(command, dir, 30000, &r);
    if (!ok || r.exit_code != 0 || r.timed_out || r.execution_failed) mask |= BAD_STATUS;
    if (r.stdout_total_len != 100000) mask |= BAD_OUT_TOTAL;
    if (r.stderr_total_len != 100000) mask |= BAD_ERR_TOTAL;
    if (r.stdout_len != SHELL_BUFFER_MAX-1 || r.stderr_len != SHELL_BUFFER_MAX-1 ||
        !r.stdout_truncated || !r.stderr_truncated ||
        r.stdout_buf[SHELL_BUFFER_MAX-1] != 0 || r.stderr_buf[SHELL_BUFFER_MAX-1] != 0) mask |= BAD_CAPTURE;
    for (i=0; i<r.stdout_len && i<SHELL_BUFFER_MAX; i++) if (r.stdout_buf[i] != 'O') {mask |= BAD_OUT_PURITY; break;}
    for (i=0; i<r.stderr_len && i<SHELL_BUFFER_MAX; i++) if (r.stderr_buf[i] != 'E') {mask |= BAD_ERR_PURITY; break;}
    snprintf(pattern, sizeof(pattern), "%s\\*", dir);
    find = FindFirstFileA(pattern, &fd);
    if (find != INVALID_HANDLE_VALUE) {
        do {if (strcmp(fd.cFileName,".") && strcmp(fd.cFileName,"..")) mask |= BAD_FILES;}
        while (FindNextFileA(find,&fd));
        if (GetLastError()!=ERROR_NO_MORE_FILES) mask |= BAD_FILES;
        FindClose(find);
    } else mask |= BAD_FILES;
    printf("mode=%s mask=%u out=%llu err=%llu stored=%llu/%llu exit=%d timeout=%d launch=%d\n",
           mode,mask,(unsigned long long)r.stdout_total_len,(unsigned long long)r.stderr_total_len,
           (unsigned long long)r.stdout_len,(unsigned long long)r.stderr_len,r.exit_code,r.timed_out,r.execution_failed);
    return mask;
}

int main(int argc, char **argv)
{
    char temp[MAX_PATH], dir[MAX_PATH];
    DWORD n;
    int i, failures=0;
    if (argc != 2 || strchr(argv[1],'"')) return 2;
    n = GetTempPathA(sizeof(temp),temp);
    if (!n || n >= sizeof(temp)) return 2;
    if (snprintf(dir,sizeof(dir),"%ssymbols-dual-%lu-%lu",temp,(unsigned long)GetCurrentProcessId(),(unsigned long)GetTickCount()) >= sizeof(dir)) return 2;
    if (!CreateDirectoryA(dir,NULL)) return 2;
    for (i=0;i<3;i++) if (observe(argv[1],"normal",dir)!=0) failures++;
    if (observe(argv[1],"stderr_x",dir)!=BAD_ERR_PURITY) failures++;
    if (observe(argv[1],"stderr_short",dir)!=BAD_ERR_TOTAL) failures++;
    if (!RemoveDirectoryA(dir)) failures++;
    printf("WINDOWS_CONCURRENT_DUAL_STREAM failures=%d; native cases=5\n",failures);
    return failures?1:0;
}
