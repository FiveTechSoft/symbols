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
    while (*p != '\0' && *p != '"')
    {
        if (*p == '\\')
        {
            unsigned v = 0, d = 0;
            char tmp[4];
            int nb = 0;
            p++;
            char esc_c = 0;
            if (*p == 'n') esc_c = '\n';
            else if (*p == 'r') esc_c = '\r';
            else if (*p == 't') esc_c = '\t';
            else if (*p == 'b') esc_c = '\b';
            else if (*p == 'f') esc_c = '\f';
            else if (*p == '\\' || *p == '"' || *p == '/') esc_c = *p;

            if (esc_c != 0)
            {
                if (o + 1 < size)
                    out[o++] = esc_c;
                p++;
            }
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
                    /* non-BMP: emit replacement */
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
                    if (o + (size_t)nb < size)
                    {
                        memcpy(out + o, tmp, (size_t)nb);
                        o += (size_t)nb;
                    }
                }
            }
            else
            {
                if (*p != '\0') p++;
            }
        }
        else
        {
            if (o + 1 < size)
                out[o++] = *p;
            p++;
        }
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
                        else if (*s == '[')
                        {
                            const char *tkey = strstr(s, "\"text\"");
                            if (tkey != NULL)
                            {
                                const char *tc = strchr(tkey, ':');
                                if (tc != NULL)
                                {
                                    tc++;
                                    while (IsWs(*tc)) tc++;
                                    if (*tc == '"' && TakeJsonString(&tc, content, sizeof(content)))
                                    {
                                        strncpy(last, content, sizeof(last) - 1);
                                        found = 1;
                                        ok = 1;
                                    }
                                }
                            }
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

    const char *arr_start = strchr(p, '[');
    if (arr_start == NULL)
        return 0;

    int depth = 1;
    const char *arr_end = arr_start + 1;
    while (*arr_end != '\0' && depth > 0)
    {
        if (*arr_end == '[') depth++;
        else if (*arr_end == ']') depth--;
        arr_end++;
    }

    p = arr_start;
    while ((p = strstr(p, "\"name\"")) != NULL && p < arr_end && count < max_names)
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

static int IsInspectionToolName(const char *name)
{
    if (name == NULL || name[0] == '\0')
        return 0;
    if (strcmp(name, "glob") == 0 ||
        strcmp(name, "read") == 0 ||
        strcmp(name, "grep") == 0 ||
        strcmp(name, "locate_symbol") == 0 ||
        strcmp(name, "view_file") == 0 ||
        strcmp(name, "find_by_name") == 0 ||
        strcmp(name, "grep_search") == 0)
    {
        return 1;
    }
    return 0;
}

static int TakeJsonContent(const char **pp, char *out, size_t size)
{
    if (pp == NULL || *pp == NULL || out == NULL || size == 0)
        return 0;
    out[0] = '\0';
    while (IsWs(**pp)) (*pp)++;
    if (**pp == '"')
    {
        return TakeJsonString(pp, out, size);
    }
    else if (**pp == '[')
    {
        const char *p = *pp;
        /* Check if array contains multi-part text objects: [{"type":"text","text":"..."}] */
        const char *t = strstr(p, "\"text\"");
        if (t != NULL)
        {
            /* Find end of outer array */
            const char *end = p;
            int arr_depth = 0;
            int in_quote = 0;
            while (*end != '\0')
            {
                if (*end == '"' && (end == p || *(end - 1) != '\\'))
                    in_quote = !in_quote;
                if (!in_quote)
                {
                    if (*end == '[') arr_depth++;
                    else if (*end == ']') {
                        arr_depth--;
                        if (arr_depth == 0) { end++; break; }
                    }
                }
                end++;
            }

            const char *cur = p;
            size_t o = 0;
            int found_any = 0;
            while (cur < end && (t = strstr(cur, "\"text\"")) != NULL && t < end)
            {
                const char *v = t + 6;
                while (IsWs(*v)) v++;
                if (*v == ':')
                {
                    v++;
                    while (IsWs(*v)) v++;
                    if (*v == '"')
                    {
                        char chunk[4096];
                        if (TakeJsonString(&v, chunk, sizeof(chunk)))
                        {
                            if (found_any && o + 1 < size)
                                out[o++] = '\n';
                            size_t clen = strlen(chunk);
                            if (o + clen < size)
                            {
                                memcpy(out + o, chunk, clen);
                                o += clen;
                                out[o] = '\0';
                            }
                            found_any = 1;
                        }
                    }
                }
                cur = (v > t) ? v : t + 6;
            }
            *pp = end;
            return found_any ? 1 : 0;
        }
        else
        {
            /* Raw JSON array: copy balanced array literal */
            int depth = 0;
            int in_quote = 0;
            size_t o = 0;
            while (*p != '\0')
            {
                if (*p == '"' && (p == *pp || *(p - 1) != '\\'))
                    in_quote = !in_quote;
                if (!in_quote)
                {
                    if (*p == '[') depth++;
                    else if (*p == ']') {
                        depth--;
                        if (o + 1 < size) out[o++] = *p;
                        if (depth == 0) { p++; break; }
                        p++;
                        continue;
                    }
                }
                if (o + 1 < size) out[o++] = *p;
                p++;
            }
            out[o] = '\0';
            *pp = p;
            return o > 0 ? 1 : 0;
        }
    }
    return 0;
}

void ServerInspectToolResponse(OPENAI_TOOL_RESPONSE *resp)
{
    if (resp == NULL || resp->content[0] == '\0')
        return;

    resp->is_error = 0;

    /* 1. Check for JSON "exit_code": N, "returncode": N, "exitCode": N, "status_code": N, or "code": N */
    const char *ec = strstr(resp->content, "\"exit_code\"");
    if (!ec) ec = strstr(resp->content, "\"returncode\"");
    if (!ec) ec = strstr(resp->content, "\"return_code\"");
    if (!ec) ec = strstr(resp->content, "\"exitCode\"");
    if (!ec) ec = strstr(resp->content, "\"status_code\"");
    if (!ec) ec = strstr(resp->content, "\"code\"");
    if (ec)
    {
        const char *col = strchr(ec, ':');
        if (col)
        {
            col++;
            while (*col == ' ' || *col == '\t' || *col == '\r' || *col == '\n') col++;
            if (*col == '-' || isdigit((unsigned char)*col))
            {
                resp->has_exit_code = 1;
                resp->exit_code = atoi(col);
                if (resp->exit_code != 0)
                    resp->is_error = 1;
            }
        }
    }

    /* 2a. Check for JSON "status": "error" or "status": "failed" (strictly inspect value) */
    const char *st = strstr(resp->content, "\"status\"");
    if (st)
    {
        const char *col = strchr(st, ':');
        if (col)
        {
            col++;
            while (*col == ' ' || *col == '\t' || *col == '\r' || *col == '\n' || *col == '"') col++;
            if (strncasecmp(col, "error", 5) == 0 ||
                strncasecmp(col, "fail", 4) == 0)
            {
                resp->is_error = 1;
            }
        }
    }

    /* 2b. Check for JSON "isError": true or "is_error": true */
    const char *ie = strstr(resp->content, "\"isError\"");
    if (!ie) ie = strstr(resp->content, "\"is_error\"");
    if (ie)
    {
        const char *col = strchr(ie, ':');
        if (col)
        {
            col++;
            while (*col == ' ' || *col == '\t' || *col == '\r' || *col == '\n') col++;
            if (strncmp(col, "true", 4) == 0)
            {
                resp->is_error = 1;
            }
        }
    }

    /* 2c. Check for JSON "error": <string> (ignore "error": null / false / "" / 0) */
    const char *ef = strstr(resp->content, "\"error\"");
    if (ef)
    {
        const char *col = strchr(ef, ':');
        if (col)
        {
            col++;
            while (*col == ' ' || *col == '\t' || *col == '\r' || *col == '\n') col++;
            if (strncmp(col, "true", 4) == 0)
            {
                resp->is_error = 1;
            }
            else if (*col == '"' && *(col + 1) != '"')
            {
                resp->is_error = 1;
            }
        }
    }

    /* Read-only inspection tools (glob, read, grep) must never be scanned for build/compiler errors */
    if (IsInspectionToolName(resp->name))
    {
        return;
    }

    /* 3. Run abductive compiler/linter diagnostic parser on command/build output */
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
        strstr(resp->content, "No such file or directory") != NULL ||
        strstr(resp->content, "Error:") != NULL ||
        strstr(resp->content, "error:") != NULL ||
        strstr(resp->content, "FATAL:") != NULL ||
        strstr(resp->content, "fatal:") != NULL ||
        strstr(resp->content, "No tests were found") != NULL ||
        strstr(resp->content, "is not recognized as an internal") != NULL ||
        strstr(resp->content, "command not found") != NULL ||
        strstr(resp->content, "Permission denied") != NULL)
    {
        if (!strstr(resp->content, "0 failed") &&
            !strstr(resp->content, "0 errors") &&
            !strstr(resp->content, "failures=0") &&
            !strstr(resp->content, "0 tests failed"))
        {
            resp->is_error = 1;
        }
    }
}

