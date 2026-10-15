/* Native Windows OS launch failure. No hook and no non-Windows stub. */
#include "agent_shell.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>

static unsigned failure_mask(int ok, const SHELL_EXEC_RESULT *r)
{
    unsigned mask = 0;
    if (ok != 0 || !r->execution_failed || r->exit_code != -1 || r->timed_out) mask |= 1;
    if (r->stdout_len || r->stderr_len || r->stdout_total_len || r->stderr_total_len ||
        r->stdout_truncated || r->stderr_truncated || r->stdout_buf[0] || r->stderr_buf[0]) mask |= 2;
    if (GetFileAttributesA("witness.txt") != INVALID_FILE_ATTRIBUTES) mask |= 4;
    return mask;
}

static int child(const char *mode)
{
    SHELL_EXEC_RESULT r;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    DWORD before, after, error;
    char command[] = "cmd.exe /c echo witness>witness.txt";
    int ok, i;
    unsigned mask;
    if (!strcmp(mode, "positive")) {
        memset(&r, 0xa5, sizeof(r));
        ok = AgentShellExec("echo witness>witness.txt", ".", 5000, &r);
        mask = failure_mask(ok, &r);
        printf("positive ok=%d exit=%d launch=%d timeout=%d failure_mask=%u\n",
               ok, r.exit_code, r.execution_failed, r.timed_out, mask);
        return !(ok == 1 && r.exit_code == 0 && !r.execution_failed && !r.timed_out && mask == 5);
    }
    if (strcmp(mode, "invalid")) return 2;
    memset(&si, 0, sizeof(si)); si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));
    ok = CreateProcessA(NULL, command, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, ".", &si, &pi);
    error = ok ? 0 : GetLastError();
    printf("invalid-image native probe ok=%d error=%lu\n", ok, (unsigned long)error);
    if (ok) {
        TerminateProcess(pi.hProcess, 2);
        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        return 1;
    }
    if (error != ERROR_BAD_EXE_FORMAT || GetFileAttributesA("witness.txt") != INVALID_FILE_ATTRIBUTES) return 1;
    /* Warm up diagnostics and the handle-count API before the measured calls. */
    if (!GetProcessHandleCount(GetCurrentProcess(), &before)) return 2;
    printf("handle probe warmed; native failure calls=3\n"); fflush(stdout);
    if (!GetProcessHandleCount(GetCurrentProcess(), &before)) return 2;
    for (i = 0; i < 3; i++) {
        memset(&r, 0xa5, sizeof(r));
        ok = AgentShellExec("echo witness>witness.txt", ".", 5000, &r);
        mask = failure_mask(ok, &r);
        printf("invalid iteration=%d mask=%u ok=%d exit=%d launch=%d timeout=%d stored=%llu/%llu total=%llu/%llu\n",
               i, mask, ok, r.exit_code, r.execution_failed, r.timed_out,
               (unsigned long long)r.stdout_len, (unsigned long long)r.stderr_len,
               (unsigned long long)r.stdout_total_len, (unsigned long long)r.stderr_total_len);
        if (mask) return 1;
    }
    if (!GetProcessHandleCount(GetCurrentProcess(), &after)) return 2;
    printf("handles before=%lu after=%lu\n", (unsigned long)before, (unsigned long)after);
    return after > before ? 1 : 0;
}

static int run_copy(const char *exe, const char *dir, const char *mode)
{
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit;
    HANDLE job;
    DWORD code = 2, wait;
    char command[MAX_PATH + 64];
    int n = snprintf(command, sizeof(command), "\"%s\" --child %s", exe, mode);
    if (n < 0 || (size_t)n >= sizeof(command)) return 0;
    memset(&si, 0, sizeof(si)); si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    memset(&pi, 0, sizeof(pi));
    memset(&limit, 0, sizeof(limit));
    limit.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    job = CreateJobObjectA(NULL, NULL);
    if (!job) return 0;
    if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limit, sizeof(limit))) {CloseHandle(job); return 0;}
    if (!CreateProcessA(exe, command, NULL, NULL, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED, NULL, dir, &si, &pi)) {
        printf("private runner launch error=%lu\n", (unsigned long)GetLastError());
        CloseHandle(job); return 0;
    }
    if (!AssignProcessToJobObject(job, pi.hProcess) || ResumeThread(pi.hThread) == (DWORD)-1) {
        TerminateProcess(pi.hProcess, 2); WaitForSingleObject(pi.hProcess, 5000);
    } else {
        wait = WaitForSingleObject(pi.hProcess, 30000);
        if (wait == WAIT_OBJECT_0) GetExitCodeProcess(pi.hProcess, &code);
        else {TerminateJobObject(job, 2); WaitForSingleObject(pi.hProcess, 5000);}
    }
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess); CloseHandle(job);
    printf("private runner mode=%s exit=%lu\n", mode, (unsigned long)code);
    return code == 0;
}

