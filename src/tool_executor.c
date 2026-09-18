/* tool_executor: minimal tool execution over static in-memory
   tables. English code comments (project rule); Spanish only in NLG
   literals, which always carry source marking ("fuente externa" /
   "calculo") so tool data is never confused with corpus record.
   Tables are external-source stand-ins: consulted, never merged
   into the corpus KB. No HTTP, DB, MCP or filesystem. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "bible_chat.h"
#include "tool_contract.h"

static int ToolEvalExpr(const char *expr, double *out);

/* relation fixtures: keyed by display token (deduced stems like
   "rey" or the user's own echoed word like "mother"). */
static const struct
{
    const char *subject;
    const char *rel;
    const char *object;
} REL_TBL[] = {
    {"babylonia", "rey", "nebuchadnezzar"},
    {"david", "mother", "nitzevet"},
    {"saul", "rey", "israel"},
};

/* person fixtures: detail is display-ready fixture data. */
static const struct
{
    const char *name;
    const char *detail;
} PERSON_TBL[] = {
    {"jonas", "nacio en Gathepher"},
    {"jesse", "nacio en Bethlehem"},
};

void ToolExecute(const ToolRequest *req, ToolResult *res)
{
    memset(res, 0, sizeof(*res));
    if (req == NULL)
        return;
    res->tool = req->tool;
    switch (req->tool)
    {
    case TOOL_LOOKUP_RELATION:
        for (size_t i = 0;
             i < sizeof(REL_TBL) / sizeof(REL_TBL[0]) &&
             res->nitems < 8;
             i++)
            if (strcmp(REL_TBL[i].subject, req->subject) == 0 &&
                strcmp(REL_TBL[i].rel, req->relation) == 0)
            {
                strncpy(res->items[res->nitems], REL_TBL[i].object,
                        CHAT_TOKEN_MAX - 1);
                res->items[res->nitems][CHAT_TOKEN_MAX - 1] = '\0';
                res->nitems++;
            }
        res->ok = res->nitems > 0;
        break;
    case TOOL_LOOKUP_PERSON:
        for (size_t i = 0;
             i < sizeof(PERSON_TBL) / sizeof(PERSON_TBL[0]); i++)
            if (strcmp(PERSON_TBL[i].name, req->subject) == 0)
            {
                strncpy(res->items[0], PERSON_TBL[i].detail,
                        CHAT_TOKEN_MAX - 1);
                res->items[0][CHAT_TOKEN_MAX - 1] = '\0';
                res->nitems = 1;
                res->ok = 1;
                break;
            }
        break;
    case TOOL_CALCULATOR:
    {
        /* symbolic operators only (+ - * / % ^ parens); word forms
           ("por", "entre") are out of scope and fail closed. */
        double v = 0.0;
        if (ToolEvalExpr(req->subject, &v))
        {
            snprintf(res->number, sizeof(res->number), "%g", v);
            res->ok = 1;
        }
        break;
    }
    default:
        break;
    }
}

/* tiny expression parser: recursive descent over + - * / % ^,
   parens and unary minus (doubles; division by zero fails). */
static const char *g_ep;

static void EpSkip(void)
{
    while (*g_ep && isspace((unsigned char)*g_ep))
        g_ep++;
}

static int EpNumber(double *v)
{
    char *end = NULL;
    EpSkip();
    if (!isdigit((unsigned char)*g_ep) && *g_ep != '.')
        return 0;
    *v = strtod(g_ep, &end);
    if (end == g_ep)
        return 0;
    g_ep = end;
    return 1;
}

static int EpExpr(double *v);

static int EpFact(double *v)
{
    EpSkip();
    if (*g_ep == '(')
    {
        g_ep++;
        if (!EpExpr(v))
            return 0;
        EpSkip();
        if (*g_ep != ')')
            return 0;
        g_ep++;
        return 1;
    }
    if (*g_ep == '-')
    {
        g_ep++;
        if (!EpFact(v))
            return 0;
        *v = -*v;
        return 1;
    }
    return EpNumber(v);
}

