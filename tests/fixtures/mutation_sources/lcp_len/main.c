/* Source: src/parser.c (LcpLen), verbatim.
   Oracle: common-prefix lengths. */
#include <stddef.h>

static size_t LcpLen(const char *a, const char *b)
{
    size_t n = 0;
    if (a == NULL || b == NULL)
        return 0;
    while (a[n] != '\0' && a[n] == b[n])
        n++;
    return n;
}

int main(void)
{
    int bad = 0;
    bad += LcpLen("COMEN", "COME") != 4;
    bad += LcpLen("COMEN", "COMAR") != 3;
    bad += LcpLen("HIJOS", "HIJO_DE") != 4;
    bad += LcpLen("abc", "abc") != 3;
    bad += LcpLen("abc", "xyz") != 0;
    bad += LcpLen("", "abc") != 0;
    bad += LcpLen(NULL, "abc") != 0;
    return bad == 0 ? 0 : 1;
}
