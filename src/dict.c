/* dict: cross-lingual translation table.
   Loads english-spanish.txt (ALIAS = CANONICAL).
   HARDCODING=0: all translations come from the external file. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "dict.h"

void DictInit(DICT *dict)
{
    if (dict == NULL)
        return;
    dict->count = 0;
    memset(dict->entries, 0, sizeof(dict->entries));
}

/* Case-insensitive string comparison (up to n chars) */
static int strieq(const char *a, const char *b)
{
    while (*a && *b)
    {
        int ca = tolower((unsigned char)*a);
        int cb = tolower((unsigned char)*b);
        if (ca != cb)
            return 0;
        a++;
        b++;
    }
    return *a == *b;
}

uint32_t DictLoad(DICT *dict, const char *filepath)
{
    FILE *f;
    char line[256];

    if (dict == NULL || filepath == NULL || filepath[0] == '\0')
        return 0;

    f = fopen(filepath, "r");
    if (f == NULL)
        return 0;

    while (fgets(line, sizeof(line), f) != NULL &&
           dict->count < DICT_MAX)
    {
        char *p = line;
        char *eq;
        size_t len;

        /* Skip whitespace */
        while (*p == ' ' || *p == '\t')
            p++;

        /* Skip comments and blank lines */
        if (*p == '#' || *p == '\0' || *p == '\n' || *p == '\r')
            continue;

        /* Remove trailing newline */
        len = strlen(p);
        while (len > 0 && (p[len - 1] == '\n' || p[len - 1] == '\r'))
            p[--len] = '\0';

        /* Find '=' separator */
        eq = strchr(p, '=');
        if (eq == NULL)
            continue;

        /* Extract alias (left side, trimmed) */
        {
            size_t alen = (size_t)(eq - p);
            while (alen > 0 && (p[alen - 1] == ' ' || p[alen - 1] == '\t'))
                alen--;
            if (alen == 0 || alen >= DICT_TOKEN_MAX)
                continue;
            memcpy(dict->entries[dict->count].alias, p, alen);
            dict->entries[dict->count].alias[alen] = '\0';
        }

        /* Extract canonical (right side, trimmed) */
        {
            const char *r = eq + 1;
            size_t rlen;
            while (*r == ' ' || *r == '\t')
                r++;
            rlen = strlen(r);
            while (rlen > 0 && (r[rlen - 1] == ' ' || r[rlen - 1] == '\t'))
                rlen--;
            if (rlen == 0 || rlen >= DICT_TOKEN_MAX)
                continue;
            memcpy(dict->entries[dict->count].canonical, r, rlen);
            dict->entries[dict->count].canonical[rlen] = '\0';
        }

        dict->count++;
    }

    fclose(f);
    return dict->count;
}

const char *DictTranslate(const DICT *dict, const char *entity)
{
    uint32_t i;

    if (dict == NULL || entity == NULL || entity[0] == '\0')
        return NULL;

    for (i = 0; i < dict->count; i++)
    {
        if (strieq(dict->entries[i].alias, entity))
            return dict->entries[i].canonical;
    }
    return NULL;
}

int DictLooksForeign(const char *entity)
{
    const char *p;
    static const char *accents = "áéíóúñüÁÉÍÓÚÑÜ";
    size_t accent_len;

    if (entity == NULL || entity[0] == '\0')
        return 0;

    /* Check for accented characters */
    accent_len = strlen(accents);
    for (p = entity; *p; p++)
    {
        unsigned char c = (unsigned char)*p;
        if (c >= 0x80)  /* UTF-8 multi-byte = non-ASCII */
            return 1;
        for (size_t i = 0; i < accent_len; i++)
        {
            if (*p == accents[i])
                return 1;
        }
    }

    /* Check for common Spanish suffixes (structural, not vocabulary) */
    {
        size_t L = strlen(entity);
        /* -cion, -cion, -dad, -mente, -oso, -osa, -ivo, -iva, -ado, -ida */
        if (L >= 4)
        {
            const char *end = entity + L - 4;
            if (strcmp(end, "cion") == 0 || strcmp(end, "tion") == 0)
                return 1;
            if (L >= 5)
            {
                const char *end5 = entity + L - 5;
                if (strcmp(end5, "mente") == 0)
                    return 1;
                if (L >= 6)
                {
                    const char *end6 = entity + L - 6;
                    if (strcmp(end6, "amiento") == 0 ||
                        strcmp(end6, "imiento") == 0)
                        return 1;
                }
            }
        }
        if (L >= 3)
        {
            const char *end = entity + L - 3;
            if (strcmp(end, "ado") == 0 || strcmp(end, "ida") == 0 ||
                strcmp(end, "oso") == 0 || strcmp(end, "osa") == 0 ||
                strcmp(end, "ivo") == 0 || strcmp(end, "iva") == 0)
                return 1;
        }
        if (L >= 4)
        {
            const char *end = entity + L - 4;
            if (strcmp(end, "dad") == 0)  /* -idad */
                return 1;
        }
    }

    return 0;
}
