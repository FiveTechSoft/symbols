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
    if (strncmp(line, "/persona", 8) == 0 || strncmp(line, ":persona", 8) == 0 ||
        (strncmp(line, "persona ", 8) == 0 && !strchr(line, '?')))
    {
        const char *p = strchr(line, ' ');
        while (p && isspace((unsigned char)*p)) p++;
        if (p && *p)
        {
            PERSONA_ID pid = PersonaFindByName(p);
            ChatSetPersona(&w->ch, pid);
            if (pid == PERSONA_PIRATE_QUANTUM)
                printf("Ahoy, capitan! Ahora os habla el contramaestre cuantico desde el castillo de proa.\n");
            else
                printf("Modo persona configurado a: %s\n", PersonaGetName(pid));
            return;
        }
    }
    if (strcmp(line, "modo pirata") == 0 || strcmp(line, "/pirata") == 0 ||
        strcmp(line, ":pirata") == 0)
    {
        ChatSetPersona(&w->ch, PERSONA_PIRATE_QUANTUM);
        printf("Ahoy, capitan! Ahora os habla el contramaestre cuantico desde el castillo de proa.\n");
        return;
    }

    /* Episodic memory commands: /learn, /aprende, /memory, /forget */
    if (strncmp(line, "/learn ", 7) == 0 || strncmp(line, "/aprende ", 9) == 0 ||
        strncmp(line, ":learn ", 7) == 0 || strncmp(line, ":aprende ", 9) == 0)
    {
        const char *p = strchr(line, ' ');
        while (p && isspace((unsigned char)*p)) p++;
        char s[64], r[64], o[64];
        if (p && sscanf(p, "%63s %63s %63s", s, r, o) == 3)
        {
            int rc = ChatLearnTriple(&w->ch, s, r, o, "user");
            if (rc == 1)
                printf("[memoria] Hecho aprendido y guardado persistentemente: %s --%s--> %s.\n", s, r, o);
            else if (rc == 2)
                printf("[memoria] Hecho ya conocido, reforzado en memoria: %s --%s--> %s.\n", s, r, o);
            else
                printf("[memoria] No se pudo incorporar la tripleta.\n");
        }
        else
        {
            printf("Uso: /learn SUJETO RELACION OBJETO  (ej: /learn Juan hermano_de Pedro)\n");
        }
        return;
    }

    if (strcmp(line, "/memory") == 0 || strcmp(line, "/memoria") == 0 ||
        strcmp(line, ":memory") == 0 || strcmp(line, ":memoria") == 0 ||
        strcmp(line, "/episodic") == 0)
    {
        uint32_t cnt = ChatEpisodicCount(&w->ch);
        if (cnt == 0)
        {
            printf("[memoria] No hay recuerdos episodicos guardados actualmente en data/memory/episodic.tsv.\n");
        }
        else
        {
            printf("[memoria] %u recuerdos episodicos continuos guardados en data/memory/episodic.tsv:\n", cnt);
            for (uint32_t i = 0; i < cnt; i++)
            {
                const EPISODIC_RECORD *rec = ChatEpisodicGet(&w->ch, i);
                if (rec)
                {
                    printf("  %u. %s --%s--> %s (origen: %s)\n",
                           i + 1, rec->subject, rec->relation, rec->object, rec->source);
                }
            }
        }
        return;
    }

    /* Selective forgetting: /forget SUJETO RELACION OBJETO */
    if (strncmp(line, "/forget ", 8) == 0 || strncmp(line, "/olvida ", 8) == 0 ||
        strncmp(line, ":forget ", 8) == 0)
    {
        const char *p = strchr(line, ' ');
        while (p && isspace((unsigned char)*p)) p++;
        char s_tok[64], r_tok[64], o_tok[64];
        if (p && sscanf(p, "%63s %63s %63s", s_tok, r_tok, o_tok) == 3)
        {
            int rc = ChatForgetTriple(&w->ch, s_tok, r_tok, o_tok);
            if (rc == 1)
                printf("[memoria] Hecho olvidado: %s --%s--> %s (eliminado de la sesion y del disco).\n", s_tok, r_tok, o_tok);
            else if (rc == 0)
                printf("[memoria] Ese hecho no estaba en la memoria episodica: %s --%s--> %s.\n", s_tok, r_tok, o_tok);
            else
                printf("[memoria] No se pudo escribir la memoria en disco; el hecho sigue intacto.\n");
        }
        else
        {
            printf("Uso: /forget SUJETO RELACION OBJETO  (ej: /forget Juan hermano_de Pedro)\n");
        }
        return;
    }

    if (strcmp(line, "/forget") == 0 || strcmp(line, "/olvida") == 0 ||
        strcmp(line, ":forget") == 0 || strcmp(line, "/clear-memory") == 0)
    {
        if (ChatEpisodicClear(&w->ch))
            printf("[memoria] Memoria episodica borrada tanto de la sesion como de disco.\n");
        else
            printf("[memoria] No se pudo escribir la memoria en disco; sigue intacta.\n");
        return;
    }

    /* Conversational natural language learning:
       "aprende que S es P de O" / "recuerda que S es P de O" / "learn that S is P of O" */
    if (strncasecmp(line, "aprende que ", 12) == 0 ||
        strncasecmp(line, "recuerda que ", 13) == 0 ||
        strncasecmp(line, "learn that ", 11) == 0)
    {
        const char *p = strchr(line, ' ');
        if (p) p = strchr(p + 1, ' ');
        while (p && isspace((unsigned char)*p)) p++;
        char s[64], copula[32], r[64], prep[32], o[64];
        if (p && sscanf(p, "%63s %31s %63s %31s %63s", s, copula, r, prep, o) == 5 &&
            (strcasecmp(copula, "es") == 0 || strcasecmp(copula, "is") == 0) &&
            (strcasecmp(prep, "de") == 0 || strcasecmp(prep, "of") == 0))
        {
            char full_rel[64];
            snprintf(full_rel, sizeof(full_rel), "%s_%s", r, prep);
            int rc = ChatLearnTriple(&w->ch, s, full_rel, o, "conversation");
            if (rc)
            {
                printf("[memoria] Hecho registrado: %s es %s de %s (guardado en memoria continua).\n", s, r, o);
                return;
            }
            printf("[memoria] No se pudo guardar el hecho de forma persistente; no quedo registrado.\n");
            return;
        }
    }


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
    if (ChatTryTextLine(&w->ch, line, out, sizeof(out)))
    {
        printf("%s", out);
        return;
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
