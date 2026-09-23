/* Source: src/chat.c (ChatLooksLikeIndexLine), verbatim.
   Oracle: outputs of the original function on boundary inputs. */
#include <string.h>

static int ChatLooksLikeIndexLine(const char *sent)
{
    size_t i, n, letters = 0, digits = 0, lower = 0;
    const char *p;
    if (sent == NULL)
        return 1;
    p = sent;
    while (*p == ' ' || *p == '\t')
        p++;
    while (*p >= '0' && *p <= '9')
        p++;
    if (*p == ' ' && p[1] == ':' && p[2] == ' ')
        p += 3;
    while (*p >= '0' && *p <= '9')
        p++;
    while (*p == ' ' || *p == '\t')
        p++;
    n = strlen(p);
    if (n < 8)
        return 1;
    for (i = 0; p[i] != '\0'; i++)
    {
        unsigned char c = (unsigned char)p[i];
        if (c >= '0' && c <= '9')
            digits++;
        else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c >= 0x80)
        {
            letters++;
            if (c >= 'a' && c <= 'z')
                lower++;
        }
    }
    if (lower == 0 && n < 24)
        return 1;
    if (digits >= 2 && letters > 0 && digits * 3 >= letters && n < 80)
        return 1;
    return 0;
}

int main(void)
{
    int bad = 0;
    bad += (ChatLooksLikeIndexLine("short")) != 1;
    bad += (ChatLooksLikeIndexLine("1234567")) != 1;
    bad += (ChatLooksLikeIndexLine("the soul is wholly only libido")) != 0;
    bad += (ChatLooksLikeIndexLine("12 : 34 the soul is wholly libido")) != 0;
    bad += (ChatLooksLikeIndexLine("ANIMA AND ANIMUS")) != 1;
    bad += (ChatLooksLikeIndexLine("ANIMA AND ANIMUS IN THE WORKS OF JUNG")) != 0;
    bad += (ChatLooksLikeIndexLine("anima, 12, 45, 78")) != 1;
    bad += (ChatLooksLikeIndexLine("anima 12 45 78 and many more words follow here")) != 0;
    bad += (ChatLooksLikeIndexLine("abcdefgh")) != 0;
    bad += (ChatLooksLikeIndexLine("ABCDEFGH")) != 1;
    bad += (ChatLooksLikeIndexLine("ab 12 cd")) != 1;
    bad += (ChatLooksLikeIndexLine(NULL)) != 1;
    bad += (ChatLooksLikeIndexLine("   7 page ten of it")) != 0;
    return bad == 0 ? 0 : 1;
}
