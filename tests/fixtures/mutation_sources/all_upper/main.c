/* Source: src/text_lex.c (IsAllUpperStr), verbatim.
   Oracle: outputs of the original function on boundary inputs. */
#include <stddef.h>

static int IsAllUpperStr(const char *s)
{
    if (s == NULL || s[0] == '\0')
        return 0;
    for (size_t i = 0; s[i] != '\0'; i++)
    {
        if (s[i] >= 'a' && s[i] <= 'z')
            return 0;
    }
    return 1;
}

int main(void)
{
    int bad = 0;
    bad += (IsAllUpperStr("ABC")) != 1;
    bad += (IsAllUpperStr("AbC")) != 0;
    bad += (IsAllUpperStr("a")) != 0;
    bad += (IsAllUpperStr("z")) != 0;
    bad += (IsAllUpperStr("`")) != 1;
    bad += (IsAllUpperStr("{")) != 1;
    bad += (IsAllUpperStr("123")) != 1;
    bad += (IsAllUpperStr("")) != 0;
    bad += (IsAllUpperStr(NULL)) != 0;
    bad += (IsAllUpperStr("A B-C")) != 1;
    return bad == 0 ? 0 : 1;
}
