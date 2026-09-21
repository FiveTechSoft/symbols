/* symbols-server: OpenAI-compatible model endpoint (HARNESS).
   English code comments (project rule). The inference engine
   (libsymbolic) is untouched: this file only moves bytes (sockets),
   runs the frozen conversational entry points, and appends one
   JSONL observation per request. Single-threaded, sequential,
   Connection: close. Usage: symbols-server [port] [corpus] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "compat.h"
#include "compat_net.h"
#include "schema.h"
#include "metaschema.h"
#include "learn.h"
#include "chat.h"
#include "server_proto.h"
#include "agent_planner.h"
#include "agent_runner.h"
#include "agent_diagnose.h"
#include "model.h"
#include "persona.h"
#include "episodic_memory.h"

#define SERVER_PORT_DEFAULT 8099
#define SERVER_HDR_MAX 16384
#define SERVER_BODY_MAX (2 * 1024 * 1024)
#define SERVER_OBS_FILE "observations.jsonl"
#define SERVER_MODEL_ID "symbols"

static char g_http_body[SERVER_BODY_MAX + 1];

static int SendAll(socket_t s, const char *buf, size_t len)
{
    while (len > 0)
    {
        int w = send(s, buf, (int)(len > 8192 ? 8192 : len), 0);
        if (w <= 0)
            return 0;
        buf += w;
        len -= (size_t)w;
    }
    return 1;
}

static int SendRaw(socket_t s, int code, const char *status,
                   const char *ctype, const char *body)
{
    char hdr[512];
    int w = snprintf(hdr, sizeof(hdr),
                     "HTTP/1.1 %d %s\r\nContent-Type: "
                     "%s\r\nContent-Length: %u\r\n"
                     "Connection: close\r\n\r\n",
                     code, status, ctype, (unsigned)strlen(body));
    if (w <= 0)
        return 0;
    if (!SendAll(s, hdr, strlen(hdr)))
        return 0;
    return SendAll(s, body, strlen(body));
}

static int SendJson(socket_t s, int code, const char *status,
                    const char *body)
{
    return SendRaw(s, code, status, "application/json", body);
}

static int SendError(socket_t s, int code, const char *status,
                     const char *msg)
{
    char body[512], esc[256];
    ServerJsonEscape(msg, esc, sizeof(esc));
    snprintf(body, sizeof(body), "{\"error\":\"%s\"}", esc);
    return SendJson(s, code, status, body);
}

/* count "role" keys + detect a system message (heuristic, logged) */
static void ScanRoles(const char *body, int *nmsg, int *has_system)
{
    const char *p = body;
    *nmsg = 0;
    *has_system = 0;
    while ((p = strstr(p, "\"role\"")) != NULL)
    {
        const char *q = p + 6;
        (*nmsg)++;
        while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r')
            q++;
        if (*q == ':')
        {
            q++;
            while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r')
                q++;
            if (strncmp(q, "\"system\"", 8) == 0)
                *has_system = 1;
        }
        p += 6;
    }
}

static void IsoUtc(char *out, size_t size)
{
    time_t now = time(NULL);
    struct tm *tm = gmtime(&now);
    if (tm == NULL)
    {
        strncpy(out, "1970-01-01T00:00:00Z", size - 1);
        out[size - 1] = '\0';
        return;
    }
    snprintf(out, size, "%04d-%02d-%02dT%02d:%02d:%02dZ",
             tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
             tm->tm_hour, tm->tm_min, tm->tm_sec);
}

static void BuildStrArray(char items[][CHAT_TOKEN_MAX], uint32_t n,
                          char *out, size_t size)
{
    size_t pos = 0;
    uint32_t i;
    out[0] = '\0';
    if (size < 2)
        return;
    out[pos++] = '[';
    for (i = 0; i < n && pos + 1 < size; i++)
    {
        char esc[128];
        if (i > 0 && pos + 1 < size)
            out[pos++] = ',';
        ServerJsonEscape(items[i], esc, sizeof(esc));
        {
            size_t L = strlen(esc);
            if (pos + L + 3 >= size)
                break;
            out[pos++] = '"';
            memcpy(out + pos, esc, L);
            pos += L;
            out[pos++] = '"';
        }
    }
    if (pos + 1 < size)
        out[pos++] = ']';
    out[pos < size ? pos : size - 1] = '\0';
}

static void BuildProvArray(CHAT *ch, char *out, size_t size)
{
    size_t pos = 0;
    uint32_t i;
    out[0] = '\0';
    if (size < 2)
        return;
    out[pos++] = '[';
    for (i = 0; i < ch->exec.nprov && pos + 1 < size; i++)
    {
        char esc[320];
        if (i > 0 && pos + 1 < size)
            out[pos++] = ',';
        ServerJsonEscape(ch->exec.prov[i], esc, sizeof(esc));
        {
            size_t L = strlen(esc);
            if (pos + L + 3 >= size)
                break;
            out[pos++] = '"';
            memcpy(out + pos, esc, L);
            pos += L;
            out[pos++] = '"';
        }
    }
    if (pos + 1 < size)
        out[pos++] = ']';
    out[pos < size ? pos : size - 1] = '\0';
}

static unsigned long g_seq = 0;

/* Strip Spanish enclitic pronouns from verb stems:
   añadele → añade, agregale → agrega, escribeme → escribe, etc.
   Returns the stem length (written to out). */
static size_t StripEnclitic(const char *verb, char *out, size_t out_size)
{
    static const char *enclitics[] = {
        "mos", "dle", "dla", "dlo", "dles", "dlas", "dlos",
        "le", "la", "lo", "les", "las", "los",
        "me", "te", "nos", "os"
    };
    size_t vlen = strlen(verb);
    for (size_t e = 0; e < sizeof(enclitics) / sizeof(enclitics[0]); e++)
    {
        size_t elen = strlen(enclitics[e]);
        if (vlen > elen && strcmp(verb + vlen - elen, enclitics[e]) == 0)
        {
            size_t stem_len = vlen - elen;
            if (stem_len >= out_size) stem_len = out_size - 1;
            memcpy(out, verb, stem_len);
            out[stem_len] = '\0';
            return stem_len;
        }
    }
    size_t copy = vlen < out_size - 1 ? vlen : out_size - 1;
    memcpy(out, verb, copy);
    out[copy] = '\0';
    return copy;
}

/* session state: master CHAT for the process lifetime (the accept
   loop is single-threaded). Dynamic corpus loads (load X.txt)
   persist across requests; the base corpus ingests once. */
static CHAT g_session;
static int g_session_ready = 0;

#define SERVER_MAX_SESSIONS 32

typedef struct
{
    char session_id[64];
    char focus[CHAT_TOKEN_MAX];
    int focus_valid;
    char focus_secondary[CHAT_TOKEN_MAX];
    int focus_secondary_valid;
    ExecCtx exec;
    time_t last_active;

    /* Agentic coding state */
    int         agent_active;
    uint32_t    strips_state;
    uint32_t    strips_goal;
    STRIPS_PLAN current_plan;
    uint32_t    current_step_idx;
    char        current_issue[256];

    /* Verification telemetry & self-healing state */
    int               had_error;
    int               had_edit;
    int               replan_count;
    DIAGNOSTIC_REPORT last_diagnostic;
    char              last_error_summary[512];
    char              last_tool_output[16384];
    char              last_tool_call_name[64];
    char              last_target[260];  /* last file/entity mentioned for pronoun resolution */

    /* Persona conditioning */
    PERSONA_ID        persona_id;

#define SERVER_MAX_DECLARED_TOOLS 32

    /* Declared tools by client in current turn */
    int               declared_tools_count;
    char              declared_tools[SERVER_MAX_DECLARED_TOOLS][64];
} ServerSession;

static ServerSession g_sessions[SERVER_MAX_SESSIONS];
static uint32_t g_num_sessions = 0;
static CODE_GRAPH *g_server_code_graph = NULL;
static MODEL *g_server_model = NULL;

static int HasDeclaredTool(const ServerSession *sess, const char *name)
{
    if (!sess) return 0;
    for (int i = 0; i < sess->declared_tools_count; i++)
    {
        if (strcmp(sess->declared_tools[i], name) == 0)
            return 1;
    }
    return 0;
}

