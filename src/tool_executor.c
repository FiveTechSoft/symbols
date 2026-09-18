/* tool_executor: minimal tool execution over static in-memory
   tables. English code comments (project rule); Spanish only in NLG
   literals, which always carry source marking ("fuente externa" /
   "calculo") so tool data is never confused with corpus record.
   Tables are external-source stand-ins: consulted, never merged
   into the corpus KB. No HTTP, DB, MCP or filesystem. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <direct.h>
#include <io.h>
#include <windows.h>
#include "bible_chat.h"
#include "tool_contract.h"

static int ToolEvalExpr(const char *expr, double *out);

/* relation/person fixtures live in tool_config.c (file rows win,
   frozen compiled fallback): consulted here read-only. */

/* shell allowlist lives in tool_config.c (file rows win, frozen
   compiled fallback): consulted here read-only. */
int ShellLookup(const char *tok, char *backend, size_t bs, char *dflt,
                size_t ds)
{
    uint32_t i, nrows = ShellAllowCount();
    if (tok == NULL)
        return 0;
    for (i = 0; i < nrows; i++)
    {
        const ShellAllowRow *row = ShellAllowAt(i);
        if (row == NULL)
            continue;
        if (strcmp(tok, row->trigger) == 0)
        {
            if (backend != NULL && bs > 0)
            {
                strncpy(backend, row->backend, bs - 1);
                backend[bs - 1] = '\0';
            }
            if (dflt != NULL && ds > 0)
            {
                strncpy(dflt, row->dflt, ds - 1);
                dflt[ds - 1] = '\0';
            }
            return 1;
        }
    }
    return 0;
}

void ShellSandboxPath(char *out, size_t size)
{
    const char *tmp = getenv("TMPDIR");
    if (tmp == NULL || tmp[0] == '\0')
        tmp = getenv("TMP");
    if (tmp == NULL || tmp[0] == '\0')
        tmp = getenv("TEMP");
    if (tmp == NULL || tmp[0] == '\0')
        tmp = ".";
    if (size == 0)
        return;
    snprintf(out, size, "%s/symbols_shell_sandbox", tmp);
}

/* structural refuse: length cap plus shell metacharacters (no
   chaining, redirection, expansion or quoting games). */
static int ShellBlocked(const char *argv)
{
    const char *p;
    if (argv == NULL || strlen(argv) > 256)
        return 1;
    for (p = argv; *p != '\0'; p++)
        if (*p == '&' || *p == '|' || *p == ';' || *p == '<' ||
            *p == '>' || *p == '`' || *p == '$' || *p == '%' ||
            *p == '!' || *p == '"')
            return 1;
    return 0;
}

static void ShellFlatten(char *text)
{
    size_t r = 0, w = 0;
    int sp = 1;
    while (text[r] != '\0')
    {
        char c = text[r++];
        if (c == '\r' || c == '\n' || c == '\t')
            c = ' ';
        if (c == ' ')
        {
            if (sp)
                continue;
            sp = 1;
        }
        else
            sp = 0;
        text[w++] = c;
    }
    while (w > 0 && text[w - 1] == ' ')
        w--;
    text[w] = '\0';
}

/* Execute through CreateProcess with anonymous pipes: stdout and
   stderr merged into one bounded buffer, strict timeout (kills on
   expiry, never hangs the REPL), NULL stdin (no input waits), fixed
   sandbox as working directory (no _chdir games: thread-safe).
   exit_code -2 = could not even launch; -1 + timed_out = killed. */
