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

/* --- Enclitic pronoun stripping + declarative edit-verb table --- */

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

static const struct { const char *es; const char *en; } EDIT_VERBS[] = {
    {"modifica",  "edit"},   {"modificar", "edit"},
    {"edita",     "edit"},   {"editar",    "edit"},
    {"cambia",    "edit"},   {"cambiar",   "edit"},
    {"reemplaza", "replace"}, {"reemplazar", "replace"},
    {"actualiza", "update"},  {"actualizar", "update"},
    {"añade",     "add"},    {"anade",      "add"},
    {"agrega",    "add"},    {"escribe",    "write"},
    {"insert",    "insert"},
};
#define EDIT_VERBS_N (sizeof(EDIT_VERBS) / sizeof(EDIT_VERBS[0]))

/* Strip Spanish/Portuguese diacritics: á→a, é→e, í→i, ó→o, ú→u, ñ→n, ü→u */
static void FoldAccent(const char *in, char *out, size_t n)
{
    size_t o = 0;
    if (in == NULL || out == NULL || n < 1) { if (out && n > 0) out[0] = '\0'; return; }
    while (*in && o + 1 < n)
    {
        unsigned char c = (unsigned char)*in;
        if (c == 0xC3) { /* UTF-8 2-byte: á=0xA1, é=0xA9, í=0xAD, ó=0xB3, ú=0xBA, ñ=0xB1, ü=0xBC */
            unsigned char next = (unsigned char)in[1];
            switch (next) {
                case 0xA1: out[o++] = 'a'; in += 2; continue;
                case 0xA9: out[o++] = 'e'; in += 2; continue;
                case 0xAD: out[o++] = 'i'; in += 2; continue;
                case 0xB3: out[o++] = 'o'; in += 2; continue;
                case 0xBA: out[o++] = 'u'; in += 2; continue;
                case 0xB1: out[o++] = 'n'; in += 2; continue;
                case 0xBC: out[o++] = 'u'; in += 2; continue;
                default: break;
            }
        }
        out[o++] = *in++;
    }
    out[o] = '\0';
}

static int MatchEditVerb(const char *lower)
{
    char buf[1024];
    char *tok;
    strncpy(buf, lower, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    tok = strtok(buf, " \t\r\n,.;:!?");
    while (tok)
    {
        char stem[256];
        char folded[256];
        char folded_stem[256];
        size_t tlen = strlen(tok);
        StripEnclitic(tok, stem, sizeof(stem));
        FoldAccent(tok, folded, sizeof(folded));
        FoldAccent(stem, folded_stem, sizeof(folded_stem));
        for (size_t i = 0; i < EDIT_VERBS_N; i++)
        {
            char tbl_folded[256];
            FoldAccent(EDIT_VERBS[i].es, tbl_folded, sizeof(tbl_folded));
            if (strcmp(tok, EDIT_VERBS[i].es) == 0 ||
                strcmp(tok, EDIT_VERBS[i].en) == 0 ||
                strcmp(stem, EDIT_VERBS[i].es) == 0 ||
                strcmp(stem, EDIT_VERBS[i].en) == 0 ||
                strcmp(folded, tbl_folded) == 0 ||
                strcmp(folded, EDIT_VERBS[i].en) == 0 ||
                strcmp(folded_stem, tbl_folded) == 0 ||
                strcmp(folded_stem, EDIT_VERBS[i].en) == 0)
                return 1;
        }
        tok = strtok(NULL, " \t\r\n,.;:!?");
    }
    return 0;
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

/* Derive a stable session key for clients (OpenCode 1.18.x) that send
   no explicit session identifier. The key hashes the two request spans
   that are byte-stable across every request of one session and differ
   across independent sessions: the first system message content (it
   carries the client working directory in its env block) and the first
   user message content. Spans are hashed as raw JSON string-token bytes,
   so no decoding buffer limits apply. Two sessions opened in the same
   directory with the same opening prompt are indistinguishable on the
   wire; that is a client protocol limitation, not extra sharing. */
static uint64_t FnvaUpdate(uint64_t h, const char *p, const char *end)
{
    while (p < end)
    {
        h ^= (unsigned char)*p;
        h *= 1099511628211ULL;
        p++;
    }
    return h;
}

/* Find the raw byte span of a JSON string token starting at the opening
   quote. Returns 1 and sets [start,end) when the token is complete. */
static int JsonStringSpan(const char *open, const char **start, const char **end)
{
    const char *p;
    if (open == NULL || *open != '"')
        return 0;
    p = open + 1;
    while (*p != '\0')
    {
        if (*p == '\\')
        {
            if (*(p + 1) == '\0')
                return 0;
            p += 2;
            continue;
        }
        if (*p == '"')
        {
            *start = open + 1;
            *end = p;
            return 1;
        }
        p++;
    }
    return 0;
}

int ServerDeriveSessionKey(const char *body, char *out, size_t size)
{
    const char *p;
    const char *sys_start = NULL, *sys_end = NULL;
    const char *usr_start = NULL, *usr_end = NULL;
    char pending_role[32];
    uint64_t h = 1469598103934665603ULL;
    if (body == NULL || out == NULL || size == 0)
        return 0;
    out[0] = '\0';
    pending_role[0] = '\0';
    p = body;
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
                    const char *rs, *re;
                    if (JsonStringSpan(q, &rs, &re) &&
                        (size_t)(re - rs) < sizeof(pending_role))
                    {
                        memcpy(pending_role, rs, (size_t)(re - rs));
                        pending_role[re - rs] = '\0';
                        p = re + 1;
                        continue;
                    }
                }
            }
        }
        else if (*p == '"' && strncmp(p, "\"content\"", 9) == 0 &&
                 pending_role[0] != '\0')
        {
            const char *q = p + 9;
            while (IsWs(*q)) q++;
            if (*q == ':')
            {
                q++;
                while (IsWs(*q)) q++;
                if (*q == '"')
                {
                    const char *cs, *ce;
                    if (JsonStringSpan(q, &cs, &ce))
                    {
                        if (strcmp(pending_role, "system") == 0 &&
                            sys_start == NULL)
                        {
                            sys_start = cs;
                            sys_end = ce;
                        }
                        else if (strcmp(pending_role, "user") == 0 &&
                                 usr_start == NULL)
                        {
                            usr_start = cs;
                            usr_end = ce;
                        }
                        pending_role[0] = '\0';
                        p = ce + 1;
                        if (usr_start != NULL)
                            break; /* first user message ends the stable prefix */
                        continue;
                    }
                }
            }
        }
        p++;
    }
    if (sys_start == NULL && usr_start == NULL)
        return 0;
    if (sys_start != NULL)
        h = FnvaUpdate(h, sys_start, sys_end);
    h = FnvaUpdate(h, "\xff", "\xff" + 1);
    if (usr_start != NULL)
        h = FnvaUpdate(h, usr_start, usr_end);
    snprintf(out, size, "auto-%016llx", (unsigned long long)h);
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

