/* server_proto: OpenAI-compatible wire layer (pure, testable).
   English code comments (project rule). No sockets: only byte
   handling, JSON building, and the engine call. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "compat.h"
#include "chat.h"
#include "server_proto.h"
#include "agent_diagnose.h"

int ServerJsonEscape(const char *in, char *out, size_t size)
{
    size_t o = 0;
    if (in == NULL || out == NULL || size == 0)
        return 0;
    out[0] = '\0';
    while (*in != '\0' && o + 1 < size)
    {
        unsigned char c = (unsigned char)*in++;
        if (c == '"')
        {
            if (o + 2 >= size)
                return 0;
            out[o++] = '\\';
            out[o++] = '"';
        }
        else if (c == '\\')
        {
            if (o + 2 >= size)
                return 0;
            out[o++] = '\\';
            out[o++] = '\\';
        }
        else if (c < 0x20)
        {
            static const char *hex = "0123456789abcdef";
            if (o + 6 >= size)
                return 0;
            out[o++] = '\\';
            out[o++] = 'u';
            out[o++] = '0';
            out[o++] = '0';
            out[o++] = hex[(c >> 4) & 0xF];
            out[o++] = hex[c & 0xF];
        }
        else
            out[o++] = (char)c;
    }
    out[o] = '\0';
    return *in == '\0';
}

/* decode one \uXXXX (BMP); returns bytes written (1-3) or 0 */
static int DecodeU(unsigned v, char *out)
{
    if (v < 0x80)
    {
        out[0] = (char)v;
        return 1;
    }
    if (v < 0x800)
    {
        out[0] = (char)(0xC0 | (v >> 6));
        out[1] = (char)(0x80 | (v & 0x3F));
        return 2;
    }
    out[0] = (char)(0xE0 | (v >> 12));
    out[1] = (char)(0x80 | ((v >> 6) & 0x3F));
    out[2] = (char)(0x80 | (v & 0x3F));
    return 3;
}

static int HexVal(char c, unsigned *v)
{
    if (c >= '0' && c <= '9')
        *v = (unsigned)(c - '0');
    else if (c >= 'a' && c <= 'f')
        *v = (unsigned)(c - 'a' + 10);
    else if (c >= 'A' && c <= 'F')
        *v = (unsigned)(c - 'A' + 10);
    else
        return 0;
    return 1;
}

/* copy a JSON string starting at the opening quote; *pp ends past
   the closing quote. Returns 1 on success. */
static int TakeJsonString(const char **pp, char *out, size_t size)
{
    size_t o = 0;
    const char *p;
    unsigned pending_hi = 0;
    int have_hi = 0;
    if (pp == NULL || *pp == NULL || **pp != '"')
        return 0;
    p = *pp + 1;
    out[0] = '\0';
    while (*p != '\0' && *p != '"' && o + 1 < size)
    {
        if (*p == '\\')
        {
            unsigned v = 0, d = 0;
            char tmp[4];
            int nb = 0;
            p++;
            if (*p == 'n')
            {
                out[o++] = '\n';
                p++;
            }
            else if (*p == 'r')
            {
                out[o++] = '\r';
                p++;
            }
            else if (*p == 't')
            {
                out[o++] = '\t';
                p++;
            }
            else if (*p == 'b')
            {
                out[o++] = '\b';
                p++;
            }
            else if (*p == 'f')
            {
                out[o++] = '\f';
                p++;
            }
            else if (*p == '\\' || *p == '"' || *p == '/')
                out[o++] = *p++;
            else if (*p == 'u')
            {
                p++;
                v = 0;
                for (int k = 0; k < 4; k++)
                {
                    if (!HexVal(*p, &d))
                        return 0;
                    v = (v << 4) | d;
                    p++;
                }
                if (have_hi)
                {
                    /* second half must be a low surrogate */
                    if (v < 0xDC00 || v > 0xDFFF)
                        return 0;
                    v = 0x10000 + ((pending_hi - 0xD800) << 10) +
                        (v - 0xDC00);
                    have_hi = 0;
                    /* non-BMP: emit replacement (outside vocab) */
                    if (o + 1 < size)
                        out[o++] = '?';
                }
                else if (v >= 0xD800 && v <= 0xDBFF)
                {
                    pending_hi = v;
                    have_hi = 1;
                }
                else if (v >= 0xDC00 && v <= 0xDFFF)
                    return 0;
                else
                {
                    nb = DecodeU(v, tmp);
                    if (o + (size_t)nb >= size)
                        return 0;
                    memcpy(out + o, tmp, (size_t)nb);
                    o += (size_t)nb;
                }
            }
            else
                return 0;
        }
        else
            out[o++] = *p++;
    }
    out[o] = '\0';
    if (have_hi)
        return 0;
    if (*p != '"')
        return 0;
    *pp = p + 1;
    return 1;
}

