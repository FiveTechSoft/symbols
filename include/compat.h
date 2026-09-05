#ifndef COMPAT_H
#define COMPAT_H

/* Cuñas de portabilidad MSVC para funciones POSIX.
   strtok_s tiene la misma firma y semantica que strtok_r
   (str, delimitadores, contexto); _strdup equivale a strdup. */
#ifdef _MSC_VER
#define strtok_r strtok_s
#define strdup _strdup
#endif

#endif