static int HasLangMarker(const char *lower)
{
    static const char *langs[] = {
        "en c11", "in c11", "c code", "codigo c", "código c",
        "lenguaje c", "c language", "en python", "in python",
        "en javascript", "in javascript", "en typescript", "in typescript",
        "en js", "in js", "en c", "in c"
    };
    if (!lower)
        return 0;
    for (size_t k = 0; k < sizeof(langs) / sizeof(langs[0]); k++)
    {
        size_t n = strlen(langs[k]);
        const char *p = lower;
        while ((p = strstr(p, langs[k])) != NULL)
        {
            char after = p[n];
            int after_ok = (after == '\0' ||
                            (!isalnum((unsigned char)after) && after != '_'));
            int before_ok = (p == lower ||
                             (!isalnum((unsigned char)*(p - 1)) && *(p - 1) != '_'));
            if (before_ok && after_ok)
                return 1;
            p++;
        }
    }
    return 0;
}

static int HasWorkspaceToken(const char *lower)
{
    static const char *ws[] = {
        "folder", "directory", "carpeta", "directorio", "files",
        "archivos", "ficheros", "workspace", "repo", "repository",
        "repositorio", "subcarpetas", "subcarpeta", "subfolders",
        "subdirectories", "subdirectorio", "codebase", "project",
        "proyecto", "dir", "ls", "pwd", "tree"
    };
    if (!lower)
        return 0;
    for (size_t k = 0; k < sizeof(ws) / sizeof(ws[0]); k++)
    {
        if (MatchWordBoundary(lower, ws[k]))
            return 1;
    }
    return (strchr(lower, '*') != NULL);
}

static int HasFilenameToken(const char *lower)
{
    const char *p;
    if (!lower)
        return 0;
    p = lower;
    while ((p = strchr(p, '.')) != NULL)
    {
        if (p != lower && isalnum((unsigned char)*(p - 1)) &&
            p[1] != '\0' && isalnum((unsigned char)p[1]))
            return 1;
        p++;
    }
    return 0;
}

int ServerIsFileCreationTask(const char *text)
{
    char lower[1024];
    size_t i = 0;
    int has_verb = 0;
    int has_noun = 0;
    if (text == NULL || text[0] == '\0')
        return 0;
    while (text[i] != '\0' && i < sizeof(lower) - 1)
    {
        lower[i] = (char)tolower((unsigned char)text[i]);
        i++;
    }
    lower[i] = '\0';

    if (strchr(lower, '?') != NULL || strstr(lower, "\xC2\xBF") != NULL)
        return 0;

    if (MatchWordBoundary(lower, "crea") ||
        MatchWordBoundary(lower, "crear") ||
        MatchWordBoundary(lower, "create") ||
        MatchWordBoundary(lower, "touch"))
        has_verb = 1;
    if (MatchWordBoundary(lower, "haz") || MatchWordBoundary(lower, "hacer"))
        has_verb = 1;
    if (strstr(lower, "nuevo archivo") != NULL ||
        strstr(lower, "nuevo fichero") != NULL ||
        strstr(lower, "new file") != NULL)
        has_verb = 1;

    if (MatchWordBoundary(lower, "fichero") ||
        MatchWordBoundary(lower, "archivo") ||
        MatchWordBoundary(lower, "file"))
        has_noun = 1;
    if (HasFilenameToken(lower))
        has_noun = 1;

    return (has_verb && has_noun);
}