static int IsWs(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

int ServerExtractQuery(const char *body, char *out, size_t size)
{
    const char *p;
    char last[4096];
    int found = 0;
    if (body == NULL || out == NULL || size == 0)
        return 0;
    out[0] = '\0';
    last[0] = '\0';
    p = body;
    while (*p != '\0')
    {
        /* look for "role" key */
        if (*p == '"' && strncmp(p, "\"role\"", 6) == 0)
        {
            const char *q = p + 6;
            char role[32], content[4096];
            while (IsWs(*q))
                q++;
            if (*q != ':')
            {
                p++;
                continue;
            }
            q++;
            while (IsWs(*q))
                q++;
            if (*q != '"')
            {
                p++;
                continue;
            }
            {
                const char *qq = q;
                char tmp[32];
                size_t o = 0;
                qq++;
                while (*qq != '\0' && *qq != '"' && o + 1 < sizeof(tmp))
                    tmp[o++] = *qq++;
                tmp[o] = '\0';
                if (*qq != '"')
                {
                    p++;
                    continue;
                }
                strncpy(role, tmp, sizeof(role) - 1);
                q = qq + 1;
            }
            if (strcmp(role, "user") != 0)
            {
                p = q;
                continue;
            }
            /* find "content" key after this role */
            {
                const char *r = q;
                int ok = 0;
                while (*r != '\0')
                {
                    if (*r == '"' && strncmp(r, "\"content\"", 9) == 0)
                    {
                        const char *s = r + 9;
                        while (IsWs(*s))
                            s++;
                        if (*s != ':')
                            break;
                        s++;
                        while (IsWs(*s))
                            s++;
                        if (*s == '"' &&
                            TakeJsonString(&s, content,
                                           sizeof(content)))
                        {
                            strncpy(last, content,
                                    sizeof(last) - 1);
                            found = 1;
                            ok = 1;
                        }
                        break;
                    }
                    /* stop at next role key (another message) */
                    if (*r == '"' && strncmp(r, "\"role\"", 6) == 0)
                        break;
                    r++;
                }
                (void)ok;
            }
            p = q;
            continue;
        }
        p++;
    }
    if (!found)
        return 0;
    strncpy(out, last, size - 1);
    out[size - 1] = '\0';
    return 1;
}

int ServerExtractSession(const char *body, char *out, size_t size)
{
    const char *p;
    if (body == NULL || out == NULL || size == 0)
        return 0;
    out[0] = '\0';
    p = body;
    while (*p != '\0')
    {
        if (*p == '"')
        {
            if (strncmp(p, "\"user\"", 6) == 0)
            {
                const char *q = p + 6;
                while (IsWs(*q))
                    q++;
                if (*q == ':')
                {
                    q++;
                    while (IsWs(*q))
                        q++;
                    if (*q == '"')
                        return TakeJsonString(&q, out, size);
                }
            }
            else if (strncmp(p, "\"session_id\"", 12) == 0)
            {
                const char *q = p + 12;
                while (IsWs(*q))
                    q++;
                if (*q == ':')
                {
                    q++;
                    while (IsWs(*q))
                        q++;
                    if (*q == '"')
                        return TakeJsonString(&q, out, size);
                }
            }
        }
        p++;
    }
    return 0;
}

static uint32_t WordCount(const char *s)
{
    uint32_t n = 0;
    int in = 0;
    while (*s != '\0')
    {
        if (isspace((unsigned char)*s))
            in = 0;
        else if (!in)
        {
            in = 1;
            n++;
        }
        s++;
    }
    return n;
}

int ServerBuildResponse(const char *model, long created,
                        unsigned long seq, const char *content,
                        const char *query_for_tokens, char *out,
                        size_t size)
{
    char esc[8192];
    uint32_t pt, ct;
    int w;
    if (model == NULL || content == NULL ||
        query_for_tokens == NULL || out == NULL || size == 0)
        return 0;
    if (!ServerJsonEscape(content, esc, sizeof(esc)))
        return 0;
    pt = WordCount(query_for_tokens);
    ct = WordCount(content);
    w = snprintf(out, size,
                 "{\"id\":\"chatcmpl-symbols-%lu\",\"object\":\"chat."
                 "completion\",\"created\":%ld,\"model\":\"%s\","
                 "\"choices\":[{\"index\":0,\"message\":{\"role\":"
                 "\"assistant\",\"content\":\"%s\"},\"finish_reason\":"
                 "\"stop\"}],\"usage\":{\"prompt_tokens\":%u,"
                 "\"completion_tokens\":%u,\"total_tokens\":%u}}",
                 seq, created, model, esc, pt, ct, pt + ct);
    return (w > 0 && (size_t)w < size) ? 1 : 0;
}

int ServerBuildModels(const char *out_model, char *out, size_t size)
{
    int w;
    if (out_model == NULL || out == NULL || size == 0)
        return 0;
    w = snprintf(out, size,
                 "{\"object\":\"list\",\"data\":[{\"id\":\"%s\","
                 "\"object\":\"model\",\"created\":0,\"owned_by\":"
                 "\"symbols\"}]}",
                 out_model);
    return (w > 0 && (size_t)w < size) ? 1 : 0;
}

int ServerWantsStream(const char *body)
{
    /* top-level "stream" key only: track depth, skip strings */
    int depth = 0;
    const char *p;
    if (body == NULL)
        return 0;
    p = body;
    while (*p != '\0')
    {
        if (*p == '"')
        {
            const char *q = p + 1;
            while (*q != '\0' && *q != '"')
            {
                if (*q == '\\' && *(q + 1) != '\0')
                    q += 2;
                else
                    q++;
            }
            if (*q != '"')
                return 0;
            if (depth == 1 && (size_t)(q - p - 1) == 6 &&
                strncmp(p + 1, "stream", 6) == 0)
            {
                const char *v = q + 1;
                while (*v == ' ' || *v == '\t' || *v == '\n' ||
                       *v == '\r')
                    v++;
                if (*v != ':')
                {
                    p = q + 1;
                    continue;
                }
                v++;
                while (*v == ' ' || *v == '\t' || *v == '\n' ||
                       *v == '\r')
                    v++;
                return strncmp(v, "true", 4) == 0 ? 1 : 0;
            }
            p = q + 1;
            continue;
        }
        if (*p == '{' || *p == '[')
            depth++;
        else if (*p == '}' || *p == ']')
            depth--;
        p++;
    }
    return 0;
}

int ServerBuildStreamResponse(const char *model, long created,
                              unsigned long seq, const char *content,
                              char *out, size_t size)
{
    char esc[8192];
    size_t pos = 0;
    int w;
    if (model == NULL || content == NULL || out == NULL || size == 0)
        return 0;
    if (!ServerJsonEscape(content, esc, sizeof(esc)))
        return 0;
    out[0] = '\0';
    w = snprintf(out + pos, size - pos,
                 "data: {\"id\":\"chatcmpl-symbols-%lu\",\"object\":"
                 "\"chat.completion.chunk\",\"created\":%ld,\"model\":"
                 "\"%s\",\"choices\":[{\"index\":0,\"delta\":{\"role\":"
                 "\"assistant\",\"content\":\"%s\"},\"finish_reason\":"
                 "null}]}\n\n",
                 seq, created, model, esc);
    if (w <= 0)
        return 0;
    pos += (size_t)w;
    w = snprintf(out + pos, size - pos,
                 "data: {\"id\":\"chatcmpl-symbols-%lu\",\"object\":"
                 "\"chat.completion.chunk\",\"created\":%ld,\"model\":"
                 "\"%s\",\"choices\":[{\"index\":0,\"delta\":{},"
                 "\"finish_reason\":\"stop\"}]}\n\ndata: [DONE]\n\n",
                 seq, created, model);
    if (w <= 0)
        return 0;
    pos += (size_t)w;
    return pos < size ? 1 : 0;
}

int ServerIsUnknown(const char *text)
{
    if (text == NULL || text[0] == '\0')
        return 1;
    if (strncmp(text, "No entendi", sizeof("No entendi") - 1) == 0)
        return 1;
    if (strncmp(text, "No tengo constancia",
                sizeof("No tengo constancia") - 1) == 0)
        return 1;
    if (strncmp(text, "Hay ", sizeof("Hay ") - 1) == 0)
        return 1;
    return 0;
}

const char *ServerStatusOf(const char *text)
{
    if (text == NULL || text[0] == '\0')
        return "UNKNOWN";
    if (strncmp(text, "No entendi", sizeof("No entendi") - 1) == 0)
        return "ABSTAIN";
    if (strncmp(text, "Hay ", sizeof("Hay ") - 1) == 0)
        return "AMBIGUOUS";
    if (strncmp(text, "No tengo constancia",
                sizeof("No tengo constancia") - 1) == 0)
        return "UNKNOWN";
    return "ANSWER";
}

int ServerAnswerQuery(CHAT *ch, const char *query, char *out,
                      size_t size)
{
    size_t n;
    if (ch == NULL || query == NULL || out == NULL || size == 0)
        return 0;
    out[0] = '\0';
    if (!ChatHandleToBuf(ch, query, out, size))
        return 0;
    n = strlen(out);
    while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r' ||
                     out[n - 1] == ' '))
        out[--n] = '\0';
    return 1;
}

