#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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
    check("complete Spanish file-first alias supported", ServerIsExplicitReplaceTask("edita sample.txt y cambia 'foo bar' por 'baz qux'"));
    check("negation cannot bypass grammar", ServerReplacementNeedsClarification("do not replace foo with bar in sample.txt"));
    check("mention cannot bypass grammar", ServerReplacementNeedsClarification("what happens if I replace foo with bar in sample.txt?"));
    check("quoted imperative cannot mutate", ServerReplacementNeedsClarification("\"replace foo with bar in sample.txt\""));
    check("uppercase alias cannot bypass", ServerReplacementNeedsClarification("EDITA sample.txt Y CAMBIA foo POR bar"));
    check("supported sed unchanged", !ServerReplacementNeedsClarification("sed -i s/foo/bar/ sample.txt"));
    check("positive not rejected", !ServerReplacementNeedsClarification("replace foo with bar in sample.txt"));
    check("file-first multi-action rejected", ServerReplacementNeedsClarification("edita sample.txt y cambia foo por bar y borra other.txt"));
    check("file-first missing conjunction rejected", ServerReplacementNeedsClarification("edita sample.txt cambia foo por bar"));
    check("file-first quoted mention rejected", ServerReplacementNeedsClarification("\"edita sample.txt y cambia foo por bar\""));
    check("file-first negation rejected", ServerReplacementNeedsClarification("no edita sample.txt y cambia foo por bar"));
    {
        char file[260], old_s[256], new_s[256];
        int parsed = ServerParseReplacement("edita sample.txt y cambia 'foo bar' por 'baz qux'", file, sizeof(file), old_s, sizeof(old_s), new_s, sizeof(new_s));
        check("file-first bytes exact", parsed == 1 && strcmp(file, "sample.txt") == 0 && strcmp(old_s, "foo bar") == 0 && strcmp(new_s, "baz qux") == 0);
        check("plain edit retains inspection route", ServerParseReplacement("edita sample.txt", file, sizeof(file), old_s, sizeof(old_s), new_s, sizeof(new_s)) == 0);
        check("quoted file-first literal not intent", ServerParseReplacement("\"edita sample.txt y cambia foo por bar\"", file, sizeof(file), old_s, sizeof(old_s), new_s, sizeof(new_s)) == 0);
        check("file-first second file rejected", ServerParseReplacement("edita sample.txt y cambia foo por bar en other.txt", file, sizeof(file), old_s, sizeof(old_s), new_s, sizeof(new_s)) == -1);
        check("file-first unclosed quote rejected", ServerParseReplacement("edita sample.txt y cambia 'foo por bar", file, sizeof(file), old_s, sizeof(old_s), new_s, sizeof(new_s)) == -1);
    }
    {
        char file[260], old_s[256], new_s[256];
        check("tab inside value rejected", ServerParseReplacement("replace \"foo\tbar\" with value in sample.txt", file, sizeof(file), old_s, sizeof(old_s), new_s, sizeof(new_s)) == -1);
        check("backspace inside value rejected", ServerParseReplacement("replace \"foo\bbar\" with value in sample.txt", file, sizeof(file), old_s, sizeof(old_s), new_s, sizeof(new_s)) == -1);
        check("tab inside alias rejected", ServerParseReplacement("edita sample.txt y cambia \"foo\tbar\" por value", file, sizeof(file), old_s, sizeof(old_s), new_s, sizeof(new_s)) == -1);
    }
    {
        char file[260], old_s[256], new_s[256];
        int all_rejected = 1;
        unsigned c;
        for (c = 1; c < 32; c++)
        {
            char request[] = "replace \"fooXbar\" with value in sample.txt";
            *strchr(request, 'X') = (char)c;
            if (ServerParseReplacement(request, file, sizeof(file), old_s, sizeof(old_s), new_s, sizeof(new_s)) != -1) all_rejected = 0;
        }
        check("every non-NUL C0 control rejected inside value", all_rejected);
    }
    {
        char file[260], old_s[256], new_s[256], out[1024];
        check("control between tokens rejected", ServerParseReplacement("replace\tfoo with bar in sample.txt", file, sizeof(file), old_s, sizeof(old_s), new_s, sizeof(new_s)) == -1);
        check("trailing control rejected", ServerParseReplacement("replace foo with bar in sample.txt\t", file, sizeof(file), old_s, sizeof(old_s), new_s, sizeof(new_s)) == -1);
        check("newline between tokens rejected", ServerParseReplacement("replace foo with bar\n in sample.txt", file, sizeof(file), old_s, sizeof(old_s), new_s, sizeof(new_s)) == -1);
        check("JSON NUL does not reuse prior user", !ServerExtractQuery("{\"messages\":[{\"role\":\"user\",\"content\":\"replace old with DANGER in old.txt\"},{\"role\":\"user\",\"content\":\"replace foo with bar in sample.txt\\u0000 extra\"}]}", out, sizeof(out)));
        check("array JSON NUL rejected", !ServerExtractQuery("{\"messages\":[{\"role\":\"user\",\"content\":[{\"type\":\"text\",\"text\":\"replace foo with bar in sample.txt\\u0000 extra\"}]}]}", out, sizeof(out)));
    }
    {
        char out[1024];
        check("leading control cannot bypass", ServerReplacementNeedsClarification("\t replace foo with bar in sample.txt"));
        check("control glued to verb cannot bypass", ServerReplacementNeedsClarification("replace\bfoo with bar in sample.txt"));
        check("array later NUL rejects earlier text", !ServerExtractQuery("{\"messages\":[{\"role\":\"user\",\"content\":[{\"text\":\"replace foo with bar in sample.txt\"},{\"text\":\"bad\\u0000 tail\"}]}]}", out, sizeof(out)));
        check("array earlier NUL rejects later text", !ServerExtractQuery("{\"messages\":[{\"role\":\"user\",\"content\":[{\"text\":\"bad\\u0000 tail\"},{\"text\":\"replace foo with bar in sample.txt\"}]}]}", out, sizeof(out)));
        check("NUL role cannot become user", !ServerExtractQuery("{\"messages\":[{\"role\":\"user\\u0000x\",\"content\":\"replace foo with bar in sample.txt\"}]}", out, sizeof(out)));
        check("history NUL cannot be ignored", !ServerExtractQuery("{\"messages\":[{\"role\":\"assistant\",\"content\":\"bad\\u0000tail\"},{\"role\":\"user\",\"content\":\"replace foo with bar in sample.txt\"}]}", out, sizeof(out)));
    }
    {
        char *body = malloc(20000), *filler = malloc(7001), out[8192];
        const char *query = "replace foo with bar in sample.txt";
        if (!body || !filler) return 2;
        memset(filler, 'x', 7000); filler[7000] = 0;
        snprintf(body, 20000, "{\"tools\":[{\"description\":\"%s\"}],\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}]}", filler, query);
        check("schema7000 does not limit valid last user", ServerExtractQuery(body, out, sizeof(out)) && strcmp(out, query) == 0);
        snprintf(body, 20000, "{\"messages\":[{\"role\":\"assistant\",\"content\":\"%s\"},{\"role\":\"user\",\"content\":\"%s\"}]}", filler, query);
        check("history7000 does not limit valid last user", ServerExtractQuery(body, out, sizeof(out)) && strcmp(out, query) == 0);
        snprintf(body, 20000, "{\"tools\":[{\"description\":\"%s\\u0000tail\"}],\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}]}", filler, query);
        check("NUL after schema7000 is still validated", !ServerExtractQuery(body, out, sizeof(out)));
        snprintf(body, 20000, "{\"messages\":[{\"role\":\"assistant\",\"content\":\"%s\\u0000tail\"},{\"role\":\"user\",\"content\":\"%s\"}]}", filler, query);
        check("NUL after history7000 is still validated", !ServerExtractQuery(body, out, sizeof(out)));
        snprintf(body, 20000, "{\"messages\":[{\"role\":\"user\",\"content\":\"%s\"},{\"role\":\"user\",\"content\":\"%s\"}]}", filler, query);
        check("earlier long user does not block valid last user", ServerExtractQuery(body, out, sizeof(out)) && strcmp(out, query) == 0);
        snprintf(body, 20000, "{\"messages\":[{\"role\":\"user\",\"content\":\"%s\"},{\"role\":\"user\",\"content\":\"replace foo with bar in sample.txt %s then delete other.txt\"}]}", query, filler);
        check("long last user cannot execute prefix or prior user", !ServerExtractQuery(body, out, sizeof(out)));
        free(filler); free(body);
    }
    {
        char out[4096], body[12000], first[3001], second[3001];
        memset(first, 'a', 3000); first[3000] = 0;
        memset(second, 'b', 3000); second[3000] = 0;
        check("two text blocks reject executable first", !ServerExtractQuery("{\"messages\":[{\"role\":\"user\",\"content\":[{\"text\":\"replace foo with bar in sample.txt\"},{\"text\":\"do not execute\"}]}]}", out, sizeof(out)));
        check("two text blocks reject executable last", !ServerExtractQuery("{\"messages\":[{\"role\":\"user\",\"content\":[{\"text\":\"do not execute\"},{\"text\":\"replace foo with bar in sample.txt\"}]}]}", out, sizeof(out)));
        snprintf(body, sizeof(body), "{\"messages\":[{\"role\":\"user\",\"content\":[{\"text\":\"%s\"},{\"text\":\"%s\"}]}]}", first, second);
        check("combined text overflow cannot drop second block", !ServerExtractQuery(body, out, sizeof(out)));
    }
    return failed ? 1 : 0;
}
