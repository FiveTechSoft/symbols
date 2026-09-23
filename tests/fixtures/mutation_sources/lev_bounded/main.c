/* Source: src/chat.c (LevBounded), verbatim.
   Oracle: bounded byte edit distances. */
#include <stdint.h>
#include <string.h>

static uint32_t LevBounded(const char *a, const char *b,
                           uint32_t bound)
{
    size_t la;
    size_t lb;
    size_t i;
    size_t j;
    uint32_t prev[128];
    uint32_t cur[128];
    if (a == NULL || b == NULL)
        return bound + 1;
    la = strlen(a);
    lb = strlen(b);
    if (la >= 128 || lb >= 128)
        return bound + 1;
    if (la > lb + bound || lb > la + bound)
        return bound + 1;
    for (j = 0; j <= lb; j++)
        prev[j] = (uint32_t)j;
    for (i = 1; i <= la; i++)
    {
        uint32_t rowmin;
        cur[0] = (uint32_t)i;
        rowmin = cur[0];
        for (j = 1; j <= lb; j++)
        {
            uint32_t cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            uint32_t v = prev[j] + 1;
            if (cur[j - 1] + 1 < v)
                v = cur[j - 1] + 1;
            if (prev[j - 1] + cost < v)
                v = prev[j - 1] + cost;
            cur[j] = v;
            if (v < rowmin)
                rowmin = v;
        }
        if (rowmin > bound)
            return bound + 1;
        for (j = 0; j <= lb; j++)
            prev[j] = cur[j];
    }
    return prev[lb] <= bound ? prev[lb] : bound + 1;
}

int main(void)
{
    int bad = 0;
    bad += LevBounded("gato", "gato", 2) != 0;
    bad += LevBounded("gato", "pato", 2) != 1;
    bad += LevBounded("gato", "gatos", 2) != 1;
    bad += LevBounded("gato", "gta", 2) != 2;
    bad += LevBounded("kitten", "sitting", 3) != 3;
    bad += LevBounded("kitten", "sitting", 2) != 3;
    bad += LevBounded("abc", "abcdef", 2) != 3;
    bad += LevBounded("", "ab", 2) != 2;
    bad += LevBounded(NULL, "ab", 1) != 2;
    bad += LevBounded("abcd", "dcba", 4) != 4;
    return bad == 0 ? 0 : 1;
}
