/* Source: src/task_ops.c (ident_char, count_token_in), verbatim.
   Oracle: whole-identifier occurrence counts. */
#include <ctype.h>
#include <string.h>

static int ident_char(int c)  { return isalnum(c) || c == '_'; }

static int count_token_in(const char *s, const char *ident)
{
    size_t n = strlen(ident);
    int count = 0;
    if (n == 0)
        return 0;
    for (const char *p = strstr(s, ident); p; p = strstr(p + 1, ident)) {
        int left_ok = (p == s) || !ident_char((unsigned char)p[-1]);
        int right_ok = !ident_char((unsigned char)p[n]);
        if (left_ok && right_ok)
            count++;
    }
    return count;
}

int main(void)
{
    int bad = 0;
    bad += count_token_in("x = x + 1;", "x") != 2;
    bad += count_token_in("max(maxval, max_len)", "max") != 1;
    bad += count_token_in("foo", "foo") != 1;
    bad += count_token_in("foobar barfoo", "foo") != 0;
    bad += count_token_in("a_b a b", "a") != 1;
    bad += count_token_in("anything", "") != 0;
    bad += count_token_in("f(f(f))", "f") != 3;
    return bad == 0 ? 0 : 1;
}
