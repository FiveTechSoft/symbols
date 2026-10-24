#include <stdio.h>
#include <string.h>
#include "server_proto.h"
static unsigned failed;
static void check(const char *name, int ok)
{
    printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    failed += !ok;
}
int main(void)
{
    check("English replacement priority", ServerIsExplicitReplaceTask("replace old with new in sample.txt"));
    check("Spanish replacement priority", ServerIsExplicitReplaceTask("cambia old por new en sample.txt"));
    check("later edit verb is not imperative", !ServerIsExplicitReplaceTask("echo replace old with new in sample.txt"));
    check("later edit verb cannot mutate", !ServerIsEditTask("echo replace old with new in sample.txt"));
    check("later verb cannot resolve remembered target", !ServerHasEditVerb("echo replace old with new in sample.txt"));
    check("literal sed preserved", !ServerIsExplicitReplaceTask("sed -i s/old/new/ sample.txt"));
    check("question cannot mutate", !ServerIsExplicitReplaceTask("How do I replace old with new in sample.txt?"));
    check("destination required", !ServerIsExplicitReplaceTask("replace old with new"));
    check("replacement required", !ServerIsExplicitReplaceTask("replace old in sample.txt"));
    {
        char file[260], old_s[256], new_s[256];
        int parsed = ServerParseReplacement("replace 'foo bar' with 'baz qux' in sample.txt", file, sizeof(file), old_s, sizeof(old_s), new_s, sizeof(new_s));
        check("multiword replacement exact", parsed == 1 && strcmp(old_s, "foo bar") == 0 && strcmp(new_s, "baz qux") == 0 && strcmp(file, "sample.txt") == 0);
        check("multiple files not silently truncated", ServerParseReplacement("replace foo with bar in a.txt and b.txt", file, sizeof(file), old_s, sizeof(old_s), new_s, sizeof(new_s)) == -1);
        check("extra action not silently truncated", ServerParseReplacement("replace foo with bar in a.txt then delete b.txt", file, sizeof(file), old_s, sizeof(old_s), new_s, sizeof(new_s)) == -1);
        check("unclosed quote refused", ServerParseReplacement("replace 'foo with bar in sample.txt", file, sizeof(file), old_s, sizeof(old_s), new_s, sizeof(new_s)) == -1);
    }
    check("legacy alias cannot bypass grammar", ServerReplacementNeedsClarification("edita sample.txt y cambia 'foo bar' por 'baz qux'"));
    check("negation cannot bypass grammar", ServerReplacementNeedsClarification("do not replace foo with bar in sample.txt"));
    check("mention cannot bypass grammar", ServerReplacementNeedsClarification("what happens if I replace foo with bar in sample.txt?"));
    check("quoted imperative cannot mutate", ServerReplacementNeedsClarification("\"replace foo with bar in sample.txt\""));
    check("uppercase alias cannot bypass", ServerReplacementNeedsClarification("EDITA sample.txt Y CAMBIA foo POR bar"));
    check("supported sed unchanged", !ServerReplacementNeedsClarification("sed -i s/foo/bar/ sample.txt"));
    check("positive not rejected", !ServerReplacementNeedsClarification("replace foo with bar in sample.txt"));
    return failed ? 1 : 0;
}