int ServerMapContent(const char *raw, char *out, size_t size)
{
    if (raw == NULL || out == NULL || size == 0)
        return 0;
    if (ServerIsUnknown(raw))
    {
        strncpy(out, "I don't know.", size - 1);
        out[size - 1] = '\0';
        return 1;
    }
    strncpy(out, raw, size - 1);
    out[size - 1] = '\0';
    return 1;
}

int ServerBuildObservation(const char *ts, const char *model, int nmsg,
                           int has_system, const char *query,
                           const ChatParse *parsed, const char *focus,
                           const char *entities_json,
                           const char *prov_json, const char *result,
                           const char *status, long latency_ms,
                           char *out, size_t size)
{
    char q[4096], r[8192], f[128];
    int w;
    if (ts == NULL || model == NULL || query == NULL || result == NULL ||
        status == NULL || out == NULL || size == 0)
        return 0;
    if (parsed == NULL || focus == NULL || entities_json == NULL ||
        prov_json == NULL)
        return 0;
    if (!ServerJsonEscape(query, q, sizeof(q)))
        return 0;
    if (!ServerJsonEscape(result, r, sizeof(r)))
        return 0;
    if (!ServerJsonEscape(focus, f, sizeof(f)))
        return 0;
    w = snprintf(out, size,
                 "{\"ts\":\"%s\",\"model\":\"%s\",\"nmsg\":%d,"
                 "\"system\":%d,\"query\":\"%s\",\"intent\":\"%s\","
                 "\"slot_a\":\"%s\",\"slot_b\":\"%s\",\"family\":\"%s\","
                 "\"focus\":\"%s\",\"entities\":%s,\"prov\":%s,"
                 "\"result\":\"%s\",\"status\":\"%s\",\"latency_ms\":"
                 "%ld}",
                 ts, model, nmsg, has_system, q, parsed->intent,
                 parsed->slot_a, parsed->slot_b, parsed->family, f,
                 entities_json, prov_json, r, status, latency_ms);
    return (w > 0 && (size_t)w < size) ? 1 : 0;
}

