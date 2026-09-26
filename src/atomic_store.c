#include "atomic_store.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define MKDIR(d) _mkdir(d)
#else
#include <sys/stat.h>
#include <unistd.h>
#define MKDIR(d) mkdir(d, 0755)
#endif

int AtomicStorePrepareParent(const char *filepath)
{
    char path[512];
    size_t n = strlen(filepath);
    if (!n || n >= sizeof(path)) return 0;
    memcpy(path, filepath, n + 1);
    for (char *p = path; *p; ++p)
        if (*p == '/' || *p == '\\') {
            char save = *p; *p = 0;
            if (*path && MKDIR(path) != 0) {
#ifdef _WIN32
                DWORD error = GetLastError();
                if (error != ERROR_ALREADY_EXISTS) return 0;
                DWORD attrs = GetFileAttributesA(path);
                if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) return 0;
#else
                struct stat st;
                if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) return 0;
#endif
            }
            *p = save;
        }
    return 1;
}

int AtomicStoreReplace(const char *path, const void *data, size_t len)
{
    if (!path || (!data && len) || !AtomicStorePrepareParent(path)) return 0;
    size_t n = strlen(path);
    if (n > 506) return 0;
    char tmp[520];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (!f) return 0;
    int ok = !len || fwrite(data, 1, len, f) == len;
    if (fflush(f) != 0) ok = 0;
    if (fclose(f) != 0) ok = 0;
    if (!ok) { remove(tmp); return 0; }
#ifdef _WIN32
    if (!MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
#else
    if (rename(tmp, path) != 0)
#endif
    { remove(tmp); return 0; }
    return 1;
}
