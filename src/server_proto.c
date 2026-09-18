/* server_proto: OpenAI-compatible wire layer (pure, testable).
   English code comments (project rule). No sockets: only byte
   handling, JSON building, and the engine call. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <io.h>
#include <windows.h> /* GetTempPathA (capture sink location only) */
#include "bible_chat.h"
#include "server_proto.h"

#define SERVER_CAP_FILE "server_proto_cap.tmp"

/* capture sink lives in the system temp dir: the server must answer
   identically no matter which CWD it was started from (C:\ root is
   not writable, which used to yield empty replies). */
static void CapPath(char *out, size_t size)
{
    char tmp[512];
    DWORD n = GetTempPathA(sizeof(tmp), tmp);
    if (n == 0 || n >= sizeof(tmp))
        strncpy(tmp, ".", sizeof(tmp) - 1);
    snprintf(out, size, "%s%s", tmp, SERVER_CAP_FILE);
}

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
    int saved;
    FILE *cap;
    FILE *in;
    size_t n = 0;
    int c;
    if (ch == NULL || query == NULL || out == NULL || size == 0)
        return 0;
    out[0] = '\0';
    fflush(stdout);
    saved = _dup(1);
    {
        char cappath[640];
        CapPath(cappath, sizeof(cappath));
        cap = fopen(cappath, "w");
        if (saved < 0 || cap == NULL)
            return 0;
        fflush(cap);
        _dup2(_fileno(cap), 1);
        ChatHandle(ch, query);
        fflush(stdout);
        _dup2(saved, 1);
        _close(saved);
        fclose(cap);
        in = fopen(cappath, "r");
        if (in == NULL)
            return 0;
        while ((c = fgetc(in)) != EOF && n + 1 < size)
            out[n++] = (char)c;
        fclose(in);
        remove(cappath);
    }
    out[n] = '\0';
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