/* ============================================================
   OpenAI Tool Calling (Function Calling) Wire Implementation
   ============================================================ */

int ServerExtractToolsDeclared(const char *body, char names[][64], uint32_t max_names)
{
    if (body == NULL || names == NULL || max_names == 0)
        return 0;

    uint32_t count = 0;
    const char *p = strstr(body, "\"tools\"");
    if (p == NULL)
        return 0;

    while ((p = strstr(p, "\"name\"")) != NULL && count < max_names)
    {
        const char *q = p + 6;
        while (IsWs(*q)) q++;
        if (*q == ':')
        {
            q++;
            while (IsWs(*q)) q++;
            if (*q == '"')
            {
                char name[64];
                if (TakeJsonString(&q, name, sizeof(name)))
                {
                    int dup = 0;
                    for (uint32_t i = 0; i < count; i++)
                    {
                        if (strcmp(names[i], name) == 0)
                        {
                            dup = 1;
                            break;
                        }
                    }
                    if (!dup && name[0] != '\0')
                    {
                        strncpy(names[count], name, 63);
                        names[count][63] = '\0';
                        count++;
                    }
                }
            }
        }
        p = q;
    }
    return (int)count;
}

void ServerInspectToolResponse(OPENAI_TOOL_RESPONSE *resp)
{
    if (resp == NULL || resp->content[0] == '\0')
        return;

    /* 1. Check for JSON "exit_code": N, "returncode": N, or "code": N */
    const char *ec = strstr(resp->content, "\"exit_code\"");
    if (!ec) ec = strstr(resp->content, "\"returncode\"");
    if (!ec) ec = strstr(resp->content, "\"code\"");
    if (ec)
    {
        const char *col = strchr(ec, ':');
        if (col)
        {
            col++;
            while (*col == ' ' || *col == '\t') col++;
            resp->has_exit_code = 1;
            resp->exit_code = atoi(col);
            if (resp->exit_code != 0)
                resp->is_error = 1;
        }
    }

    /* 2. Check for JSON "status": "error" or "status": "failed" */
    const char *st = strstr(resp->content, "\"status\"");
    if (st)
    {
        if (strstr(st, "\"error\"") || strstr(st, "\"failed\"") || strstr(st, "\"fail\""))
            resp->is_error = 1;
    }

    /* 3. Run abductive compiler/linter diagnostic parser */
    DIAGNOSTIC_REPORT diag;
    memset(&diag, 0, sizeof(diag));
    DiagnosticParseOutput(resp->content, &diag);
    if (diag.error_count > 0)
    {
        resp->is_error = 1;
    }

    /* 4. Check for explicit build / test failure patterns */
    if (strstr(resp->content, "FAILED") != NULL ||
        strstr(resp->content, "BUILD FAILED") != NULL ||
        strstr(resp->content, "Assertion failed") != NULL ||
        strstr(resp->content, "No such file or directory") != NULL)
    {
        if (!strstr(resp->content, "0 failed") &&
            !strstr(resp->content, "failures=0") &&
            !strstr(resp->content, "0 tests failed"))
        {
            resp->is_error = 1;
        }
    }
}

