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
#include <winsock2.h>
#include <ws2tcpip.h>
#include "schema.h"
#include "metaschema.h"
#include "learn.h"
#include "bible_chat.h"
#include "server_proto.h"

#define SERVER_PORT_DEFAULT 8099
#define SERVER_HDR_MAX 16384
#define SERVER_BODY_MAX 65536
#define SERVER_OBS_FILE "observations.jsonl"
#define SERVER_MODEL_ID "symbols"

static int SendAll(SOCKET s, const char *buf, size_t len)
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

static int SendJson(SOCKET s, int code, const char *status,
                    const char *body)
{
    char hdr[512];
    int w = snprintf(hdr, sizeof(hdr),
                     "HTTP/1.1 %d %s\r\nContent-Type: "
                     "application/json\r\nContent-Length: %u\r\n"
                     "Connection: close\r\n\r\n",
                     code, status, (unsigned)strlen(body));
    if (w <= 0)
        return 0;
    if (!SendAll(s, hdr, strlen(hdr)))
        return 0;
    return SendAll(s, body, strlen(body));
}

static int SendError(SOCKET s, int code, const char *status,
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

static void HandleCompletions(SOCKET s, const char *body,
                              const char *corpus)
{
    char query[4096], raw[4096], content[4096], resp[12288];
    char obs[16384], ts[32], ent_json[2048], prov_json[4096];
    CHAT ch;
    ChatParse parsed;
    int nmsg = 0, has_system = 0;
    const char *status;
    clock_t t0;
    long latency_ms;
    FILE *log;
    if (!ServerExtractQuery(body, query, sizeof(query)))
    {
        SendError(s, 400, "Bad Request", "no user message found");
        return;
    }
    ScanRoles(body, &nmsg, &has_system);
    ChatInit(&ch, corpus);
    memset(&parsed, 0, sizeof(parsed));
    ChatParseLine(&ch, query, &parsed);
    t0 = clock();
    if (!ServerAnswerQuery(&ch, query, raw, sizeof(raw)))
        strncpy(raw, "No entendi la pregunta.", sizeof(raw) - 1);
    latency_ms = (long)((clock() - t0) * 1000 / CLOCKS_PER_SEC);
    ServerMapContent(raw, content, sizeof(content));
    status = ServerStatusOf(raw);
    if (!ServerBuildResponse(SERVER_MODEL_ID, (long)time(NULL),
                             ++g_seq, content, query, resp,
                             sizeof(resp)))
    {
        SendError(s, 500, "Internal Error", "response too large");
        return;
    }
    SendJson(s, 200, "OK", resp);
    BuildStrArray(ch.exec.entities, ch.exec.nent, ent_json,
                  sizeof(ent_json));
    BuildProvArray(&ch, prov_json, sizeof(prov_json));
    IsoUtc(ts, sizeof(ts));
    if (ServerBuildObservation(ts, SERVER_MODEL_ID, nmsg, has_system,
                               query, &parsed, ch.focus_valid ?
                               ch.focus : "", ent_json, prov_json,
                               content, status, latency_ms, obs,
                               sizeof(obs)))
    {
        /* CWD first (repo-adjacent log), Temp fallback: the request
           is always served, the log never blocks it. */
        log = fopen(SERVER_OBS_FILE, "a");
        if (log == NULL)
        {
            char tmp[512], alt[640];
            DWORD tn = GetTempPathA(sizeof(tmp), tmp);
            if (tn > 0 && tn < sizeof(tmp))
            {
                snprintf(alt, sizeof(alt), "%s%s", tmp,
                         SERVER_OBS_FILE);
                log = fopen(alt, "a");
            }
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

static void HandleClient(SOCKET s, const char *corpus)
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
    if (strcmp(method, "GET") == 0 &&
        strcmp(path, "/v1/models") == 0)
    {
        char body[512];
        ServerBuildModels(SERVER_MODEL_ID, body, sizeof(body));
        SendJson(s, 200, "OK", body);
        return;
    }
    if (strcmp(method, "POST") == 0 &&
        strcmp(path, "/v1/chat/completions") == 0)
    {
        static char body[SERVER_BODY_MAX + 1];
        const char *bstart;
        char *cl;
        size_t hbody, got;
        cl = strstr(hdr, "Content-Length:");
        if (cl != NULL)
            content_len = strtol(cl + 15, NULL, 10);
        if (content_len < 0 || content_len > SERVER_BODY_MAX)
        {
            SendError(s, 400, "Bad Request", "bad content length");
            return;
        }
        bstart = strstr(hdr, "\r\n\r\n") + 4;
        hbody = hlen - (size_t)(bstart - hdr);
        if (hbody > (size_t)content_len)
            hbody = (size_t)content_len;
        memcpy(body, bstart, hbody);
        got = hbody;
        while (got < (size_t)content_len)
        {
            rc = recv(s, body + got,
                      (int)((size_t)content_len - got), 0);
            if (rc <= 0)
                return;
            got += (size_t)rc;
        }
        body[content_len] = '\0';
        HandleCompletions(s, body, corpus);
        return;
    }
    SendError(s, 404, "Not Found", "unknown path");
}

int main(int argc, char **argv)
{
    WSADATA ws;
    SOCKET ls;
    struct sockaddr_in addr;
    int port = SERVER_PORT_DEFAULT;
    char corpus[1024];
    corpus[0] = '\0';
    if (argc > 1)
        port = atoi(argv[1]);
    if (port <= 0 || port > 65535)
        port = SERVER_PORT_DEFAULT;
    if (argc > 2)
    {
        /* explicit path: must exist, else fail-closed */
        FILE *probe = fopen(argv[2], "r");
        if (probe == NULL)
        {
            fprintf(stderr, "corpus not found: %s\n", argv[2]);
            return 1;
        }
        fclose(probe);
        strncpy(corpus, argv[2], sizeof(corpus) - 1);
    }
    else
    {
        /* CWD layout first, then exe-relative (build-* / dirs) */
        static const char *rel =
            "../data/bible/bible_relations.tsv";
        char exedir[768];
        FILE *probe;
        DWORD elen = GetModuleFileNameA(NULL, exedir, sizeof(exedir));
        corpus[0] = '\0';
        probe = fopen("data/bible/bible_relations.tsv", "r");
        if (probe != NULL)
        {
            fclose(probe);
            strncpy(corpus, "data/bible/bible_relations.tsv",
                    sizeof(corpus) - 1);
        }
        else if (elen > 0 && elen < sizeof(exedir))
        {
            char *sep = strrchr(exedir, '\\');
            if (sep == NULL)
                sep = strrchr(exedir, '/');
            if (sep != NULL)
            {
                *sep = '\0';
                snprintf(corpus, sizeof(corpus), "%s/%s", exedir,
                         rel);
                probe = fopen(corpus, "r");
                if (probe != NULL)
                    fclose(probe);
                else
                    corpus[0] = '\0';
            }
        }
        if (corpus[0] == '\0')
        {
            fprintf(stderr,
                    "corpus not found (tried CWD "
                    "data/bible/bible_relations.tsv and exe-"
                    "relative %s); refusing to serve an empty "
                    "model\n",
                    rel);
            return 1;
        }
    }
    if (WSAStartup(MAKEWORD(2, 2), &ws) != 0)
    {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }
    ls = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ls == INVALID_SOCKET)
    {
        fprintf(stderr, "socket failed\n");
        return 1;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((u_short)port);
    if (bind(ls, (struct sockaddr *)&addr, sizeof(addr)) != 0 ||
        listen(ls, 4) != 0)
    {
        fprintf(stderr, "bind/listen on 127.0.0.1:%d failed\n", port);
        return 1;
    }
    fprintf(stderr, "symbols-server on 127.0.0.1:%d (model %s)\n",
            port, SERVER_MODEL_ID);
    fprintf(stderr, "symbols-server corpus: %s\n", corpus);
    for (;;)
    {
        SOCKET c = accept(ls, NULL, NULL);
        if (c == INVALID_SOCKET)
            continue;
        HandleClient(c, corpus);
        closesocket(c);
    }
    return 0;
}
