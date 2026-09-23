/* Source: src/numeric.c (NumericParseMeasure), verbatim.
   Oracle: the parse cases of tests/test_numeric.c (units in ASCII). */
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

int NumericParseMeasure(const char *text, double *value,
                        char *unit, size_t unit_size)
{
    char core[64];
    char norm[64];
    size_t i, ncore = 0, n = 0;
    int has_digit = 0;
    int has_dot = 0, has_comma = 0, has_slash = 0;
    size_t last_sep = 0, frac_digits = 0;

    if (text == NULL)
        return 0;
    while (*text == ' ' || *text == '\t')
        text++;

    /* Split numeric core from trailing unit: the core ends at the
       last byte that belongs to [0-9 . , / -] after an optional sign.
       A '-' only counts inside the core (thousands never follow it). */
    {
        const char *p = text;
        const char *core_end = NULL;
        int pos = 0;
        if (*p == '-' || *p == '+')
            p++;
        while (*p != '\0')
        {
            unsigned char c = (unsigned char)*p;
            int ok = (c >= '0' && c <= '9') || c == '.' || c == ',' ||
                     c == '/' || c == ' ' || c == '\'';
            if (c == '-' && pos > 0)
                ok = 0;
            if (!ok)
                break;
            core_end = p + 1;
            p++;
            pos++;
        }
        if (core_end == NULL)
            return 0;
        n = (size_t)(core_end - text);
        if (n >= sizeof(core))
            return 0;
        memcpy(core, text, n);
        core[n] = '\0';
        /* Unit: trailing run, trimmed of spaces. */
        {
            const char *u = core_end;
            while (*u == ' ' || *u == '\t')
                u++;
            if (unit != NULL && unit_size > 0)
            {
                size_t ul = strlen(u);
                while (ul > 0 && (u[ul - 1] == ' ' || u[ul - 1] == '\t'))
                    ul--;
                if (ul >= unit_size)
                    ul = unit_size - 1;
                memcpy(unit, u, ul);
                unit[ul] = '\0';
            }
        }
    }

    /* Strip inner spaces/apostrophes (thousands grouping variants). */
    ncore = 0;
    for (i = 0; i < n && ncore + 1 < sizeof(core); i++)
    {
        if (core[i] == ' ' || core[i] == '\t' || core[i] == '\'')
            continue;
        core[ncore++] = core[i];
    }
    core[ncore] = '\0';

    for (i = 0; i < ncore; i++)
    {
        if (core[i] >= '0' && core[i] <= '9')
            has_digit = 1;
        else if (core[i] == '.')
        {
            has_dot = 1;
            last_sep = i;
        }
        else if (core[i] == ',')
        {
            has_comma = 1;
            last_sep = i;
        }
        else if (core[i] == '/')
            has_slash = 1;
        else if (!(i == 0 && (core[i] == '-' || core[i] == '+')))
            return 0;
    }
    if (!has_digit || has_slash)
        return 0;
    frac_digits = (has_dot || has_comma) ? (ncore - 1 - last_sep) : 0;

    /* Normalize to strtod form. */
    {
        size_t o = 0;
        for (i = 0; i < ncore && o + 1 < sizeof(norm); i++)
        {
            char c = core[i];
            if (c == '.')
            {
                if (has_comma)
                    continue;               /* thousands dot */
                if (frac_digits == 3 && ncore > 4)
                    continue;               /* thousands dot, es style */
                norm[o++] = '.';
            }
            else if (c == ',')
            {
                if (has_dot)
                    norm[o++] = '.';        /* decimal comma */
                else
                    norm[o++] = '.';
            }
            else
                norm[o++] = c;
        }
        norm[o] = '\0';
    }

    {
        char *end = NULL;
        double v = strtod(norm, &end);
        if (end == norm || *end != '\0')
            return 0;
        if (value != NULL)
            *value = v;
    }
    return 1;
}

static int same(double a, double b) { double d = a - b; return d < 1e-9 && d > -1e-9; }

int main(void)
{
    double v;
    char u[32];
    int bad = 0, ok;
    v = 0; u[0] = 0; ok = NumericParseMeasure("505.990 KM", &v, u, sizeof(u));
    bad += !(ok == 1 && same(v, 505990.0) && strcmp(u, "KM") == 0);
    v = 0; u[0] = 0; ok = NumericParseMeasure("8,47", &v, u, sizeof(u));
    bad += !(ok == 1 && same(v, 8.47) && strcmp(u, "") == 0);
    v = 0; u[0] = 0; ok = NumericParseMeasure("47.5", &v, u, sizeof(u));
    bad += !(ok == 1 && same(v, 47.5) && strcmp(u, "") == 0);
    v = 0; u[0] = 0; ok = NumericParseMeasure("-3", &v, u, sizeof(u));
    bad += !(ok == 1 && same(v, -3.0) && strcmp(u, "") == 0);
    v = 0; u[0] = 0; ok = NumericParseMeasure("1978", &v, u, sizeof(u));
    bad += !(ok == 1 && same(v, 1978.0) && strcmp(u, "") == 0);
    v = 0; u[0] = 0; ok = NumericParseMeasure("1.234,56", &v, u, sizeof(u));
    bad += !(ok == 1 && same(v, 1234.56) && strcmp(u, "") == 0);
    v = 0; u[0] = 0; ok = NumericParseMeasure("3 MILLONES", &v, u, sizeof(u));
    bad += !(ok == 1 && same(v, 3.0) && strcmp(u, "MILLONES") == 0);
    v = 0; u[0] = 0; ok = NumericParseMeasure("667 M", &v, u, sizeof(u));
    bad += !(ok == 1 && same(v, 667.0) && strcmp(u, "M") == 0);
    v = 0; u[0] = 0; ok = NumericParseMeasure("1.234", &v, u, sizeof(u));
    bad += !(ok == 1 && same(v, 1234.0) && strcmp(u, "") == 0);
    v = 0; u[0] = 0; ok = NumericParseMeasure("12/10/1492", &v, u, sizeof(u));
    bad += ok != 0;
    v = 0; u[0] = 0; ok = NumericParseMeasure("today", &v, u, sizeof(u));
    bad += ok != 0;
    v = 0; u[0] = 0; ok = NumericParseMeasure("KM", &v, u, sizeof(u));
    bad += ok != 0;
    v = 0; u[0] = 0; ok = NumericParseMeasure("", &v, u, sizeof(u));
    bad += ok != 0;
    return bad == 0 ? 0 : 1;
}