int ServerExtractLastToolResponse(const char *body, OPENAI_TOOL_RESPONSE *out)
{
    if (body == NULL || out == NULL)
        return 0;

    memset(out, 0, sizeof(*out));
    const char *p = body;
    int found = 0;

    while (*p != '\0')
    {
        if (*p == '"' && strncmp(p, "\"role\"", 6) == 0)
        {
            const char *q = p + 6;
            while (IsWs(*q)) q++;
            if (*q == ':')
            {
                q++;
                while (IsWs(*q)) q++;
                if (strncmp(q, "\"tool\"", 6) == 0)
                {
                    const char *obj_start = p;
                    while (obj_start > body && *obj_start != '{') obj_start--;

                    const char *obj_end = q;
                    int depth = 1;
                    while (*obj_end != '\0')
                    {
                        if (*obj_end == '{') depth++;
                        else if (*obj_end == '}') {
                            depth--;
                            if (depth == 0) break;
                        }
                        obj_end++;
                    }

                    const char *s = obj_start;
                    while (s < obj_end)
                    {
                        if (strncmp(s, "\"tool_call_id\"", 14) == 0)
                        {
                            const char *v = s + 14;
                            while (IsWs(*v)) v++;
                            if (*v == ':') {
                                v++;
                                while (IsWs(*v)) v++;
                                TakeJsonString(&v, out->tool_call_id, sizeof(out->tool_call_id));
                            }
                        }
                        else if (strncmp(s, "\"name\"", 6) == 0)
                        {
                            const char *v = s + 6;
                            while (IsWs(*v)) v++;
                            if (*v == ':') {
                                v++;
                                while (IsWs(*v)) v++;
                                TakeJsonString(&v, out->name, sizeof(out->name));
                            }
                        }
                        else if (strncmp(s, "\"content\"", 9) == 0)
                        {
                            const char *v = s + 9;
                            while (IsWs(*v)) v++;
                            if (*v == ':') {
                                v++;
                                while (IsWs(*v)) v++;
                                TakeJsonString(&v, out->content, sizeof(out->content));
                            }
                        }
                        s++;
                    }
                    out->has_response = 1;
                    ServerInspectToolResponse(out);
                    found = 1;
                    p = obj_end;
                    continue;
                }
            }
        }
        p++;
    }
    return found;
}

