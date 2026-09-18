/* chat_clarify: conversational wrapper over the pure engine.
   English code comments (project rule); Spanish only in NLG literals,
   which reuse legacy templates plus the one minimal clarification
   question below. Resolution inputs are digits and the candidates'
   own names: zero new vocabulary. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "chat_clarify.h"

#define CLAR_MAX_TOKS 16

void ClarifyInit(CLARIFY *w, const char *corpus_path)
{
    memset(w, 0, sizeof(*w));
    ChatInit(&w->ch, corpus_path);
}

/* minimal disambiguation question: legacy prefix + candidates in
   ingest order (deterministic for a frozen corpus), each numbered so
   the reply needs no ordinal vocabulary. No identity is asserted:
   these are competing constancias, homonyms and spelling variants
   alike (James->Alphaeus+Zebedee, Saul->Cis+Kish). */
static void ClarifyAsk(const CLARIFY *w, char *out, size_t size)
{
    char capC[CHAT_TOKEN_MAX];
    size_t pos = 0;
    out[0] = '\0';
    if (size == 0 || w->ncand == 0)
        return;
    ChatCapStr(w->child, capC, sizeof(capC));
    {
        int wr = snprintf(out + pos, size - pos,
                          "Hay %u constancias del padre de %s: ?",
                          w->ncand, capC);
        if (wr > 0)
            pos += (size_t)wr;
    }
    for (uint32_t i = 0; i < w->ncand && pos + 1 < size; i++)
    {
        char cap[CHAT_TOKEN_MAX];
        const char *sep =
            (i == 0) ? "" : ((i + 1 == w->ncand) ? " o " : ", ");
        size_t L = strlen(sep);
        ChatCapStr(w->cand[i], cap, sizeof(cap));
        if (pos + L >= size)
            break;
        memcpy(out + pos, sep, L);
        pos += L;
        {
            int wr = snprintf(out + pos, size - pos, "%s (%u)", cap,
                              i + 1);
            if (wr <= 0)
                break;
            pos += (size_t)wr;
        }
    }
    if (pos + 2 < size)
    {
        out[pos++] = '?';
        out[pos++] = '\n';
    }
    out[pos < size ? pos : size - 1] = '\0';
}

/* split reply on whitespace; strip leading inverted marks and
   trailing ASCII punctuation (positions and bytes, no words). */
static uint32_t ReplyToks(const char *line,
                          char toks[][CHAT_TOKEN_MAX], uint32_t max)
{
    uint32_t n = 0;
    const char *p = line;
    while (*p && n < max)
    {
        while (*p && isspace((unsigned char)*p))
            p++;
        if (!*p)
            break;
        const char *s = p;
        while (*p && !isspace((unsigned char)*p))
            p++;
        size_t len = (size_t)(p - s);
        while (len >= 2 && (unsigned char)s[0] == 0xC2 &&
               (unsigned char)s[1] == 0xBF)
        {
            s += 2;
            len -= 2;
        }
        while (len > 0)
        {
            char c = s[len - 1];
            if (c == '?' || c == '!' || c == '.' || c == ',' ||
                c == ';' || c == ':')
                len--;
            else
                break;
        }
        if (len == 0 || len >= CHAT_TOKEN_MAX)
            continue;
        memcpy(toks[n], s, len);
        toks[n][len] = '\0';
        n++;
    }
    return n;
}

/* Try the reply as a resolution of the pending clarification.
   Returns 1 when consumed (resolved, re-asked or closed UNKNOWN),
   0 when it is a fresh query (pending discarded by the caller). */
static int ClarifyTryResolve(CLARIFY *w, const char *line, char *out,
                             size_t size)
{
    char toks[CLAR_MAX_TOKS][CHAT_TOKEN_MAX];
    uint32_t n = ReplyToks(line, toks, CLAR_MAX_TOKS);
    int dig = -1, dig_conflict = 0, dig_oor = 0;
    int hit = -1, hit_multi = 0;
    for (uint32_t i = 0; i < n; i++)
    {
        int alldig = toks[i][0] != '\0';
        for (size_t k = 0; toks[i][k] != '\0' && alldig; k++)
            if (!isdigit((unsigned char)toks[i][k]))
                alldig = 0;
        if (alldig)
        {
            long v = strtol(toks[i], NULL, 10);
            if (v >= 1 && v <= (long)w->ncand)
            {
                if (dig < 0)
                    dig = (int)v;
                else if (dig != (int)v)
                    dig_conflict = 1;
            }
            else
                dig_oor = 1;
            continue;
        }
        {
            char norm[CHAT_TOKEN_MAX];
            ChatNormTok(toks[i], norm, sizeof(norm));
            for (uint32_t c = 0; c < w->ncand; c++)
                if (strcmp(norm, w->cand[c]) == 0)
                {
                    if (hit < 0)
                        hit = (int)c;
                    else if (hit != (int)c)
                        hit_multi = 1;
                }
        }
    }
    {
        int pick = -1;
        if (hit >= 0 && !hit_multi && dig < 0)
            pick = hit;
        else if (dig > 0 && !dig_conflict && hit < 0)
            pick = dig - 1;
        else if (dig > 0 && !dig_conflict && hit == dig - 1)
            pick = hit;
        if (pick >= 0)
        {
            ChatAnswerParentSingle(&w->ch, w->child, w->cand[pick],
                                   out, size);
            w->pending = 0;
            w->attempts = 0;
            return 1;
        }
    }
    if (hit < 0 && dig < 0 && !dig_oor)
        return 0; /* fresh query: caller discards pending */
    w->attempts++;
    if (w->attempts >= 2)
    {
        if (size > 0)
            snprintf(out, size,
                     "No tengo constancia de a quien te refieres en los "
                     "textos cargados.\n");
        w->pending = 0;
        w->attempts = 0;
        return 1;
    }
    ClarifyAsk(w, out, size);
    return 1;
}

void ClarifyHandle(CLARIFY *w, const char *line)
{
    char toks[CLAR_MAX_TOKS][CHAT_TOKEN_MAX];
    QueryPlan plan;
    uint32_t ntok = 0;
    SURFACE_FLAGS sf;
    char out[CLAR_OUT_MAX];
    memset(&sf, 0, sizeof(sf));
    if (w->pending)
    {
        if (ClarifyTryResolve(w, line, out, sizeof(out)))
        {
            printf("%s", out);
            return;
        }
        w->pending = 0;
        w->attempts = 0;
    }
    if (ChatBuildPlan(&w->ch, line, &plan, toks, &ntok, &sf) >= 2)
    {
        ChatHandle(&w->ch, line);
        return;
    }
    {
        char slot[CHAT_TOKEN_MAX];
        int st = ChatResolveLine(&w->ch, line, out, sizeof(out), slot,
                                 sizeof(slot), NULL, 0, NULL);
        if (st < 0)
        {
            printf("No entendi la pregunta.\n");
            return;
        }
        if (st == GOAL_AMBIGUOUS)
        {
            uint32_t m = ChatParentsList(&w->ch, slot, w->cand,
                                         CLAR_MAX_CAND);
            if (m >= 2)
            {
                w->pending = 1;
                w->attempts = 0;
                strncpy(w->child, slot, sizeof(w->child) - 1);
                w->child[sizeof(w->child) - 1] = '\0';
                w->ncand = m;
                ClarifyAsk(w, out, sizeof(out));
                printf("%s", out);
                return;
            }
        }
        printf("%s", out);
    }
}
