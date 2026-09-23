/* OpenCode 1.18.32 parent side: see include/server_subagent.h. */
#include "server_subagent.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------ JSON walk */

static const char *SaWs(const char *p)
{
    while (p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
        p++;
    return p;
}

/* p at the opening quote; returns the char after the closing quote. */
static const char *SaSkipString(const char *p)
{
    p++;
    while (*p && *p != '"')
    {
        if (*p == '\\' && p[1])
            p++;
        p++;
    }
    return *p ? p + 1 : p;
}

/* p at a value; returns the char after it. */
static const char *SaSkipValue(const char *p)
{
    p = SaWs(p);
    if (*p == '"')
        return SaSkipString(p);
    if (*p == '{' || *p == '[')
    {
        int depth = 0;
        while (*p)
        {
            if (*p == '"') { p = SaSkipString(p); continue; }
            if (*p == '{' || *p == '[') depth++;
            else if (*p == '}' || *p == ']')
            {
                depth--;
                if (depth == 0)
                    return p + 1;
            }
            p++;
        }
        return p;
    }
    while (*p && *p != ',' && *p != '}' && *p != ']')
        p++;
    return p;
}

/* Value of a top-level key inside the object [obj, end). */
static const char *SaKey(const char *obj, const char *end, const char *key)
{
    const char *p = SaWs(obj);
    size_t kl = strlen(key);
    if (*p != '{')
        return NULL;
    p++;
    while (p < end)
    {
        p = SaWs(p);
        if (*p != '"')
            return NULL;
        const char *k = p + 1;
        const char *ke = SaSkipString(p);
        p = SaWs(ke);
        if (*p != ':')
            return NULL;
        p = SaWs(p + 1);
        if ((size_t)(ke - 1 - k) == kl && strncmp(k, key, kl) == 0)
            return p;
        p = SaWs(SaSkipValue(p));
        if (*p == ',')
            p++;
        else
            return NULL;
    }
    return NULL;
}

static void SaPutUtf8(unsigned v, char *out, size_t *o, size_t size)
{
    char b[4];
    int n;
    if (v < 0x80) { b[0] = (char)v; n = 1; }
    else if (v < 0x800) { b[0] = (char)(0xC0 | (v >> 6)); b[1] = (char)(0x80 | (v & 0x3F)); n = 2; }
    else { b[0] = (char)(0xE0 | (v >> 12)); b[1] = (char)(0x80 | ((v >> 6) & 0x3F)); b[2] = (char)(0x80 | (v & 0x3F)); n = 3; }
    if (*o + (size_t)n < size)
    {
        memcpy(out + *o, b, (size_t)n);
        *o += (size_t)n;
    }
}

/* Unescaped copy of the JSON string at p (NULL/non-string -> ""). */
static int SaString(const char *p, char *out, size_t size)
{
    size_t o = 0;
    if (size == 0)
        return 0;
    out[0] = '\0';
    p = SaWs(p);
    if (!p || *p != '"')
        return 0;
    p++;
    while (*p && *p != '"')
    {
        if (*p == '\\' && p[1])
        {
            p++;
            char c = *p;
            if (c == 'u')
            {
                unsigned v = 0;
                int k;
                for (k = 1; k <= 4 && isxdigit((unsigned char)p[k]); k++)
                    v = v * 16 + (unsigned)(isdigit((unsigned char)p[k]) ? p[k] - '0' : (tolower((unsigned char)p[k]) - 'a' + 10));
                p += k;
                if (v >= 0xD800 && v <= 0xDFFF) v = '?';
                SaPutUtf8(v, out, &o, size);
                continue;
            }
            c = c == 'n' ? '\n' : c == 't' ? '\t' : c == 'r' ? '\r' : c == 'b' ? '\b' : c == 'f' ? '\f' : c;
            if (o + 1 < size) out[o++] = c;
            p++;
            continue;
        }
        if (o + 1 < size) out[o++] = *p;
        p++;
    }
    out[o] = '\0';
    return 1;
}

/* Text of a message content: a string or an array of {type:text,text}. */
static void SaContent(const char *v, char *out, size_t size)
{
    size_t o = 0;
    out[0] = '\0';
    v = SaWs(v);
    if (!v)
        return;
    if (*v == '"')
    {
        SaString(v, out, size);
        return;
    }
    if (*v != '[')
        return;
    const char *end = SaSkipValue(v);
    const char *p = SaWs(v + 1);
    while (p < end && *p == '{')
    {
        const char *pe = SaSkipValue(p);
        const char *t = SaKey(p, pe, "text");
        if (t)
        {
            char part[8192];
            SaString(t, part, sizeof(part));
            size_t n = strlen(part);
            if (o && o + 1 < size) out[o++] = '\n';
            if (o + n >= size) n = size - o - 1;
            memcpy(out + o, part, n);
            o += n;
            out[o] = '\0';
        }
        p = SaWs(pe);
        if (*p == ',') p = SaWs(p + 1);
    }
}

/* Start of the array value of a top-level body key ("messages"/"tools"). */
static const char *SaBodyArray(const char *body, const char *key)
{
    const char *v = SaKey(body, body + strlen(body), key);
    v = SaWs(v);
    return (v && *v == '[') ? v : NULL;
}

/* -------------------------------------------------------------- agents */

int SaParseAgents(const char *body, SA_AGENT *out, int max)
{
    static char desc[16384];
    const char *arr = SaBodyArray(body, "tools");
    int n = 0;
    if (!arr || !out || max <= 0)
        return 0;
    const char *end = SaSkipValue(arr);
    const char *p = SaWs(arr + 1);
    desc[0] = '\0';
    while (p < end && *p == '{')
    {
        const char *pe = SaSkipValue(p);
        const char *fn = SaKey(p, pe, "function");
        if (fn && *fn == '{')
        {
            const char *fe = SaSkipValue(fn);
            char name[64];
            SaString(SaKey(fn, fe, "name"), name, sizeof(name));
            if (strcmp(name, "task") == 0)
            {
                SaString(SaKey(fn, fe, "description"), desc, sizeof(desc));
                break;
            }
        }
        p = SaWs(pe);
        if (*p == ',') p = SaWs(p + 1);
    }
    /* "- name: description" lines; a name is [a-z0-9_-] and the line
       must follow the agent-list heading (the text that ends in ':'). */
    const char *list = strstr(desc, "agent types");
    const char *q = list ? list : desc;
    while ((q = strstr(q, "\n- ")) != NULL && n < max)
    {
        q += 3;
        size_t k = 0;
        while (q[k] && (islower((unsigned char)q[k]) || isdigit((unsigned char)q[k]) || q[k] == '_' || q[k] == '-') && k < 63)
            k++;
        if (k == 0 || q[k] != ':')
            continue;
        memcpy(out[n].name, q, k);
        out[n].name[k] = '\0';
        const char *d = q + k + 1;
        while (*d == ' ') d++;
        size_t dl = strcspn(d, "\n");
        if (dl >= sizeof(out[n].desc)) dl = sizeof(out[n].desc) - 1;
        memcpy(out[n].desc, d, dl);
        out[n].desc[dl] = '\0';
        n++;
    }
    return n;
}

/* ---------------------------------------------------------- task output */

static int SaAttr(const char *tag, const char *attr, char *out, size_t size)
{
    char pat[32];
    snprintf(pat, sizeof(pat), "%s=\"", attr);
    const char *p = strstr(tag, pat);
    const char *gt = strchr(tag, '>');
    if (!p || (gt && p > gt))
        return 0;
    p += strlen(pat);
    size_t k = strcspn(p, "\"");
    if (k >= size) k = size - 1;
    memcpy(out, p, k);
    out[k] = '\0';
    return k > 0;
}

static void SaBetween(const char *s, const char *open, const char *close, char *out, size_t size)
{
    const char *a = strstr(s, open), *b;
    out[0] = '\0';
    if (!a)
        return;
    a += strlen(open);
    b = strstr(a, close);
    size_t k = b ? (size_t)(b - a) : strlen(a);
    while (k && (*a == '\n' || *a == ' ')) { a++; k--; }
    while (k && (a[k - 1] == '\n' || a[k - 1] == ' ')) k--;
    if (k >= size) k = size - 1;
    memcpy(out, a, k);
    out[k] = '\0';
}

int SaParseTaskOutput(const char *out, char *task_id, size_t id_size,
                      char *state, size_t state_size,
                      char *text, size_t text_size)
{
    task_id[0] = state[0] = text[0] = '\0';
    if (!out)
        return 0;
    const char *tag = strstr(out, "<task ");
    if (tag)
    {
        SaAttr(tag, "id", task_id, id_size);
        if (!SaAttr(tag, "state", state, state_size))
            snprintf(state, state_size, "completed");
        if (strstr(tag, "<task_error>"))
        {
            SaBetween(tag, "<task_error>", "</task_error>", text, text_size);
            snprintf(state, state_size, "error");
        }
        else
            SaBetween(tag, "<task_result>", "</task_result>", text, text_size);
        return task_id[0] != '\0';
    }
    const char *id = strstr(out, "task_id: ");
    if (id)
    {
        id += 9;
        size_t k = strcspn(id, " \n");
        if (k >= id_size) k = id_size - 1;
        memcpy(task_id, id, k);
        task_id[k] = '\0';
    }
    if (strstr(out, "<task_result>"))
    {
        snprintf(state, state_size, "completed");
        SaBetween(out, "<task_result>", "</task_result>", text, text_size);
    }
    else
    {
        snprintf(state, state_size, "error");
        snprintf(text, text_size, "%s", out);
    }
    return task_id[0] != '\0';
}

int SaChildFiles(const char *text, char files[][260], int max)
{
    const char *b = text ? strstr(text, "Subagent result (Symbols):") : NULL;
    int n = 0;
    if (!b)
        return -1;
    const char *f = strstr(b, "- files changed: ");
    if (!f)
        return -1;
    f += strlen("- files changed: ");
    size_t line = strcspn(f, "\n");
    if (line == 4 && strncmp(f, "none", 4) == 0)
        return 0;
    const char *e = f + line;
    while (f < e && n < max)
    {
        size_t k = 0;
        while (f + k < e && f[k] != ',') k++;
        size_t s = 0;
        while (s < k && f[s] == ' ') s++;
        size_t len = k - s;
        if (len >= 260) len = 259;
        if (len)
        {
            memcpy(files[n], f + s, len);
            files[n][len] = '\0';
            n++;
        }
        f += k;
        if (f < e && *f == ',') f++;
    }
    return n;
}

/* ---------------------------------------------------------------- parts */

static const char *SaRel(const char *path, const char *workdir)
{
    size_t wl = workdir ? strlen(workdir) : 0;
    if (wl && strncmp(path, workdir, wl) == 0 && path[wl] == '/')
        return path + wl + 1;
    return path;
}

static int SaIsNameChar(char c)
{
    return isalnum((unsigned char)c) || c == '_' || c == '-' || c == '.' || c == '/';
}

int SaNamedParts(const char *query, const char *listing, const char *workdir,
                 char parts[][260], int max)
{
    int n = 0;
    const char *q = query;
    if (!query || !listing)
        return 0;
    while (*q && n < max)
    {
        if (!SaIsNameChar(*q) || (q > query && SaIsNameChar(q[-1])))
        {
            q++;
            continue;
        }
        size_t k = 0;
        while (SaIsNameChar(q[k])) k++;
        char tok[260];
        size_t tl = k < sizeof(tok) ? k : sizeof(tok) - 1;
        memcpy(tok, q, tl);
        tok[tl] = '\0';
        while (tl && tok[tl - 1] == '.') tok[--tl] = '\0';   /* sentence dot */
        q += k;
        const char *dot = strrchr(tok, '.');
        if (!dot || dot == tok || !isalnum((unsigned char)dot[1]))
            continue;
        /* the token must be a listed path or a path suffix after '/' */
        const char *l = listing;
        while (*l)
        {
            size_t ll = strcspn(l, "\n");
            char path[520];
            size_t pl = ll < sizeof(path) ? ll : sizeof(path) - 1;
            memcpy(path, l, pl);
            path[pl] = '\0';
            const char *rel = SaRel(path, workdir);
            size_t rl = strlen(rel);
            if (strcmp(rel, tok) == 0 ||
                (rl > tl && rel[rl - tl - 1] == '/' && strcmp(rel + rl - tl, tok) == 0))
            {
                int dup = 0;
                for (int i = 0; i < n; i++)
                    if (strcmp(parts[i], rel) == 0) dup = 1;
                if (!dup && strlen(rel) < 260)
                    snprintf(parts[n++], 260, "%s", rel);
                break;
            }
            l += ll;
            if (*l == '\n') l++;
        }
    }
    return n;
}

/* The request as seen by one part: the other parts' file names are
   removed together with a joining "and" or comma, so a child that picks
   the first named file picks its own. */
static int SaTokenAt(const char *q, const char *start, size_t len)
{
    return (start == q || !SaIsNameChar(start[-1])) && !SaIsNameChar(start[len]);
}

static void SaPartQuery(const char *query, char parts[][260], int np, int own,
                        char *out, size_t size)
{
    size_t o = 0;
    const char *q = query;
    while (*q && o + 1 < size)
    {
        int cut = 0;
        for (int i = 0; i < np && !cut; i++)
        {
            if (i == own)
                continue;
            const char *base = strrchr(parts[i], '/');
            const char *names[2] = { parts[i], base ? base + 1 : NULL };
            for (int k = 0; k < 2 && !cut; k++)
            {
                size_t l = names[k] ? strlen(names[k]) : 0;
                if (l && strncmp(q, names[k], l) == 0 && SaTokenAt(query, q, l))
                {
                    q += l;
                    /* drop the joiner that followed, else the one before */
                    const char *r = q;
                    while (*r == ' ') r++;
                    if (*r == ',') r++;
                    while (*r == ' ') r++;
                    if (strncmp(r, "and ", 4) == 0 || strncmp(r, "y ", 2) == 0)
                        q = r + (r[0] == 'a' ? 4 : 2);
                    else if (r != q)
                        q = r;
                    else
                    {
                        while (o && out[o - 1] == ' ') o--;
                        if (o >= 4 && strncmp(out + o - 4, " and", 4) == 0) o -= 4;
                        else if (o >= 2 && strncmp(out + o - 2, " y", 2) == 0) o -= 2;
                        else if (o >= 1 && out[o - 1] == ',') o--;
                    }
                    cut = 1;
                }
            }
        }
        if (!cut)
            out[o++] = *q++;
    }
    out[o] = '\0';
}

/* --------------------------------------------------------------- memory */

static const char *SaExt(const char *part)
{
    const char *d = part ? strrchr(part, '.') : NULL;
    return d ? d + 1 : "-";
}

void SaMemoryRecord(const char *agent, const char *part, const char *outcome)
{
    const char *path = getenv("SYMBOLS_SUBAGENT_MEMORY");
    if (!path || !path[0] || !agent || !outcome)
        return;
    FILE *f = fopen(path, "a");
    if (!f)
        return;
    fprintf(f, "%s\t%s\t%s\n", agent, SaExt(part), outcome);
    fclose(f);
}

int SaMemoryScore(const char *agent, const char *part)
{
    const char *path = getenv("SYMBOLS_SUBAGENT_MEMORY");
    char line[512], a[128], e[64], o[64];
    int score = 0;
    if (!path || !path[0] || !agent)
        return 0;
    FILE *f = fopen(path, "r");
    if (!f)
        return 0;
    while (fgets(line, sizeof(line), f))
        if (sscanf(line, "%127[^\t]\t%63[^\t]\t%63s", a, e, o) == 3 &&
            strcmp(a, agent) == 0 && strcmp(e, SaExt(part)) == 0)
            score += strcmp(o, "verified") == 0 ? 1 : -1;
    fclose(f);
    return score;
}

static int SaOverlap(const char *desc, const char *words)
{
    char w[64];
    int hits = 0;
    const char *p = words;
    while (*p)
    {
        size_t k = strcspn(p, " ");
        if (k && k < sizeof(w))
        {
            memcpy(w, p, k);
            w[k] = '\0';
            /* whole-word, case-insensitive */
            const char *d = desc;
            while (*d)
            {
                size_t j = 0;
                while (j < k && d[j] && tolower((unsigned char)d[j]) == w[j]) j++;
                if (j == k && (d == desc || !isalpha((unsigned char)d[-1])) && !isalpha((unsigned char)d[k]))
                {
                    hits++;
                    break;
                }
                d++;
            }
        }
        p += k;
        while (*p == ' ') p++;
    }
    return hits;
}

int SaChooseAgent(const SA_AGENT *agents, int n, const char *part)
{
    int best = -1, bs = 0, bo = 0;
    for (int i = 0; i < n; i++)
    {
        int s = SaMemoryScore(agents[i].name, part);
        int o = SaOverlap(agents[i].desc, SA_PARALLEL_WORDS);
        if (best < 0 || s > bs || (s == bs && o > bo))
        {
            best = i;
            bs = s;
            bo = o;
        }
    }
    return best;
}

/* -------------------------------------------------------------- history */

typedef struct
{
    char id[64];
    int  is_task;          /* 1 task, 2 our re-verification */
    char part[260];
    char subagent[64];
    char resume_id[128];
    int  answered;
    int  order;            /* index of the answering tool message */
    char output[8192];
} SA_CALL;

typedef struct
{
    SA_CALL c[SA_MAX_TASKS];
    int n;
    int last_answers_ours; /* last message is a tool result for one of c[] */
    int last_is_tool;
    char listing[16384];   /* latest glob/list output */
    int listing_is_last;
    char last_user[4096];  /* text parts of the latest user message */
} SA_HIST;

static void SaParseHistory(const char *body, SA_HIST *h)
{
    static char text[16384];
    memset(h, 0, sizeof(*h));
    const char *arr = SaBodyArray(body, "messages");
    if (!arr)
        return;
    const char *end = SaSkipValue(arr);
    const char *p = SaWs(arr + 1);
    int idx = 0;
    /* pending glob/list call ids */
    char listing_ids[8][64];
    int nlist = 0;
    while (p < end && *p == '{')
    {
        const char *pe = SaSkipValue(p);
        char role[16];
        SaString(SaKey(p, pe, "role"), role, sizeof(role));
        h->last_is_tool = strcmp(role, "tool") == 0;
        h->last_answers_ours = 0;
        h->listing_is_last = 0;
        if (strcmp(role, "assistant") == 0)
        {
            const char *tc = SaKey(p, pe, "tool_calls");
            if (tc && *tc == '[')
            {
                const char *te = SaSkipValue(tc);
                const char *c = SaWs(tc + 1);
                while (c < te && *c == '{')
                {
                    const char *ce = SaSkipValue(c);
                    char id[64], name[64];
                    static char args[SERVER_ARG_JSON_MAX];
                    SaString(SaKey(c, ce, "id"), id, sizeof(id));
                    const char *fn = SaKey(c, ce, "function");
                    name[0] = args[0] = '\0';
                    if (fn && *fn == '{')
                    {
                        const char *fe = SaSkipValue(fn);
                        SaString(SaKey(fn, fe, "name"), name, sizeof(name));
                        SaString(SaKey(fn, fe, "arguments"), args, sizeof(args));
                    }
                    const char *ae = args + strlen(args);
                    char desc[300] = "";
                    SaString(SaKey(args, ae, "description"), desc, sizeof(desc));
                    if (h->n < SA_MAX_TASKS && strcmp(name, "task") == 0)
                    {
                        SA_CALL *k = &h->c[h->n++];
                        memset(k, 0, sizeof(*k));
                        snprintf(k->id, sizeof(k->id), "%s", id);
                        k->is_task = 1;
                        if (strncmp(desc, "part: ", 6) == 0)
                            snprintf(k->part, sizeof(k->part), "%s", desc + 6);
                        SaString(SaKey(args, ae, "subagent_type"), k->subagent, sizeof(k->subagent));
                        SaString(SaKey(args, ae, "task_id"), k->resume_id, sizeof(k->resume_id));
                    }
                    else if (h->n < SA_MAX_TASKS && strcmp(name, "bash") == 0 &&
                             strncmp(desc, SA_VERIFY_MARK, strlen(SA_VERIFY_MARK)) == 0)
                    {
                        SA_CALL *k = &h->c[h->n++];
                        memset(k, 0, sizeof(*k));
                        snprintf(k->id, sizeof(k->id), "%s", id);
                        k->is_task = 2;
                        snprintf(k->part, sizeof(k->part), "%s", desc + strlen(SA_VERIFY_MARK));
                    }
                    else if ((strcmp(name, "glob") == 0 || strcmp(name, "list") == 0) && nlist < 8)
                        snprintf(listing_ids[nlist++], 64, "%s", id);
                    c = SaWs(ce);
                    if (*c == ',') c = SaWs(c + 1);
                }
            }
        }
        else if (strcmp(role, "user") == 0)
            SaContent(SaKey(p, pe, "content"), h->last_user, sizeof(h->last_user));
        else if (strcmp(role, "tool") == 0)
        {
            char tcid[64];
            SaString(SaKey(p, pe, "tool_call_id"), tcid, sizeof(tcid));
            SaContent(SaKey(p, pe, "content"), text, sizeof(text));
            for (int i = 0; i < h->n; i++)
                if (!h->c[i].answered && strcmp(h->c[i].id, tcid) == 0)
                {
                    h->c[i].answered = 1;
                    h->c[i].order = idx;
                    { size_t tl = strlen(text); if (tl >= sizeof(h->c[i].output)) tl = sizeof(h->c[i].output) - 1; memcpy(h->c[i].output, text, tl); h->c[i].output[tl] = '\0'; }
                    h->last_answers_ours = 1;
                }
            for (int i = 0; i < nlist; i++)
                if (strcmp(listing_ids[i], tcid) == 0)
                {
                    { size_t tl = strlen(text); if (tl >= sizeof(h->listing)) tl = sizeof(h->listing) - 1; memcpy(h->listing, text, tl); h->listing[tl] = '\0'; }
                    h->listing_is_last = 1;
                }
        }
        idx++;
        p = SaWs(pe);
        if (*p == ',') p = SaWs(p + 1);
    }
}

/* ------------------------------------------------------------- decision */

static void SaAddCall(OPENAI_TOOL_CALLS *calls, const char *name, const char *args)
{
    static unsigned long seq = 0;
    if (calls->count >= SERVER_MAX_TOOL_CALLS)
        return;
    OPENAI_TOOL_CALL *c = &calls->calls[calls->count++];
    snprintf(c->id, sizeof(c->id), "call_sa_%lu", ++seq);
    snprintf(c->name, sizeof(c->name), "%s", name);
    snprintf(c->arguments, sizeof(c->arguments), "%s", args);
}

static void SaTaskCall(OPENAI_TOOL_CALLS *calls, const char *agent, const char *part,
                       const char *resume_id, const char *prompt)
{
    char ep[3400], ea[64], epart[600], eid[260], args[SERVER_ARG_JSON_MAX];
    ServerJsonEscape(prompt, ep, sizeof(ep));
    if (strlen(ep) > 3000)
    {
        /* keep the prompt within the argument buffer without splitting
           an escape sequence */
        size_t k = 3000, bs = 0;
        while (k > bs && ep[k - 1 - bs] == '\\') bs++;
        ep[k - (bs % 2)] = '\0';
    }
    ServerJsonEscape(agent, ea, sizeof(ea));
    ServerJsonEscape(part, epart, sizeof(epart));
    if (resume_id && resume_id[0])
    {
        ServerJsonEscape(resume_id, eid, sizeof(eid));
        snprintf(args, sizeof(args),
                 "{\"description\":\"part: %.200s\",\"prompt\":\"%.3000s\",\"subagent_type\":\"%s\",\"task_id\":\"%s\"}",
                 epart, ep, ea, eid);
    }
    else
        snprintf(args, sizeof(args),
                 "{\"description\":\"part: %.200s\",\"prompt\":\"%.3000s\",\"subagent_type\":\"%s\"}",
                 epart, ep, ea);
    SaAddCall(calls, "task", args);
}

static int SaHasTool(char declared[][64], int n, const char *name)
{
    for (int i = 0; i < n; i++)
        if (strcmp(declared[i], name) == 0)
            return 1;
    return 0;
}

static int SaVerifyExit(const char *out)
{
    const char *m = out ? strstr(out, "symbols-exit=") : NULL;
    if (!m)
        return -1;
    /* a clean rebuild: exit 0 and no compiler diagnostic */
    if (strstr(out, "warning:") || strstr(out, "error:"))
        return 1;
    return atoi(m + 13);
}

static void SaTail(const char *s, size_t keep, char *out, size_t size)
{
    size_t n = strlen(s);
    snprintf(out, size, "%s", n > keep ? s + n - keep : s);
}

enum { P_WAIT, P_VERIFY, P_FAILED, P_DONE, P_UNVERIFIED };

int SaMentionedAgent(const char *user_text, const SA_AGENT *agents, int n)
{
    const char *m = user_text ? strstr(user_text, SA_MENTION_MARK) : NULL;
    if (!m)
        return -1;
    m += strlen(SA_MENTION_MARK);
    for (int a = 0; a < n; a++)
    {
        size_t l = strlen(agents[a].name);
        if (l && strncmp(m, agents[a].name, l) == 0 && !SaIsNameChar(m[l]) && m[l] != '-')
            return a;
    }
    return -1;
}

/* The user's own request without OpenCode's mention scaffolding: drops a
   leading "@name " and the appended SA_MENTION_MARK sentence. */
static void SaMentionQuery(const char *query, const char *name, char *out, size_t size)
{
    const char *q = query;
    while (*q == ' ' || *q == '\n') q++;
    size_t l = strlen(name);
    if (q[0] == '@' && strncmp(q + 1, name, l) == 0 && (q[1 + l] == ' ' || q[1 + l] == '\0'))
        q += 1 + l;
    while (*q == ' ') q++;
    snprintf(out, size, "%s", q);
    char *m = strstr(out, SA_MENTION_MARK);
    if (m)
    {
        /* cut back to the start of the sentence that carries the mark */
        char *c = m;
        while (c > out && c[-1] != '\n' && c[-1] != '.') c--;
        *c = '\0';
        size_t k = strlen(out);
        while (k && (out[k - 1] == ' ' || out[k - 1] == '\n')) out[--k] = '\0';
    }
}

int SaDecide(const char *body, char declared[][64], int ndeclared,
             const char *query, SA_DECISION *d)
{
    static SA_HIST h;
    static SA_AGENT agents[SA_MAX_AGENTS];
    char workdir[512] = "";
    memset(d, 0, sizeof(*d));
    if (!body)
        return SA_NONE;
    SaParseHistory(body, &h);
    ServerExtractWorkingDir(body, workdir, sizeof(workdir));
    int ntask = 0;
    for (int i = 0; i < h.n; i++)
        if (h.c[i].is_task == 1) ntask++;

    if (ntask == 0)
    {
        /* Delegate right after the workspace listing arrives, only for
           2+ named parts, and only when the client offers `task`. */
        if (!SaHasTool(declared, ndeclared, "task") || !h.listing_is_last || !query)
            return SA_NONE;
        char parts[SA_MAX_PARTS][260];
        int na = SaParseAgents(body, agents, SA_MAX_AGENTS);
        if (na == 0)
            return SA_NONE;
        /* An explicit @agent the client relays (declared rule) is honored
           even for one part; it overrides the agent choice and memory. */
        int mentioned = SaMentionedAgent(h.last_user, agents, na);
        char mq[4096];
        if (mentioned >= 0)
        {
            SaMentionQuery(query, agents[mentioned].name, mq, sizeof(mq));
            query = mq;
        }
        int np = SaNamedParts(query, h.listing, workdir, parts, SA_MAX_PARTS);
        if (np < (mentioned >= 0 ? 1 : 2))
            return SA_NONE;
        if (mentioned < 0)
            for (int i = 0; i < np; i++)
            {
                int a = SaChooseAgent(agents, na, parts[i]);
                if (a < 0 || SaMemoryScore(agents[a].name, parts[i]) <= -2)
                    return SA_NONE;   /* memory says delegation fails here */
            }
        for (int i = 0; i < np; i++)
        {
            int a = mentioned >= 0 ? mentioned : SaChooseAgent(agents, na, parts[i]);
            char prompt[2600], own[1800];
            SaPartQuery(query, parts, np, i, own, sizeof(own));
            snprintf(prompt, sizeof(prompt),
                     np > 1 ? "%s\n\nYour part is only `%s`; do not change any other file "
                     "(other parts run in parallel). Make the change, build or run "
                     "the relevant check, and end with the files you changed and the "
                     "check's result." : "%s\n\nYour part is only `%s`; do not change any other file. Make the change, build or run "
                     "the relevant check, and end with the files you changed and the "
                     "check's result.", own, parts[i]);
            SaTaskCall(&d->calls, agents[a].name, parts[i], NULL, prompt);
        }
        fprintf(stderr, "[subagent] delegating %d part%s%s%s\n", np, np == 1 ? "" : "s",
                mentioned >= 0 ? " to explicit @" : "", mentioned >= 0 ? agents[mentioned].name : "");
        d->kind = SA_CALLS;
        return d->kind;
    }

    if (!h.last_is_tool || !h.last_answers_ours)
    {
        /* delegation already handed over: the normal loop owns the rest */
        d->kind = SA_DIRECT;
        return d->kind;
    }

    /* Per-part status from the latest attempt and any later re-check. */
    char parts[SA_MAX_PARTS][260];
    int np = 0;
    for (int i = 0; i < h.n; i++)
    {
        if (h.c[i].is_task != 1 || !h.c[i].part[0])
            continue;
        int dup = 0;
        for (int j = 0; j < np; j++)
            if (strcmp(parts[j], h.c[i].part) == 0) dup = 1;
        if (!dup && np < SA_MAX_PARTS)
            snprintf(parts[np++], 260, "%s", h.c[i].part);
    }
    int status[SA_MAX_PARTS], attempts[SA_MAX_PARTS], last_task[SA_MAX_PARTS], last_ver[SA_MAX_PARTS];
    char reason[SA_MAX_PARTS][1200];
    char rid[SA_MAX_PARTS][128];
    int need_verify = 0, failed_once = 0, failed_twice = 0;
    for (int k = 0; k < np; k++)
    {
        attempts[k] = 0; last_task[k] = last_ver[k] = -1;
        reason[k][0] = rid[k][0] = '\0';
        for (int i = 0; i < h.n; i++)
            if (strcmp(h.c[i].part, parts[k]) == 0)
            {
                if (h.c[i].is_task == 1) { attempts[k]++; last_task[k] = i; last_ver[k] = -1; }
                else if (last_task[k] >= 0) last_ver[k] = i;
            }
        SA_CALL *t = &h.c[last_task[k]];
        char state[16], text[4096], files[SERVER_CHILD_MAX_FILES][260];
        if (!t->answered) { status[k] = P_WAIT; continue; }
        SaParseTaskOutput(t->output, rid[k], sizeof(rid[k]), state, sizeof(state), text, sizeof(text));
        if (strcmp(state, "completed") != 0)
        {
            status[k] = P_FAILED;
            SaTail(text, 600, reason[k], sizeof(reason[k]));
        }
        else
        {
            int nf = SaChildFiles(text, files, SERVER_CHILD_MAX_FILES), outside = 0;
            for (int f = 0; f < nf; f++)
                if (strcmp(SaRel(files[f], workdir), parts[k]) != 0) outside = 1;
            if (outside)
            {
                status[k] = P_FAILED;
                snprintf(reason[k], sizeof(reason[k]), "the child changed files outside its part `%s`", parts[k]);
            }
            else if (last_ver[k] < 0)
            {
                char cmd[512];
                status[k] = ServerDeriveSingleCCommand(query, parts[k], cmd, sizeof(cmd)) ? P_VERIFY : P_UNVERIFIED;
            }
            else if (!h.c[last_ver[k]].answered)
                status[k] = P_WAIT;
            else if (SaVerifyExit(h.c[last_ver[k]].output) == 0)
                status[k] = P_DONE;
            else
            {
                status[k] = P_FAILED;
                char tail[900];
                SaTail(h.c[last_ver[k]].output, 800, tail, sizeof(tail));
                snprintf(reason[k], sizeof(reason[k]), "re-verification here failed:\n%s", tail);
            }
        }
        if (status[k] == P_VERIFY) need_verify++;
        if (status[k] == P_FAILED) { if (attempts[k] < 2) failed_once++; else failed_twice++; }
    }
    for (int k = 0; k < np; k++)
        if (status[k] == P_WAIT)
            return SA_NONE;

    if (need_verify)
    {
        for (int k = 0; k < np; k++)
            if (status[k] == P_VERIFY)
            {
                char cmd[512], full[700], ec[800], edesc[400], args[SERVER_ARG_JSON_MAX];
                ServerDeriveSingleCCommand(query, parts[k], cmd, sizeof(cmd));
                snprintf(full, sizeof(full), "%s; echo \"symbols-exit=$?\"", cmd);
                ServerJsonEscape(full, ec, sizeof(ec));
                snprintf(full, sizeof(full), "%s%s", SA_VERIFY_MARK, parts[k]);
                ServerJsonEscape(full, edesc, sizeof(edesc));
                snprintf(args, sizeof(args), "{\"command\":\"%s\",\"description\":\"%s\"}", ec, edesc);
                SaAddCall(&d->calls, "bash", args);
            }
        d->kind = SA_CALLS;
        return d->kind;
    }
    if (failed_once)
    {
        for (int k = 0; k < np; k++)
            if (status[k] == P_FAILED && attempts[k] < 2)
            {
                char prompt[2400];
                snprintf(prompt, sizeof(prompt),
                         "Your earlier work on `%s` is not done: %s\n\nFix only `%s`, build or run "
                         "the check again, and end with the files you changed and the check's result.",
                         parts[k], reason[k], parts[k]);
                SaTaskCall(&d->calls, h.c[last_task[k]].subagent, parts[k], rid[k], prompt);
                SaMemoryRecord(h.c[last_task[k]].subagent, parts[k], "failed");
            }
        d->kind = SA_CALLS;
        return d->kind;
    }
    /* Final: record outcomes, then either report or do the rest directly. */
    size_t o = 0;
    o += (size_t)snprintf(d->text + o, sizeof(d->text) - o, np == 1 ? "Delegated %d part:" : "Delegated %d parts in parallel:", np);
    for (int k = 0; k < np && o < sizeof(d->text); k++)
    {
        const char *agent = h.c[last_task[k]].subagent;
        const char *st = status[k] == P_DONE ? "done, rebuilt here: ok"
                       : status[k] == P_UNVERIFIED ? "child reports done; no build check applies to this file, not re-verified"
                       : "failed after one resume; doing it directly";
        SaMemoryRecord(agent, parts[k], status[k] == P_DONE ? "verified" : status[k] == P_FAILED ? "failed" : "unverified");
        o += (size_t)snprintf(d->text + o, sizeof(d->text) - o, "\n- `%s` -> %s (task %s, %d attempt%s): %s",
                              parts[k], agent, rid[k][0] ? rid[k] : "-", attempts[k],
                              attempts[k] == 1 ? "" : "s", st);
    }
    d->kind = failed_twice ? SA_DIRECT : SA_TEXT;
    fprintf(stderr, "[subagent] %s\n", d->text);
    return d->kind;
}

/* ---------------------------------------------------------------- strip */

static int SaOurCallId(const SA_HIST *h, const char *id)
{
    for (int i = 0; i < h->n; i++)
        if (strcmp(h->c[i].id, id) == 0)
            return 1;
    return 0;
}

/* Does this element (tools[] or messages[] entry) belong to us? */
static int SaDropElement(const SA_HIST *h, const char *e, const char *ee, int in_tools)
{
    if (in_tools)
    {
        const char *fn = SaKey(e, ee, "function");
        char name[64] = "";
        if (fn && *fn == '{')
            SaString(SaKey(fn, SaSkipValue(fn), "name"), name, sizeof(name));
        return strcmp(name, "task") == 0;
    }
    char role[16], id[64];
    SaString(SaKey(e, ee, "role"), role, sizeof(role));
    if (strcmp(role, "tool") == 0)
    {
        SaString(SaKey(e, ee, "tool_call_id"), id, sizeof(id));
        return SaOurCallId(h, id);
    }
    if (strcmp(role, "assistant") == 0)
    {
        const char *tc = SaKey(e, ee, "tool_calls");
        if (!tc || *tc != '[')
            return 0;
        const char *te = SaSkipValue(tc), *c = SaWs(tc + 1);
        int ours = 0, other = 0;
        while (c < te && *c == '{')
        {
            const char *ce = SaSkipValue(c);
            SaString(SaKey(c, ce, "id"), id, sizeof(id));
            if (SaOurCallId(h, id)) ours++; else other++;
            c = SaWs(ce);
            if (*c == ',') c = SaWs(c + 1);
        }
        return ours > 0 && other == 0;
    }
    return 0;
}

static int SaCopyArray(const SA_HIST *h, const char **src, const char *arr, int in_tools,
                       char *out, size_t size, size_t *o)
{
    /* copy everything before the array, then kept elements */
    size_t pre = (size_t)(arr - *src);
    if (*o + pre + 2 >= size) return 0;
    memcpy(out + *o, *src, pre);
    *o += pre;
    out[(*o)++] = '[';
    const char *end = SaSkipValue(arr);
    const char *p = SaWs(arr + 1);
    int first = 1;
    while (p < end && *p != ']')
    {
        const char *pe = SaSkipValue(p);
        if (!SaDropElement(h, p, pe, in_tools))
        {
            size_t n = (size_t)(pe - p);
            if (*o + n + 2 >= size) return 0;
            if (!first) out[(*o)++] = ',';
            memcpy(out + *o, p, n);
            *o += n;
            first = 0;
        }
        p = SaWs(pe);
        if (*p == ',') p = SaWs(p + 1);
    }
    out[(*o)++] = ']';
    *src = end;
    return 1;
}

int SaStripExchanges(const char *body, char *out, size_t size)
{
    static SA_HIST h;
    if (!body || !out || size == 0)
        return 0;
    SaParseHistory(body, &h);
    const char *tools = SaBodyArray(body, "tools");
    const char *msgs = SaBodyArray(body, "messages");
    const char *first = tools, *second = msgs;
    int first_tools = 1;
    if (tools && msgs && msgs < tools) { first = msgs; second = tools; first_tools = 0; }
    const char *src = body;
    size_t o = 0;
    if (first && !SaCopyArray(&h, &src, first, first_tools, out, size, &o))
        return 0;
    if (second && !SaCopyArray(&h, &src, second, !first_tools, out, size, &o))
        return 0;
    size_t rest = strlen(src);
    if (o + rest + 1 > size)
        return 0;
    memcpy(out + o, src, rest + 1);
    return 1;
}