int ServerExtractLastRole(const char *body, char *out, size_t size)
{
    if (body == NULL || out == NULL || size == 0)
        return 0;
    out[0] = '\0';
    const char *p = body;
    char last[32] = {0};
    int found = 0;

    while (*p != '\0')
    {
        if (*p == '"' && strncmp(p, "\"role\"", 6) == 0)
        {
            const char *q = p + 6;
            while (IsWs(*q)) q++;
            if (*q == ':')
            {
                q++;
                while (IsWs(*q)) q++;
                if (*q == '"')
                {
                    q++;
                    char tmp[32];
                    size_t o = 0;
                    while (*q != '\0' && *q != '"' && o + 1 < sizeof(tmp))
                        tmp[o++] = *q++;
                    tmp[o] = '\0';
                    if (*q == '"')
                    {
                        strncpy(last, tmp, sizeof(last) - 1);
                        last[sizeof(last) - 1] = '\0';
                        found = 1;
                    }
                }
            }
        }
        p++;
    }
    if (!found) return 0;
    strncpy(out, last, size - 1);
    out[size - 1] = '\0';
    return 1;
}

int ServerBuildToolCallResponse(const char *model, long created,
                                unsigned long seq, const OPENAI_TOOL_CALLS *tc,
                                const char *content_thought, char *out,
                                size_t size)
{
    if (model == NULL || tc == NULL || tc->count == 0 || out == NULL || size == 0)
        return 0;

    char tool_calls_json[8192];
    tool_calls_json[0] = '\0';
    size_t rem = sizeof(tool_calls_json);

    for (uint32_t i = 0; i < tc->count && i < SERVER_MAX_TOOL_CALLS; i++)
    {
        char item[4096];
        char esc_args[4096];
        if (!ServerJsonEscape(tc->calls[i].arguments, esc_args, sizeof(esc_args)))
            return 0;

        int len = snprintf(item, sizeof(item),
                           "%s{\"id\":\"%s\",\"type\":\"function\",\"function\":"
                           "{\"name\":\"%s\",\"arguments\":\"%s\"}}",
                           (i > 0) ? "," : "",
                           tc->calls[i].id,
                           tc->calls[i].name,
                           esc_args);
        if (len <= 0 || (size_t)len >= sizeof(item))
            return 0;

        strncat(tool_calls_json, item, rem - strlen(tool_calls_json) - 1);
    }

    char content_json[1024];
    if (content_thought != NULL && content_thought[0] != '\0')
    {
        char esc_thought[512];
        ServerJsonEscape(content_thought, esc_thought, sizeof(esc_thought));
        snprintf(content_json, sizeof(content_json), "\"%s\"", esc_thought);
    }
    else
    {
        strncpy(content_json, "null", sizeof(content_json) - 1);
    }

    int w = snprintf(out, size,
                     "{\"id\":\"chatcmpl-symbols-%lu\",\"object\":\"chat."
                     "completion\",\"created\":%ld,\"model\":\"%s\","
                     "\"choices\":[{\"index\":0,\"message\":{\"role\":"
                     "\"assistant\",\"content\":%s,\"tool_calls\":[%s]},"
                     "\"finish_reason\":\"tool_calls\"}],\"usage\":{\"prompt_tokens\":10,"
                     "\"completion_tokens\":25,\"total_tokens\":35}}",
                     seq, created, model, content_json, tool_calls_json);

    return (w > 0 && (size_t)w < size) ? 1 : 0;
}