static int EpTerm(double *v)
{
    double rhs = 0.0;
    if (!EpFact(v))
        return 0;
    for (;;)
    {
        EpSkip();
        if (*g_ep == '*' || *g_ep == '/' || *g_ep == '%')
        {
            char op = *g_ep++;
            if (!EpFact(&rhs))
                return 0;
            if (op == '*')
                *v *= rhs;
            else if (op == '/')
            {
                if (rhs == 0.0)
                    return 0;
                *v /= rhs;
            }
            else
            {
                long a = (long)*v, b = (long)rhs;
                if (b == 0 || (double)a != *v || (double)b != rhs)
                    return 0;
                *v = (double)(a % b);
            }
        }
        else if (*g_ep == '^')
        {
            g_ep++;
            if (!EpFact(&rhs))
                return 0;
            *v = pow(*v, rhs);
        }
        else
            return 1;
    }
}

static int EpExpr(double *v)
{
    double rhs = 0.0;
    if (!EpTerm(v))
        return 0;
    for (;;)
    {
        EpSkip();
        if (*g_ep == '+' || *g_ep == '-')
        {
            char op = *g_ep++;
            if (!EpTerm(&rhs))
                return 0;
            if (op == '+')
                *v += rhs;
            else
                *v -= rhs;
        }
        else
            return 1;
    }
}

static int ToolEvalExpr(const char *expr, double *out)
{
    double v = 0.0;
    if (expr == NULL || out == NULL)
        return 0;
    g_ep = expr;
    if (!EpExpr(&v))
        return 0;
    EpSkip();
    if (*g_ep != '\0')
        return 0;
    *out = v;
    return 1;
}

int ToolAnswerGoal(const ToolRequest *req, const ToolResult *res,
                   char *out, size_t size)
{
    char capS[CHAT_TOKEN_MAX];
    if (size > 0)
        out[0] = '\0';
    if (req == NULL || res == NULL || !res->ok ||
        res->tool != req->tool)
        return 0;
    switch (req->tool)
    {
    case TOOL_CALCULATOR:
        if (res->number[0] == '\0' || req->subject[0] == '\0')
            return 0;
        if (size > 0)
            snprintf(out, size, "Segun calculo, %s = %s.\n",
                     req->subject, res->number);
        return 1;
    case TOOL_LOOKUP_RELATION:
        if (res->nitems == 0 || req->subject[0] == '\0' ||
            req->relation[0] == '\0')
            return 0;
        ChatCapStr(req->subject, capS, sizeof(capS));
        if (size > 0)
        {
            size_t pos = 0;
            int wr = snprintf(out + pos, size - pos,
                              "Segun fuente externa, %s es %s de ",
                              capS, req->relation);
            if (wr > 0)
                pos += (size_t)wr;
            for (uint32_t i = 0; i < res->nitems && pos + 1 < size;
                 i++)
            {
                char cap[CHAT_TOKEN_MAX];
                ChatCapStr(res->items[i], cap, sizeof(cap));
                if (i > 0)
                {
                    if (pos + 2 >= size)
                        break;
                    out[pos++] = ',';
                    out[pos++] = ' ';
                }
                wr = snprintf(out + pos, size - pos, "%s", cap);
                if (wr <= 0)
                    break;
                pos += (size_t)wr;
            }
            if (pos + 2 < size)
            {
                out[pos++] = '.';
                out[pos++] = '\n';
            }
            out[pos < size ? pos : size - 1] = '\0';
        }
        return 1;
    case TOOL_LOOKUP_PERSON:
        if (res->nitems == 0 || req->subject[0] == '\0')
            return 0;
        ChatCapStr(req->subject, capS, sizeof(capS));
        if (size > 0)
            snprintf(out, size, "Segun fuente externa, %s: %s.\n", capS,
                     res->items[0]);
        return 1;
    default:
        return 0;
    }
}
