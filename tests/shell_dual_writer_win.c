/* Test-only source of independently scheduled stdout/stderr byte streams. */
#include <windows.h>
#include <string.h>

typedef struct {
    HANDLE stream, start, ready;
    char byte;
    DWORD target, written;
    int failed;
} WRITER;

static DWORD WINAPI write_stream(LPVOID arg)
{
    WRITER *w = (WRITER *)arg;
    char bytes[4000];
    memset(bytes, w->byte, sizeof(bytes));
    if (!SetEvent(w->ready) || WaitForSingleObject(w->start, 10000) != WAIT_OBJECT_0) {
        w->failed = 1;
        return 1;
    }
    while (w->written < w->target) {
        DWORD n = 0, want = w->target - w->written;
        if (want > sizeof(bytes)) want = (DWORD)sizeof(bytes);
        if (!WriteFile(w->stream, bytes, want, &n, NULL) || n == 0) {
            w->failed = 1;
            return 1;
        }
        w->written += n;
    }
    return 0;
}

int main(int argc, char **argv)
{
    WRITER w[2];
    HANDLE threads[2] = {NULL, NULL}, ready[2] = {NULL, NULL}, start = NULL;
    int i, result = 2;
    const char *mode = argc == 2 ? argv[1] : "";
    if (strcmp(mode, "normal") && strcmp(mode, "stderr_x") && strcmp(mode, "stderr_short")) return 2;
    memset(w, 0, sizeof(w));
    start = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (!start) return 2;
    for (i = 0; i < 2; i++) {
        ready[i] = CreateEventA(NULL, TRUE, FALSE, NULL);
        if (!ready[i]) goto cleanup;
        w[i].ready = ready[i]; w[i].start = start;
        w[i].stream = GetStdHandle(i ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);
        w[i].byte = i ? (!strcmp(mode, "stderr_x") ? 'X' : 'E') : 'O';
        w[i].target = i && !strcmp(mode, "stderr_short") ? 99999 : 100000;
        threads[i] = CreateThread(NULL, 0, write_stream, &w[i], 0, NULL);
        if (!threads[i]) goto cleanup;
    }
    if (WaitForMultipleObjects(2, ready, TRUE, 10000) != WAIT_OBJECT_0 || !SetEvent(start)) goto cleanup;
    if (WaitForMultipleObjects(2, threads, TRUE, 20000) != WAIT_OBJECT_0) goto cleanup;
    result = (w[0].failed || w[1].failed || w[0].written != w[0].target || w[1].written != w[1].target) ? 3 : 0;
cleanup:
    /* On setup/wait failure, exit the process immediately: the owning shell job
       controls its lifetime, and no worker may outlive this stack frame. */
    if (result == 2) ExitProcess(2);
    for (i = 0; i < 2; i++) {
        if (threads[i]) CloseHandle(threads[i]);
        if (ready[i]) CloseHandle(ready[i]);
    }
    CloseHandle(start);
    return result;
}
