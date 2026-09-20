/* ============================================================
   c_std_lib.h: Standard C Library Knowledge & Signature Model.
   ISO/IEC 9899 C11 Standard Library Declarations for CodeGraph.
   ============================================================ */

#ifndef C_STD_LIB_H
#define C_STD_LIB_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Standard file stream handle structure */
typedef struct
{
    int   _file_descriptor;
    char *_buffer_base;
    int   _buffer_size;
    int   _flags;
} C_FILE;

/* Memory allocation block metadata */
typedef struct
{
    size_t size;
    bool   is_allocated;
} C_MEM_BLOCK;

/* Standard dynamic memory allocation */
void *malloc(size_t size) { (void)size; return NULL; }
void *calloc(size_t num, size_t size) { (void)num; (void)size; return NULL; }
void *realloc(void *ptr, size_t new_size) { (void)ptr; (void)new_size; return NULL; }
void  free(void *ptr) { (void)ptr; }

/* Standard I/O functions */
int     printf(const char *format) { (void)format; return 0; }
int     snprintf(char *str, size_t size, const char *format) { (void)str; (void)size; (void)format; return 0; }
C_FILE *fopen(const char *filename, const char *mode) { (void)filename; (void)mode; return NULL; }
int     fclose(C_FILE *stream) { (void)stream; return 0; }
size_t  fread(void *ptr, size_t size, size_t count, C_FILE *stream) { (void)ptr; (void)size; (void)count; (void)stream; return 0; }
size_t  fwrite(const void *ptr, size_t size, size_t count, C_FILE *stream) { (void)ptr; (void)size; (void)count; (void)stream; return 0; }

/* Standard string and memory manipulation */
size_t strlen(const char *str) { (void)str; return 0; }
int    strcmp(const char *str1, const char *str2) { (void)str1; (void)str2; return 0; }
int    strncmp(const char *str1, const char *str2, size_t num) { (void)str1; (void)str2; (void)num; return 0; }
char  *strcpy(char *destination, const char *source) { (void)destination; (void)source; return NULL; }
char  *strncpy(char *destination, const char *source, size_t num) { (void)destination; (void)source; (void)num; return NULL; }
void  *memcpy(void *destination, const void *source, size_t num) { (void)destination; (void)source; (void)num; return NULL; }
void  *memmove(void *destination, const void *source, size_t num) { (void)destination; (void)source; (void)num; return NULL; }
void  *memset(void *ptr, int value, size_t num) { (void)ptr; (void)value; (void)num; return NULL; }

/* Program termination and process control */
void exit(int status) { (void)status; }
void abort(void) { }

/* Mathematical utilities */
int abs(int n) { return n < 0 ? -n : n; }

#endif /* C_STD_LIB_H */