static int scenario(const char *self, const char *root, const char *mode)
{
    char dir[MAX_PATH], exe[MAX_PATH], shell[MAX_PATH], system[MAX_PATH], real_shell[MAX_PATH];
    char witness[MAX_PATH], image[64];
    WIN32_FIND_DATAA fd;
    HANDLE h, find;
    DWORD n, written;
    int ok = 0;
    if (snprintf(dir, sizeof(dir), "%s\\%s", root, mode) >= sizeof(dir) ||
        snprintf(exe, sizeof(exe), "%s\\runner.exe", dir) >= sizeof(exe) ||
        snprintf(shell, sizeof(shell), "%s\\cmd.exe", dir) >= sizeof(shell) ||
        snprintf(witness, sizeof(witness), "%s\\witness.txt", dir) >= sizeof(witness)) return 0;
    if (!CreateDirectoryA(dir, NULL)) return 0;
    if (!CopyFileA(self, exe, TRUE)) goto cleanup;
    if (!strcmp(mode, "positive")) {
        n = GetSystemDirectoryA(system, sizeof(system));
        if (!n || n >= sizeof(system) || snprintf(real_shell, sizeof(real_shell), "%s\\cmd.exe", system) >= sizeof(real_shell)) goto cleanup;
        if (!CopyFileA(real_shell, shell, TRUE)) goto cleanup;
    } else {
        memset(image, 'X', sizeof(image));
        h = CreateFileA(shell, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE) goto cleanup;
        ok = WriteFile(h, image, sizeof(image), &written, NULL) && written == sizeof(image);
        if (!CloseHandle(h)) ok = 0;
        if (!ok) goto cleanup;
    }
    ok = run_copy(exe, dir, mode);
    /* Inspect names before deleting only our declared files. */
    if (snprintf(system, sizeof(system), "%s\\*", dir) >= sizeof(system)) {ok = 0; goto cleanup;}
    find = FindFirstFileA(system, &fd);
    if (find == INVALID_HANDLE_VALUE) ok = 0;
    else {
        do {
            if (strcmp(fd.cFileName, ".") && strcmp(fd.cFileName, "..") &&
                strcmp(fd.cFileName, "runner.exe") && strcmp(fd.cFileName, "cmd.exe") &&
                (strcmp(mode, "positive") || strcmp(fd.cFileName, "witness.txt"))) ok = 0;
        } while (FindNextFileA(find, &fd));
        if (GetLastError() != ERROR_NO_MORE_FILES) ok = 0;
        FindClose(find);
    }
cleanup:
    if (GetFileAttributesA(witness) != INVALID_FILE_ATTRIBUTES && !DeleteFileA(witness)) ok = 0;
    if (GetFileAttributesA(shell) != INVALID_FILE_ATTRIBUTES && !DeleteFileA(shell)) ok = 0;
    if (GetFileAttributesA(exe) != INVALID_FILE_ATTRIBUTES && !DeleteFileA(exe)) ok = 0;
    if (!RemoveDirectoryA(dir)) ok = 0;
    return ok;
}

int main(int argc, char **argv)
{
    char self[MAX_PATH], temp[MAX_PATH], root[MAX_PATH];
    DWORD n;
    int ok;
    if (argc == 3 && !strcmp(argv[1], "--child")) return child(argv[2]);
    if (argc != 1) return 2;
    n = GetModuleFileNameA(NULL, self, sizeof(self)); if (!n || n >= sizeof(self)) return 2;
    n = GetTempPathA(sizeof(temp), temp); if (!n || n >= sizeof(temp)) return 2;
    if (snprintf(root, sizeof(root), "%ssymbols-spawn-%lu-%lu", temp,
                 (unsigned long)GetCurrentProcessId(), (unsigned long)GetTickCount()) >= sizeof(root)) return 2;
    if (!CreateDirectoryA(root, NULL)) return 2;
    ok = scenario(self, root, "positive");
    /* Stop on a failed control, rather than interpreting the failure arm. */
    if (ok) ok = scenario(self, root, "invalid");
    if (!RemoveDirectoryA(root)) ok = 0;
    if (ok) puts("WINDOWS_SPAWN_FAILURE native cases=3; invalid-image repeats=3; sensitivity mask=5");
    return ok ? 0 : 1;
}