int ServerExtractCreatePath(const char *text, char *out, size_t n)
{
    static const char *skip[] = {
        "crea", "crear", "create", "touch", "haz", "hacer", "make",
        "un", "una", "el", "la", "los", "las", "the", "a", "an",
        "new", "nuevo", "nueva", "fichero", "archivo", "file", "files",
        "llamado", "llamada", "named", "called", "por", "favor", "please",
        "me", "con", "contenido", "vacio", "vacia", "empty", "en", "in",
        "carpeta", "directorio", "folder", "directory", "dir", "workspace",
        "repo", "repositorio", "proyecto", "project", "src", "include",
        "tests", "docs", "data", "build"
    };
    static const char *exts[] = {
        ".c", ".h", ".cpp", ".hpp", ".py", ".ts", ".js", ".md", ".txt",
        ".json", ".yml", ".yaml", ".toml", ".sh", ".bat", ".ps1", ".csv",
        ".ini", ".cfg", ".xml", ".html", ".css", ".rs", ".go", ".java"
    };
    char buf[512];
    char best_ext[260];
    char best_bare[260];
    char *tok;
    size_t i;
    if (text == NULL || out == NULL || n < 2)
        return 0;
    strncpy(buf, text, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    best_ext[0] = '\0';
    best_bare[0] = '\0';
    tok = strtok(buf, " \t\r\n,;:\"'()<>{}`¿?");
    while (tok)
    {
        size_t tlen = strlen(tok);
        int skipped = 0;
        const char *dot;
        while (tlen > 0 && (tok[tlen - 1] == '.' || tok[tlen - 1] == ',' ||
                            tok[tlen - 1] == '!' || tok[tlen - 1] == ')'))
            tok[--tlen] = '\0';
        for (i = 0; i < sizeof(skip) / sizeof(skip[0]); i++)
        {
            if (strcmp(tok, skip[i]) == 0)
            {
                skipped = 1;
                break;
            }
        }
        if (!skipped && tlen > 0)
        {
            dot = strrchr(tok, '.');
            if (dot && dot != tok)
            {
                int ok_ext = 0;
                for (i = 0; i < sizeof(exts) / sizeof(exts[0]); i++)
                {
                    if (strcmp(dot, exts[i]) == 0)
                    {
                        ok_ext = 1;
                        break;
                    }
                }
                if (ok_ext)
                {
                    strncpy(best_ext, tok, sizeof(best_ext) - 1);
                    best_ext[sizeof(best_ext) - 1] = '\0';
                }
            }
            else if (tlen < sizeof(best_bare) - 5)
            {
                int alnum = 1;
                for (i = 0; i < tlen; i++)
                {
                    unsigned char c = (unsigned char)tok[i];
                    if (!isalnum(c) && c != '_' && c != '-' && c != '/')
                        alnum = 0;
                }
                if (alnum)
                {
                    strncpy(best_bare, tok, sizeof(best_bare) - 1);
                    best_bare[sizeof(best_bare) - 1] = '\0';
                }
            }
        }
        tok = strtok(NULL, " \t\r\n,;:\"'()<>{}`¿?");
    }
    if (best_ext[0] != '\0')
    {
        strncpy(out, best_ext, n - 1);
        out[n - 1] = '\0';
        return 1;
    }
    if (best_bare[0] != '\0')
    {
        snprintf(out, n, "%s.txt", best_bare);
        return 1;
    }
    strncpy(out, "nuevo.txt", n - 1);
    out[n - 1] = '\0';
    return 1;
}

/* Extract only tokens with recognized file extensions (for last_target tracking).
   Returns 1 if a file reference was found, 0 otherwise. */
int ServerExtractFileRef(const char *text, char *out, size_t n)
{
    static const char *exts[] = {
        ".c", ".h", ".cpp", ".hpp", ".py", ".ts", ".js", ".md", ".txt",
        ".json", ".yml", ".yaml", ".toml", ".sh", ".bat", ".ps1", ".csv",
        ".ini", ".cfg", ".xml", ".html", ".css", ".rs", ".go", ".java"
    };
    char buf[512];
    char *tok;
    size_t i;
    if (text == NULL || out == NULL || n < 2)
        return 0;
    strncpy(buf, text, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    out[0] = '\0';
    tok = strtok(buf, " \t\r\n,;:\"'()<>{}`¿?");
    while (tok)
    {
        size_t tlen = strlen(tok);
        const char *dot;
        while (tlen > 0 && (tok[tlen - 1] == '.' || tok[tlen - 1] == ',' ||
                            tok[tlen - 1] == '!' || tok[tlen - 1] == ')'))
            tok[--tlen] = '\0';
        dot = strrchr(tok, '.');
        if (dot && dot != tok)
        {
            for (i = 0; i < sizeof(exts) / sizeof(exts[0]); i++)
            {
                if (strcmp(dot, exts[i]) == 0)
                {
                    strncpy(out, tok, n - 1);
                    out[n - 1] = '\0';
                    return 1;
                }
            }
        }
        tok = strtok(NULL, " \t\r\n,;:\"'()<>{}`¿?");
    }
    return 0;
}

static const char *FindIstr(const char *hay, const char *needle)
{
    size_t nlen, i, j;
    if (hay == NULL || needle == NULL || needle[0] == '\0')
        return NULL;
    nlen = strlen(needle);
    for (i = 0; hay[i] != '\0'; i++)
    {
        for (j = 0; j < nlen; j++)
        {
            char h = hay[i + j];
            char n = needle[j];
            if (h >= 'A' && h <= 'Z') h = (char)(h + 32);
            if (n >= 'A' && n <= 'Z') n = (char)(n + 32);
            if (h == '\0' || h != n)
                break;
        }
        if (j == nlen)
            return hay + i;
    }
    return NULL;
}

static void JsonEscapeArg(const char *in, char *out, size_t n)
{
    size_t o = 0;
    if (out == NULL || n == 0)
        return;
    out[0] = '\0';
    if (in == NULL)
        return;
    for (; *in != '\0' && o + 2 < n; in++)
    {
        if (*in == '\\' || *in == '"')
        {
            if (o + 3 >= n)
                break;
            out[o++] = '\\';
        }
        if (*in == '\n' || *in == '\r')
            continue;
        out[o++] = *in;
    }
    out[o] = '\0';
}

int ServerIsDiffTask(const char *text)
{
    char lower[1024];
    size_t i = 0;
    if (text == NULL || text[0] == '\0')
        return 0;
    if (ServerIsFileCreationTask(text))
        return 0;
    while (text[i] != '\0' && i < sizeof(lower) - 1)
    {
        lower[i] = (char)tolower((unsigned char)text[i]);
        i++;
    }
    lower[i] = '\0';
    if (MatchWordBoundary(lower, "aplica") || MatchWordBoundary(lower, "apply") ||
        strstr(lower, "apply_patch") != NULL)
        return 0;
    if (strstr(lower, "git diff") != NULL)
        return 1;
    if ((MatchWordBoundary(lower, "muestra") || MatchWordBoundary(lower, "mostrar") ||
         MatchWordBoundary(lower, "show") || MatchWordBoundary(lower, "ver") ||
         MatchWordBoundary(lower, "dame") || MatchWordBoundary(lower, "display") ||
         MatchWordBoundary(lower, "qué") || MatchWordBoundary(lower, "que")) &&
        (MatchWordBoundary(lower, "diff") || MatchWordBoundary(lower, "cambios") ||
         MatchWordBoundary(lower, "changes") || MatchWordBoundary(lower, "cambió") ||
         MatchWordBoundary(lower, "cambio")))
        return 1;
    if (strstr(lower, "qué cambió") != NULL || strstr(lower, "que cambio") != NULL ||
        strstr(lower, "qué cambia") != NULL || strstr(lower, "que cambia") != NULL)
        return 1;
    return 0;
}

int ServerExtractEditSpec(const char *text, char *file, size_t fn,
                          char *old_s, size_t on, char *new_s, size_t nn,
                          int *has_replace)
{
    const char *sep;
    const char *orig_old;
    const char *orig_new;
    size_t ol, nl;
    if (has_replace)
        *has_replace = 0;
    if (file && fn)
        file[0] = '\0';
    if (old_s && on)
        old_s[0] = '\0';
    if (new_s && nn)
        new_s[0] = '\0';
    if (text == NULL)
        return 0;
    if (file && fn)
    {
        /* Use ServerExtractFileRef for strict extension-only matching
           to avoid false positives like "linea.txt" from best_bare fallback */
        if (!ServerExtractFileRef(text, file, fn))
            file[0] = '\0';
    }
    sep = FindIstr(text, " por ");
    if (sep == NULL)
        sep = FindIstr(text, " with ");
    if (sep == NULL)
        return (file && file[0] != '\0');
    orig_old = sep;
    while (orig_old > text && orig_old[-1] != ' ' && orig_old[-1] != '\t')
        orig_old--;
    ol = (size_t)(sep - orig_old);
    orig_new = sep;
    while (*orig_new == ' ' || *orig_new == '\t')
        orig_new++;
    while (*orig_new && *orig_new != ' ' && *orig_new != '\t')
        orig_new++;
    while (*orig_new == ' ' || *orig_new == '\t')
        orig_new++;
    nl = 0;
    while (orig_new[nl] && orig_new[nl] != ' ' && orig_new[nl] != '\t')
        nl++;
    if (ol == 0 || nl == 0)
        return (file && file[0] != '\0');
    if (old_s && on)
    {
        if (ol >= on)
            ol = on - 1;
        memcpy(old_s, orig_old, ol);
        old_s[ol] = '\0';
    }
    if (new_s && nn)
    {
        if (nl >= nn)
            nl = nn - 1;
        memcpy(new_s, orig_new, nl);
        new_s[nl] = '\0';
    }
    if (has_replace)
        *has_replace = 1;
    return 1;
}

/* Detect edit verbs (with or without enclitics) — no file check */
int ServerHasEditVerb(const char *text)
{
    char lower[1024];
    size_t i = 0;
    if (text == NULL || text[0] == '\0')
        return 0;
    while (text[i] != '\0' && i < sizeof(lower) - 1)
    {
        lower[i] = (char)tolower((unsigned char)text[i]);
        i++;
    }
    lower[i] = '\0';
    return MatchEditVerb(lower);
}

int ServerIsEditTask(const char *text)
{
    char lower[1024];
    char file[260], old_s[256], new_s[256];
    int has_rep = 0;
    size_t i = 0;
    int has_verb = 0;
    int has_file = 0;
    if (text == NULL || text[0] == '\0')
        return 0;
    if (ServerIsFileCreationTask(text) || ServerIsDiffTask(text))
        return 0;
    while (text[i] != '\0' && i < sizeof(lower) - 1)
    {
        lower[i] = (char)tolower((unsigned char)text[i]);
        i++;
    }
    lower[i] = '\0';
    if (MatchEditVerb(lower))
        has_verb = 1;
    if (!has_verb)
        return 0;
    ServerExtractEditSpec(text, file, sizeof(file), old_s, sizeof(old_s),
                          new_s, sizeof(new_s), &has_rep);
    if (file[0] != '\0' && strchr(file, '.') != NULL &&
        strcmp(file, "nuevo.txt") != 0)
        has_file = 1;
    if (has_rep && has_file)
        return 1;
    if (has_file && MatchEditVerb(lower))
        return 1;
    return 0;
}

int ServerMapDiffToolCall(const char *query, const char names[][64],
                          uint32_t nnames, OPENAI_TOOL_CALL *out)
{
    const char *cmd = "git diff";
    char tmp_names[8][64];
    uint32_t i, n = 0;
    if (query != NULL && FindIstr(query, "git diff") != NULL)
        cmd = query;
    if (nnames > 8)
        nnames = 8;
    for (i = 0; i < nnames; i++)
    {
        strncpy(tmp_names[n], names[i], 63);
        tmp_names[n][63] = '\0';
        n++;
    }
    if (!ServerMapShellToolCall(cmd, tmp_names, n, out))
        return 0;
    if (FindIstr(out->arguments, "git diff") == NULL)
    {
        strncpy(out->arguments, "{\"command\":\"git diff\"}",
                sizeof(out->arguments) - 1);
    }
    return 1;
}

int ServerMapEditToolCall(const char *query, const char names[][64],
                          uint32_t nnames, OPENAI_TOOL_CALL *out)
{
    char file[260], old_s[256], new_s[256];
    char esc_file[320], esc_old[320], esc_new[320];
    int has_rep = 0;
    int has_edit = 0, has_read = 0, has_patch = 0;
    uint32_t i;
    const char *tool;
    if (query == NULL || out == NULL)
        return 0;
    memset(out, 0, sizeof(*out));
    ServerExtractEditSpec(query, file, sizeof(file), old_s, sizeof(old_s),
                          new_s, sizeof(new_s), &has_rep);
    if (file[0] == '\0')
        strncpy(file, "nuevo.txt", sizeof(file) - 1);
    for (i = 0; i < nnames; i++)
    {
        if (strcmp(names[i], "edit") == 0)
            has_edit = 1;
        if (strcmp(names[i], "read") == 0)
            has_read = 1;
        if (strcmp(names[i], "apply_patch") == 0)
            has_patch = 1;
    }
    JsonEscapeArg(file, esc_file, sizeof(esc_file));
    JsonEscapeArg(old_s, esc_old, sizeof(esc_old));
    JsonEscapeArg(new_s, esc_new, sizeof(esc_new));
    strncpy(out->id, "call_edit_1", sizeof(out->id) - 1);
    if (has_rep && has_edit)
    {
        strncpy(out->name, "edit", sizeof(out->name) - 1);
        snprintf(out->arguments, sizeof(out->arguments),
                 "{\"filePath\":\"%s\",\"oldString\":\"%s\",\"newString\":\"%s\"}",
                 esc_file, esc_old, esc_new);
        return 1;
    }
    if (has_rep && has_patch)
    {
        strncpy(out->name, "apply_patch", sizeof(out->name) - 1);
        snprintf(out->arguments, sizeof(out->arguments),
                 "{\"file\":\"%s\",\"diff\":\"--- a/%s\\n+++ b/%s\\n@@ -1 +1 @@\\n-%s\\n+%s\\n\"}",
                 esc_file, esc_file, esc_file, esc_old, esc_new);
        return 1;
    }
    /* No replacement text: for append/add/write intents, dispatch read
       first so the caller can see the file and append; otherwise show. */
    {
        char lower_q[1024];
        size_t qi = 0;
        while (query[qi] != '\0' && qi < sizeof(lower_q) - 1)
        {
            lower_q[qi] = (char)tolower((unsigned char)query[qi]);
            qi++;
        }
        lower_q[qi] = '\0';
        if (MatchEditVerb(lower_q))
            tool = has_edit ? "edit" : (has_read ? "read" : NULL);
        else
            tool = has_read ? "read" : (has_edit ? "edit" : NULL);
    }
    if (tool == NULL)
        return 0;
    /* For edit without replacement text: check if it's an append/add intent.
       Append intents dispatch write with content extracted from query;
       generic edit intents dispatch read for inspection. */
    if (strcmp(tool, "edit") == 0 && !has_rep)
    {
        char lq_lower[1024];
        size_t qi = 0;
        while (query[qi] != '\0' && qi < sizeof(lq_lower) - 1)
        {
            lq_lower[qi] = (char)tolower((unsigned char)query[qi]);
            qi++;
        }
        lq_lower[qi] = '\0';
        /* Check if it's an append/add/write intent */
        if (strstr(lq_lower, "anade") == lq_lower || strstr(lq_lower, "añade") == lq_lower ||
            strstr(lq_lower, "add ") == lq_lower ||
            strstr(lq_lower, "agrega") == lq_lower ||
            strstr(lq_lower, "escribe") == lq_lower || strstr(lq_lower, "write ") == lq_lower ||
            strstr(lq_lower, "inserta") == lq_lower || strstr(lq_lower, "insert ") == lq_lower)
        {
            const char *content = "Line added";
            const char *p = NULL;
            if (strstr(lq_lower, "escribe ") == lq_lower || strstr(lq_lower, "write ") == lq_lower)
                p = query + 7;
            else if (strstr(lq_lower, "anade ") == lq_lower || strstr(lq_lower, "add ") == lq_lower)
                p = query + (lq_lower[0] == 'a' && lq_lower[1] == 'n' ? 5 : 4);
            else if (strstr(lq_lower, "agrega ") == lq_lower)
                p = query + 7;
            else if (strstr(lq_lower, "inserta ") == lq_lower || strstr(lq_lower, "insert ") == lq_lower)
                p = query + 8;
            if (p)
            {
                while (*p == ' ') p++;
                if (*p)
                {
                    const char *suffix;
                    suffix = strstr(p, " a ");
                    if (suffix == NULL) suffix = strstr(p, " to ");
                    if (suffix == NULL) suffix = strstr(p, " en ");
                    if (suffix == NULL) suffix = strstr(p, " in ");
                    if (suffix != NULL)
                    {
                        static char content_buf[256];
                        size_t clen = (size_t)(suffix - p);
                        if (clen >= sizeof(content_buf)) clen = sizeof(content_buf) - 1;
                        memcpy(content_buf, p, clen);
                        content_buf[clen] = '\0';
                        content = content_buf;
                    }
                    else
                        content = p;
                }
            }
            /* First step: dispatch read so we can see current content.
               Second step (in agentic loop) will dispatch write with
               old content + new line appended. */
            strncpy(out->name, "read", sizeof(out->name) - 1);
            snprintf(out->arguments, sizeof(out->arguments),
                     "{\"filePath\":\"%s\"}", esc_file);
            return 1;
        }
        /* Generic edit without replacement: dispatch read for inspection */
        tool = has_read ? "read" : "edit";
    }
    strncpy(out->name, tool, sizeof(out->name) - 1);
    if (has_rep)
        snprintf(out->arguments, sizeof(out->arguments),
                 "{\"filePath\":\"%s\"}", esc_file);
    else
        snprintf(out->arguments, sizeof(out->arguments),
                 "{\"filePath\":\"%s\"}", esc_file);
    return 1;
}

static int LooksLikeDefinitionQuestion(const char *lower)
{
    if (!lower)
        return 0;
    if (strstr(lower, "what is a") != NULL ||
        strstr(lower, "what is the") != NULL ||
        strstr(lower, "que es un") != NULL ||
        strstr(lower, "que es una") != NULL ||
        strstr(lower, "qué es un") != NULL ||
        strstr(lower, "qué es una") != NULL ||
        strstr(lower, "que es el") != NULL ||
        strstr(lower, "qué es el") != NULL)
        return 1;
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

    /* Implementation prompts ("lista en C") are not workspace listing.
       Folder/glob tokens still win so "lista las subcarpetas" stays inspection. */
    if (HasLangMarker(lower) && !HasWorkspaceToken(lower))
        return 0;

    /* Workspace reads without a language marker are inspection, not fopen samples. */
    if (!HasLangMarker(lower))
    {
        if (MatchWordBoundary(lower, "read") &&
            (MatchWordBoundary(lower, "file") ||
             MatchWordBoundary(lower, "archivo") ||
             MatchWordBoundary(lower, "fichero")))
            return 1;
        if (strstr(lower, "leer archivo") != NULL ||
            strstr(lower, "leer fichero") != NULL)
            return 1;
        if (HasFilenameToken(lower) &&
            (MatchWordBoundary(lower, "cat") ||
             MatchWordBoundary(lower, "open") ||
             MatchWordBoundary(lower, "type") ||
             MatchWordBoundary(lower, "view")))
            return 1;
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
    static const char *names[] = {
        "fibonacci", "factorial", "quicksort", "mergesort", "bubblesort", "qsort"
    };
    if (!text)
        return 0;
    for (size_t k = 0; k < sizeof(names) / sizeof(names[0]); k++)
    {
        if (MatchWordBoundary(text, names[k]))
            return 1;
    }
    if (strstr(text, "lista enlazada") != NULL ||
        strstr(text, "linked list") != NULL ||
        strstr(text, "array dinamico") != NULL ||
        strstr(text, "dynamic array") != NULL ||
        strstr(text, "binary search") != NULL ||
        strstr(text, "busqueda binaria") != NULL ||
        strstr(text, "búsqueda binaria") != NULL)
        return 1;

    char buf[512];
    strncpy(buf, text, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *tok = strtok(buf, " \t\r\n,;\"'¿?.!():");
    while (tok)
    {
        if (strlen(tok) >= 6)
        {
            if (EditDistance(tok, "fibonacci") <= 2) return 1;
            if (EditDistance(tok, "factorial") <= 2) return 1;
            if (EditDistance(tok, "quicksort") <= 2) return 1;
            if (EditDistance(tok, "mergesort") <= 2) return 1;
        }
        tok = strtok(NULL, " \t\r\n,;\"'¿?.!():");
    }
    return 0;
}

int ServerIsShellTask(const char *text)
{
    static const char *cmds[] = {
        "cmake", "gcc", "g++", "clang", "cl", "ctest", "git", "make",
        "ninja", "cargo", "npm", "npx", "pip", "python", "py", "node",
        "go", "rustc", "dotnet", "powershell", "pwsh", "cmd",
        "dir", "ls", "pwd", "echo", "mkdir", "rmdir", "rm", "cp",
        "mv", "curl", "wget", "tar", "zip", "unzip"
    };
    char tok[64];
    size_t i = 0, t = 0;
    if (text == NULL || text[0] == '\0')
        return 0;
    if (ServerIsFileCreationTask(text))
        return 0;
    if (LooksLikeDefinitionQuestion(text))
        return 0;
    while (text[i] != '\0' && (text[i] == ' ' || text[i] == '\t'))
        i++;
    while (text[i] != '\0' && t + 1 < sizeof(tok) &&
           text[i] != ' ' && text[i] != '\t')
    {
        tok[t++] = (char)tolower((unsigned char)text[i]);
        i++;
    }
    tok[t] = '\0';
    if (strcmp(tok, "run") == 0 || strcmp(tok, "ejecuta") == 0 ||
        strcmp(tok, "ejecutar") == 0 || strcmp(tok, "corre") == 0)
    {
        t = 0;
        while (text[i] != '\0' && (text[i] == ' ' || text[i] == '\t'))
            i++;
        while (text[i] != '\0' && t + 1 < sizeof(tok) &&
               text[i] != ' ' && text[i] != '\t')
        {
            tok[t++] = (char)tolower((unsigned char)text[i]);
            i++;
        }
        tok[t] = '\0';
    }
    if (tok[0] == '\0')
        return 0;
    for (i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++)
    {
        if (strcmp(tok, cmds[i]) == 0)
            return 1;
    }

    /* Natural language shell intent: "list files", "show directories",
       "lista archivos", "muestra subcarpetas", etc. */
    {
        char lower[1024];
        size_t j = 0;
        while (text[j] != '\0' && j < sizeof(lower) - 1)
        {
            lower[j] = (char)tolower((unsigned char)text[j]);
            j++;
        }
        lower[j] = '\0';
        if (strstr(lower, "list ") != NULL || strstr(lower, "lista ") != NULL ||
            strstr(lower, "show ") != NULL || strstr(lower, "muestra ") != NULL ||
            strstr(lower, "explore ") != NULL || strstr(lower, "explora ") != NULL)
        {
            if (strstr(lower, "file") != NULL || strstr(lower, "archivo") != NULL ||
                strstr(lower, "fichero") != NULL || strstr(lower, "folder") != NULL ||
                strstr(lower, "carpeta") != NULL || strstr(lower, "director") != NULL ||
                strstr(lower, "subcarpet") != NULL || strstr(lower, "subfolder") != NULL ||
                strstr(lower, "content") != NULL || strstr(lower, "contenido") != NULL)
                return 1;
        }
    }

    return 0;
}

int ServerMapShellToolCall(const char *query, const char names[][64],
                           uint32_t nnames, OPENAI_TOOL_CALL *out)
{
    const char *tool = NULL;
    uint32_t k;
    char esc[SERVER_ARG_JSON_MAX];
    size_t o = 0;
    const char *p;
    if (query == NULL || out == NULL)
        return 0;
    for (k = 0; k < nnames; k++)
    {
        if (strcmp(names[k], "bash") == 0)
        {
            tool = "bash";
            break;
        }
    }
    if (tool == NULL)
    {
        for (k = 0; k < nnames; k++)
        {
            if (strcmp(names[k], "execute_command") == 0)
            {
                tool = "execute_command";
                break;
            }
        }
    }
    if (tool == NULL)
        return 0;
    memset(out, 0, sizeof(*out));
    strncpy(out->name, tool, sizeof(out->name) - 1);
    strncpy(out->id, "call_shell_1", sizeof(out->id) - 1);
    /* Natural language file listing: translate to dir/ls */
    {
        char lower_q[1024];
        size_t j = 0;
        while (query[j] != '\0' && j < sizeof(lower_q) - 1)
        {
            lower_q[j] = (char)tolower((unsigned char)query[j]);
            j++;
        }
        lower_q[j] = '\0';
        if ((strstr(lower_q, "list ") != NULL || strstr(lower_q, "lista ") != NULL ||
             strstr(lower_q, "show ") != NULL || strstr(lower_q, "muestra ") != NULL ||
             strstr(lower_q, "explore ") != NULL || strstr(lower_q, "explora ") != NULL) &&
            (strstr(lower_q, "file") != NULL || strstr(lower_q, "archivo") != NULL ||
             strstr(lower_q, "folder") != NULL || strstr(lower_q, "carpeta") != NULL ||
             strstr(lower_q, "director") != NULL || strstr(lower_q, "contenido") != NULL ||
             strstr(lower_q, "content") != NULL))
        {
            memset(out, 0, sizeof(*out));
            strncpy(out->name, tool, sizeof(out->name) - 1);
            strncpy(out->id, "call_shell_1", sizeof(out->id) - 1);
            snprintf(out->arguments, sizeof(out->arguments),
                     "{\"command\":\"dir *.*\"}");
            return 1;
        }
    }

    p = query;
    while (*p == ' ' || *p == '\t')
        p++;
    {
        const char *q = p;
        char lead[32];
        size_t n = 0;
        while (*q && *q != ' ' && *q != '\t' && n + 1 < sizeof(lead))
            lead[n++] = (char)tolower((unsigned char)*q++);
        lead[n] = '\0';
        if (strcmp(lead, "run") == 0 || strcmp(lead, "ejecuta") == 0 ||
            strcmp(lead, "ejecutar") == 0 || strcmp(lead, "corre") == 0)
        {
            while (*q == ' ' || *q == '\t')
                q++;
            p = q;
        }
    }
    for (; *p != '\0' && o + 2 < sizeof(esc); p++)
    {
        if (*p == '\\' || *p == '"')
        {
            if (o + 3 >= sizeof(esc))
                break;
            esc[o++] = '\\';
        }
        if (*p == '\n' || *p == '\r')
            continue;
        esc[o++] = *p;
    }
    esc[o] = '\0';
    snprintf(out->arguments, sizeof(out->arguments),
             "{\"command\":\"%s\"}", esc);
    return 1;
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

    if (LooksLikeDefinitionQuestion(lower))
        return 0;

    if (ServerIsFileCreationTask(text))
        return 0;

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

    /* Inspection (workspace read/list) beats synthesis unless the prompt
       is an algorithm name or an "en C" implementation request. */
    if (ServerIsInspectionTask(text))
        return 0;

    if (MatchesAlgorithmKeyword(lower))
        return 1;

    static const char *synth_verbs[] = {
        "escribe", "escribir", "crea", "crear", "genera", "generar",
        "haz", "hacer", "programa", "programar", "implementa", "implementar",
        "desarrolla", "desarrollar", "write", "generate", "implement", "code",
        "dame", "give", "muestra", "mostrar", "leer", "read"
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
        "quicksort", "ordenar", "ordenamiento",
        "busqueda", "búsqueda", "puntero", "punteros", "pointer", "pointers",
        "invertir", "reverse", "ejemplo", "example",
        "lista", "list", "nodo", "node", "vector", "pila", "stack", "cola", "queue",
        "archivo", "fichero", "file", "lectura", "qsort"
    };
    int has_noun = 0;
    for (size_t k = 0; k < sizeof(synth_nouns) / sizeof(synth_nouns[0]); k++)
    {
        if (MatchWordBoundary(lower, synth_nouns[k]))
        {
            has_noun = 1;
            break;
        }
    }

    int has_lang = HasLangMarker(lower);

    if (has_noun && has_lang)
        return 1;

    if (has_verb && has_noun)
        return 1;

    return 0;
}

static int TokenEditClose(const char *lower, const char *name)
{
    char buf[512];
    char *tok;
    if (!lower || !name)
        return 0;
    strncpy(buf, lower, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    tok = strtok(buf, " \t\r\n,;\"'¿?.!():");
    while (tok)
    {
        if (strlen(tok) >= 6 && EditDistance(tok, name) <= 2)
            return 1;
        tok = strtok(NULL, " \t\r\n,;\"'¿?.!():");
    }
    return 0;
}

void ServerSynthesizeCode(const char *query, char *out, size_t out_sz)
{
    char lower[1024];
    size_t i = 0;
    if (!query || !out || out_sz == 0)
        return;

    while (query[i] != '\0' && i < sizeof(lower) - 1)
    {
        lower[i] = (char)tolower((unsigned char)query[i]);
        i++;
    }
    lower[i] = '\0';

    if (MatchWordBoundary(lower, "fibonacci") ||
        MatchWordBoundary(lower, "fibinacci") ||
        TokenEditClose(lower, "fibonacci"))
    {
        snprintf(out, out_sz,
            "Aqui tienes la implementacion de la funcion de Fibonacci en C (C11):\n\n"
            "```c\n"
            "#include <stdio.h>\n"
            "#include <stdint.h>\n\n"
            "/**\n"
            " * Calcula el n-esimo termino de la sucesion de Fibonacci de forma iterativa.\n"
            " * Complejidad: O(n) tiempo, O(1) memoria auxiliar.\n"
            " * Utiliza uint64_t para soportar hasta F(93) sin desbordamiento de 64 bits.\n"
            " */\n"
            "uint64_t fibonacci(uint32_t n)\n"
            "{\n"
            "    if (n == 0)\n"
            "        return 0;\n"
            "    if (n == 1)\n"
            "        return 1;\n\n"
            "    uint64_t prev = 0;\n"
            "    uint64_t curr = 1;\n"
            "    for (uint32_t i = 2; i <= n; i++)\n"
            "    {\n"
            "        uint64_t next = prev + curr;\n"
            "        prev = curr;\n"
            "        curr = next;\n"
            "    }\n"
            "    return curr;\n"
            "}\n\n"
            "int main(void)\n"
            "{\n"
            "    printf(\"--- Sucesion de Fibonacci (0 a 10) ---\\n\");\n"
            "    for (uint32_t i = 0; i <= 10; i++)\n"
            "    {\n"
            "        printf(\"F(%%u) = %%llu\\n\", i, (unsigned long long)fibonacci(i));\n"
            "    }\n"
            "    return 0;\n"
            "}\n"
            "```\n\n"
            "- **Rendimiento**: Ejecucion en tiempo lineal O(n) sin la sobrecarga exponencial de la recursion ingenua.\n"
            "- **Invariantes**: Seguro ante desbordamiento para terminos basicos y compilable con `gcc -Wall -Wextra -Werror`.");
        return;
    }

    if (MatchWordBoundary(lower, "factorial") || TokenEditClose(lower, "factorial"))
    {
        snprintf(out, out_sz,
            "Aqui tienes la implementacion de la funcion factorial en C (C11):\n\n"
            "```c\n"
            "#include <stdio.h>\n"
            "#include <stdint.h>\n\n"
            "uint64_t factorial(uint32_t n)\n"
            "{\n"
            "    if (n > 20)\n"
            "        return 0;\n"
            "    uint64_t res = 1;\n"
            "    for (uint32_t i = 2; i <= n; i++)\n"
            "        res *= i;\n"
            "    return res;\n"
            "}\n"
            "```");
        return;
    }

    if (strstr(lower, "binary search") != NULL ||
        strstr(lower, "busqueda binaria") != NULL ||
        strstr(lower, "búsqueda binaria") != NULL)
    {
        snprintf(out, out_sz,
            "Aqui tienes la implementacion de busqueda binaria en C (C11):\n\n"
            "```c\n"
            "#include <stdio.h>\n\n"
            "int BinarySearch(const int *arr, int len, int target)\n"
            "{\n"
            "    int left = 0;\n"
            "    int right = len - 1;\n"
            "    while (left <= right)\n"
            "    {\n"
            "        int mid = left + (right - left) / 2;\n"
            "        if (arr[mid] == target)\n"
            "            return mid;\n"
            "        if (arr[mid] < target)\n"
            "            left = mid + 1;\n"
            "        else\n"
            "            right = mid - 1;\n"
            "    }\n"
            "    return -1;\n"
            "}\n"
            "```");
        return;
    }

    if (MatchWordBoundary(lower, "qsort"))
    {
        snprintf(out, out_sz,
            "Aqui tienes el uso de qsort de la biblioteca estandar de C (C11):\n\n"
            "```c\n"
            "#include <stdio.h>\n"
            "#include <stdlib.h>\n\n"
            "static int CompareInts(const void *a, const void *b)\n"
            "{\n"
            "    int arg1 = *(const int *)a;\n"
            "    int arg2 = *(const int *)b;\n"
            "    if (arg1 < arg2) return -1;\n"
            "    if (arg1 > arg2) return 1;\n"
            "    return 0;\n"
            "}\n\n"
            "int main(void)\n"
            "{\n"
            "    int values[] = {40, 10, 100, 90, 20, 25};\n"
            "    size_t count = sizeof(values) / sizeof(values[0]);\n"
            "    qsort(values, count, sizeof(int), CompareInts);\n"
            "    return 0;\n"
            "}\n"
            "```");
        return;
    }

    if (MatchWordBoundary(lower, "mergesort") || strstr(lower, "merge sort") != NULL)
    {
        snprintf(out, out_sz,
            "Aqui tienes la implementacion de Mergesort en C (C11):\n\n"
            "```c\n"
            "#include <stdio.h>\n"
            "#include <stdlib.h>\n\n"
            "static void Merge(int arr[], int tmp[], int left, int mid, int right)\n"
            "{\n"
            "    int i = left, j = mid + 1, k = left;\n"
            "    while (i <= mid && j <= right)\n"
            "        tmp[k++] = (arr[i] <= arr[j]) ? arr[i++] : arr[j++];\n"
            "    while (i <= mid) tmp[k++] = arr[i++];\n"
            "    while (j <= right) tmp[k++] = arr[j++];\n"
            "    for (i = left; i <= right; i++) arr[i] = tmp[i];\n"
            "}\n\n"
            "void MergeSort(int arr[], int tmp[], int left, int right)\n"
            "{\n"
            "    if (left >= right)\n"
            "        return;\n"
            "    int mid = left + (right - left) / 2;\n"
            "    MergeSort(arr, tmp, left, mid);\n"
            "    MergeSort(arr, tmp, mid + 1, right);\n"
            "    Merge(arr, tmp, left, mid, right);\n"
            "}\n"
            "```\n\n"
            "- Complejidad: O(n log n) tiempo, O(n) memoria auxiliar.");
        return;
    }

    if (MatchWordBoundary(lower, "bubblesort") || strstr(lower, "bubble sort") != NULL)
    {
        snprintf(out, out_sz,
            "Aqui tienes la implementacion de Bubble Sort en C (C11):\n\n"
            "```c\n"
            "#include <stdio.h>\n\n"
            "void BubbleSort(int arr[], int n)\n"
            "{\n"
            "    for (int i = 0; i < n - 1; i++)\n"
            "        for (int j = 0; j < n - 1 - i; j++)\n"
            "            if (arr[j] > arr[j + 1])\n"
            "            {\n"
            "                int tmp = arr[j];\n"
            "                arr[j] = arr[j + 1];\n"
            "                arr[j + 1] = tmp;\n"
            "            }\n"
            "}\n"
            "```");
        return;
    }

    if (MatchWordBoundary(lower, "quicksort") ||
        MatchWordBoundary(lower, "ordenar") ||
        MatchWordBoundary(lower, "ordenamiento"))
    {
        snprintf(out, out_sz,
            "Aqui tienes la implementacion de Quicksort en C (C11):\n\n"
            "```c\n"
            "#include <stdio.h>\n\n"
            "static void Swap(int *a, int *b)\n"
            "{\n"
            "    int tmp = *a;\n"
            "    *a = *b;\n"
            "    *b = tmp;\n"
            "}\n\n"
            "static int Partition(int arr[], int low, int high)\n"
            "{\n"
            "    int pivot = arr[high];\n"
            "    int i = low - 1;\n"
            "    for (int j = low; j < high; j++)\n"
            "    {\n"
            "        if (arr[j] <= pivot)\n"
            "        {\n"
            "            i++;\n"
            "            Swap(&arr[i], &arr[j]);\n"
            "        }\n"
            "    }\n"
            "    Swap(&arr[i + 1], &arr[high]);\n"
            "    return i + 1;\n"
            "}\n\n"
            "void QuickSort(int arr[], int low, int high)\n"
            "{\n"
            "    if (low < high)\n"
            "    {\n"
            "        int pi = Partition(arr, low, high);\n"
            "        QuickSort(arr, low, pi - 1);\n"
            "        QuickSort(arr, pi + 1, high);\n"
            "    }\n"
            "}\n"
            "```\n\n"
            "- Complejidad: O(n log n) promedio, O(n^2) peor caso.\n"
            "- Memoria auxiliar: O(log n) promedio / O(n) peor caso en la pila de llamadas (no O(1)).");
        return;
    }

    if (strstr(lower, "lista enlazada") != NULL ||
        strstr(lower, "linked list") != NULL ||
        MatchWordBoundary(lower, "nodo") ||
        MatchWordBoundary(lower, "node") ||
        MatchWordBoundary(lower, "lista") ||
        MatchWordBoundary(lower, "list"))
    {
        snprintf(out, out_sz,
            "Aqui tienes la implementacion de una lista enlazada simple en C (C11):\n\n"
            "```c\n"
            "#include <stdio.h>\n"
            "#include <stdlib.h>\n\n"
            "typedef struct Node\n"
            "{\n"
            "    int data;\n"
            "    struct Node *next;\n"
            "} Node;\n\n"
            "Node *InsertHead(Node *head, int value)\n"
            "{\n"
            "    Node *newNode = (Node *)malloc(sizeof(Node));\n"
            "    if (newNode == NULL)\n"
            "        return head;\n"
            "    newNode->data = value;\n"
            "    newNode->next = head;\n"
            "    return newNode;\n"
            "}\n\n"
            "void FreeList(Node *head)\n"
            "{\n"
            "    while (head != NULL)\n"
            "    {\n"
            "        Node *next = head->next;\n"
            "        free(head);\n"
            "        head = next;\n"
            "    }\n"
            "}\n"
            "```");
        return;
    }

    if (MatchWordBoundary(lower, "vector") ||
        strstr(lower, "array dinamico") != NULL ||
        strstr(lower, "dynamic array") != NULL)
    {
        snprintf(out, out_sz,
            "Aqui tienes la implementacion de un array dinamico (vector) en C (C11):\n\n"
            "```c\n"
            "#include <stdio.h>\n"
            "#include <stdlib.h>\n"
            "#include <stdbool.h>\n\n"
            "typedef struct\n"
            "{\n"
            "    int *items;\n"
            "    size_t size;\n"
            "    size_t capacity;\n"
            "} Vector;\n\n"
            "bool VectorInit(Vector *vec, size_t initial_cap)\n"
            "{\n"
            "    vec->items = (int *)malloc(initial_cap * sizeof(int));\n"
            "    if (vec->items == NULL)\n"
            "        return false;\n"
            "    vec->size = 0;\n"
            "    vec->capacity = initial_cap;\n"
            "    return true;\n"
            "}\n\n"
            "bool VectorPush(Vector *vec, int value)\n"
            "{\n"
            "    if (vec->size >= vec->capacity)\n"
            "    {\n"
            "        int *grown = (int *)realloc(vec->items, vec->capacity * 2 * sizeof(int));\n"
            "        if (grown == NULL)\n"
            "            return false;\n"
            "        vec->items = grown;\n"
            "        vec->capacity *= 2;\n"
            "    }\n"
            "    vec->items[vec->size++] = value;\n"
            "    return true;\n"
            "}\n\n"
            "void VectorFree(Vector *vec)\n"
            "{\n"
            "    free(vec->items);\n"
            "    vec->items = NULL;\n"
            "    vec->size = 0;\n"
            "    vec->capacity = 0;\n"
            "}\n"
            "```");
        return;
    }

    if (strstr(lower, "leer archivo") != NULL ||
        strstr(lower, "leer fichero") != NULL ||
        strstr(lower, "read file") != NULL)
    {
        snprintf(out, out_sz,
            "Aqui tienes la funcion para leer un archivo linea por linea en C (C11):\n\n"
            "```c\n"
            "#include <stdio.h>\n"
            "#include <string.h>\n\n"
            "int ReadFileLineByLine(const char *filepath)\n"
            "{\n"
            "    FILE *file = fopen(filepath, \"r\");\n"
            "    if (file == NULL)\n"
            "        return -1;\n"
            "    char buffer[512];\n"
            "    while (fgets(buffer, sizeof(buffer), file) != NULL)\n"
            "        printf(\"%%s\", buffer);\n"
            "    fclose(file);\n"
            "    return 0;\n"
            "}\n"
            "```");
        return;
    }

    if (MatchWordBoundary(lower, "cola") ||
        MatchWordBoundary(lower, "queue") ||
        MatchWordBoundary(lower, "fifo"))
    {
        snprintf(out, out_sz,
            "Aqui tienes la implementacion de una Cola (Queue FIFO) en C (C11):\n\n"
            "```c\n"
            "#include <stdio.h>\n"
            "#include <stdbool.h>\n\n"
            "#define QUEUE_CAPACITY 64\n\n"
            "typedef struct\n"
            "{\n"
            "    int data[QUEUE_CAPACITY];\n"
            "    int head;\n"
            "    int tail;\n"
            "    int count;\n"
            "} Queue;\n\n"
            "void QueueInit(Queue *q) { q->head = 0; q->tail = 0; q->count = 0; }\n"
            "bool QueueIsEmpty(const Queue *q) { return q->count == 0; }\n"
            "bool QueueIsFull(const Queue *q) { return q->count == QUEUE_CAPACITY; }\n\n"
            "bool Enqueue(Queue *q, int val)\n"
            "{\n"
            "    if (QueueIsFull(q)) return false;\n"
            "    q->data[q->tail] = val;\n"
            "    q->tail = (q->tail + 1) %% QUEUE_CAPACITY;\n"
            "    q->count++;\n"
            "    return true;\n"
            "}\n\n"
            "bool Dequeue(Queue *q, int *out_val)\n"
            "{\n"
            "    if (QueueIsEmpty(q)) return false;\n"
            "    *out_val = q->data[q->head];\n"
            "    q->head = (q->head + 1) %% QUEUE_CAPACITY;\n"
            "    q->count--;\n"
            "    return true;\n"
            "}\n"
            "```");
        return;
    }

    if (MatchWordBoundary(lower, "pila") ||
        MatchWordBoundary(lower, "stack") ||
        MatchWordBoundary(lower, "lifo"))
    {
        snprintf(out, out_sz,
            "Aqui tienes la implementacion de una Pila (Stack LIFO) en C (C11):\n\n"
            "```c\n"
            "#include <stdio.h>\n"
            "#include <stdbool.h>\n\n"
            "#define STACK_CAPACITY 64\n\n"
            "typedef struct\n"
            "{\n"
            "    int data[STACK_CAPACITY];\n"
            "    int top;\n"
            "} Stack;\n\n"
            "void StackInit(Stack *s) { s->top = -1; }\n"
            "bool StackIsEmpty(const Stack *s) { return s->top == -1; }\n"
            "bool StackIsFull(const Stack *s) { return s->top == STACK_CAPACITY - 1; }\n\n"
            "bool StackPush(Stack *s, int val)\n"
            "{\n"
            "    if (StackIsFull(s)) return false;\n"
            "    s->data[++s->top] = val;\n"
            "    return true;\n"
            "}\n\n"
            "bool StackPop(Stack *s, int *out_val)\n"
            "{\n"
            "    if (StackIsEmpty(s)) return false;\n"
            "    *out_val = s->data[s->top--];\n"
            "    return true;\n"
            "}\n"
            "```");
        return;
    }

    if (MatchWordBoundary(lower, "invertir") || MatchWordBoundary(lower, "reverse"))
    {
        snprintf(out, out_sz,
            "Aqui tienes la funcion para invertir una cadena de texto en C (in-place):\n\n"
            "```c\n"
            "#include <stdio.h>\n"
            "#include <string.h>\n\n"
            "void ReverseString(char *str)\n"
            "{\n"
            "    size_t i, j;\n"
            "    if (str == NULL)\n"
            "        return;\n"
            "    j = strlen(str);\n"
            "    if (j == 0)\n"
            "        return;\n"
            "    j--;\n"
            "    for (i = 0; i < j; i++, j--)\n"
            "    {\n"
            "        char tmp = str[i];\n"
            "        str[i] = str[j];\n"
            "        str[j] = tmp;\n"
            "    }\n"
            "}\n"
            "```");
        return;
    }

    snprintf(out, out_sz,
        "Aqui tienes la estructura en C (C11) para tu solicitud ('%s'):\n\n"
        "```c\n"
        "#include <stdio.h>\n"
        "int main(void) { return 0; }\n"
        "```\n",
        query);
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
                "I am Symbols, a local symbolic AI assistant for software development. "
                "I can help you inspect the repository, write and edit C, Python, and JavaScript code, "
                "analyze functions and dependencies, and answer queries from the indexed knowledge base without GPU or cloud dependencies.");
        }
        else
        {
            snprintf(out, out_sz,
                "Hello. What would you like to do?");
        }
    }
    else
    {
        if (is_identity)
        {
            snprintf(out, out_sz,
                "Soy Symbols, un asistente y motor de inteligencia artificial simbólica local. "
                "Puedo ayudarte a explorar el repositorio, generar y modificar código en C, Python y JavaScript, "
                "analizar funciones y dependencias, y responder consultas sobre el conocimiento indexado sin dependencias externas ni GPUs.");
        }
        else
        {
            snprintf(out, out_sz,
                "Hola. ¿Qué quieres hacer?");
        }
    }

    return 1;
}