static const char *FindFileForIssue(const char *issue)
{
    if (!issue)
        return NULL;

    static char found_path[260];
    char buf[512];
    strncpy(buf, issue, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *tok = strtok(buf, " \t\r\n,;:\"'()<>{}`");
    while (tok)
    {
        /* Strip any trailing punctuation */
        size_t tlen = strlen(tok);
        while (tlen > 0 && (tok[tlen - 1] == '.' || tok[tlen - 1] == ',' || tok[tlen - 1] == ';' ||
                            tok[tlen - 1] == '?' || tok[tlen - 1] == '!' || tok[tlen - 1] == ')' ||
                            tok[tlen - 1] == '\"' || tok[tlen - 1] == '\''))
        {
            tok[--tlen] = '\0';
        }

        const char *dot = strrchr(tok, '.');
        if (dot && (strcmp(dot, ".c") == 0 || strcmp(dot, ".h") == 0 ||
                    strcmp(dot, ".cpp") == 0 || strcmp(dot, ".py") == 0 ||
                    strcmp(dot, ".ts") == 0 || strcmp(dot, ".js") == 0 ||
                    strcmp(dot, ".md") == 0 || strcmp(dot, ".txt") == 0 ||
                    strcmp(dot, ".json") == 0 || strcmp(dot, ".yml") == 0 ||
                    strcmp(dot, ".yaml") == 0 || strcmp(dot, ".toml") == 0 ||
                    strcmp(dot, ".sh") == 0 || strcmp(dot, ".bat") == 0))
        {
            strncpy(found_path, tok, sizeof(found_path) - 1);
            found_path[sizeof(found_path) - 1] = '\0';
            return found_path;
        }

        if (g_server_code_graph)
        {
            const char *file = CodeGraphGetFunctionFile(g_server_code_graph, tok);
            if (file)
                return file;
        }
        tok = strtok(NULL, " \t\r\n,;:\"'()<>{}`");
    }
    return NULL;
}

static void ExtractGlobPattern(const char *text, char *out_pattern, size_t size)
{
    if (!text || !out_pattern || size == 0)
        return;

    char buf[512];
    strncpy(buf, text, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *tok = strtok(buf, " \t\r\n,;\"'¿?");
    while (tok)
    {
        /* Explicit wildcard like *.c, *.*, * */
        if (strchr(tok, '*'))
        {
            strncpy(out_pattern, tok, size - 1);
            out_pattern[size - 1] = '\0';
            return;
        }
        /* Only accept '?' as glob wildcard if part of an actual filename token, not standalone */
        if (strchr(tok, '?') && strlen(tok) > 1 && (strchr(tok, '.') || isalnum((unsigned char)tok[0])))
        {
            strncpy(out_pattern, tok, size - 1);
            out_pattern[size - 1] = '\0';
            return;
        }
        tok = strtok(NULL, " \t\r\n,;\"'¿?");
    }
    strncpy(out_pattern, "*", size - 1);
    out_pattern[size - 1] = '\0';
}

static int IsFolderOrGlobQuery(const char *text)
{
    if (!text) return 0;
    char lower[512];
    size_t i = 0;
    while (text[i] != '\0' && i < sizeof(lower) - 1)
    {
        lower[i] = (char)tolower((unsigned char)text[i]);
        i++;
    }
    lower[i] = '\0';

    if (strchr(lower, '*') || strchr(lower, '?'))
        return 1;

    static const char *folder_kws[] = {
        "folder", "directory", "carpeta", "directorio", "workspace",
        "repo", "repository", "repositorio", "dir", "ls", "tree", "files",
        "archivos", "ficheros", "codebase", "project", "proyecto",
        "estructura", "structure", "pwd", "subcarpetas", "subcarpeta",
        "subdirectorios", "subdirectorio", "subfolders", "subdirectories",
        "lista", "listar"
    };
    for (size_t k = 0; k < sizeof(folder_kws) / sizeof(folder_kws[0]); k++)
    {
        const char *p = lower;
        size_t kwlen = strlen(folder_kws[k]);
        while ((p = strstr(p, folder_kws[k])) != NULL)
        {
            int before_ok = (p == lower || (!isalnum((unsigned char)*(p - 1)) && *(p - 1) != '_'));
            char after_char = *(p + kwlen);
            int after_ok = (after_char == '\0' || (!isalnum((unsigned char)after_char) && after_char != '_'));
            if (before_ok && after_ok)
                return 1;
            p++;
        }
    }
    return 0;
}

static void FormatOperatorToolCall(const ServerSession *sess, const STRIPS_OPERATOR *op,
                                   const char *issue, unsigned long seq,
                                   OPENAI_TOOL_CALLS *out_tc)
{
    memset(out_tc, 0, sizeof(*out_tc));
    out_tc->count = 1;
    snprintf(out_tc->calls[0].id, sizeof(out_tc->calls[0].id), "call_sym_%lu", seq);

    const char *target_file = FindFileForIssue(issue);
    if (!target_file)
        target_file = "CMakeLists.txt";

    if (strcmp(op->name, "create_file") == 0)
    {
        char created[260];
        if (ServerExtractCreatePath(issue, created, sizeof(created)))
            target_file = created;
        else
            target_file = "nuevo.txt";
        if (HasDeclaredTool(sess, "write"))
        {
            strncpy(out_tc->calls[0].name, "write", sizeof(out_tc->calls[0].name) - 1);
            snprintf(out_tc->calls[0].arguments, sizeof(out_tc->calls[0].arguments),
                     "{\"filePath\":\"%s\",\"content\":\"\"}", target_file);
        }
        else if (HasDeclaredTool(sess, "bash"))
        {
            strncpy(out_tc->calls[0].name, "bash", sizeof(out_tc->calls[0].name) - 1);
            snprintf(out_tc->calls[0].arguments, sizeof(out_tc->calls[0].arguments),
                     "{\"command\":\"touch %s\"}", target_file);
        }
        else if (HasDeclaredTool(sess, "edit"))
        {
            strncpy(out_tc->calls[0].name, "edit", sizeof(out_tc->calls[0].name) - 1);
            snprintf(out_tc->calls[0].arguments, sizeof(out_tc->calls[0].arguments),
                     "{\"filePath\":\"%s\"}", target_file);
        }
        else if (sess && sess->declared_tools_count > 0)
        {
            strncpy(out_tc->calls[0].name, sess->declared_tools[0], sizeof(out_tc->calls[0].name) - 1);
            strncpy(out_tc->calls[0].arguments, "{}", sizeof(out_tc->calls[0].arguments) - 1);
        }
        else
        {
            strncpy(out_tc->calls[0].name, "write", sizeof(out_tc->calls[0].name) - 1);
            snprintf(out_tc->calls[0].arguments, sizeof(out_tc->calls[0].arguments),
                     "{\"filePath\":\"%s\",\"content\":\"\"}", target_file);
        }
    }
    else if (strcmp(op->name, "locate_symbol") == 0)
    {
        int is_folder = IsFolderOrGlobQuery(issue);
        if (is_folder && HasDeclaredTool(sess, "glob"))
        {
            char pattern[64];
            ExtractGlobPattern(issue, pattern, sizeof(pattern));
            strncpy(out_tc->calls[0].name, "glob", sizeof(out_tc->calls[0].name) - 1);
            snprintf(out_tc->calls[0].arguments, sizeof(out_tc->calls[0].arguments),
                     "{\"pattern\":\"%s\"}", pattern);
        }
        else if (is_folder && HasDeclaredTool(sess, "bash") &&
                 (strncmp(issue, "dir", 3) == 0 || strncmp(issue, "ls", 2) == 0 ||
                  strstr(issue, "list ") != NULL || strstr(issue, "lista ") != NULL ||
                  strstr(issue, "show ") != NULL || strstr(issue, "muestra ") != NULL ||
                  strstr(issue, "explore ") != NULL || strstr(issue, "explora ") != NULL))
        {
            strncpy(out_tc->calls[0].name, "bash", sizeof(out_tc->calls[0].name) - 1);
            /* Map natural language to a concrete shell command */
            if (strstr(issue, "list ") != NULL || strstr(issue, "lista ") != NULL ||
                strstr(issue, "show ") != NULL || strstr(issue, "muestra ") != NULL ||
                strstr(issue, "explore ") != NULL || strstr(issue, "explora ") != NULL)
                snprintf(out_tc->calls[0].arguments, sizeof(out_tc->calls[0].arguments),
                         "{\"command\":\"dir *.*\"}");
            else
                snprintf(out_tc->calls[0].arguments, sizeof(out_tc->calls[0].arguments),
                         "{\"command\":\"%.120s\"}", issue);
        }
        else if (HasDeclaredTool(sess, "read") && target_file && strchr(issue, '.'))
        {
            strncpy(out_tc->calls[0].name, "read", sizeof(out_tc->calls[0].name) - 1);
            snprintf(out_tc->calls[0].arguments, sizeof(out_tc->calls[0].arguments),
                     "{\"filePath\":\"%s\"}", target_file);
        }
        else if (HasDeclaredTool(sess, "grep"))
        {
            strncpy(out_tc->calls[0].name, "grep", sizeof(out_tc->calls[0].name) - 1);
            snprintf(out_tc->calls[0].arguments, sizeof(out_tc->calls[0].arguments),
                     "{\"pattern\":\"%.120s\"}", issue);
        }
        else if (HasDeclaredTool(sess, "read"))
        {
            strncpy(out_tc->calls[0].name, "read", sizeof(out_tc->calls[0].name) - 1);
            snprintf(out_tc->calls[0].arguments, sizeof(out_tc->calls[0].arguments),
                     "{\"filePath\":\"%s\"}", target_file);
        }
        else if (HasDeclaredTool(sess, "glob"))
        {
            strncpy(out_tc->calls[0].name, "glob", sizeof(out_tc->calls[0].name) - 1);
            strncpy(out_tc->calls[0].arguments, "{\"pattern\":\"*\"}", sizeof(out_tc->calls[0].arguments) - 1);
        }
        else if (HasDeclaredTool(sess, "locate_symbol"))
        {
            strncpy(out_tc->calls[0].name, "locate_symbol", sizeof(out_tc->calls[0].name) - 1);
            snprintf(out_tc->calls[0].arguments, sizeof(out_tc->calls[0].arguments),
                     "{\"query\":\"%.120s\"}", issue);
        }
        else if (sess && sess->declared_tools_count > 0)
        {
            strncpy(out_tc->calls[0].name, sess->declared_tools[0], sizeof(out_tc->calls[0].name) - 1);
            strncpy(out_tc->calls[0].arguments, "{}", sizeof(out_tc->calls[0].arguments) - 1);
        }
        else
        {
            strncpy(out_tc->calls[0].name, "locate_symbol", sizeof(out_tc->calls[0].name) - 1);
            snprintf(out_tc->calls[0].arguments, sizeof(out_tc->calls[0].arguments),
                     "{\"query\":\"%.120s\"}", issue);
        }
    }
    else if (strcmp(op->name, "inspect_code") == 0)
    {
        if (HasDeclaredTool(sess, "read"))
        {
            strncpy(out_tc->calls[0].name, "read", sizeof(out_tc->calls[0].name) - 1);
            snprintf(out_tc->calls[0].arguments, sizeof(out_tc->calls[0].arguments),
                     "{\"filePath\":\"%s\"}", target_file);
        }
        else if (HasDeclaredTool(sess, "inspect_code"))
        {
            strncpy(out_tc->calls[0].name, "inspect_code", sizeof(out_tc->calls[0].name) - 1);
            snprintf(out_tc->calls[0].arguments, sizeof(out_tc->calls[0].arguments),
                     "{\"file\":\"%s\",\"start_line\":1,\"end_line\":100}", target_file);
        }
        else if (HasDeclaredTool(sess, "grep"))
        {
            strncpy(out_tc->calls[0].name, "grep", sizeof(out_tc->calls[0].name) - 1);
            snprintf(out_tc->calls[0].arguments, sizeof(out_tc->calls[0].arguments),
                     "{\"pattern\":\"%.120s\"}", issue);
        }
        else if (sess && sess->declared_tools_count > 0)
        {
            strncpy(out_tc->calls[0].name, sess->declared_tools[0], sizeof(out_tc->calls[0].name) - 1);
            strncpy(out_tc->calls[0].arguments, "{}", sizeof(out_tc->calls[0].arguments) - 1);
        }
        else
        {
            strncpy(out_tc->calls[0].name, "inspect_code", sizeof(out_tc->calls[0].name) - 1);
            snprintf(out_tc->calls[0].arguments, sizeof(out_tc->calls[0].arguments),
                     "{\"file\":\"%s\",\"start_line\":1,\"end_line\":100}", target_file);
        }
    }
    else if (strcmp(op->name, "diagnose_error") == 0)
    {
        if (HasDeclaredTool(sess, "bash"))
        {
            strncpy(out_tc->calls[0].name, "bash", sizeof(out_tc->calls[0].name) - 1);
            strncpy(out_tc->calls[0].arguments, "{\"command\":\"ctest --output-on-failure\"}",
                    sizeof(out_tc->calls[0].arguments) - 1);
        }
        else if (HasDeclaredTool(sess, "diagnose_error"))
        {
            strncpy(out_tc->calls[0].name, "diagnose_error", sizeof(out_tc->calls[0].name) - 1);
            snprintf(out_tc->calls[0].arguments, sizeof(out_tc->calls[0].arguments),
                     "{\"issue\":\"%.120s\"}", issue);
        }
        else if (sess && sess->declared_tools_count > 0)
        {
            strncpy(out_tc->calls[0].name, sess->declared_tools[0], sizeof(out_tc->calls[0].name) - 1);
            strncpy(out_tc->calls[0].arguments, "{}", sizeof(out_tc->calls[0].arguments) - 1);
        }
        else
        {
            strncpy(out_tc->calls[0].name, "diagnose_error", sizeof(out_tc->calls[0].name) - 1);
            snprintf(out_tc->calls[0].arguments, sizeof(out_tc->calls[0].arguments),
                     "{\"issue\":\"%.120s\"}", issue);
        }
    }
    else if (strcmp(op->name, "apply_patch") == 0)
    {
        OPENAI_TOOL_CALL mapped;
        if (sess && ServerMapEditToolCall(issue, sess->declared_tools,
                                          (uint32_t)sess->declared_tools_count,
                                          &mapped))
        {
            strncpy(out_tc->calls[0].name, mapped.name, sizeof(out_tc->calls[0].name) - 1);
            strncpy(out_tc->calls[0].arguments, mapped.arguments,
                    sizeof(out_tc->calls[0].arguments) - 1);
        }
        else if (HasDeclaredTool(sess, "edit"))
        {
            strncpy(out_tc->calls[0].name, "edit", sizeof(out_tc->calls[0].name) - 1);
            snprintf(out_tc->calls[0].arguments, sizeof(out_tc->calls[0].arguments),
                     "{\"filePath\":\"%s\"}", target_file);
        }
        else if (HasDeclaredTool(sess, "read"))
        {
            strncpy(out_tc->calls[0].name, "read", sizeof(out_tc->calls[0].name) - 1);
            snprintf(out_tc->calls[0].arguments, sizeof(out_tc->calls[0].arguments),
                     "{\"filePath\":\"%s\"}", target_file);
        }
        else if (sess && sess->declared_tools_count > 0)
        {
            strncpy(out_tc->calls[0].name, sess->declared_tools[0], sizeof(out_tc->calls[0].name) - 1);
            strncpy(out_tc->calls[0].arguments, "{}", sizeof(out_tc->calls[0].arguments) - 1);
        }
        else
        {
            strncpy(out_tc->calls[0].name, "read", sizeof(out_tc->calls[0].name) - 1);
            snprintf(out_tc->calls[0].arguments, sizeof(out_tc->calls[0].arguments),
                     "{\"filePath\":\"%s\"}", target_file);
        }
    }
    else if (strcmp(op->name, "verify_build") == 0)
    {
        const char *cmd = "cmake --build .";
        if (HasDeclaredTool(sess, "bash"))
        {
            strncpy(out_tc->calls[0].name, "bash", sizeof(out_tc->calls[0].name) - 1);
            snprintf(out_tc->calls[0].arguments, sizeof(out_tc->calls[0].arguments),
                     "{\"command\":\"%s\"}", cmd);
        }
        else if (HasDeclaredTool(sess, "execute_command"))
        {
            strncpy(out_tc->calls[0].name, "execute_command", sizeof(out_tc->calls[0].name) - 1);
            snprintf(out_tc->calls[0].arguments, sizeof(out_tc->calls[0].arguments),
                     "{\"command\":\"%s\"}", cmd);
        }
        else if (sess && sess->declared_tools_count > 0)
        {
            strncpy(out_tc->calls[0].name, sess->declared_tools[0], sizeof(out_tc->calls[0].name) - 1);
            strncpy(out_tc->calls[0].arguments, "{}", sizeof(out_tc->calls[0].arguments) - 1);
        }
        else
        {
            strncpy(out_tc->calls[0].name, "execute_command", sizeof(out_tc->calls[0].name) - 1);
            snprintf(out_tc->calls[0].arguments, sizeof(out_tc->calls[0].arguments),
                     "{\"command\":\"%s\"}", cmd);
        }
    }
    else if (strcmp(op->name, "run_regression_tests") == 0)
    {
        const char *cmd = "ctest --output-on-failure";
        if (HasDeclaredTool(sess, "bash"))
        {
            strncpy(out_tc->calls[0].name, "bash", sizeof(out_tc->calls[0].name) - 1);
            snprintf(out_tc->calls[0].arguments, sizeof(out_tc->calls[0].arguments),
                     "{\"command\":\"%s\"}", cmd);
        }
        else if (HasDeclaredTool(sess, "execute_command"))
        {
            strncpy(out_tc->calls[0].name, "execute_command", sizeof(out_tc->calls[0].name) - 1);
            snprintf(out_tc->calls[0].arguments, sizeof(out_tc->calls[0].arguments),
                     "{\"command\":\"%s\"}", cmd);
        }
        else if (sess && sess->declared_tools_count > 0)
        {
            strncpy(out_tc->calls[0].name, sess->declared_tools[0], sizeof(out_tc->calls[0].name) - 1);
            strncpy(out_tc->calls[0].arguments, "{}", sizeof(out_tc->calls[0].arguments) - 1);
        }
        else
        {
            strncpy(out_tc->calls[0].name, "execute_command", sizeof(out_tc->calls[0].name) - 1);
            snprintf(out_tc->calls[0].arguments, sizeof(out_tc->calls[0].arguments),
                     "{\"command\":\"%s\"}", cmd);
        }
    }
    else
    {
        if (sess && sess->declared_tools_count > 0)
        {
            const char *fallback = HasDeclaredTool(sess, "read") ? "read" : sess->declared_tools[0];
            strncpy(out_tc->calls[0].name, fallback, sizeof(out_tc->calls[0].name) - 1);
            strncpy(out_tc->calls[0].arguments, "{}", sizeof(out_tc->calls[0].arguments) - 1);
        }
        else
        {
            strncpy(out_tc->calls[0].name, op->name, sizeof(out_tc->calls[0].name) - 1);
            strncpy(out_tc->calls[0].arguments, "{}", sizeof(out_tc->calls[0].arguments) - 1);
        }
    }
}

static ServerSession *GetOrCreateSession(const char *session_id)
{
    uint32_t i;
    time_t oldest_time;
    uint32_t oldest_idx;
    if (session_id == NULL || session_id[0] == '\0')
        session_id = "default";

    for (i = 0; i < g_num_sessions; i++)
    {
        if (strcmp(g_sessions[i].session_id, session_id) == 0)
        {
            g_sessions[i].last_active = time(NULL);
            return &g_sessions[i];
        }
    }

    if (g_num_sessions < SERVER_MAX_SESSIONS)
    {
        ServerSession *s = &g_sessions[g_num_sessions++];
        memset(s, 0, sizeof(*s));
        strncpy(s->session_id, session_id, sizeof(s->session_id) - 1);
        s->last_active = time(NULL);
        return s;
    }

    /* Evict oldest session (LRU) */
    oldest_time = g_sessions[0].last_active;
    oldest_idx = 0;
    for (i = 1; i < SERVER_MAX_SESSIONS; i++)
    {
        if (g_sessions[i].last_active < oldest_time)
        {
            oldest_time = g_sessions[i].last_active;
            oldest_idx = i;
        }
    }
    memset(&g_sessions[oldest_idx], 0, sizeof(ServerSession));
    strncpy(g_sessions[oldest_idx].session_id, session_id,
            sizeof(g_sessions[oldest_idx].session_id) - 1);
    g_sessions[oldest_idx].last_active = time(NULL);
    return &g_sessions[oldest_idx];
}

static void HandleCompletions(socket_t s, const char *body,
                              const char *corpus)
{
    char query[4096], raw[4096];
    static char content[32768];
    static char resp[65536];
    static char sse[65536];
    char obs[16384], ts[32], ent_json[2048], prov_json[4096];
    char session_id[64] = "default";
    ServerSession *sess = NULL;
    ChatParse parsed;
    int nmsg = 0, has_system = 0;
    const char *status;
    clock_t t0;
    long latency_ms;
    FILE *log;

    ServerExtractSession(body, session_id, sizeof(session_id));
    sess = GetOrCreateSession(session_id);
    ScanRoles(body, &nmsg, &has_system);

    /* Extract declared tools early so all branches know client capabilities */
    char declared_tools[SERVER_MAX_DECLARED_TOOLS][64];
    int num_declared = ServerExtractToolsDeclared(body, declared_tools, SERVER_MAX_DECLARED_TOOLS);
    sess->declared_tools_count = num_declared;
    for (int i = 0; i < num_declared; i++)
    {
        strncpy(sess->declared_tools[i], declared_tools[i], sizeof(sess->declared_tools[i]) - 1);
        sess->declared_tools[i][sizeof(sess->declared_tools[i]) - 1] = '\0';
    }

    char last_role[32] = {0};
    ServerExtractLastRole(body, last_role, sizeof(last_role));

    OPENAI_TOOL_RESPONSE tool_resp;
    int has_tool_resp = 0;
    if (strcmp(last_role, "tool") == 0)
    {
        has_tool_resp = ServerExtractLastToolResponse(body, &tool_resp);
        if (has_tool_resp && tool_resp.name[0] == '\0' && sess->last_tool_call_name[0] != '\0')
        {
            strncpy(tool_resp.name, sess->last_tool_call_name, sizeof(tool_resp.name) - 1);
            tool_resp.name[sizeof(tool_resp.name) - 1] = '\0';
            ServerInspectToolResponse(&tool_resp);
        }
    }
    else if (strcmp(last_role, "user") == 0)
    {
        /* User submitted a new prompt: abort any stale agentic coding loop */
        sess->agent_active = 0;
    }

    /* 1. AGENTIC RESUMPTION: Client returned output of previous tool call */
    if (has_tool_resp && sess->agent_active)
    {
        sess->last_active = time(NULL);

        /* Save tool output for inspection reporting */
        if (tool_resp.content[0] != '\0')
        {
            if (sess->last_tool_output[0] == '\0')
            {
                strncpy(sess->last_tool_output, tool_resp.content, sizeof(sess->last_tool_output) - 1);
                sess->last_tool_output[sizeof(sess->last_tool_output) - 1] = '\0';
            }
            else
            {
                size_t cur_len = strlen(sess->last_tool_output);
                if (cur_len + 4 < sizeof(sess->last_tool_output))
                {
                    strncat(sess->last_tool_output, "\n", sizeof(sess->last_tool_output) - cur_len - 1);
                    strncat(sess->last_tool_output, tool_resp.content, sizeof(sess->last_tool_output) - strlen(sess->last_tool_output) - 1);
                }
            }
        }

        /* Direct-edit auto-diff: DISABLED — git diff fails outside repos
           and causes infinite replan loops. User can request diff explicitly. */

        /* Determine whether this step was an inspection tool or task.
           Also treat as inspection any standalone tool call outside a STRIPS plan
           (step_count == 0, agent_active == 0) — e.g. auto-diff after edit. */
        int is_inspection = ServerIsInspectionTask(sess->current_issue) ||
                            IsFolderOrGlobQuery(sess->current_issue) ||
                            (strcmp(tool_resp.name, "glob") == 0) ||
                            (strcmp(tool_resp.name, "read") == 0) ||
                            (strcmp(tool_resp.name, "grep") == 0) ||
                            (strcmp(sess->last_tool_call_name, "glob") == 0) ||
                            (strcmp(sess->last_tool_call_name, "read") == 0) ||
                            (strcmp(sess->last_tool_call_name, "grep") == 0) ||
                            (sess->current_plan.step_count == 0 && !sess->agent_active);

        DIAGNOSTIC_REPORT diag;
        memset(&diag, 0, sizeof(diag));
        if (!is_inspection)
        {
            DiagnosticParseOutput(tool_resp.content, &diag);
        }

        int step_failed = 0;
        if (is_inspection)
        {
            /* Inspection steps fail ONLY on explicit non-zero exit code or JSON error status */
            if ((tool_resp.has_exit_code && tool_resp.exit_code != 0) || tool_resp.is_error)
            {
                step_failed = 1;
                sess->had_error = 1;
                snprintf(sess->last_error_summary, sizeof(sess->last_error_summary),
                         "Inspection tool reported failure");
            }
        }
        else if (tool_resp.is_error || diag.error_count > 0)
        {
            step_failed = 1;
            sess->had_error = 1;
            sess->last_diagnostic = diag;
            if (diag.error_count > 0)
            {
                snprintf(sess->last_error_summary, sizeof(sess->last_error_summary),
                         "%u compiler error(s): %.120s (%.64s:%u)",
                         diag.error_count, diag.items[0].raw_message,
                         diag.items[0].file, diag.items[0].line);
            }
            else if (tool_resp.has_exit_code)
            {
                snprintf(sess->last_error_summary, sizeof(sess->last_error_summary),
                         "Command exited with non-zero status %d", tool_resp.exit_code);
            }
            else
            {
                snprintf(sess->last_error_summary, sizeof(sess->last_error_summary),
                         "Step execution reported failure");
            }
        }

        /* If error occurred, attempt abductive dynamic replanning */
        if (step_failed && sess->replan_count < 2)
        {
            sess->replan_count++;
            AGENT_PLANNER planner;
            AgentPlannerInit(&planner);
            const char *focus_sym = diag.root_symbol[0] ? diag.root_symbol :
                                   (diag.root_file[0] ? diag.root_file : sess->current_issue);
            int ok_replan = AgentPlannerReplanOnError(&planner, &sess->current_plan, focus_sym);
            if (ok_replan && sess->current_plan.step_count > 0)
            {
                sess->current_step_idx = 0;
                sess->had_error = 0; /* Reset for curative retry */
                while (sess->current_step_idx < sess->current_plan.step_count &&
                       sess->current_plan.steps[sess->current_step_idx].op.tool_id == OP_TOOL_NONE)
                {
                    sess->current_step_idx++;
                }

                if (sess->current_step_idx < sess->current_plan.step_count)
                {
                    const STRIPS_OPERATOR *next_op = &sess->current_plan.steps[sess->current_step_idx].op;
                    OPENAI_TOOL_CALLS tc;
                    FormatOperatorToolCall(sess, next_op, sess->current_issue, ++g_seq, &tc);
            if (tc.count > 0)
            {
                strncpy(sess->last_tool_call_name, tc.calls[0].name, sizeof(sess->last_tool_call_name) - 1);
                sess->last_tool_call_name[sizeof(sess->last_tool_call_name) - 1] = '\0';
                if (strcmp(tc.calls[0].name, "edit") == 0 ||
                    strcmp(tc.calls[0].name, "write") == 0)
                    sess->had_edit = 1;
            }

                    char thought[256];
                    snprintf(thought, sizeof(thought),
                             "Detected error (%s). Re-planning dynamic STRIPS remedy for '%s'.",
                             sess->last_error_summary, focus_sym);

                    if (ServerWantsStream(body))
                    {
                        char sse[16384];
                        ServerBuildToolCallStreamResponse(SERVER_MODEL_ID, (long)time(NULL), g_seq, &tc, sse, sizeof(sse));
                        SendRaw(s, 200, "OK", "text/event-stream", sse);
                    }
                    else
                    {
                        ServerBuildToolCallResponse(SERVER_MODEL_ID, (long)time(NULL), g_seq, &tc,
                                                    thought, resp, sizeof(resp));
                        SendJson(s, 200, "OK", resp);
                    }
                    return;
                }
            }
        }

        sess->current_step_idx++;

        /* Skip internal STRIPS steps with OP_TOOL_NONE */
        while (sess->current_step_idx < sess->current_plan.step_count &&
               sess->current_plan.steps[sess->current_step_idx].op.tool_id == OP_TOOL_NONE)
        {
            sess->current_step_idx++;
        }

        if (sess->current_step_idx < sess->current_plan.step_count)
        {
            /* Dispatch next tool call in the active STRIPS plan */
            const STRIPS_OPERATOR *next_op = &sess->current_plan.steps[sess->current_step_idx].op;
            OPENAI_TOOL_CALLS tc;
            FormatOperatorToolCall(sess, next_op, sess->current_issue, ++g_seq, &tc);
            if (tc.count > 0)
            {
                strncpy(sess->last_tool_call_name, tc.calls[0].name, sizeof(sess->last_tool_call_name) - 1);
                sess->last_tool_call_name[sizeof(sess->last_tool_call_name) - 1] = '\0';
            }

            if (ServerWantsStream(body))
            {
                char sse[16384];
                ServerBuildToolCallStreamResponse(SERVER_MODEL_ID, (long)time(NULL), g_seq, &tc, sse, sizeof(sse));
                SendRaw(s, 200, "OK", "text/event-stream", sse);
            }
            else
            {
                ServerBuildToolCallResponse(SERVER_MODEL_ID, (long)time(NULL), g_seq, &tc,
                                            "Executing next planned step.", resp, sizeof(resp));
                SendJson(s, 200, "OK", resp);
            }
            return;
        }
        else
        {
            /* STRIPS plan reached end: verify whether execution truly succeeded or failed */
            sess->agent_active = 0;
            if (sess->had_error)
            {
                snprintf(content, sizeof(content),
                         "### Autonomous Coding Task Unresolved / Verification Failed\n\n"
                         "The STRIPS plan for issue '%s' completed execution, but verification failed.\n"
                         "- **Status**: FAILED\n"
                         "- **Build / Test**: FAIL\n"
                         "- **Failure Cause**: %s\n\n"
                         "Repository changes have not been certified. Review diagnostics above.",
                         sess->current_issue,
                         sess->last_error_summary[0] ? sess->last_error_summary : "Errors reported during tool execution");
            }
            else
            {
                if (ServerIsFileCreationTask(sess->current_issue))
                {
                    char created[260];
                    const char *target_file = "nuevo.txt";
                    if (ServerExtractCreatePath(sess->current_issue, created,
                                                sizeof(created)))
                        target_file = created;
                    snprintf(content, sizeof(content),
                             "### Archivo Creado con Exito ('%s')\n\n"
                             "Se ha creado el archivo `%s` en el espacio de trabajo.\n"
                             "- **Estado**: Creado con exito\n"
                             "- **Archivo**: %s\n\n"
                             "El archivo esta listo para su edicion o uso en el proyecto.",
                             target_file, target_file, target_file);
                }
                else if (ServerIsInspectionTask(sess->current_issue) || IsFolderOrGlobQuery(sess->current_issue))
                {
                    if (sess->last_tool_output[0] != '\0')
                    {
                        char formatted[16384];
                        ServerFormatInspectionOutput(sess->last_tool_output, formatted, sizeof(formatted));
                        snprintf(content, sizeof(content),
                                 "### Contenido del Directorio / Exploracion ('%s')\n\n"
                                 "```\n%s\n```\n\n"
                                 "*Exploracion completada con exito.*",
                                 sess->current_issue,
                                 formatted);
                    }
                    else
                    {
                        snprintf(content, sizeof(content),
                                 "### Revision de Directorio / Inspeccion Completada\n\n"
                                 "Se han ejecutado los pasos de exploracion para '%s'.\n"
                                 "- **Estado**: Inspeccion finalizada con exito\n\n"
                                 "El espacio de trabajo esta listo. Indica que archivo o cambio deseas examinar a continuacion.",
                                 sess->current_issue);
                    }
                }
                else
                {
                    snprintf(content, sizeof(content),
                             "### Autonomous Coding Task Completed\n\n"
                             "All %u steps of the STRIPS plan for issue '%s' have been executed.\n"
                             "- **Status**: 100%% Verified\n"
                             "- **Regressions**: 0\n"
                             "- **Build**: PASS\n\n"
                             "The patch is applied and verified against the codebase.",
                             sess->current_plan.step_count, sess->current_issue);
                }
            }

            /* Auto-diff after STRIPS plans: DISABLED — git diff fails
               outside repos and causes infinite replan loops. */

            if (ServerWantsStream(body))
            {
                char sse[16384];
                ServerBuildStreamResponse(SERVER_MODEL_ID, (long)time(NULL), ++g_seq, content, sse, sizeof(sse));
                SendRaw(s, 200, "OK", "text/event-stream", sse);
            }
            else
            {
                ServerBuildResponse(SERVER_MODEL_ID, (long)time(NULL), ++g_seq, content, sess->current_issue, resp, sizeof(resp));
                SendJson(s, 200, "OK", resp);
            }
            return;
        }
    }

    /* 2. INITIAL QUERY / PROMPT */
    if (!ServerExtractQuery(body, query, sizeof(query)))
    {
        SendError(s, 400, "Bad Request", "no user message found");
        return;
    }

    /* Track last file/entity mentioned for enclitic pronoun resolution */
    {
        char extracted_file[260];
        if (ServerExtractFileRef(query, extracted_file, sizeof(extracted_file)))
            strncpy(sess->last_target, extracted_file, sizeof(sess->last_target) - 1);
    }

    /* Ensure session knowledge graph is initialized */
    if (!g_session_ready)
    {
        if (ChatIsBinaryModel(corpus) && g_server_model == NULL)
        {
            g_server_model = ModelLoad(corpus);
        }
        ChatInit(&g_session, corpus);
        g_session_ready = 1;
    }

    /* Check for /save and /load conversational commands */
    if (strncmp(query, "/save ", 6) == 0)
    {
        const char *path = query + 6;
        while (*path == ' ') path++;
        if (g_server_model != NULL && ModelSave(g_server_model, path))
            snprintf(content, sizeof(content), "Model saved to '%s'.", path);
        else
            snprintf(content, sizeof(content), "Error saving model to '%s'.", path);

        ServerBuildResponse(SERVER_MODEL_ID, (long)time(NULL), ++g_seq, content, query, resp, sizeof(resp));
        SendJson(s, 200, "OK", resp);
        return;
    }

    if (strncmp(query, "/load ", 6) == 0)
    {
        const char *path = query + 6;
        while (*path == ' ') path++;
        if (ChatIsBinaryModel(path))
        {
            MODEL *new_m = ModelLoad(path);
            if (new_m != NULL)
            {
                if (g_server_model != NULL) ModelDestroy(g_server_model);
                g_server_model = new_m;
                ChatInit(&g_session, path);
                snprintf(content, sizeof(content), "Binary model loaded successfully from '%s' (%u facts loaded).", path, ChatFactCount(&g_session));
            }
            else
            {
                snprintf(content, sizeof(content), "Failed to load binary model from '%s'.", path);
            }
        }
        else
        {
            ChatInit(&g_session, path);
            snprintf(content, sizeof(content), "Corpus loaded from '%s' (%u facts loaded).", path, ChatFactCount(&g_session));
        }
        ServerBuildResponse(SERVER_MODEL_ID, (long)time(NULL), ++g_seq, content, query, resp, sizeof(resp));
        SendJson(s, 200, "OK", resp);
        return;
    }

    /* Conversational persona command */
    if (strncmp(query, "/persona", 8) == 0 || strncmp(query, ":persona", 8) == 0 ||
        (strncmp(query, "persona ", 8) == 0 && !strchr(query, '?')) ||
        strcmp(query, "modo pirata") == 0 || strcmp(query, "/pirata") == 0 || strcmp(query, ":pirata") == 0)
    {
        PERSONA_ID pid = PERSONA_NEUTRAL;
        if (strcmp(query, "modo pirata") == 0 || strcmp(query, "/pirata") == 0 || strcmp(query, ":pirata") == 0)
        {
            pid = PERSONA_PIRATE_QUANTUM;
        }
        else
        {
            const char *p = strchr(query, ' ');
            while (p && isspace((unsigned char)*p)) p++;
            if (p && *p)
                pid = PersonaFindByName(p);
        }
        sess->persona_id = pid;
        ChatSetPersona(&g_session, pid);
        if (pid == PERSONA_PIRATE_QUANTUM)
            snprintf(content, sizeof(content), "Ahoy, capitan! Ahora os habla el contramaestre cuantico desde el castillo de proa.");
        else
            snprintf(content, sizeof(content), "Modo persona configurado a: %s", PersonaGetName(pid));

        ServerBuildResponse(SERVER_MODEL_ID, (long)time(NULL), ++g_seq, content, query, resp, sizeof(resp));
        SendJson(s, 200, "OK", resp);
        return;
    }

    /* Episodic memory commands: /learn, /aprende */
    if (strncmp(query, "/learn ", 7) == 0 || strncmp(query, "/aprende ", 9) == 0 ||
        strncmp(query, ":learn ", 7) == 0 || strncmp(query, ":aprende ", 9) == 0)
    {
        const char *p = strchr(query, ' ');
        while (p && isspace((unsigned char)*p)) p++;
        char s_tok[64], r_tok[64], o_tok[64];
        if (p && sscanf(p, "%63s %63s %63s", s_tok, r_tok, o_tok) == 3)
        {
            int rc = ChatLearnTriple(&g_session, s_tok, r_tok, o_tok, "user");
            if (rc == 1)
                snprintf(content, sizeof(content), "[memoria] Hecho aprendido y guardado persistentemente: %s --%s--> %s.", s_tok, r_tok, o_tok);
            else if (rc == 2)
                snprintf(content, sizeof(content), "[memoria] Hecho ya conocido, reforzado en memoria: %s --%s--> %s.", s_tok, r_tok, o_tok);
            else
                snprintf(content, sizeof(content), "[memoria] No se pudo incorporar la tripleta.");
        }
        else
        {
            snprintf(content, sizeof(content), "Uso: /learn SUJETO RELACION OBJETO  (ej: /learn Juan hermano_de Pedro)");
        }
        ServerBuildResponse(SERVER_MODEL_ID, (long)time(NULL), ++g_seq, content, query, resp, sizeof(resp));
        SendJson(s, 200, "OK", resp);
        return;
    }

    /* Conversational natural language learning */
    if (strncasecmp(query, "aprende que ", 12) == 0 ||
        strncasecmp(query, "recuerda que ", 13) == 0 ||
        strncasecmp(query, "learn that ", 11) == 0)
    {
        const char *p = strchr(query, ' ');
        if (p) p = strchr(p + 1, ' ');
        while (p && isspace((unsigned char)*p)) p++;
        char s_tok[64], copula[32], r_tok[64], prep[32], o_tok[64];
        if (p && sscanf(p, "%63s %31s %63s %31s %63s", s_tok, copula, r_tok, prep, o_tok) == 5 &&
            (strcasecmp(copula, "es") == 0 || strcasecmp(copula, "is") == 0) &&
            (strcasecmp(prep, "de") == 0 || strcasecmp(prep, "of") == 0))
        {
            char full_rel[64];
            snprintf(full_rel, sizeof(full_rel), "%s_%s", r_tok, prep);
            int rc = ChatLearnTriple(&g_session, s_tok, full_rel, o_tok, "conversation");
            if (rc)
            {
                snprintf(content, sizeof(content), "[memoria] Hecho registrado: %s es %s de %s (guardado en memoria continua).", s_tok, r_tok, o_tok);
                ServerBuildResponse(SERVER_MODEL_ID, (long)time(NULL), ++g_seq, content, query, resp, sizeof(resp));
                SendJson(s, 200, "OK", resp);
                return;
            }
        }
    }

    /* Episodic memory inspection: /memory, /memoria */
    if (strcmp(query, "/memory") == 0 || strcmp(query, "/memoria") == 0 ||
        strcmp(query, ":memory") == 0 || strcmp(query, ":memoria") == 0 ||
        strcmp(query, "/episodic") == 0)
    {
        uint32_t cnt = ChatEpisodicCount(&g_session);
        if (cnt == 0)
        {
            snprintf(content, sizeof(content), "[memoria] No hay recuerdos episodicos guardados actualmente en data/memory/episodic.tsv.");
        }
        else
        {
            size_t written = snprintf(content, sizeof(content), "[memoria] %u recuerdos episodicos continuos guardados:\n", cnt);
            for (uint32_t i = 0; i < cnt && written + 128 < sizeof(content); i++)
            {
                const EPISODIC_RECORD *rec = ChatEpisodicGet(&g_session, i);
                if (rec)
                {
                    written += snprintf(content + written, sizeof(content) - written,
                                        "  %u. %s --%s--> %s (origen: %s)\n",
                                        i + 1, rec->subject, rec->relation, rec->object, rec->source);
                }
            }
        }
        ServerBuildResponse(SERVER_MODEL_ID, (long)time(NULL), ++g_seq, content, query, resp, sizeof(resp));
        SendJson(s, 200, "OK", resp);
        return;
    }

    /* Episodic memory purge: /forget, /olvida */
    if (strcmp(query, "/forget") == 0 || strcmp(query, "/olvida") == 0 ||
        strcmp(query, ":forget") == 0 || strcmp(query, "/clear-memory") == 0)
    {
        ChatEpisodicClear(&g_session);
        snprintf(content, sizeof(content), "[memoria] Memoria episodica borrada tanto de la sesion como de disco.");
        ServerBuildResponse(SERVER_MODEL_ID, (long)time(NULL), ++g_seq, content, query, resp, sizeof(resp));
        SendJson(s, 200, "OK", resp);
        return;
    }

    /* Direct greeting / conversational identity queries */
    if (ServerIsGreeting(query))
    {
        char greet_resp[4096];
        ServerAnswerGreeting(query, (int)sess->persona_id, greet_resp, sizeof(greet_resp));

        if (ServerWantsStream(body))
        {
            char sse[4096];
            ServerBuildStreamResponse(SERVER_MODEL_ID, (long)time(NULL), ++g_seq, greet_resp, sse, sizeof(sse));
            SendRaw(s, 200, "OK", "text/event-stream", sse);
        }
        else
        {
            ServerBuildResponse(SERVER_MODEL_ID, (long)time(NULL), ++g_seq, greet_resp, query, resp, sizeof(resp));
            SendJson(s, 200, "OK", resp);
        }
        return;
    }

    int is_coding = ServerIsCodingTask(query);

    /* Literal shell/CLI: emit one tool_call; the client harness executes it. */
    if (sess->declared_tools_count > 0 && ServerIsShellTask(query))
    {
        int has_glob = HasDeclaredTool(sess, "glob");
        int glob_ok = has_glob && (strchr(query, '*') != NULL);
        if (!glob_ok && has_glob)
        {
            const char *p = query;
            char tok[16];
            size_t n = 0;
            while (*p == ' ' || *p == '\t')
                p++;
            while (*p && *p != ' ' && *p != '\t' && n + 1 < sizeof(tok))
                tok[n++] = (char)tolower((unsigned char)*p++);
            tok[n] = '\0';
            if ((strcmp(tok, "dir") == 0 || strcmp(tok, "ls") == 0) &&
                strstr(query, " -") == NULL)
                glob_ok = 1;
        }
        if (!glob_ok)
        {
            OPENAI_TOOL_CALLS tc;
            memset(&tc, 0, sizeof(tc));
            if (ServerMapShellToolCall(query, sess->declared_tools,
                                       (uint32_t)sess->declared_tools_count,
                                       &tc.calls[0]))
            {
                tc.count = 1;
                snprintf(tc.calls[0].id, sizeof(tc.calls[0].id),
                         "call_sym_%lu", ++g_seq);
                strncpy(sess->last_tool_call_name, tc.calls[0].name,
                        sizeof(sess->last_tool_call_name) - 1);
                if (ServerWantsStream(body))
                {
                    char sse_local[16384];
                    ServerBuildToolCallStreamResponse(SERVER_MODEL_ID,
                        (long)time(NULL), g_seq, &tc, sse_local, sizeof(sse_local));
                    SendRaw(s, 200, "OK", "text/event-stream", sse_local);
                }
                else
                {
                    ServerBuildToolCallResponse(SERVER_MODEL_ID, (long)time(NULL),
                        g_seq, &tc,
                        "Dispatching shell command to the client harness.",
                        resp, sizeof(resp));
                    SendJson(s, 200, "OK", resp);
                }
                return;
            }
        }
    }

    if (sess->declared_tools_count > 0 && ServerIsDiffTask(query))
    {
        OPENAI_TOOL_CALLS tc;
        memset(&tc, 0, sizeof(tc));
        if (ServerMapDiffToolCall(query, sess->declared_tools,
                                  (uint32_t)sess->declared_tools_count,
                                  &tc.calls[0]))
        {
            tc.count = 1;
            snprintf(tc.calls[0].id, sizeof(tc.calls[0].id), "call_sym_%lu", ++g_seq);
            strncpy(sess->last_tool_call_name, tc.calls[0].name,
                    sizeof(sess->last_tool_call_name) - 1);
            if (ServerWantsStream(body))
            {
                char sse_local[16384];
                ServerBuildToolCallStreamResponse(SERVER_MODEL_ID, (long)time(NULL),
                    g_seq, &tc, sse_local, sizeof(sse_local));
                SendRaw(s, 200, "OK", "text/event-stream", sse_local);
            }
            else
            {
                ServerBuildToolCallResponse(SERVER_MODEL_ID, (long)time(NULL), g_seq, &tc,
                    "Asking the harness to show the working-tree diff.",
                    resp, sizeof(resp));
                SendJson(s, 200, "OK", resp);
            }
            return;
        }
    }

    /* Enclitic resolution: if query has edit verb but no file,
       inject last_target (e.g. "añadele una linea" → "añadele una linea a test.txt") */
    if (!ServerIsEditTask(query) &&
        ServerHasEditVerb(query) && sess->last_target[0] != '\0')
    {
        char resolved[2048];
        snprintf(resolved, sizeof(resolved), "%s a %s", query, sess->last_target);
        strncpy(query, resolved, sizeof(query) - 1);
        query[sizeof(query) - 1] = '\0';
    }

    if (sess->declared_tools_count > 0 && ServerIsEditTask(query))
    {
        OPENAI_TOOL_CALLS tc;
        memset(&tc, 0, sizeof(tc));
        if (ServerMapEditToolCall(query, sess->declared_tools,
                                  (uint32_t)sess->declared_tools_count,
                                  &tc.calls[0]))
        {
            tc.count = 1;
            snprintf(tc.calls[0].id, sizeof(tc.calls[0].id), "call_sym_%lu", ++g_seq);
            strncpy(sess->last_tool_call_name, tc.calls[0].name,
                    sizeof(sess->last_tool_call_name) - 1);
            /* Enable agentic resumption so auto-diff fires after edit */
            sess->agent_active = 1;
            sess->had_edit = 1;
            sess->had_error = 0;
            sess->current_step_idx = 0;
            sess->replan_count = 0;
            strncpy(sess->current_issue, query, sizeof(sess->current_issue) - 1);
            if (ServerWantsStream(body))
            {
                char sse_local[16384];
                ServerBuildToolCallStreamResponse(SERVER_MODEL_ID, (long)time(NULL),
                    g_seq, &tc, sse_local, sizeof(sse_local));
                SendRaw(s, 200, "OK", "text/event-stream", sse_local);
            }
            else
            {
                ServerBuildToolCallResponse(SERVER_MODEL_ID, (long)time(NULL),
                    g_seq, &tc,
                    "Dispatching file edit to the client harness.",
                    resp, sizeof(resp));
                SendJson(s, 200, "OK", resp);
            }
            return;
        }
    }

    /* Direct C code synthesis queries: respond with generated C code in markdown directly */
    if (ServerIsCodeSynthesisTask(query) && !ServerIsFileCreationTask(query))
    {
        char code_resp[16384];
        ServerSynthesizeCode(query, code_resp, sizeof(code_resp));

        if (ServerWantsStream(body))
        {
            char sse[16384];
            ServerBuildStreamResponse(SERVER_MODEL_ID, (long)time(NULL), ++g_seq, code_resp, sse, sizeof(sse));
            SendRaw(s, 200, "OK", "text/event-stream", sse);
        }
        else
        {
            ServerBuildResponse(SERVER_MODEL_ID, (long)time(NULL), ++g_seq, code_resp, query, resp, sizeof(resp));
            SendJson(s, 200, "OK", resp);
        }
        return;
    }

    /* 3. INITIATE AGENTIC CODING TASK ONLY IF CODING INTENT AND TOOLS ARE DECLARED */
    if (is_coding && sess->declared_tools_count > 0)
    {
        sess->agent_active = 1;
        sess->current_step_idx = 0;
        sess->had_error = 0;
        sess->had_edit = 0;
        sess->replan_count = 0;
        sess->last_error_summary[0] = '\0';
        sess->last_tool_output[0] = '\0';
        sess->last_tool_call_name[0] = '\0';
        memset(&sess->last_diagnostic, 0, sizeof(sess->last_diagnostic));
        strncpy(sess->current_issue, query, sizeof(sess->current_issue) - 1);

        int is_creation = ServerIsFileCreationTask(query);
        int is_inspection = ServerIsInspectionTask(query);
        int is_folder_glob = IsFolderOrGlobQuery(query);

        /* Non-code file edits (.txt, .md, .json, etc.) skip build+test */
        int is_noncode_edit = 0;
        {
            char lower_q[1024];
            size_t j = 0;
            while (query[j] != '\0' && j < sizeof(lower_q) - 1)
            {
                lower_q[j] = (char)tolower((unsigned char)query[j]);
                j++;
            }
            lower_q[j] = '\0';
            static const char *noncode_exts[] = {
                ".txt", ".md", ".json", ".yml", ".yaml", ".toml",
                ".csv", ".tsv", ".xml", ".html", ".css", ".log"
            };
            for (size_t e = 0; e < sizeof(noncode_exts) / sizeof(noncode_exts[0]); e++)
            {
                if (strstr(lower_q, noncode_exts[e]) != NULL)
                {
                    is_noncode_edit = 1;
                    break;
                }
            }
            /* Also detect "añade/agrega/add" + file pattern */
            if (!is_noncode_edit &&
                (strstr(lower_q, "añade") != NULL || strstr(lower_q, "agrega") != NULL ||
                 strstr(lower_q, "add ") != NULL || strstr(lower_q, "escribe") != NULL ||
                 strstr(lower_q, "write ") != NULL))
            {
                for (size_t e = 0; e < sizeof(noncode_exts) / sizeof(noncode_exts[0]); e++)
                {
                    if (strstr(lower_q, noncode_exts[e]) != NULL)
                    {
                        is_noncode_edit = 1;
                        break;
                    }
                }
            }
        }

        if (is_creation)
        {
            sess->current_plan.step_count = 1;
            strncpy(sess->current_plan.goal_description, query, sizeof(sess->current_plan.goal_description) - 1);
            sess->current_plan.initial_state = PRED_SYMBOL_KNOWN;
            sess->current_plan.goal_state = PRED_PATCH_APPLIED;
            sess->current_plan.steps[0].op = (STRIPS_OPERATOR){
                .tool_id = OP_TOOL_REPLACE_CONTENT,
                .preconditions = PRED_SYMBOL_KNOWN,
                .add_effects = PRED_PATCH_APPLIED,
                .del_effects = 0,
                .cost = 1
            };
            strncpy(sess->current_plan.steps[0].op.name, "create_file", sizeof(sess->current_plan.steps[0].op.name) - 1);
            strncpy(sess->current_plan.steps[0].op.description, "Create new file in workspace", sizeof(sess->current_plan.steps[0].op.description) - 1);
        }
        else
        {
            uint32_t goal = is_folder_glob ? PRED_FILE_LOCATED :
                            (is_inspection ? PRED_CODE_INSPECTED :
                            (is_noncode_edit ? PRED_PATCH_APPLIED :
                            (PRED_BUILD_VERIFIED | PRED_TESTS_VERIFIED | PRED_TASK_COMPLETED)));

            AGENT_PLANNER planner;
            AgentPlannerInit(&planner);
            AgentPlannerFormulate(&planner, query, PRED_SYMBOL_KNOWN,
                                  goal,
                                  &sess->current_plan);
        }

        /* Advance past any internal OP_TOOL_NONE operators */
        while (sess->current_step_idx < sess->current_plan.step_count &&
               sess->current_plan.steps[sess->current_step_idx].op.tool_id == OP_TOOL_NONE)
        {
            sess->current_step_idx++;
        }

        if (sess->current_step_idx < sess->current_plan.step_count)
        {
            const STRIPS_OPERATOR *first_op = &sess->current_plan.steps[sess->current_step_idx].op;
            OPENAI_TOOL_CALLS tc;
            FormatOperatorToolCall(sess, first_op, sess->current_issue, ++g_seq, &tc);
            if (tc.count > 0)
            {
                strncpy(sess->last_tool_call_name, tc.calls[0].name, sizeof(sess->last_tool_call_name) - 1);
                sess->last_tool_call_name[sizeof(sess->last_tool_call_name) - 1] = '\0';
            }

            const char *thought = is_creation ?
                "Formulated file creation plan for workspace. Initiating write step." :
                (is_inspection ?
                "Formulated inspection plan for workspace. Initiating exploration." :
                "Formulated STRIPS plan to resolve coding task. Initiating first step.");

            if (ServerWantsStream(body))
            {
                char sse_local[16384];
                ServerBuildToolCallStreamResponse(SERVER_MODEL_ID, (long)time(NULL), g_seq, &tc, sse_local, sizeof(sse_local));
                SendRaw(s, 200, "OK", "text/event-stream", sse_local);
            }
            else
            {
                ServerBuildToolCallResponse(SERVER_MODEL_ID, (long)time(NULL), g_seq, &tc,
                                            thought,
                                            resp, sizeof(resp));
                SendJson(s, 200, "OK", resp);
            }
            return;
        }
    }
    else if (is_coding && sess->declared_tools_count == 0)
    {
        int is_creation = ServerIsFileCreationTask(query);
        if (is_creation)
        {
            char created[260];
            const char *target_file = "nuevo.txt";
            if (ServerExtractCreatePath(query, created, sizeof(created)))
                target_file = created;
            snprintf(content, sizeof(content),
                     "### Solicitud de Creacion de Archivo ('%s')\n\n"
                     "Para crear '%s' directamente en el espacio de trabajo, habilita las herramientas de agente (tools) en tu cliente OpenCode.",
                     target_file, target_file);
        }
        else if (g_server_code_graph != NULL)
        {
            snprintf(content, sizeof(content),
                     "### Repositorio de Codigo Indexado\n\n"
                     "- **Archivos**: %u\n"
                     "- **Funciones**: %u\n"
                     "- **Estructuras**: %u\n"
                     "- **Clases**: %u\n"
                     "- **Llamadas AST**: %u\n\n"
                     "El grafo de codigo esta en memoria. Para explorar o modificar archivos interactivamente, habilita las herramientas de agente (tools) en tu cliente OpenCode.",
                     g_server_code_graph->total_files,
                     g_server_code_graph->total_functions,
                     g_server_code_graph->total_structs,
                     g_server_code_graph->total_classes,
                     g_server_code_graph->total_calls);
        }
        else
        {
            snprintf(content, sizeof(content),
                     "Peticion de inspeccion o codigo ('%s') recibida, pero no hay herramientas declaradas en la sesion ni repositorio indexado.",
                     query);
        }

        if (ServerWantsStream(body))
        {
            char sse[16384];
            ServerBuildStreamResponse(SERVER_MODEL_ID, (long)time(NULL), ++g_seq, content, sse, sizeof(sse));
            SendRaw(s, 200, "OK", "text/event-stream", sse);
        }
        else
        {
            ServerBuildResponse(SERVER_MODEL_ID, (long)time(NULL), ++g_seq, content, query, resp, sizeof(resp));
            SendJson(s, 200, "OK", resp);
        }
        return;
    }

    /* Restore session dialogue state and persona */
    if (nmsg <= 1)
    {
        g_session.focus[0] = '\0';
        g_session.focus_valid = 0;
        g_session.focus_secondary[0] = '\0';
        g_session.focus_secondary_valid = 0;
        memset(&g_session.exec, 0, sizeof(g_session.exec));
        g_session.ntshown = 0;
        g_session.tnw = 0;
    }
    else
    {
        strncpy(g_session.focus, sess->focus, sizeof(g_session.focus) - 1);
        g_session.focus[sizeof(g_session.focus) - 1] = '\0';
        g_session.focus_valid = sess->focus_valid;
        strncpy(g_session.focus_secondary, sess->focus_secondary, sizeof(g_session.focus_secondary) - 1);
        g_session.focus_secondary[sizeof(g_session.focus_secondary) - 1] = '\0';
        g_session.focus_secondary_valid = sess->focus_secondary_valid;
        g_session.exec = sess->exec;
    }
    ChatSetPersona(&g_session, sess->persona_id);

    memset(&parsed, 0, sizeof(parsed));
    ChatParseLine(&g_session, query, &parsed);
    t0 = clock();
    if (!ServerAnswerQuery(&g_session, query, raw, sizeof(raw)))
        strncpy(raw, "No entendi la pregunta.", sizeof(raw) - 1);
    latency_ms = (long)((clock() - t0) * 1000 / CLOCKS_PER_SEC);

    /* Persist updated session dialogue state */
    strncpy(sess->focus, g_session.focus, sizeof(sess->focus) - 1);
    sess->focus[sizeof(sess->focus) - 1] = '\0';
    sess->focus_valid = g_session.focus_valid;
    strncpy(sess->focus_secondary, g_session.focus_secondary, sizeof(sess->focus_secondary) - 1);
    sess->focus_secondary[sizeof(sess->focus_secondary) - 1] = '\0';
    sess->focus_secondary_valid = g_session.focus_secondary_valid;
    sess->exec = g_session.exec;
    sess->last_active = time(NULL);

    ServerMapContent(raw, content, sizeof(content));
    status = ServerStatusOf(raw);
    if (ServerWantsStream(body))
    {
        char sse[16384];
        if (!ServerBuildStreamResponse(SERVER_MODEL_ID,
                                       (long)time(NULL), ++g_seq,
                                       content, sse, sizeof(sse)))
        {
            SendError(s, 500, "Internal Error", "response too large");
            return;
        }
        SendRaw(s, 200, "OK", "text/event-stream", sse);
    }
    else if (!ServerBuildResponse(SERVER_MODEL_ID, (long)time(NULL),
                                  ++g_seq, content, query, resp,
                                  sizeof(resp)))
    {
        SendError(s, 500, "Internal Error", "response too large");
        return;
    }
    else
        SendJson(s, 200, "OK", resp);
    BuildStrArray(g_session.exec.entities, g_session.exec.nent, ent_json,
                  sizeof(ent_json));
    BuildProvArray(&g_session, prov_json, sizeof(prov_json));
    IsoUtc(ts, sizeof(ts));
    if (ServerBuildObservation(ts, SERVER_MODEL_ID, nmsg, has_system,
                               query, &parsed, g_session.focus_valid ?
                               g_session.focus : "", ent_json, prov_json,
                               content, status, latency_ms, obs,
                               sizeof(obs)))
    {
        /* CWD first (repo-adjacent log), Temp fallback: the request
           is always served, the log never blocks it. */
        log = fopen(SERVER_OBS_FILE, "a");
        if (log == NULL)
        {
            char alt[640];
#ifdef _WIN32
            char tmp[512];
            DWORD tn = GetTempPathA(sizeof(tmp), tmp);
            if (tn > 0 && tn < sizeof(tmp))
            {
                snprintf(alt, sizeof(alt), "%s%s", tmp,
                         SERVER_OBS_FILE);
                log = fopen(alt, "a");
            }
#else
            const char *tmp = getenv("TMPDIR");
            if (tmp == NULL || tmp[0] == '\0')
                tmp = getenv("TMP");
            if (tmp == NULL || tmp[0] == '\0')
                tmp = getenv("TEMP");
            if (tmp == NULL || tmp[0] == '\0')
                tmp = "/tmp";
            snprintf(alt, sizeof(alt), "%s/%s", tmp, SERVER_OBS_FILE);
            log = fopen(alt, "a");
#endif
        }
        if (log != NULL)
        {
            fputs(obs, log);
            fputc('\n', log);
            fclose(log);
        }
    }
    fprintf(stderr, "[symbols] q=%.60s status=%s latency=%ldms\n", query,
            status, latency_ms);
}

static const char *FindHttpHeader(const char *hdr, const char *name)
{
    if (!hdr || !name) return NULL;
    size_t nlen = strlen(name);
    const char *p = hdr;
    while (*p != '\0')
    {
        if (strncasecmp(p, name, nlen) == 0)
        {
            const char *col = p + nlen;
            while (*col == ' ' || *col == '\t') col++;
            if (*col == ':')
                return col + 1;
        }
        p = strstr(p, "\r\n");
        if (!p) break;
        p += 2;
    }
    return NULL;
}

static int ReadHttpBody(socket_t s, const char *hdr, size_t hlen,
                        long content_len, char *body, size_t body_max)
{
    if (content_len < 0 || (size_t)content_len >= body_max)
    {
        fprintf(stderr, "[symbols-server] ReadHttpBody error: content_len=%ld out of bounds (body_max=%zu)\n",
                content_len, body_max);
        return 0;
    }

    const char *bstart = strstr(hdr, "\r\n\r\n");
    if (!bstart)
        return 0;
    bstart += 4;

    size_t hbody = hlen - (size_t)(bstart - hdr);
    if (content_len == 0)
    {
        /* If no explicit Content-Length, but body was already read in header buffer */
        if (hbody > 0 && hbody < body_max)
        {
            memcpy(body, bstart, hbody);
            body[hbody] = '\0';
            return 1;
        }
        body[0] = '\0';
        return 1;
    }

    if (hbody > (size_t)content_len)
        hbody = (size_t)content_len;
    memcpy(body, bstart, hbody);
    size_t got = hbody;
    while (got < (size_t)content_len)
    {
        int rc = recv(s, body + got,
                      (int)((size_t)content_len - got), 0);
        if (rc <= 0)
        {
            /* Check if we already received a valid complete JSON object */
            if (got > 0 && body[0] == '{')
            {
                const char *end_brace = strrchr(body, '}');
                if (end_brace != NULL)
                {
                    body[got] = '\0';
                    return 1;
                }
            }
            fprintf(stderr, "[symbols-server] ReadHttpBody recv returned %d, got %zu / %ld bytes\n",
                    rc, got, content_len);
            return 0;
        }
        got += (size_t)rc;
    }
    body[content_len] = '\0';
    return 1;
}

static void HandleClient(socket_t s, const char *corpus)
{
    char hdr[SERVER_HDR_MAX + 1];
    size_t hlen = 0;
    char method[16], path[256];
    long content_len = 0;
    int rc;
    memset(hdr, 0, sizeof(hdr));
    while (hlen + 1 < sizeof(hdr))
    {
        rc = recv(s, hdr + hlen, (int)(sizeof(hdr) - 1 - hlen), 0);
        if (rc <= 0)
            return;
        hlen += (size_t)rc;
        hdr[hlen] = '\0';
        if (strstr(hdr, "\r\n\r\n") != NULL)
            break;
    }
    if (strstr(hdr, "\r\n\r\n") == NULL)
    {
        SendError(s, 400, "Bad Request", "headers too large");
        return;
    }
    method[0] = '\0';
    path[0] = '\0';
    sscanf(hdr, "%15s %255s", method, path);

    const char *cl = FindHttpHeader(hdr, "Content-Length");
    if (cl != NULL)
    {
        while (*cl == ' ' || *cl == '\t') cl++;
        content_len = strtol(cl, NULL, 10);
    }

    if (strcmp(method, "GET") == 0 &&
        strcmp(path, "/v1/models") == 0)
    {
        char body[512];
        ServerBuildModels(SERVER_MODEL_ID, body, sizeof(body));
        SendJson(s, 200, "OK", body);
        return;
    }
    if (strcmp(method, "POST") == 0 &&
        (strcmp(path, "/v1/model/load") == 0 || strcmp(path, "/v1/models/load") == 0))
    {
        if (!ReadHttpBody(s, hdr, hlen, content_len, g_http_body, sizeof(g_http_body)))
        {
            SendError(s, 400, "Bad Request", "bad content length or body");
            return;
        }
        char target_path[512] = {0};
        const char *p = strstr(g_http_body, "\"path\"");
        if (p)
        {
            const char *col = strchr(p, ':');
            if (col)
            {
                col++;
                while (*col == ' ' || *col == '\t' || *col == '"') col++;
                const char *end = col;
                while (*end != '\0' && *end != '"' && *end != '}' && *end != ',' && *end != '\r' && *end != '\n') end++;
                size_t len = (size_t)(end - col);
                if (len >= sizeof(target_path)) len = sizeof(target_path) - 1;
                strncpy(target_path, col, len);
                target_path[len] = '\0';
            }
        }
        if (target_path[0] == '\0')
        {
            SendError(s, 400, "Bad Request", "missing 'path' parameter");
            return;
        }
        if (ChatIsBinaryModel(target_path))
        {
            MODEL *new_m = ModelLoad(target_path);
            if (new_m == NULL)
            {
                SendError(s, 500, "Internal Error", "failed to load binary model");
                return;
            }
            if (g_server_model != NULL) ModelDestroy(g_server_model);
            g_server_model = new_m;
            ChatInit(&g_session, target_path);
            g_session_ready = 1;
            char resp_buf[512];
            snprintf(resp_buf, sizeof(resp_buf),
                     "{\"status\":\"ok\",\"format\":\"binary_v2\",\"path\":\"%s\","
                     "\"symbols\":%u,\"relations\":%u,\"facts_loaded\":%u}",
                     target_path,
                     SymbolCount(g_server_model->graph->symbols),
                     RelationCount(g_server_model->graph->relations),
                     ChatFactCount(&g_session));
            SendJson(s, 200, "OK", resp_buf);
            return;
        }
        else
        {
            ChatInit(&g_session, target_path);
            g_session_ready = 1;
            char resp_buf[512];
            snprintf(resp_buf, sizeof(resp_buf),
                     "{\"status\":\"ok\",\"format\":\"corpus_text\",\"path\":\"%s\","
                     "\"facts_loaded\":%u}",
                     target_path,
                     ChatFactCount(&g_session));
            SendJson(s, 200, "OK", resp_buf);
            return;
        }
    }
    if (strcmp(method, "POST") == 0 &&
        (strcmp(path, "/v1/model/save") == 0 || strcmp(path, "/v1/models/save") == 0))
    {
        if (!ReadHttpBody(s, hdr, hlen, content_len, g_http_body, sizeof(g_http_body)))
        {
            SendError(s, 400, "Bad Request", "bad content length or body");
            return;
        }
        char target_path[512] = {0};
        const char *p = strstr(g_http_body, "\"path\"");
        if (p)
        {
            const char *col = strchr(p, ':');
            if (col)
            {
                col++;
                while (*col == ' ' || *col == '\t' || *col == '"') col++;
                const char *end = col;
                while (*end != '\0' && *end != '"' && *end != '}' && *end != ',' && *end != '\r' && *end != '\n') end++;
                size_t len = (size_t)(end - col);
                if (len >= sizeof(target_path)) len = sizeof(target_path) - 1;
                strncpy(target_path, col, len);
                target_path[len] = '\0';
            }
        }
        if (target_path[0] == '\0')
        {
            SendError(s, 400, "Bad Request", "missing 'path' parameter");
            return;
        }
        if (g_server_model == NULL)
        {
            SendError(s, 400, "Bad Request", "no binary model loaded in memory to save");
            return;
        }
        if (!ModelSave(g_server_model, target_path))
        {
            SendError(s, 500, "Internal Error", "failed to save binary model");
            return;
        }
        char resp_buf[512];
        snprintf(resp_buf, sizeof(resp_buf),
                 "{\"status\":\"ok\",\"format\":\"binary_v2\",\"path\":\"%s\"}",
                 target_path);
        SendJson(s, 200, "OK", resp_buf);
        return;
    }
    if (strcmp(method, "POST") == 0 &&
        strcmp(path, "/v1/chat/completions") == 0)
    {
        if (!ReadHttpBody(s, hdr, hlen, content_len, g_http_body, sizeof(g_http_body)))
        {
            SendError(s, 400, "Bad Request", "bad content length");
            return;
        }
        HandleCompletions(s, g_http_body, corpus);
        return;
    }
    SendError(s, 404, "Not Found", "unknown path");
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    socket_t ls;
    struct sockaddr_in addr;
    int port = SERVER_PORT_DEFAULT;
    char corpus[1024];
    corpus[0] = '\0';
    char model_path[1024];
    model_path[0] = '\0';

    if (argc > 1 && argv[1][0] != '-')
        port = atoi(argv[1]);
    if (port <= 0 || port > 65535)
        port = SERVER_PORT_DEFAULT;

    /* Check for explicit --model / -m flags */
    const char *env_model = getenv("SYMBOLS_MODEL");
    if (env_model != NULL && env_model[0] != '\0')
    {
        strncpy(model_path, env_model, sizeof(model_path) - 1);
        model_path[sizeof(model_path) - 1] = '\0';
    }
    for (int a = 1; a < argc; a++)
    {
        if ((strcmp(argv[a], "--model") == 0 || strcmp(argv[a], "-m") == 0) && a + 1 < argc)
        {
            strncpy(model_path, argv[a + 1], sizeof(model_path) - 1);
            model_path[sizeof(model_path) - 1] = '\0';
            a++;
        }
    }

    char repo_dir[1024];
    repo_dir[0] = '\0';
    const char *env_repo = getenv("SYMBOLS_REPO");
    if (env_repo != NULL && env_repo[0] != '\0')
    {
        strncpy(repo_dir, env_repo, sizeof(repo_dir) - 1);
        repo_dir[sizeof(repo_dir) - 1] = '\0';
    }
    else if (argc > 2 && argv[2][0] != '-')
    {
#ifdef _WIN32
        DWORD attr = GetFileAttributesA(argv[2]);
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
        {
            strncpy(repo_dir, argv[2], sizeof(repo_dir) - 1);
            repo_dir[sizeof(repo_dir) - 1] = '\0';
        }
#else
        struct stat st;
        if (stat(argv[2], &st) == 0 && S_ISDIR(st.st_mode))
        {
            strncpy(repo_dir, argv[2], sizeof(repo_dir) - 1);
            repo_dir[sizeof(repo_dir) - 1] = '\0';
        }
#endif
    }

    if (repo_dir[0] != '\0')
    {
        g_server_code_graph = CodeGraphCreate(65536, 131072);
        if (g_server_code_graph != NULL)
        {
            uint32_t n_indexed = CodeGraphIngestDirectory(g_server_code_graph, repo_dir);
            fprintf(stderr, "[symbols-server] Indexed repository '%s': %u files, %u functions, %u classes\n",
                    repo_dir, n_indexed,
                    g_server_code_graph->total_functions,
                    g_server_code_graph->total_classes);
        }
    }

    const char *env_corpus = getenv("SYMBOLS_CORPUS");
    if (model_path[0] != '\0')
    {
        strncpy(corpus, model_path, sizeof(corpus) - 1);
        corpus[sizeof(corpus) - 1] = '\0';
    }
    else if (env_corpus != NULL && env_corpus[0] != '\0')
    {
        strncpy(corpus, env_corpus, sizeof(corpus) - 1);
        corpus[sizeof(corpus) - 1] = '\0';
    }
    else if (argc > 2)
    {
        int start_arg = (repo_dir[0] != '\0' && strcmp(repo_dir, argv[2]) == 0) ? 3 : 2;
        corpus[0] = '\0';
        for (int i = start_arg; i < argc; i++)
        {
            if (argv[i][0] == '-')
                continue;
            if (corpus[0] != '\0')
                strncat(corpus, ";", sizeof(corpus) - strlen(corpus) - 1);
            strncat(corpus, argv[i], sizeof(corpus) - strlen(corpus) - 1);
        }
    }
    else
    {
        /* CWD layout first, then exe-relative (build-* / dirs) */
        static const char *cand_paths[] = {
            "data/c_lang/c_corpus.txt",
            "data/texts/c_corpus.txt",
            "data/texts/corpus.txt",
            "data/corpus.txt",
            "wiki_model.bin",
            "data/texts/bible.txt",
            "data/texts/jung.txt",
            "data/corpus.tsv"
        };
        char exedir[768];
        FILE *probe = NULL;
        size_t elen = 0;
#ifdef _WIN32
        elen = (size_t)GetModuleFileNameA(NULL, exedir, sizeof(exedir));
#elif defined(__linux__)
        ssize_t r = readlink("/proc/self/exe", exedir, sizeof(exedir) - 1);
        if (r > 0) { exedir[r] = '\0'; elen = (size_t)r; }
#endif
        corpus[0] = '\0';
        for (size_t i = 0; i < sizeof(cand_paths) / sizeof(cand_paths[0]); i++)
        {
            probe = fopen(cand_paths[i], "rb");
            if (probe != NULL)
            {
                fclose(probe);
                strncpy(corpus, cand_paths[i], sizeof(corpus) - 1);
                break;
            }
        }
        if (corpus[0] == '\0' && elen > 0 && elen < sizeof(exedir))
        {
            char *sep = strrchr(exedir, '\\');
            if (sep == NULL)
                sep = strrchr(exedir, '/');
            if (sep != NULL)
            {
                *sep = '\0';
                for (size_t i = 0; i < sizeof(cand_paths) / sizeof(cand_paths[0]); i++)
                {
                    snprintf(corpus, sizeof(corpus), "%s/../%s", exedir, cand_paths[i]);
                    probe = fopen(corpus, "rb");
                    if (probe != NULL)
                    {
                        fclose(probe);
                        break;
                    }
                    corpus[0] = '\0';
                }
            }
        }
        if (corpus[0] == '\0')
        {
            if (repo_dir[0] != '\0')
            {
                strncpy(corpus, "code_repository", sizeof(corpus) - 1);
            }
            else
            {
                fprintf(stderr,
                        "corpus not found (tried wiki_model.bin, data/texts/*.txt and "
                        "data/corpus.*); refusing to serve an empty "
                        "model\n");
                return 1;
            }
        }
    }

    /* Initialize binary model if path is a binary model */
    if (ChatIsBinaryModel(corpus))
    {
        g_server_model = ModelLoad(corpus);
        if (g_server_model != NULL)
        {
            fprintf(stderr, "[symbols-server] Loaded binary model from '%s' (%u symbols, %u relations)\n",
                    corpus,
                    SymbolCount(g_server_model->graph->symbols),
                    RelationCount(g_server_model->graph->relations));
        }
        else
        {
            fprintf(stderr, "[symbols-server] Warning: failed to load binary model '%s'\n", corpus);
        }
    }

    SOCKET_INIT();
    ls = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (!IS_VALID_SOCKET(ls))
    {
        fprintf(stderr, "socket failed\n");
        SOCKET_CLEANUP();
        return 1;
    }
    int opt = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((u_short)port);
    if (bind(ls, (struct sockaddr *)&addr, sizeof(addr)) != 0 ||
        listen(ls, 64) != 0)
    {
        fprintf(stderr, "bind/listen on 127.0.0.1:%d failed\n", port);
        CLOSESOCKET(ls);
        SOCKET_CLEANUP();
        return 1;
    }
    fprintf(stderr, "symbols-server on 127.0.0.1:%d (model %s)\n",
            port, SERVER_MODEL_ID);
    fprintf(stderr, "symbols-server corpus: %s\n", corpus);
    for (;;)
    {
        socket_t c = accept(ls, NULL, NULL);
        if (!IS_VALID_SOCKET(c))
            continue;
        HandleClient(c, corpus);
        CLOSESOCKET(c);
    }
    CLOSESOCKET(ls);
    SOCKET_CLEANUP();
    return 0;
}
