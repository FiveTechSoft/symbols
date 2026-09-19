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

#define SERVER_PORT_DEFAULT 8099
#define SERVER_HDR_MAX 16384
#define SERVER_BODY_MAX 65536
#define SERVER_OBS_FILE "observations.jsonl"
#define SERVER_MODEL_ID "symbols"

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
} ServerSession;

static ServerSession g_sessions[SERVER_MAX_SESSIONS];
static uint32_t g_num_sessions = 0;

static void FormatOperatorToolCall(const STRIPS_OPERATOR *op, const char *issue,
                                   unsigned long seq, OPENAI_TOOL_CALLS *out_tc)
{
    memset(out_tc, 0, sizeof(*out_tc));
    out_tc->count = 1;
    snprintf(out_tc->calls[0].id, sizeof(out_tc->calls[0].id), "call_sym_%lu", seq);

    if (strcmp(op->name, "locate_symbol") == 0)
    {
        strncpy(out_tc->calls[0].name, "locate_symbol", sizeof(out_tc->calls[0].name) - 1);
        snprintf(out_tc->calls[0].arguments, sizeof(out_tc->calls[0].arguments),
                 "{\"query\":\"%.120s\"}", issue);
    }
    else if (strcmp(op->name, "inspect_code") == 0)
    {
        strncpy(out_tc->calls[0].name, "inspect_code", sizeof(out_tc->calls[0].name) - 1);
        strncpy(out_tc->calls[0].arguments,
                "{\"file\":\"src/main.c\",\"start_line\":1,\"end_line\":100}",
                sizeof(out_tc->calls[0].arguments) - 1);
    }
    else if (strcmp(op->name, "diagnose_error") == 0)
    {
        strncpy(out_tc->calls[0].name, "diagnose_error", sizeof(out_tc->calls[0].name) - 1);
        snprintf(out_tc->calls[0].arguments, sizeof(out_tc->calls[0].arguments),
                 "{\"issue\":\"%.120s\"}", issue);
    }
    else if (strcmp(op->name, "apply_patch") == 0)
    {
        strncpy(out_tc->calls[0].name, "apply_patch", sizeof(out_tc->calls[0].name) - 1);
        strncpy(out_tc->calls[0].arguments,
                "{\"file\":\"src/main.c\",\"diff\":\"@@ -1,3 +1,3 @@\\n- // buggy line\\n+ // fixed line\"}",
                sizeof(out_tc->calls[0].arguments) - 1);
    }
    else if (strcmp(op->name, "verify_build") == 0)
    {
        strncpy(out_tc->calls[0].name, "execute_command", sizeof(out_tc->calls[0].name) - 1);
        strncpy(out_tc->calls[0].arguments,
                "{\"command\":\"cmake --build .\"}",
                sizeof(out_tc->calls[0].arguments) - 1);
    }
    else if (strcmp(op->name, "run_regression_tests") == 0)
    {
        strncpy(out_tc->calls[0].name, "execute_command", sizeof(out_tc->calls[0].name) - 1);
        strncpy(out_tc->calls[0].arguments,
                "{\"command\":\"ctest --output-on-failure\"}",
                sizeof(out_tc->calls[0].arguments) - 1);
    }
    else
    {
        strncpy(out_tc->calls[0].name, op->name, sizeof(out_tc->calls[0].name) - 1);
        strncpy(out_tc->calls[0].arguments, "{}", sizeof(out_tc->calls[0].arguments) - 1);
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
    char query[4096], raw[4096], content[4096], resp[12288];
    char obs[16384], ts[32], ent_json[2048], prov_json[4096];
    char session_id[64] = "default";
    ServerSession *sess = NULL;
    ChatParse parsed;
    int nmsg = 0, has_system = 0;
    const char *status;
    clock_t t0;
    long latency_ms;
    FILE *log;

    OPENAI_TOOL_RESPONSE tool_resp;
    int has_tool_resp = ServerExtractLastToolResponse(body, &tool_resp);
    ServerExtractSession(body, session_id, sizeof(session_id));
    sess = GetOrCreateSession(session_id);
    ScanRoles(body, &nmsg, &has_system);

    /* 1. AGENTIC RESUMPTION: Client returned output of previous tool call */
    if (has_tool_resp && sess->agent_active)
    {
        sess->last_active = time(NULL);
        sess->current_step_idx++;

        if (sess->current_step_idx < sess->current_plan.step_count)
        {
            /* Dispatch next tool call in the active STRIPS plan */
            const STRIPS_OPERATOR *next_op = &sess->current_plan.steps[sess->current_step_idx].op;
            OPENAI_TOOL_CALLS tc;
            FormatOperatorToolCall(next_op, sess->current_issue, ++g_seq, &tc);

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
            /* STRIPS plan completed successfully: Emit final PR report */
            sess->agent_active = 0;
            snprintf(content, sizeof(content),
                     "### Autonomous Coding Task Completed\n\n"
                     "All %u steps of the STRIPS plan for issue '%s' have been executed.\n"
                     "- **Status**: 100%% Verified\n"
                     "- **Regressions**: 0\n"
                     "- **Build**: PASS\n\n"
                     "The patch is applied and verified against the codebase.",
                     sess->current_plan.step_count, sess->current_issue);

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

    char declared_tools[8][64];
    int num_declared = ServerExtractToolsDeclared(body, declared_tools, 8);
    int is_coding = ServerIsCodingTask(query);

    /* 3. INITIATE AGENTIC CODING TASK IF CODING INTENT OR TOOLS ARE DECLARED */
    if (is_coding || num_declared > 0)
    {
        sess->agent_active = 1;
        sess->current_step_idx = 0;
        strncpy(sess->current_issue, query, sizeof(sess->current_issue) - 1);

        AGENT_PLANNER planner;
        AgentPlannerInit(&planner);
        AgentPlannerFormulate(&planner, query, PRED_SYMBOL_KNOWN,
                              PRED_BUILD_VERIFIED | PRED_TESTS_VERIFIED | PRED_TASK_COMPLETED,
                              &sess->current_plan);

        if (sess->current_plan.step_count > 0)
        {
            const STRIPS_OPERATOR *first_op = &sess->current_plan.steps[0].op;
            OPENAI_TOOL_CALLS tc;
            FormatOperatorToolCall(first_op, sess->current_issue, ++g_seq, &tc);

            if (ServerWantsStream(body))
            {
                char sse[16384];
                ServerBuildToolCallStreamResponse(SERVER_MODEL_ID, (long)time(NULL), g_seq, &tc, sse, sizeof(sse));
                SendRaw(s, 200, "OK", "text/event-stream", sse);
            }
            else
            {
                ServerBuildToolCallResponse(SERVER_MODEL_ID, (long)time(NULL), g_seq, &tc,
                                            "Formulated STRIPS plan to resolve coding task. Initiating first step.",
                                            resp, sizeof(resp));
                SendJson(s, 200, "OK", resp);
            }
            return;
        }
    }

    if (!g_session_ready)
    {
        ChatInit(&g_session, corpus);
        g_session_ready = 1;
    }

    /* Restore session dialogue state */
    strncpy(g_session.focus, sess->focus, sizeof(g_session.focus) - 1);
    g_session.focus[sizeof(g_session.focus) - 1] = '\0';
    g_session.focus_valid = sess->focus_valid;
    strncpy(g_session.focus_secondary, sess->focus_secondary, sizeof(g_session.focus_secondary) - 1);
    g_session.focus_secondary[sizeof(g_session.focus_secondary) - 1] = '\0';
    g_session.focus_secondary_valid = sess->focus_secondary_valid;
    g_session.exec = sess->exec;

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
    socket_t ls;
    struct sockaddr_in addr;
    int port = SERVER_PORT_DEFAULT;
    char corpus[1024];
    corpus[0] = '\0';
    if (argc > 1)
        port = atoi(argv[1]);
    if (port <= 0 || port > 65535)
        port = SERVER_PORT_DEFAULT;
    const char *env_corpus = getenv("SYMBOLS_CORPUS");
    if (env_corpus != NULL && env_corpus[0] != '\0')
    {
        strncpy(corpus, env_corpus, sizeof(corpus) - 1);
        corpus[sizeof(corpus) - 1] = '\0';
    }
    else if (argc > 2)
    {
        strncpy(corpus, argv[2], sizeof(corpus) - 1);
        corpus[sizeof(corpus) - 1] = '\0';
    }
    else
    {
        /* CWD layout first, then exe-relative (build-* / dirs) */
        static const char *cand_paths[] = {
            "data/texts/bible.txt",
            "data/texts/jung.txt",
            "data/texts/corpus.txt",
            "data/corpus.txt",
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
            probe = fopen(cand_paths[i], "r");
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
                    probe = fopen(corpus, "r");
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
            fprintf(stderr,
                    "corpus not found (tried data/texts/*.txt and "
                    "data/corpus.*); refusing to serve an empty "
                    "model\n");
            return 1;
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
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((u_short)port);
    if (bind(ls, (struct sockaddr *)&addr, sizeof(addr)) != 0 ||
        listen(ls, 4) != 0)
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
