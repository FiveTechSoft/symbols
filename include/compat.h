#ifndef COMPAT_H
#define COMPAT_H

/* MSVC portability shims for POSIX functions.
   strtok_s has the same signature and semantics as strtok_r
   (str, delimiters, context); _strdup matches strdup. */
#ifdef _MSC_VER
#define strtok_r strtok_s
#define strdup _strdup
#endif

/* Filesystem and POSIX I/O shims without pulling in windows.h */
#ifdef _WIN32
#include <io.h>
#include <direct.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#define _mkdir(p) mkdir(p, 0755)
#define _access(p, m) access(p, m)
#define _dup(fd) dup(fd)
#define _dup2(f1, f2) dup2(f1, f2)
#define _close(fd) close(fd)
#define _fileno(f) fileno(f)
#endif

#endif
