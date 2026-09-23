/* Source: src/tool_executor.c (calculator: EpSkip..ToolEvalExpr), verbatim
   except the '^' branch (it needs libm) and ToolEvalExpr made non-static.
   Oracle: expected values of the calculator's grammar. */
#include <ctype.h>
#include <stdlib.h>

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

int ToolEvalExpr(const char *expr, double *out)
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

int main(void)
{
    double v;
    int bad = 0;
    bad += !(ToolEvalExpr("2+3*4", &v) == 1 && v == 14);
    bad += !(ToolEvalExpr("(2+3)*4", &v) == 1 && v == 20);
    bad += !(ToolEvalExpr("7%3", &v) == 1 && v == 1);
    bad += !(ToolEvalExpr("-2+5", &v) == 1 && v == 3);
    bad += !(ToolEvalExpr("10-4-3", &v) == 1 && v == 3);
    bad += !(ToolEvalExpr("8/2/2", &v) == 1 && v == 2);
    bad += !(ToolEvalExpr("2*(3+4)-5", &v) == 1 && v == 9);
    bad += !(ToolEvalExpr("1.5*4", &v) == 1 && v == 6);
    bad += ToolEvalExpr("1/0", &v) != 0;
    bad += ToolEvalExpr("1+", &v) != 0;
    bad += ToolEvalExpr("(1+2", &v) != 0;
    bad += ToolEvalExpr("7%0", &v) != 0;
    return bad == 0 ? 0 : 1;
}