int ServerFormatInspectionOutput(const char *in, char *out, size_t size)
{
    if (in == NULL || out == NULL || size == 0)
        return 0;
    out[0] = '\0';

    /* Check if input contains JSON array under "matches": [...] or "files": [...] */
    const char *m = strstr(in, "\"matches\"");
    if (!m) m = strstr(in, "\"files\"");
    if (m)
    {
        const char *bracket = strchr(m, '[');
        if (bracket)
        {
            const char *p = bracket + 1;
            size_t o = 0;
            int count = 0;
            while (*p != '\0' && *p != ']')
            {
                while (isspace((unsigned char)*p) || *p == ',') p++;
                if (*p == '"')
                {
                    char item[512];
                    if (TakeJsonString(&p, item, sizeof(item)))
                    {
                        if (count > 0 && o + 1 < size)
                            out[o++] = '\n';
                        size_t ilen = strlen(item);
                        if (o + ilen < size)
                        {
                            memcpy(out + o, item, ilen);
                            o += ilen;
                            out[o] = '\0';
                        }
                        count++;
                    }
                }
                else
                {
                    p++;
                }
            }
            if (count > 0)
                return 1;
        }
    }

    /* Check if input itself is a raw JSON string array: ["file1", "file2", ...] */
    const char *p = in;
    while (IsWs(*p)) p++;
    if (*p == '[')
    {
        p++;
        size_t o = 0;
        int count = 0;
        while (*p != '\0' && *p != ']')
        {
            while (isspace((unsigned char)*p) || *p == ',') p++;
            if (*p == '"')
            {
                char item[512];
                if (TakeJsonString(&p, item, sizeof(item)))
                {
                    if (count > 0 && o + 1 < size)
                        out[o++] = '\n';
                    size_t ilen = strlen(item);
                    if (o + ilen < size)
                    {
                        memcpy(out + o, item, ilen);
                        o += ilen;
                        out[o] = '\0';
                    }
                    count++;
                }
            }
            else
            {
                p++;
            }
        }
        if (count > 0)
            return 1;
    }

    /* Fallback: copy raw input */
    strncpy(out, in, size - 1);
    out[size - 1] = '\0';
    return 1;
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
                                TakeJsonContent(&v, out->content, sizeof(out->content));
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

    /* Data structures are not workspace or filesystem inspections */
    if (strstr(lower, "lista enlazada") != NULL || strstr(lower, "linked list") != NULL)
        return 0;

    static const char *inspect_keywords[] = {
        "review", "inspect", "folder", "directory", "codebase", "repo",
        "repository", "files", "workspace", "list", "explore", "project",
        "revisa", "revisar", "inspecciona", "inspeccionar", "carpeta",
        "directorio", "repositorio", "archivos", "ficheros", "explora",
        "explorar", "muestra", "mostrar", "mira", "mirar", "proyecto",
        "dir", "ls", "pwd", "tree", "subcarpetas", "subcarpeta",
        "subdirectorios", "subdirectorio", "subfolders", "subdirectories",
        "lista", "listar"
    };
    for (size_t k = 0; k < sizeof(inspect_keywords) / sizeof(inspect_keywords[0]); k++)
    {
        if (MatchWordBoundary(lower, inspect_keywords[k]))
            return 1;
    }

    /* Check for wildcard glob masks (*.*, *.c, etc.) */
    if (strstr(lower, "*.*") != NULL || strchr(lower, '*') != NULL)
    {
        for (size_t k = 0; k < strlen(lower); k++)
        {
            if (lower[k] == '*' && (k + 1 < strlen(lower) && (lower[k + 1] == '.' || isalnum((unsigned char)lower[k + 1]))))
                return 1;
            if (lower[k] == '*' && k > 0 && lower[k - 1] == '.')
                return 1;
            if (lower[k] == '*' && (k == 0 || isspace((unsigned char)lower[k - 1])) &&
                (k + 1 == strlen(lower) || isspace((unsigned char)lower[k + 1])))
                return 1;
        }
    }

    return 0;
}

