/*
 * test_opencode_parent.c - OpenCode 1.18.32 parent side (task tool):
 * agent list, task output shapes, child result block, request parts,
 * the delegate / re-verify / resume once / direct decision, and the
 * strip that hands an exhausted delegation back to the normal loop.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "server_subagent.h"

static int g_pass = 0, g_fail = 0;

static void check(const char *name, int ok)
{
    if (ok) { g_pass++; printf("  [PASS] %s\n", name); }
    else    { g_fail++; printf("  [FAIL] %s\n", name); }
}

#define TASK_TOOL "{\"type\":\"function\",\"function\":{\"name\":\"task\",\"description\":\"Launch a new agent.\\n\\nAvailable agent types and the tools they have access to:\\n- explore: Fast agent specialized for exploring codebases.\\n- general: General-purpose agent. Use this agent to execute multiple units of work in parallel.\\n\",\"parameters\":{}}}"
#define OTHER_TOOLS "{\"type\":\"function\",\"function\":{\"name\":\"bash\",\"parameters\":{}}},{\"type\":\"function\",\"function\":{\"name\":\"glob\",\"parameters\":{}}},{\"type\":\"function\",\"function\":{\"name\":\"todowrite\",\"parameters\":{}}}"
#define SYS "{\"role\":\"system\",\"content\":\"<env>\\n  Working directory: /w\\n</env>\"}"
#define USER "{\"role\":\"user\",\"content\":[{\"type\":\"text\",\"text\":\"fix the compile errors in a.c and src/b.c\"}]}"
#define GLOB "{\"role\":\"assistant\",\"content\":\"\",\"tool_calls\":[{\"id\":\"g1\",\"type\":\"function\",\"function\":{\"name\":\"glob\",\"arguments\":\"{\\\"pattern\\\":\\\"**/*\\\"}\"}}]},{\"role\":\"tool\",\"tool_call_id\":\"g1\",\"content\":\"/w/a.c\\n/w/src/b.c\\n/w/c.c\"}"
#define TASKS "{\"role\":\"assistant\",\"content\":\"\",\"tool_calls\":[{\"id\":\"t1\",\"type\":\"function\",\"function\":{\"name\":\"task\",\"arguments\":\"{\\\"description\\\":\\\"part: a.c\\\",\\\"prompt\\\":\\\"x\\\",\\\"subagent_type\\\":\\\"general\\\"}\"}},{\"id\":\"t2\",\"type\":\"function\",\"function\":{\"name\":\"task\",\"arguments\":\"{\\\"description\\\":\\\"part: src/b.c\\\",\\\"prompt\\\":\\\"y\\\",\\\"subagent_type\\\":\\\"general\\\"}\"}}]}"
#define R1_OK "{\"role\":\"tool\",\"tool_call_id\":\"t1\",\"content\":\"<task id=\\\"ses_a\\\" state=\\\"completed\\\">\\n<task_result>\\nFixed.\\n\\n---\\nSubagent result (Symbols):\\n- files changed: /w/a.c\\n- commands run: 1\\n</task_result>\\n</task>\"}"
#define R2_ERR "{\"role\":\"tool\",\"tool_call_id\":\"t2\",\"content\":\"<task id=\\\"ses_b\\\" state=\\\"error\\\">\\n<task_error>\\nboom\\n</task_error>\\n</task>\"}"
#define V1 "{\"role\":\"assistant\",\"content\":\"\",\"tool_calls\":[{\"id\":\"v1\",\"type\":\"function\",\"function\":{\"name\":\"bash\",\"arguments\":\"{\\\"command\\\":\\\"gcc a.c\\\",\\\"description\\\":\\\"symbols-reverify: a.c\\\"}\"}}]}"
#define V1_OK "{\"role\":\"tool\",\"tool_call_id\":\"v1\",\"content\":\"symbols-exit=0\\n\"}"
#define V1_WARN "{\"role\":\"tool\",\"tool_call_id\":\"v1\",\"content\":\"a.c:1:1: warning: unused\\nsymbols-exit=0\\n\"}"
#define RES2 "{\"role\":\"assistant\",\"content\":\"\",\"tool_calls\":[{\"id\":\"t3\",\"type\":\"function\",\"function\":{\"name\":\"task\",\"arguments\":\"{\\\"description\\\":\\\"part: src/b.c\\\",\\\"prompt\\\":\\\"z\\\",\\\"subagent_type\\\":\\\"general\\\",\\\"task_id\\\":\\\"ses_b\\\"}\"}}]}"
#define R3_ERR "{\"role\":\"tool\",\"tool_call_id\":\"t3\",\"content\":\"<task id=\\\"ses_b\\\" state=\\\"error\\\">\\n<task_error>\\nagain\\n</task_error>\\n</task>\"}"
#define R3_OK "{\"role\":\"tool\",\"tool_call_id\":\"t3\",\"content\":\"task_id: ses_b (for resuming to continue this task if needed)\\n\\n<task_result>\\nDone.\\n</task_result>\"}"
#define V2 "{\"role\":\"assistant\",\"content\":\"\",\"tool_calls\":[{\"id\":\"v2\",\"type\":\"function\",\"function\":{\"name\":\"bash\",\"arguments\":\"{\\\"command\\\":\\\"gcc src/b.c\\\",\\\"description\\\":\\\"symbols-reverify: src/b.c\\\"}\"}}]}"
#define V2_OK "{\"role\":\"tool\",\"tool_call_id\":\"v2\",\"content\":\"symbols-exit=0\\n\"}"
#define USER_AT(name) "{\"role\":\"user\",\"content\":[{\"type\":\"text\",\"text\":\"@" name " fix the compile error in a.c\"},{\"type\":\"text\",\"text\":\" Use the above message and context to generate a prompt and call the task tool with subagent: " name "\"}]}"
#define USER_ONE "{\"role\":\"user\",\"content\":[{\"type\":\"text\",\"text\":\"fix the compile error in a.c\"}]}"

