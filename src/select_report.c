/* select_report.c: see include/select_report.h. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "select_report.h"
#include "agent_shell.h"
#include "agent_patch.h"

#define SR_HOOK_TIMEOUT_MS 30000
#define SR_MAX_NOTES 64

static int PathArgSafe(const char *a)
{
    if (!a[0] || a[0] == '-') return 0;
    for (const char *c = a; *c; c++)
    {
        if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') ||
              *c == '_' || *c == '.' || *c == '/' || *c == '+' || *c == '-'))
            return 0;
    }
    return 1;
}

static int Full(FILE *out, const char *reason)
{
    fprintf(out, "%s\n", SELECT_REPORT_HEADER);
    fprintf(out, "mode: full\nreason: %s\n", reason);
    return 0;
}

int SelectReportPrint(FILE *out, const char *workspace, const char *build,
                      char **changed, int nchanged, unsigned timeout_ms)
{
    char script[MAX_PATCH_PATH + 32];
    char cmd[8192];
    FILE *f;
    SHELL_EXEC_RESULT *res;
    size_t n;
    const char *p;

    if (nchanged <= 0) return Full(out, "no changed files were given");
    if (!PathArgSafe(build)) return Full(out, "the build directory path has a character outside the supported set");
    snprintf(script, sizeof(script), "%s/tools/build_graph.py", workspace);
    f = fopen(script, "rb");
    if (!f) return Full(out, "tools/build_graph.py was not found in the workspace");
    fclose(f);
    n = (size_t)snprintf(cmd, sizeof(cmd), "python3 tools/build_graph.py select %s --changed", build);
    for (int i = 0; i < nchanged; i++)
    {
        if (!PathArgSafe(changed[i])) return Full(out, "a changed file path has a character outside the supported set");
        if (n + strlen(changed[i]) + 2 >= sizeof(cmd)) return Full(out, "the changed file list is too long");
        n += (size_t)snprintf(cmd + n, sizeof(cmd) - n, " %s", changed[i]);
    }
    res = (SHELL_EXEC_RESULT *)malloc(sizeof(*res));
    if (!res) return Full(out, "out of memory");
    AgentShellResultInit(res);
    AgentShellExecGuarded(cmd, workspace, timeout_ms, res);
    if (res->execution_failed || res->exit_code != 0)
    {
        free(res);
        return Full(out, "python3 tools/build_graph.py did not run to a successful exit");
    }
    p = strstr(res->stdout_buf, "\"mode\": \"");
    if (!p || res->stdout_buf[0] != '{' || res->stdout_truncated)
    {
        free(res);
        return Full(out, "the selection output was not recognised");
    }
    fprintf(out, "%s\n", SELECT_REPORT_HEADER);
    fprintf(out, "%s", res->stdout_buf);
    if (res->stdout_len && res->stdout_buf[res->stdout_len - 1] != '\n') fprintf(out, "\n");
    free(res);
    return 0;
}

/* ---- opt-in hook state for TaskOpsSolve ---- */

static char g_notes[SR_MAX_NOTES][512];
static int g_nnotes;
static int g_overflow;

static const char *GateBuild(void)
{
    const char *v = getenv("SYMBOLS_SELECT_REPORT");
    return (v && v[0] && strcmp(v, "0") != 0) ? v : NULL;
}

int SelectReportGateOn(void) { return GateBuild() != NULL; }

void SelectReportNoteReset(void)
{
    g_nnotes = 0;
    g_overflow = 0;
}

void SelectReportNote(const char *rel)
{
    if (!GateBuild() || !rel) return;
    if (g_nnotes >= SR_MAX_NOTES || strlen(rel) >= sizeof(g_notes[0])) { g_overflow = 1; return; }
    for (int i = 0; i < g_nnotes; i++)
        if (!strcmp(g_notes[i], rel)) return;
    snprintf(g_notes[g_nnotes++], sizeof(g_notes[0]), "%s", rel);
}

void SelectReportAfterVerified(const char *workspace)
{
    const char *build = GateBuild();
    char *changed[SR_MAX_NOTES];
    if (!build || !workspace) return;
    fprintf(stderr, "[symbols-agent] select-report for the verified edit (report only; the build and test commands are unchanged)\n");
    if (g_overflow || g_nnotes <= 0)
    {
        Full(stderr, g_overflow ? "too many changed files to report"
                                : "the changed files of this operator are not recorded");
        return;
    }
    for (int i = 0; i < g_nnotes; i++) changed[i] = g_notes[i];
    SelectReportPrint(stderr, workspace, build, changed, g_nnotes, SR_HOOK_TIMEOUT_MS);
}
