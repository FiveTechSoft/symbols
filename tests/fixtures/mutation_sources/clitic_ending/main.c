/* Source: src/chat.c (HasCliticEnding), verbatim.
   Oracle: outputs of the original function on boundary inputs. */
#include <string.h>

static int HasCliticEnding(const char *tok)
{
    static const char *CL[] = {
        "me", "te", "se", "nos", "lo", "la", "los", "las",
        "le", "les",
    };
    size_t L;
    size_t i;
    if (tok == NULL)
        return 0;
    L = strlen(tok);
    if (L < 4)
        return 0;
    for (i = 0; i < sizeof(CL) / sizeof(CL[0]); i++)
    {
        size_t c = strlen(CL[i]);
        if (L > c + 1 && strcmp(tok + L - c, CL[i]) == 0)
            return 1;
    }
    return 0;
}

int main(void)
{
    int bad = 0;
    bad += (HasCliticEnding("dame")) != 1;
    bad += (HasCliticEnding("dime")) != 1;
    bad += (HasCliticEnding("damelo")) != 1;
    bad += (HasCliticEnding("ase")) != 0;
    bad += (HasCliticEnding("case")) != 1;
    bad += (HasCliticEnding("ellos")) != 1;
    bad += (HasCliticEnding("nos")) != 0;
    bad += (HasCliticEnding("xnos")) != 0;
    bad += (HasCliticEnding("anos")) != 0;
    bad += (HasCliticEnding("dile")) != 1;
    bad += (HasCliticEnding("dales")) != 1;
    bad += (HasCliticEnding(NULL)) != 0;
    bad += (HasCliticEnding("casa")) != 0;
    return bad == 0 ? 0 : 1;
}
