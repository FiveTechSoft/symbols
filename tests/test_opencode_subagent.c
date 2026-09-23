/*
 * test_opencode_subagent.c - OpenCode child-session (task tool) support:
 * shape detection, the log of what the server emitted, the result block
 * appended to the final text, and the reply-text hook.
 */
#include <stdio.h>
#include <string.h>
#include "server_proto.h"

static int g_pass = 0, g_fail = 0;

static void check(const char *name, int ok)
{
    if (ok) { g_pass++; printf("  [PASS] %s\n", name); }
    else    { g_fail++; printf("  [FAIL] %s\n", name); }
}

static void hook_mark(char *content, size_t size)
{
    size_t n = strlen(content);
    if (n + 4 < size) memcpy(content + n, " [h]", 5);
}

int main(void)
{
    char a[3][64] = { "bash", "read", "edit" };
    char b[4][64] = { "bash", "read", "edit", "todowrite" };
    char c[3][64] = { "bash", "task", "read" };
    check("child shape: tools, no task/todowrite", ServerIsSubagentShape(a, 3) == 1);
    check("primary shape: todowrite declared", ServerIsSubagentShape(b, 4) == 0);
    check("primary shape: task declared", ServerIsSubagentShape(c, 3) == 0);
    check("no tools: not a child (title request)", ServerIsSubagentShape(a, 0) == 0);

    SERVER_CHILD_LOG log;
    memset(&log, 0, sizeof(log));
    ServerChildLogCall(&log, "glob", "{\"pattern\":\"**/*.c\"}");
    ServerChildLogCall(&log, "edit", "{\"filePath\":\"/w/src/a.c\",\"oldString\":\"x\",\"newString\":\"y\"}");
    ServerChildLogCall(&log, "edit", "{\"filePath\":\"/w/src/a.c\",\"oldString\":\"y\",\"newString\":\"z\"}");
    ServerChildLogCall(&log, "write", "{\"filePath\":\"/w/b \\\"q\\\".h\",\"content\":\"\"}");
    ServerChildLogCall(&log, "bash", "{\"command\":\"gcc -o t a.c\"}");
    ServerChildLogCall(&log, "bash", "{\"command\":\"./t\"}");
    check("log: two distinct files", log.nfiles == 2 && !strcmp(log.files[0], "/w/src/a.c") &&
                                       !strcmp(log.files[1], "/w/b \"q\".h"));
    check("log: commands counted, last kept", log.ncommands == 2 && !strcmp(log.last_command, "./t"));

    char txt[512] = "Hecho.";
    check("append block", ServerAppendSubagentResult(&log, 1, txt, sizeof(txt)) == 1);
    check("block lists files and last command",
          strstr(txt, "Hecho.\n\n---\nSubagent result (Symbols):\n- files changed: /w/src/a.c, /w/b \"q\".h") &&
          strstr(txt, "- commands run: 2 (last: `./t`)") && !strstr(txt, "read-only"));
    check("block appended once", ServerAppendSubagentResult(&log, 1, txt, sizeof(txt)) == 0);

    SERVER_CHILD_LOG none;
    memset(&none, 0, sizeof(none));
    char ro[256] = "Found it.";
    ServerAppendSubagentResult(&none, 0, ro, sizeof(ro));
    check("read-only block", strstr(ro, "- files changed: none") && strstr(ro, "- read-only session"));
    char tiny[16] = "abc";
    check("no room: content untouched", ServerAppendSubagentResult(&log, 1, tiny, sizeof(tiny)) == 0 && !strcmp(tiny, "abc"));

    char out[8192];
    ServerSetTextHook(hook_mark);
    check("hook applies to final replies",
          ServerBuildResponse("symbols", 1L, 1, "ok", "q", out, sizeof(out)) && strstr(out, "ok [h]"));
    check("hook applies to streamed replies",
          ServerBuildStreamResponse("symbols", 1L, 2, "ok", out, sizeof(out)) && strstr(out, "ok [h]"));
    ServerSetTextHook(NULL);
    check("no hook: text unchanged",
          ServerBuildResponse("symbols", 1L, 3, "ok", "q", out, sizeof(out)) && !strstr(out, "[h]"));

    printf("test_opencode_subagent: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
