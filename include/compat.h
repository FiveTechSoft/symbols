#ifndef COMPAT_H
#define COMPAT_H

/* MSVC portability shims for POSIX functions.
   strtok_s has the same signature and semantics as strtok_r
   (str, delimiters, context); _strdup matches strdup. */
#ifdef _MSC_VER
#define strtok_r strtok_s
#define strdup _strdup
#endif

#endif