int ServerBuildToolCallStreamResponse(const char *model, long created,
                                      unsigned long seq, const OPENAI_TOOL_CALLS *tc,
                                      char *out, size_t size)
{
    if (model == NULL || tc == NULL || tc->count == 0 || out == NULL || size == 0)
        return 0;

    char tc_delta[8192];
    tc_delta[0] = '\0';
    for (uint32_t i = 0; i < tc->count && i < SERVER_MAX_TOOL_CALLS; i++)
    {
        char item[4096];
        char esc_args[4096];
        ServerJsonEscape(tc->calls[i].arguments, esc_args, sizeof(esc_args));
        snprintf(item, sizeof(item),
                 "%s{\"index\":%u,\"id\":\"%s\",\"type\":\"function\",\"function\":"
                 "{\"name\":\"%s\",\"arguments\":\"%s\"}}",
                 (i > 0) ? "," : "",
                 i, tc->calls[i].id, tc->calls[i].name, esc_args);
        strncat(tc_delta, item, sizeof(tc_delta) - strlen(tc_delta) - 1);
    }

    int w = snprintf(out, size,
                     "data: {\"id\":\"chatcmpl-symbols-%lu\",\"object\":\"chat.completion.chunk\","
                     "\"created\":%ld,\"model\":\"%s\",\"choices\":[{\"index\":0,\"delta\":"
                     "{\"role\":\"assistant\",\"content\":null,\"tool_calls\":[%s]},\"finish_reason\":null}]}\n\n"
                     "data: {\"id\":\"chatcmpl-symbols-%lu\",\"object\":\"chat.completion.chunk\","
                     "\"created\":%ld,\"model\":\"%s\",\"choices\":[{\"index\":0,\"delta\":{},"
                     "\"finish_reason\":\"tool_calls\"}]}\n\n"
                     "data: [DONE]\n\n",
                     seq, created, model, tc_delta,
                     seq, created, model);

    return (w > 0 && (size_t)w < size) ? 1 : 0;
}

