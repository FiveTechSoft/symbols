#include <stddef.h>

int is_empty(const char *s) {
    return s == NULL || s[0] == '\0';
}
