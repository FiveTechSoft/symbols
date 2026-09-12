#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "numeric.h"

NUMERIC_TABLE *NumericCreate(uint32_t capacity)
{
    NUMERIC_TABLE *t = (NUMERIC_TABLE *)malloc(sizeof(NUMERIC_TABLE));
    if (t == NULL)
        return NULL;
    if (capacity < 8)
        capacity = 8;
    t->items = (NUMERIC_ENTRY *)calloc(capacity, sizeof(NUMERIC_ENTRY));
    if (t->items == NULL)
    {
        free(t);
        return NULL;
    }
    t->count = 0;
    t->capacity = capacity;
    return t;
}

void NumericDestroy(NUMERIC_TABLE *t)
{
    if (t == NULL)
        return;
    free(t->items);
    free(t);
}

int NumericSet(NUMERIC_TABLE *t, SYMBOL_ID id,
               double value, const char *unit)
{
    uint32_t i;
    if (t == NULL || id == SYMBOL_INVALID)
        return 0;
    for (i = 0; i < t->count; i++)
    {
        if (t->items[i].id == id)
        {
            t->items[i].value = value;
            if (unit != NULL)
            {
                strncpy(t->items[i].unit, unit, NUMERIC_UNIT_MAX - 1);
                t->items[i].unit[NUMERIC_UNIT_MAX - 1] = '\0';
            }
            else
                t->items[i].unit[0] = '\0';
            return 1;
        }
    }
    if (t->count >= t->capacity)
        return 0;
    t->items[t->count].id = id;
    t->items[t->count].value = value;
    if (unit != NULL)
    {
        strncpy(t->items[t->count].unit, unit, NUMERIC_UNIT_MAX - 1);
        t->items[t->count].unit[NUMERIC_UNIT_MAX - 1] = '\0';
    }
    else
        t->items[t->count].unit[0] = '\0';
    t->count++;
    return 1;
}

int NumericGet(const NUMERIC_TABLE *t, SYMBOL_ID id,
               double *value, char *unit, size_t unit_size)
{
    uint32_t i;
    if (t == NULL || id == SYMBOL_INVALID)
        return 0;
    for (i = 0; i < t->count; i++)
    {
        if (t->items[i].id == id)
        {
            if (value != NULL)
                *value = t->items[i].value;
            if (unit != NULL && unit_size > 0)
            {
                strncpy(unit, t->items[i].unit, unit_size - 1);
                unit[unit_size - 1] = '\0';
            }
            return 1;
        }
    }
    return 0;
}

/* Measure grammar: [sign] digits[. ,digits]* [unit]. The numeric core
   must hold at least one digit; the unit is whatever non-numeric text
   trails it (uppercased ASCII by the caller convention, UTF-8 kept). */
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