static int MatchWordBoundary(const char *text, const char *kw)
{
    size_t kwlen = strlen(kw);
    const char *p = text;
    while ((p = strstr(p, kw)) != NULL)
    {
        int before_ok = (p == text || (!isalnum((unsigned char)*(p - 1)) && *(p - 1) != '_'));
        char after_char = *(p + kwlen);
        int after_ok = (after_char == '\0' || (!isalnum((unsigned char)after_char) && after_char != '_'));
        if (before_ok && after_ok)
            return 1;
        p++;
    }
    return 0;
}

int ServerIsInspectionTask(const char *text)
{
    if (text == NULL || text[0] == '\0')
        return 0;

    char lower[1024];
    size_t i = 0;
    while (text[i] != '\0' && i < sizeof(lower) - 1)
    {
        lower[i] = (char)tolower((unsigned char)text[i]);
        i++;
    }
    lower[i] = '\0';

    /* If it contains mutation/build action keywords, it's a mutation/build task, not inspection */
    static const char *action_keywords[] = {
        "fix", "bug", "patch", "refactor", "compile", "build", "test",
        "tests", "replan", "repair", "hunk", "diff", "arregla", "corrige",
        "compila", "compilar", "ejecuta", "ejecutar"
    };
    for (size_t k = 0; k < sizeof(action_keywords) / sizeof(action_keywords[0]); k++)
    {
        if (MatchWordBoundary(lower, action_keywords[k]))
            return 0;
    }

    static const char *inspect_keywords[] = {
        "review", "inspect", "folder", "directory", "codebase", "repo",
        "repository", "files", "workspace", "list", "explore",
        "revisa", "revisar", "inspecciona", "inspeccionar", "carpeta",
        "directorio", "repositorio", "archivos", "ficheros", "explora",
        "explorar", "muestra", "mostrar", "mira", "mirar"
    };
    for (size_t k = 0; k < sizeof(inspect_keywords) / sizeof(inspect_keywords[0]); k++)
    {
        if (MatchWordBoundary(lower, inspect_keywords[k]))
            return 1;
    }

    return 0;
}

int ServerIsCodingTask(const char *text)
{
    if (text == NULL || text[0] == '\0')
        return 0;

    char lower[1024];
    size_t i = 0;
    while (text[i] != '\0' && i < sizeof(lower) - 1)
    {
        lower[i] = (char)tolower((unsigned char)text[i]);
        i++;
    }
    lower[i] = '\0';

    static const char *exts[] = {
        ".c", ".h", ".cpp", ".cc", ".py", ".js", ".ts", ".go", ".rs", ".sh", ".diff", ".patch"
    };
    for (size_t k = 0; k < sizeof(exts) / sizeof(exts[0]); k++)
    {
        const char *ep = strstr(lower, exts[k]);
        if (ep != NULL && ep > lower && isalnum((unsigned char)*(ep - 1)))
        {
            char after = *(ep + strlen(exts[k]));
            if (after == '\0' || isspace((unsigned char)after) || ispunct((unsigned char)after))
                return 1;
        }
    }

    static const char *coding_keywords[] = {
        "fix", "bug", "patch", "refactor", "compile", "build", "test",
        "tests", "gcc", "clang", "make", "cmake", "ctest", "function",
        "struct", "segfault", "syntax", "pull request", "commit", "git",
        "hunk", "diff", "regression", "rollback", "symbol", "symbols",
        "header", "workspace", "codebase", "repo", "repository", "files",
        "folder", "directory", "review", "inspect",
        "revisa", "revisar", "inspecciona", "inspeccionar", "carpeta",
        "directorio", "repositorio", "archivos", "ficheros", "codigo",
        "analiza", "analizar", "arregla", "corrige", "compilar", "compila",
        "ejecuta", "ejecutar"
    };

    for (size_t k = 0; k < sizeof(coding_keywords) / sizeof(coding_keywords[0]); k++)
    {
        if (MatchWordBoundary(lower, coding_keywords[k]))
            return 1;
    }

    if (ServerIsInspectionTask(text))
        return 1;

    return 0;
}