static int ShellRun(const char *backend, const char *argv, char *out,
                    size_t size, int *exit_code, int *timed_out)
{
    char cmd[768];
    char dir[512];
    HANDLE hRead = NULL, hWrite = NULL;
    SECURITY_ATTRIBUTES sa;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    size_t pos = 0;
    if (out == NULL || size == 0)
        return 0;
    out[0] = '\0';
    if (exit_code != NULL)
        *exit_code = -2;
    if (timed_out != NULL)
        *timed_out = 0;
    ShellSandboxPath(dir, sizeof(dir));
    _mkdir(dir);
    if (strcmp(backend, "powershell") == 0)
        snprintf(cmd, sizeof(cmd),
                 "powershell -NoProfile -NonInteractive -Command %s",
                 argv);
    else if (strcmp(backend, "bash") == 0)
    {
        if (_access("C:\\Program Files\\Git\\bin\\bash.exe", 0) == 0)
            snprintf(cmd, sizeof(cmd),
                     "\"C:\\Program Files\\Git\\bin\\bash.exe\" -c \"%s\"",
                     argv);
        else
            snprintf(cmd, sizeof(cmd), "bash -c \"%s\"", argv);
    }
    else
        snprintf(cmd, sizeof(cmd), "%s", argv);
    memset(&sa, 0, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0))
        return 0;
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.hStdInput = NULL;
    memset(&pi, 0, sizeof(pi));
    if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW,
                        NULL, dir, &si, &pi))
    {
        CloseHandle(hRead);
        CloseHandle(hWrite);
        return 0;
    }
    CloseHandle(hWrite);
    hWrite = NULL;
    if (WaitForSingleObject(pi.hProcess, SHELL_TIMEOUT_MS) ==
        WAIT_TIMEOUT)
    {
        TerminateProcess(pi.hProcess, 124);
        if (timed_out != NULL)
            *timed_out = 1;
        if (exit_code != NULL)
            *exit_code = -1;
    }
    else
    {
        DWORD ec = (DWORD)-1;
        GetExitCodeProcess(pi.hProcess, &ec);
        if (exit_code != NULL)
            *exit_code = (int)ec;
    }
    for (;;)
    {
        DWORD avail = 0, rd = 0;
        DWORD want;
        char chunk[512];
        if (!PeekNamedPipe(hRead, NULL, 0, NULL, &avail, NULL) ||
            avail == 0)
            break;
        want = avail > sizeof(chunk) ? (DWORD)sizeof(chunk) : avail;
        if (pos + 1 >= size || pos + 1 >= (size_t)SHELL_OUT_MAX)
            break;
        if (want > size - pos - 1)
            want = (DWORD)(size - pos - 1);
        if (want > SHELL_OUT_MAX - 1 - pos)
            want = (DWORD)(SHELL_OUT_MAX - 1 - pos);
        if (!ReadFile(hRead, chunk, want, &rd, NULL) || rd == 0)
            break;
        memcpy(out + pos, chunk, rd);
        pos += rd;
    }
    out[pos] = '\0';
    CloseHandle(hRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    ShellFlatten(out);
    return 1;
}

ShellResult ShellExec(const char *cmd_line)
{
    ShellResult r;
    char backend[16] = "cmd";
    char first[CHAT_TOKEN_MAX];
    size_t k = 0;
    r.exit_code = -2;
    r.timed_out = 0;
    r.out_len = 0;
    r.out_buf[0] = '\0';
    if (cmd_line == NULL)
        return r;
    while (cmd_line[k] != '\0' && cmd_line[k] != ' ' &&
           k + 1 < sizeof(first))
    {
        first[k] = cmd_line[k];
        k++;
    }
    first[k] = '\0';
    if (ShellLookup(first, backend, sizeof(backend), NULL, 0) &&
        !ShellBlocked(cmd_line))
    {
        int ec = -2, to = 0;
        char text[SHELL_OUT_MAX];
        if (ShellRun(backend, cmd_line, text, sizeof(text), &ec, &to))
        {
            r.exit_code = ec;
            r.timed_out = to;
            strncpy(r.out_buf, text, sizeof(r.out_buf) - 1);
            r.out_buf[sizeof(r.out_buf) - 1] = '\0';
            r.out_len = strlen(r.out_buf);
        }
    }
    return r;
}

/* strict filename token: ASCII alnum/dot/underscore/dash, bounded,
   no parent escape. */
static int IsSafeName(const char *s)
{
    size_t n = 0;
    if (s == NULL || s[0] == '\0')
        return 0;
    for (; *s != '\0'; s++, n++)
    {
        char c = *s;
        if (!(isalnum((unsigned char)c) || c == '.' || c == '_' ||
              c == '-'))
            return 0;
        if (n > 60)
            return 0;
    }
    return strstr(s - n, "..") == NULL;
}

static int IsGccFlag(const char *s)
{
    static const char *F[] = {
        "-O2", "-O1", "-O0", "-g", "-Wall", "-Wextra", "-o", "-c",
        "-std=c11", "-std=c99",
    };
    size_t i;
    for (i = 0; i < sizeof(F) / sizeof(F[0]); i++)
        if (strcmp(s, F[i]) == 0)
            return 1;
    return 0;
}