static void set_mem_env(const char *path)
{
    static char kv[600];
    snprintf(kv, sizeof(kv), "SYMBOLS_SUBAGENT_MEMORY=%s", path ? path : "");
#ifdef _WIN32
    _putenv(kv);
#else
    putenv(kv);
#endif
}

static char g_body[16384];
static const char *body(int with_task, const char *msgs)
{
    snprintf(g_body, sizeof(g_body), "{\"model\":\"symbols\",\"tools\":[%s%s],\"messages\":[" SYS ",%s]}",
             with_task ? TASK_TOOL "," : "", OTHER_TOOLS, msgs);
    return g_body;
}

static int decide(int with_task, const char *msgs, SA_DECISION *d)
{
    char declared[8][64] = { "bash", "glob", "todowrite", "task" };
    return SaDecide(body(with_task, msgs), declared, with_task ? 4 : 3,
                    "fix the compile errors in a.c and src/b.c", d);
}

int main(void)
{
    static SA_DECISION d;
    SA_AGENT ag[4];
    char id[64], st[16], text[512], files[4][260], parts[4][260];

    printf("=== OpenCode parent side ===\n");
    int na = SaParseAgents(body(1, USER), ag, 4);
    check("agents parsed from the task description", na == 2 && strcmp(ag[0].name, "explore") == 0 && strcmp(ag[1].name, "general") == 0);
    check("no memory: agent chosen by the declared parallel-work overlap", SaChooseAgent(ag, na, "a.c") == 1);
    check("no task tool: no agents", SaParseAgents(body(0, USER), ag, 4) == 0);

    check("task output, tag shape", SaParseTaskOutput("<task id=\"ses_1\" state=\"completed\">\n<task_result>\nok\n</task_result>\n</task>", id, sizeof(id), st, sizeof(st), text, sizeof(text)) &&
          strcmp(id, "ses_1") == 0 && strcmp(st, "completed") == 0 && strcmp(text, "ok") == 0);
    check("task output, error tag", SaParseTaskOutput("<task id=\"ses_2\" state=\"error\"><task_error>bad</task_error></task>", id, sizeof(id), st, sizeof(st), text, sizeof(text)) &&
          strcmp(st, "error") == 0 && strcmp(text, "bad") == 0);
    check("task output, task_id line shape", SaParseTaskOutput("task_id: ses_3 (for resuming)\n\n<task_result>\ndone\n</task_result>", id, sizeof(id), st, sizeof(st), text, sizeof(text)) &&
          strcmp(id, "ses_3") == 0 && strcmp(st, "completed") == 0 && strcmp(text, "done") == 0);

    check("child block: files listed", SaChildFiles("x\n---\nSubagent result (Symbols):\n- files changed: /w/a.c, b.h\n- commands run: 0", files, 4) == 2 && strcmp(files[1], "b.h") == 0);
    check("child block: none", SaChildFiles("Subagent result (Symbols):\n- files changed: none\n", files, 4) == 0);
    check("no child block (child is not Symbols)", SaChildFiles("I fixed it.", files, 4) == -1);

    check("parts: two named listed files", SaNamedParts("fix a.c and src/b.c.", "/w/a.c\n/w/src/b.c\n/w/c.c", "/w", parts, 4) == 2 &&
          strcmp(parts[0], "a.c") == 0 && strcmp(parts[1], "src/b.c") == 0);
    check("parts: basename resolves to its path", SaNamedParts("fix b.c", "/w/src/b.c", "/w", parts, 4) == 1 && strcmp(parts[0], "src/b.c") == 0);
    check("parts: unlisted names are not parts", SaNamedParts("fix x.c and y.c", "/w/a.c", "/w", parts, 4) == 0);

    check("delegates 2 parts right after the listing", decide(1, USER "," GLOB, &d) == SA_CALLS && d.calls.count == 2 &&
          strcmp(d.calls.calls[0].name, "task") == 0 && strstr(d.calls.calls[0].arguments, "\"subagent_type\":\"general\""));
    check("each part's prompt names only its own file", strstr(d.calls.calls[0].arguments, "a.c") && !strstr(d.calls.calls[0].arguments, "src/b.c") &&
          strstr(d.calls.calls[1].arguments, "src/b.c") && !strstr(d.calls.calls[1].arguments, "in a.c"));
    check("no task tool declared: no delegation", decide(0, USER "," GLOB, &d) == SA_NONE);
    check("before the listing: no delegation", decide(1, USER, &d) == SA_NONE);

    check("completed part is rebuilt here first", decide(1, USER "," GLOB "," TASKS "," R1_OK "," R2_ERR, &d) == SA_CALLS &&
          d.calls.count == 1 && strcmp(d.calls.calls[0].name, "bash") == 0 &&
          strstr(d.calls.calls[0].arguments, "symbols-reverify: a.c") && strstr(d.calls.calls[0].arguments, "symbols-exit"));
    check("failed part resumed once by task_id", decide(1, USER "," GLOB "," TASKS "," R1_OK "," R2_ERR "," V1 "," V1_OK, &d) == SA_CALLS &&
          d.calls.count == 1 && strstr(d.calls.calls[0].arguments, "\"task_id\":\"ses_b\"") && strstr(d.calls.calls[0].arguments, "part: src/b.c"));
    check("a rebuild with a warning is not clean", decide(1, USER "," GLOB "," TASKS "," R1_OK "," R2_ERR "," V1 "," V1_WARN, &d) == SA_CALLS &&
          d.calls.count == 2);
    check("failed again after resume: direct", decide(1, USER "," GLOB "," TASKS "," R1_OK "," R2_ERR "," V1 "," V1_OK "," RES2 "," R3_ERR, &d) == SA_DIRECT &&
          strstr(d.text, "doing it directly"));
    check("resumed part rebuilt, then all done", decide(1, USER "," GLOB "," TASKS "," R1_OK "," R2_ERR "," V1 "," V1_OK "," RES2 "," R3_OK, &d) == SA_CALLS &&
          strstr(d.calls.calls[0].arguments, "symbols-reverify: src/b.c"));
    check("final report after every part rebuilt", decide(1, USER "," GLOB "," TASKS "," R1_OK "," R2_ERR "," V1 "," V1_OK "," RES2 "," R3_OK "," V2 "," V2_OK, &d) == SA_TEXT &&
          strstr(d.text, "`a.c` -> general") && strstr(d.text, "`src/b.c` -> general (task ses_b, 2 attempts): done"));

    /* explicit @agent mention (declared rule): honored even for one part */
    {
        char declared1[8][64] = { "bash", "glob", "todowrite", "task" };
        const char *q_at = "@explore fix the compile error in a.c\n Use the above message and context to generate a prompt and call the task tool with subagent: explore";
        check("mention: agent found only when listed", SaMentionedAgent(q_at, ag, na) == 0 &&
              SaMentionedAgent("call the task tool with subagent: builder", ag, na) == -1 &&
              SaMentionedAgent("fix a.c", ag, na) == -1);
        check("@explore on one file delegates that one part to explore",
              SaDecide(body(1, USER_AT("explore") "," GLOB), declared1, 4, q_at, &d) == SA_CALLS && d.calls.count == 1 &&
              strstr(d.calls.calls[0].arguments, "\"subagent_type\":\"explore\"") && strstr(d.calls.calls[0].arguments, "part: a.c"));
        check("mention scaffolding is not in the child's prompt", !strstr(d.calls.calls[0].arguments, "@explore") &&
              !strstr(d.calls.calls[0].arguments, "call the task tool") && strstr(d.calls.calls[0].arguments, "fix the compile error in a.c"));
        const char *q_bad = "@builder fix the compile error in a.c\n Use the above message and context to generate a prompt and call the task tool with subagent: builder";
        check("mention of an unlisted agent: no delegation for one part",
              SaDecide(body(1, USER_AT("builder") "," GLOB), declared1, 4, q_bad, &d) == SA_NONE);
        check("one part without a mention: no delegation",
              SaDecide(body(1, USER_ONE "," GLOB), declared1, 4, "fix the compile error in a.c", &d) == SA_NONE);
    }

    static char out[16384];
    const char *b = body(1, USER "," GLOB "," TASKS "," R1_OK "," R2_ERR "," V1 "," V1_OK "," RES2 "," R3_ERR);
    check("strip succeeds", SaStripExchanges(b, out, sizeof(out)) == 1);
    check("strip drops the task tool and our calls", !strstr(out, "\"name\":\"task\"") && !strstr(out, "ses_") && !strstr(out, "symbols-reverify"));
    check("strip keeps the rest", strstr(out, "fix the compile errors") && strstr(out, "\"g1\"") && strstr(out, "\"name\":\"todowrite\"") &&
          strstr(out, "Working directory: /w"));
    char declared[3][64] = { "bash", "glob", "todowrite" };
    check("stripped body is a plain request again", SaDecide(out, declared, 3, "fix", &d) == SA_NONE);

    char mem[512];
    const char *tmpdir = getenv("TEMP");
    if (!tmpdir || !tmpdir[0]) tmpdir = getenv("TMPDIR");
    if (!tmpdir || !tmpdir[0]) tmpdir = ".";
    snprintf(mem, sizeof(mem), "%s/test_opencode_parent_mem.tsv", tmpdir);
    FILE *f = fopen(mem, "w");
    check("memory file created", f != NULL);
    if (f) {
        fclose(f);
        set_mem_env(mem);
        SaMemoryRecord("explore", "a.c", "verified");
        SaMemoryRecord("explore", "b.c", "verified");
        SaMemoryRecord("general", "a.c", "failed");
        check("memory: net score per agent and extension", SaMemoryScore("explore", "x.c") == 2 && SaMemoryScore("general", "x.c") == -1 &&
              SaMemoryScore("explore", "x.py") == 0);
        check("memory outranks the description overlap", SaChooseAgent(ag, SaParseAgents(body(1, USER), ag, 4), "z.c") == 0);
        SaMemoryRecord("general", "a.c", "failed");
        SaMemoryRecord("explore", "a.c", "failed"); SaMemoryRecord("explore", "a.c", "failed");
        SaMemoryRecord("explore", "a.c", "failed"); SaMemoryRecord("explore", "a.c", "failed");
        check("memory: repeated failures -> do it directly", decide(1, USER "," GLOB, &d) == SA_NONE);
        set_mem_env("");
        remove(mem);
    }

    printf("\nResult: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