static int EditDistance(const char *s1, const char *s2)
{
    int len1 = (int)strlen(s1);
    int len2 = (int)strlen(s2);
    if (abs(len1 - len2) > 2) return 99;

    int dp[32][32];
    if (len1 >= 30 || len2 >= 30) return 99;
    for (int i = 0; i <= len1; i++) dp[i][0] = i;
    for (int j = 0; j <= len2; j++) dp[0][j] = j;

    for (int i = 1; i <= len1; i++)
    {
        for (int j = 1; j <= len2; j++)
        {
            int cost = (tolower((unsigned char)s1[i - 1]) == tolower((unsigned char)s2[j - 1])) ? 0 : 1;
            int d1 = dp[i - 1][j] + 1;
            int d2 = dp[i][j - 1] + 1;
            int d3 = dp[i - 1][j - 1] + cost;
            int min = d1 < d2 ? d1 : d2;
            dp[i][j] = min < d3 ? min : d3;
        }
    }
    return dp[len1][len2];
}

static int MatchesAlgorithmKeyword(const char *text)
{
    if (!text) return 0;
    if (strstr(text, "fibonacci") != NULL || strstr(text, "fib") != NULL ||
        strstr(text, "factorial") != NULL || strstr(text, "quicksort") != NULL ||
        strstr(text, "mergesort") != NULL || strstr(text, "bubblesort") != NULL ||
        strstr(text, "lista enlazada") != NULL || strstr(text, "linked list") != NULL ||
        strstr(text, "array dinamico") != NULL || strstr(text, "dynamic array") != NULL ||
        strstr(text, "leer archivo") != NULL || strstr(text, "leer fichero") != NULL ||
        strstr(text, "read file") != NULL || strstr(text, "qsort") != NULL)
        return 1;

    char buf[512];
    strncpy(buf, text, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *tok = strtok(buf, " \t\r\n,;\"'¿?.!():");
    while (tok)
    {
        if (strlen(tok) >= 4)
        {
            if (EditDistance(tok, "fibonacci") <= 2) return 1;
            if (EditDistance(tok, "factorial") <= 2) return 1;
            if (EditDistance(tok, "quicksort") <= 2) return 1;
        }
        tok = strtok(NULL, " \t\r\n,;\"'¿?.!():");
    }
    return 0;
}

int ServerIsCodeSynthesisTask(const char *text)
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

    /* If it contains repository action/mutation keywords on existing files, it's a bugfix/action task */
    static const char *repo_actions[] = {
        "fix", "bug", "patch", "refactor", "compile", "build", "test",
        "tests", "replan", "repair", "hunk", "diff", "arregla", "corrige",
        "compila", "compilar", "ejecuta", "ejecutar", "ctest", "cmake"
    };
    for (size_t k = 0; k < sizeof(repo_actions) / sizeof(repo_actions[0]); k++)
    {
        if (MatchWordBoundary(lower, repo_actions[k]))
            return 0;
    }

    /* Standalone algorithm / coding prompt keywords (including typo tolerance) */
    if (MatchesAlgorithmKeyword(lower))
        return 1;

    if (ServerIsInspectionTask(text))
        return 0;

    static const char *synth_verbs[] = {
        "escribe", "escribir", "crea", "crear", "genera", "generar",
        "haz", "hacer", "programa", "programar", "implementa", "implementar",
        "desarrolla", "desarrollar", "write", "generate", "implement", "code",
        "dame", "give", "muestra", "mostrar"
    };
    int has_verb = 0;
    for (size_t k = 0; k < sizeof(synth_verbs) / sizeof(synth_verbs[0]); k++)
    {
        if (MatchWordBoundary(lower, synth_verbs[k]))
        {
            has_verb = 1;
            break;
        }
    }

    static const char *synth_nouns[] = {
        "funcion", "función", "funciones", "function", "functions",
        "metodo", "método", "method", "methods", "algoritmo", "algorithm",
        "programa", "program", "codigo", "código", "code",
        "quicksort", "sort", "ordenar", "ordenamiento",
        "busqueda", "búsqueda", "puntero", "punteros", "pointer", "pointers",
        "invertir", "reverse", "ejemplo", "example",
        "lista", "list", "nodo", "node", "vector", "pila", "stack", "cola", "queue",
        "archivo", "fichero", "file", "lectura", "qsort"
    };
    int has_noun = 0;
    for (size_t k = 0; k < sizeof(synth_nouns) / sizeof(synth_nouns[0]); k++)
    {
        if (strstr(lower, synth_nouns[k]) != NULL)
        {
            has_noun = 1;
            break;
        }
    }

    static const char *synth_langs[] = {
        "en c", "en c11", "in c", "in c11", "c code", "codigo c", "código c",
        "lenguaje c", "c language", "en python", "in python", "en js", "in js",
        "en javascript", "in javascript", "en typescript"
    };
    int has_lang = 0;
    for (size_t k = 0; k < sizeof(synth_langs) / sizeof(synth_langs[0]); k++)
    {
        if (strstr(lower, synth_langs[k]) != NULL)
        {
            has_lang = 1;
            break;
        }
    }

    if (has_noun && has_lang)
        return 1;

    if (has_verb && has_noun)
        return 1;

    return 0;
}

