/* agent_action: deterministic text protocol (pure, no deps).
   English code comments (project rule); protocol keywords follow
   the frozen spec (ACTION/FINAL/ABSTAIN/OBSERVATION). */
#include <stdio.h>
#include <string.h>
#include "agent_action.h"

static int HasCtrl(const char *s)
{
    /* quotes and newlines never travel raw: argv forbids quotes by
       allowlist, output escapes them; anything else is a bug. */
    for (; *s != '\0'; s++)
        if (*s == '"' || *s == '\n' || *s == '\r')
            return 1;
    return 0;
}

int AgentActionFormat(const AgentAction *act, char *buf, size_t sz)
{
    int w = -1;
    if (act == NULL || buf == NULL || sz == 0)
        return 0;
    buf[0] = '\0';
    if (act->type == ACTION_TOOL_CALL)
    {
        if (HasCtrl(act->tool) || HasCtrl(act->args) ||
            HasCtrl(act->prompt))
            return 0;
        w = snprintf(buf, sz, "ACTION: TOOL=%s ARGS=\"%s\" PROMPT=\"%s\"",
                     act->tool, act->args, act->prompt);
    }
    else if (act->type == ACTION_FINAL)
    {
        if (HasCtrl(act->prompt))
            return 0;
        w = snprintf(buf, sz, "FINAL: %s", act->prompt);
    }
    else if (act->type == ACTION_ABSTAIN)
    {
        if (HasCtrl(act->prompt))
            return 0;
        w = snprintf(buf, sz, "ABSTAIN: %s", act->prompt);
    }
    else
        return 0;
    return (w > 0 && (size_t)w < sz) ? 1 : 0;
}

static int TakePrefix(const char **pp, const char *prefix)
{
    size_t n = strlen(prefix);
    if (strncmp(*pp, prefix, n) != 0)
        return 0;
    *pp += n;
    return 1;
}

static int TakeQuoted(const char **pp, char *out, size_t size)
{
    size_t o = 0;
    if (**pp != '"')
        return 0;
    (*pp)++;
    while (**pp != '\0' && **pp != '"' && o + 1 < size)
        out[o++] = *(*pp)++;
    out[o] = '\0';
    if (**pp != '"')
        return 0;
    (*pp)++;
    return 1;
}

int AgentActionParse(const char *line, AgentAction *act)
{
    const char *p;
    if (line == NULL || act == NULL)
        return 0;
    memset(act, 0, sizeof(*act));
    p = line;
    if (TakePrefix(&p, "ACTION: "))
    {
        char tool[AGENT_NAME_MAX], args[AGENT_ARGS_MAX];
        char prompt[AGENT_MSG_MAX];
        if (!TakePrefix(&p, "TOOL="))
            return 0;
        {
            size_t o = 0;
            while (*p != '\0' && *p != ' ' && o + 1 < sizeof(tool))
                tool[o++] = *p++;
            tool[o] = '\0';
            if (*p != ' ')
                return 0;
            p++;
        }
        if (!TakePrefix(&p, "ARGS="))
            return 0;
        if (!TakeQuoted(&p, args, sizeof(args)))
            return 0;
        if (*p != ' ')
            return 0;
        p++;
        if (!TakePrefix(&p, "PROMPT="))
            return 0;
        if (!TakeQuoted(&p, prompt, sizeof(prompt)))
            return 0;
        if (*p != '\0')
            return 0;
        act->type = ACTION_TOOL_CALL;
        strncpy(act->tool, tool, sizeof(act->tool) - 1);
        strncpy(act->args, args, sizeof(act->args) - 1);
        strncpy(act->prompt, prompt, sizeof(act->prompt) - 1);
        return 1;
    }
    if (TakePrefix(&p, "FINAL: "))
    {
        act->type = ACTION_FINAL;
        strncpy(act->prompt, p, sizeof(act->prompt) - 1);
        return 1;
    }
    if (TakePrefix(&p, "ABSTAIN: "))
    {
        act->type = ACTION_ABSTAIN;
        strncpy(act->prompt, p, sizeof(act->prompt) - 1);
        return 1;
    }
    return 0;
}

int AgentObservationFormat(const AgentObservation *obs, char *buf,
                           size_t sz)
{
    size_t pos = 0;
    size_t i;
    int w;
    if (obs == NULL || buf == NULL || sz == 0)
        return 0;
    buf[0] = '\0';
    if (HasCtrl(obs->tool))
        return 0;
    w = snprintf(buf + pos, sz - pos, "OBSERVATION: TOOL=%s EXIT=%d OUT=\"",
                 obs->tool, obs->exit_code);
    if (w <= 0)
        return 0;
    pos += (size_t)w;
    for (i = 0; obs->output[i] != '\0' && pos + 1 < sz; i++)
    {
        char c = obs->output[i];
        const char *esc = NULL;
        char seq[2] = {0, 0};
        if (c == '\\')
            esc = "\\\\";
        else if (c == '"')
            esc = "\\\"";
        else if (c == '\n')
            esc = "\\n";
        else if (c == '\r')
            esc = "\\r";
        else if (c == '\t')
            esc = "\\t";
        else
            seq[0] = c;
        if (esc != NULL)
        {
            if (pos + 2 >= sz)
                return 0;
            buf[pos++] = esc[0];
            buf[pos++] = esc[1];
        }
        else
            buf[pos++] = seq[0];
    }
    if (obs->output[i] != '\0')
        return 0; /* truncated */
    if (pos + 2 > sz)
        return 0;
    buf[pos++] = '"';
    buf[pos] = '\0';
    return 1;
}

int AgentObservationParse(const char *line, AgentObservation *obs)
{
    const char *p;
    char tool[AGENT_NAME_MAX];
    size_t o = 0;
    long exit_code = 0;
    int neg = 0;
    size_t oo = 0;
    if (line == NULL || obs == NULL)
        return 0;
    memset(obs, 0, sizeof(*obs));
    p = line;
    if (!TakePrefix(&p, "OBSERVATION: "))
        return 0;
    if (!TakePrefix(&p, "TOOL="))
        return 0;
    while (*p != '\0' && *p != ' ' && o + 1 < sizeof(tool))
        tool[o++] = *p++;
    tool[o] = '\0';
    if (*p != ' ')
        return 0;
    p++;
    if (!TakePrefix(&p, "EXIT="))
        return 0;
    if (*p == '-')
    {
        neg = 1;
        p++;
    }
    if (*p < '0' || *p > '9')
        return 0;
    while (*p >= '0' && *p <= '9')
    {
        exit_code = exit_code * 10 + (*p - '0');
        p++;
    }
    if (neg)
        exit_code = -exit_code;
    if (*p != ' ')
        return 0;
    p++;
    if (!TakePrefix(&p, "OUT=\""))
        return 0;
    while (*p != '\0' && *p != '"' && oo + 1 < sizeof(obs->output))
    {
        if (*p == '\\')
        {
            p++;
            if (*p == 'n')
                obs->output[oo++] = '\n';
            else if (*p == 'r')
                obs->output[oo++] = '\r';
            else if (*p == 't')
                obs->output[oo++] = '\t';
            else if (*p == '\\' || *p == '"')
                obs->output[oo++] = *p;
            else
                return 0;
            if (*p != '\0')
                p++;
        }
        else
            obs->output[oo++] = *p++;
    }
    obs->output[oo] = '\0';
    if (*p != '"' || *(p + 1) != '\0')
        return 0;
    strncpy(obs->tool, tool, sizeof(obs->tool) - 1);
    obs->exit_code = (int)exit_code;
    return 1;
}
