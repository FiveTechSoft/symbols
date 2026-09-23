/* Source: src/text_lex.c (IsCountable), verbatim.
   Oracle: outputs of the original function on boundary inputs. */
#include <stddef.h>
#include <stdint.h>

static int IsCountable(const unsigned char *p, uint32_t n)
{
    uint32_t i;
    int alnum = 0;
    int nondigit = 0;
    if (p == NULL || n == 0)
        return 0;
    for (i = 0; i < n; i++)
    {
        unsigned char c = p[i];
        int a = ((c >= 'A' && c <= 'Z') ||
                 (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                 c >= 0x80);
        if (a)
            alnum = 1;
        if (!((c >= '0' && c <= '9')))
            nondigit = 1;
    }
    if (!alnum || !nondigit)
        return 0;
    if (n == 1)
    {
        unsigned char c = p[0];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
            return 0;
    }
    return 1;
}

#include <string.h>
static int cnt(const char *s) { return IsCountable((const unsigned char *)s, (uint32_t)strlen(s)); }

int main(void)
{
    int bad = 0;
    bad += (cnt("a")) != 0;
    bad += (cnt("7")) != 0;
    bad += (cnt("42")) != 0;
    bad += (cnt("x1")) != 1;
    bad += (cnt("-")) != 0;
    bad += (cnt("--")) != 0;
    bad += (cnt("ab")) != 1;
    bad += (cnt("Z9")) != 1;
    bad += (cnt("@a")) != 1;
    bad += (cnt("9z")) != 1;
    bad += (cnt("\xC3\xA9")) != 1;
    bad += (cnt("\x80")) != 1;
    bad += (IsCountable(NULL, 1)) != 0;
    bad += (IsCountable((const unsigned char *)"ab", 0)) != 0;
    return bad == 0 ? 0 : 1;
}