static int HasCSuffix(const char *s)
{
    size_t n = strlen(s);
    return n > 2 && s[n - 2] == '.' &&
           (s[n - 1] == 'c' || s[n - 1] == 'C');
}

ShellResult GccCompile(const char *source_file, const char *extra_flags)
{
    ShellResult r;
    char cmd[512];
    r.exit_code = -2;
    r.timed_out = 0;
    r.out_len = 0;
    r.out_buf[0] = '\0';
    if (!IsSafeName(source_file) || !HasCSuffix(source_file))
        return r;
    {
        /* every extra token is a gcc flag or a safe filename */
        char tmp[256];
        strncpy(tmp, extra_flags == NULL ? "" : extra_flags,
                sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';
        {
            char *tok = strtok(tmp, " \t");
            while (tok != NULL)
            {
                if (!IsGccFlag(tok) && !IsSafeName(tok))
                    return r;
                tok = strtok(NULL, " \t");
            }
        }
    }
    if (extra_flags == NULL || extra_flags[0] == '\0')
        snprintf(cmd, sizeof(cmd), "gcc %s", source_file);
    else
        snprintf(cmd, sizeof(cmd), "gcc %s %s", extra_flags,
                 source_file);
    return ShellExec(cmd);
}

/* sandbox-confined file name: safe token with a known source/text
   extension (not first/last char). Shared by planner and executor. */
static const char *FS_EXTS[] = {"c", "h", "txt", "log", "md"};

int FsNameOk(const char *name)
{
    const char *dot;
    size_t i;
    if (!IsSafeName(name))
        return 0;
    dot = strrchr(name, '.');
    if (dot == NULL || dot == name || dot[1] == '\0')
        return 0;
    for (i = 0; i < sizeof(FS_EXTS) / sizeof(FS_EXTS[0]); i++)
        if (strcmp(dot + 1, FS_EXTS[i]) == 0)
            return 1;
    return 0;
}

int FsReadSandbox(const char *name, char *out, size_t size)
{
    char dir[512], path[576];
    FILE *f;
    size_t n = 0;
    int c;
    if (out == NULL || size == 0 || !FsNameOk(name))
        return 0;
    out[0] = '\0';
    ShellSandboxPath(dir, sizeof(dir));
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    f = fopen(path, "rb");
    if (f == NULL)
        return 0;
    while ((c = fgetc(f)) != EOF && n + 1 < size && n + 1 < 2049)
        out[n++] = (char)c;
    out[n] = '\0';
    fclose(f);
    ShellFlatten(out);
    return 1;
}

void ToolExecute(const ToolRequest *req, ToolResult *res)
{
    memset(res, 0, sizeof(*res));
    if (req == NULL)
        return;
    res->tool = req->tool;
    switch (req->tool)
    {
    case TOOL_LOOKUP_RELATION:
        for (uint32_t i = 0;
             i < FixtureRelCount() && res->nitems < 8;
             i++)
        {
            const FixtureRelRow *row = FixtureRelAt(i);
            if (row == NULL)
                continue;
            if (strcmp(row->subject, req->subject) == 0 &&
                strcmp(row->rel, req->relation) == 0)
            {
                strncpy(res->items[res->nitems], row->object,
                        CHAT_TOKEN_MAX - 1);
                res->items[res->nitems][CHAT_TOKEN_MAX - 1] = '\0';
                res->nitems++;
            }
        }
        res->ok = res->nitems > 0;
        break;
    case TOOL_LOOKUP_PERSON:
        for (uint32_t i = 0; i < FixturePersonCount(); i++)
        {
            const FixturePersonRow *row = FixturePersonAt(i);
            if (row == NULL)
                continue;
            if (strcmp(row->name, req->subject) == 0)
            {
                strncpy(res->items[0], row->detail,
                        CHAT_TOKEN_MAX - 1);
                res->items[0][CHAT_TOKEN_MAX - 1] = '\0';
                res->nitems = 1;
                res->ok = 1;
                break;
            }
        }
        break;
    case TOOL_CALCULATOR:
    {
        /* symbolic operators only (+ - * / % ^ parens); word forms
           ("por", "entre") are out of scope and fail closed. */
        double v = 0.0;
        if (ToolEvalExpr(req->subject, &v))
        {
            snprintf(res->number, sizeof(res->number), "%g", v);
            res->ok = 1;
        }
        break;
    }
    case TOOL_SHELL:
    {
        /* defense in depth: argv[0] must name an allowlisted row
           with the requested backend, and pass the metachar filter
           (the planner already routes this way; the executor
           re-checks before touching any shell). gcc requests go
           through the strict GccCompile validator. */
        char first[CHAT_TOKEN_MAX];
        char be[16], df[64];
        size_t k = 0;
        while (req->subject[k] != '\0' && req->subject[k] != ' ' &&
               k + 1 < sizeof(first))
        {
            first[k] = req->subject[k];
            k++;
        }
        first[k] = '\0';
        if (ShellLookup(first, be, sizeof(be), df, sizeof(df)) &&
            strcmp(be, req->relation) == 0 &&
            !ShellBlocked(req->subject))
        {
            ShellResult sr;
            if (strcmp(first, "gcc") == 0)
            {
                /* strict gcc path: validate tokens, then compile */
                char rest[256];
                size_t rl = strlen(req->subject);
                if (rl > 4)
                {
                    strncpy(rest, req->subject + 4, sizeof(rest) - 1);
                    rest[sizeof(rest) - 1] = '\0';
                    {
                        const char *sp = rest;
                        while (*sp == ' ')
                            sp++;
                        if (strcmp(sp, "--version") == 0)
                            sr = ShellExec(req->subject);
                        else
                        {
                            /* source = first .c token, rest = flags */
                            char src[64] = "";
                            char flg[192] = "";
                            char tmp[256];
                            strncpy(tmp, sp, sizeof(tmp) - 1);
                            tmp[sizeof(tmp) - 1] = '\0';
                            {
                                char *tok = strtok(tmp, " \t");
                                while (tok != NULL)
                                {
                                    if (HasCSuffix(tok) &&
                                        src[0] == '\0')
                                        strncpy(src, tok,
                                                sizeof(src) - 1);
                                    else
                                    {
                                        if (flg[0] != '\0')
                                            strncat(flg, " ",
                                                    sizeof(flg) -
                                                    strlen(flg) - 1);
                                        strncat(flg, tok,
                                                sizeof(flg) -
                                                strlen(flg) - 1);
                                    }
                                    tok = strtok(NULL, " \t");
                                }
                            }
                            src[sizeof(src) - 1] = '\0';
                            if (src[0] != '\0')
                                sr = GccCompile(src, flg);
                            else
                            {
                                sr.exit_code = -2;
                                sr.timed_out = 0;
                                sr.out_len = 0;
                                sr.out_buf[0] = '\0';
                            }
                        }
                    }
                }
                else
                {
                    sr.exit_code = -2;
                    sr.timed_out = 0;
                    sr.out_len = 0;
                    sr.out_buf[0] = '\0';
                }
            }
            else
                sr = ShellExec(req->subject);
            res->exit_code = sr.exit_code;
            res->timed_out = sr.timed_out;
            strncpy(res->text, sr.out_buf, sizeof(res->text) - 1);
            res->text[sizeof(res->text) - 1] = '\0';
            res->ok = 1;
        }
        break;
    }
    case TOOL_FS_READ:
    {
        char content[2048];
        if (FsReadSandbox(req->subject, content, sizeof(content)))
        {
            strncpy(res->text, content, sizeof(res->text) - 1);
            res->text[sizeof(res->text) - 1] = '\0';
            res->ok = 1;
        }
        break;
    }
    default:
        break;
    }
}

/* tiny expression parser: recursive descent over + - * / % ^,
   parens and unary minus (doubles; division by zero fails). */
static const char *g_ep;

static void EpSkip(void)
{
    while (*g_ep && isspace((unsigned char)*g_ep))
        g_ep++;
}

static int EpNumber(double *v)
{
    char *end = NULL;
    EpSkip();
    if (!isdigit((unsigned char)*g_ep) && *g_ep != '.')
        return 0;
    *v = strtod(g_ep, &end);
    if (end == g_ep)
        return 0;
    g_ep = end;
    return 1;
}

static int EpExpr(double *v);

static int EpFact(double *v)
{
    EpSkip();
    if (*g_ep == '(')
    {
        g_ep++;
        if (!EpExpr(v))
            return 0;
        EpSkip();
        if (*g_ep != ')')
            return 0;
        g_ep++;
        return 1;
    }
    if (*g_ep == '-')
    {
        g_ep++;
        if (!EpFact(v))
            return 0;
        *v = -*v;
        return 1;
    }
    return EpNumber(v);
}

static int EpTerm(double *v)
{
    double rhs = 0.0;
    if (!EpFact(v))
        return 0;
    for (;;)
    {
        EpSkip();
        if (*g_ep == '*' || *g_ep == '/' || *g_ep == '%')
        {
            char op = *g_ep++;
            if (!EpFact(&rhs))
                return 0;
            if (op == '*')
                *v *= rhs;
            else if (op == '/')
            {
                if (rhs == 0.0)
                    return 0;
                *v /= rhs;
            }
            else
            {
                long a = (long)*v, b = (long)rhs;
                if (b == 0 || (double)a != *v || (double)b != rhs)
                    return 0;
                *v = (double)(a % b);
            }
        }
        else if (*g_ep == '^')
        {
            g_ep++;
            if (!EpFact(&rhs))
                return 0;
            *v = pow(*v, rhs);
        }
        else
            return 1;
    }
}

static int EpExpr(double *v)
{
    double rhs = 0.0;
    if (!EpTerm(v))
        return 0;
    for (;;)
    {
        EpSkip();
        if (*g_ep == '+' || *g_ep == '-')
        {
            char op = *g_ep++;
            if (!EpTerm(&rhs))
                return 0;
            if (op == '+')
                *v += rhs;
            else
                *v -= rhs;
        }
        else
            return 1;
    }
}

static int ToolEvalExpr(const char *expr, double *out)
{
    double v = 0.0;
    if (expr == NULL || out == NULL)
        return 0;
    g_ep = expr;
    if (!EpExpr(&v))
        return 0;
    EpSkip();
    if (*g_ep != '\0')
        return 0;
    *out = v;
    return 1;
}

int ToolAnswerGoal(const ToolRequest *req, const ToolResult *res,
                   char *out, size_t size)
{
    char capS[CHAT_TOKEN_MAX];
    if (size > 0)
        out[0] = '\0';
    if (req == NULL || res == NULL || !res->ok ||
        res->tool != req->tool)
        return 0;
    switch (req->tool)
    {
    case TOOL_CALCULATOR:
        if (res->number[0] == '\0' || req->subject[0] == '\0')
            return 0;
        if (size > 0)
            snprintf(out, size, "Segun calculo, %s = %s.\n",
                     req->subject, res->number);
        return 1;
    case TOOL_LOOKUP_RELATION:
        if (res->nitems == 0 || req->subject[0] == '\0' ||
            req->relation[0] == '\0')
            return 0;
        ChatCapStr(req->subject, capS, sizeof(capS));
        if (size > 0)
        {
            size_t pos = 0;
            int wr = snprintf(out + pos, size - pos,
                              "Segun fuente externa, %s es %s de ",
                              capS, req->relation);
            if (wr > 0)
                pos += (size_t)wr;
            for (uint32_t i = 0; i < res->nitems && pos + 1 < size;
                 i++)
            {
                char cap[CHAT_TOKEN_MAX];
                ChatCapStr(res->items[i], cap, sizeof(cap));
                if (i > 0)
                {
                    if (pos + 2 >= size)
                        break;
                    out[pos++] = ',';
                    out[pos++] = ' ';
                }
                wr = snprintf(out + pos, size - pos, "%s", cap);
                if (wr <= 0)
                    break;
                pos += (size_t)wr;
            }
            if (pos + 2 < size)
            {
                out[pos++] = '.';
                out[pos++] = '\n';
            }
            out[pos < size ? pos : size - 1] = '\0';
        }
        return 1;
    case TOOL_LOOKUP_PERSON:
        if (res->nitems == 0 || req->subject[0] == '\0')
            return 0;
        ChatCapStr(req->subject, capS, sizeof(capS));
        if (size > 0)
            snprintf(out, size, "Segun fuente externa, %s: %s.\n", capS,
                     res->items[0]);
        return 1;
    case TOOL_FS_READ:
        if (req->subject[0] == '\0')
            return 0;
        if (size > 0)
            snprintf(out, size, "Segun archivo %s: %s.\n", req->subject,
                     res->text[0] != '\0' ? res->text : "(sin salida)");
        return 1;
    case TOOL_SHELL:
        if (req->subject[0] == '\0')
            return 0;
        if (size > 0)
            snprintf(out, size, "Segun shell %s: %s.\n", req->subject,
                     res->text[0] != '\0' ? res->text : "(sin salida)");
        return 1;
    default:
        return 0;
    }
}