int ServerIsCodingTask(const char *text)
{
    if (text == NULL || text[0] == '\0')
        return 0;

    if (ServerIsCodeSynthesisTask(text))
        return 1;

    char lower[1024];
    size_t i = 0;
    while (text[i] != '\0' && i < sizeof(lower) - 1)
    {
        lower[i] = (char)tolower((unsigned char)text[i]);
        i++;
    }
    lower[i] = '\0';

    static const char *exts[] = {
        ".c", ".h", ".cpp", ".cc", ".py", ".js", ".ts", ".go", ".rs", ".sh",
        ".diff", ".patch", ".md", ".txt", ".json", ".yml", ".yaml", ".toml"
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
        "folder", "directory", "review", "inspect", "project",
        "revisa", "revisar", "inspecciona", "inspeccionar", "carpeta",
        "directorio", "repositorio", "archivos", "ficheros", "codigo",
        "analiza", "analizar", "arregla", "corrige", "compilar", "compila",
        "ejecuta", "ejecutar", "dir", "ls", "pwd", "tree", "status", "proyecto",
        "subcarpetas", "subcarpeta", "subdirectorios", "subdirectorio",
        "subfolders", "subdirectories", "lista", "listar",
        "crear", "crea", "create", "archivo", "fichero",
        "escribe", "escribir", "genera", "generar", "programa", "programar",
        "implementa", "implementar", "funcion", "función", "funciones",
        "fibonacci", "factorial"
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

int ServerIsGreeting(const char *text)
{
    if (text == NULL || text[0] == '\0')
        return 0;

    char norm[512];
    size_t i = 0, j = 0;
    while (text[i] != '\0' && j < sizeof(norm) - 1)
    {
        unsigned char c = (unsigned char)text[i];
        if (isalnum(c) || c == ' ')
            norm[j++] = (char)tolower(c);
        else if (c == 0xc3)
        {
            /* UTF-8 accented vowels */
            unsigned char c2 = (unsigned char)text[++i];
            if (c2 == 0xa1 || c2 == 0x81) norm[j++] = 'a';
            else if (c2 == 0xa9 || c2 == 0x89) norm[j++] = 'e';
            else if (c2 == 0xad || c2 == 0x8d) norm[j++] = 'i';
            else if (c2 == 0xb3 || c2 == 0x93) norm[j++] = 'o';
            else if (c2 == 0xba || c2 == 0x9a) norm[j++] = 'u';
            else norm[j++] = ' ';
        }
        else
            norm[j++] = ' ';
        i++;
    }
    norm[j] = '\0';

    /* Collapse multiple spaces and trim */
    char clean[512];
    size_t c_len = 0;
    int prev_space = 1;
    for (size_t k = 0; k < j; k++)
    {
        if (norm[k] == ' ')
        {
            if (!prev_space && c_len + 1 < sizeof(clean))
            {
                clean[c_len++] = ' ';
                prev_space = 1;
            }
        }
        else
        {
            if (c_len + 1 < sizeof(clean))
            {
                clean[c_len++] = norm[k];
                prev_space = 0;
            }
        }
    }
    if (c_len > 0 && clean[c_len - 1] == ' ')
        c_len--;
    clean[c_len] = '\0';

    if (clean[0] == '\0')
        return 0;

    static const char *GREETINGS[] = {
        "hola", "hello", "hi", "hey", "buenos dias", "buenas tardes",
        "buenas noches", "buenas", "saludos", "que tal", "como estas",
        "how are you", "good morning", "good afternoon", "good evening",
        "quien eres", "quien eres tu", "who are you", "what are you",
        "que eres", "presentate", "que sabes hacer", "what can you do",
        "hola buenos dias", "hola buenas tardes", "hola buenas noches",
        "hola que tal", "hello there"
    };

    for (size_t k = 0; k < sizeof(GREETINGS) / sizeof(GREETINGS[0]); k++)
    {
        if (strcmp(clean, GREETINGS[k]) == 0)
            return 1;
    }

    return 0;
}

int ServerAnswerGreeting(const char *query, int persona_id, char *out, size_t out_sz)
{
    if (!out || out_sz == 0)
        return 0;
    out[0] = '\0';
    if (!query) query = "";

    char lower[512];
    size_t i = 0;
    while (query[i] != '\0' && i < sizeof(lower) - 1)
    {
        lower[i] = (char)tolower((unsigned char)query[i]);
        i++;
    }
    lower[i] = '\0';

    int is_english = (strstr(lower, "hello") || strstr(lower, "hi") || strstr(lower, "hey") ||
                      strstr(lower, "who are you") || strstr(lower, "what are you") ||
                      strstr(lower, "how are you") || strstr(lower, "what can you do") ||
                      strstr(lower, "good morning") || strstr(lower, "good afternoon"));

    int is_identity = (strstr(lower, "quien eres") || strstr(lower, "who are you") ||
                       strstr(lower, "what are you") || strstr(lower, "que eres") ||
                       strstr(lower, "presentate") || strstr(lower, "que sabes hacer") ||
                       strstr(lower, "what can you do"));

    /* Persona 6: PERSONA_PIRATE_QUANTUM */
    if (persona_id == 6)
    {
        if (is_identity)
        {
            snprintf(out, out_sz,
                "¡Arrr! Soy Symbols, corsario del ciberespacio y motor simbólico de cálculo cuántico. "
                "Inspecciono código, parcho navíos de software y deduzco la verdad sin un solo gramo de alucinación estocástica.");
        }
        else
        {
            snprintf(out, out_sz,
                "¡Ahoy, camarada! Soy el contramaestre cuántico de Symbols. "
                "¿Qué singladura de código o misterio abordamos hoy en el navío?");
        }
        return 1;
    }

    if (is_english)
    {
        if (is_identity)
        {
            snprintf(out, out_sz,
                "I am Symbols, a local symbolic AI assistant and development copilot. "
                "I can help you inspect the repository, write and edit C, Python, and JavaScript code, "
                "analyze functions and dependencies, and answer queries from the indexed knowledge base without GPU or cloud dependencies.");
        }
        else
        {
            snprintf(out, out_sz,
                "Hello! I am Symbols, your local AI coding assistant. "
                "How can I help you today with your project or code?");
        }
    }
    else
    {
        if (is_identity)
        {
            snprintf(out, out_sz,
                "Soy Symbols, un copiloto y motor de inteligencia artificial simbólica local. "
                "Puedo ayudarte a explorar el repositorio, generar y modificar código en C, Python y JavaScript, "
                "analizar funciones y dependencias, y responder consultas sobre el conocimiento indexado sin dependencias externas ni GPUs.");
        }
        else
        {
            snprintf(out, out_sz,
                "¡Hola! Soy Symbols, tu copiloto local de IA y desarrollo. "
                "¿En qué puedo ayudarte hoy con tu proyecto o código?");
        }
    }

    return 1;
}



