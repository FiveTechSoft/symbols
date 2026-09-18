/* c_rules: C language knowledge as relations (data/c_lang/c_rules.tsv).
   English code comments (project rule). One linear pass at init;
   unknown TYPEs and malformed rows warn to stderr and are discarded
   (fail-closed). Escape sequences in fields decode on load: \n \r
   \t \\ (templates need real newlines); unknown escapes pass
   through literally. Zero malloc: fixed table. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "c_rules.h"

#define CRULES_LINE_MAX 1024
#define CRULES_PATH "data/c_lang/c_rules.tsv"

static CRulesTable g_crules;
static size_t g_crules_init_done = 0;

static int CRuleTypeFromName(const char *s, CRuleType *out)
{
    if (strcmp(s, "header") == 0)
        *out = C_RULE_HEADER;
    else if (strcmp(s, "type_sig") == 0)
        *out = C_RULE_TYPE_SIG;
    else if (strcmp(s, "invariant") == 0)
        *out = C_RULE_INVARIANT;
    else if (strcmp(s, "template") == 0)
        *out = C_RULE_TEMPLATE;
    else if (strcmp(s, "repair") == 0)
        *out = C_RULE_REPAIR;
    else
        return 0;
    return 1;
}

/* decode escapes in place (output never longer than input) */
static void DecodeEscapes(char *s)
{
    size_t r = 0, w = 0;
    while (s[r] != '\0')
    {
        if (s[r] == '\\' && s[r + 1] != '\0')
        {
            char e = s[r + 1];
            if (e == 'n')
            {
                s[w++] = '\n';
                r += 2;
                continue;
            }
            if (e == 'r')
            {
                s[w++] = '\r';
                r += 2;
                continue;
            }
            if (e == 't')
            {
                s[w++] = '\t';
                r += 2;
                continue;
            }
            if (e == '\\')
            {
                s[w++] = '\\';
                r += 2;
                continue;
            }
        }
        s[w++] = s[r++];
    }
    s[w] = '\0';
}

static uint32_t SplitTabs(char *line, char *f[], uint32_t max)
{
    uint32_t n = 0;
    char *p = line;
    while (n < max)
    {
        char *t;
        f[n++] = p;
        t = strchr(p, '\t');
        if (t == NULL)
            break;
        *t = '\0';
        p = t + 1;
    }
    if (strchr(p, '\t') != NULL)
        return 99;
    return n;
}

int CRulesLoad(const char *filepath, CRulesTable *tbl)
{
    FILE *f;
    char line[CRULES_LINE_MAX];
    unsigned long lineno = 0;
    if (tbl == NULL)
        return 0;
    tbl->count = 0;
    if (filepath == NULL)
        return 0;
    f = fopen(filepath, "r");
    if (f == NULL)
        return 0;
    while (fgets(line, sizeof(line), f) != NULL)
    {
        char *fld[8];
        uint32_t nf;
        char *s = line;
        CRuleType type;
        lineno++;
        if (strchr(line, '\n') == NULL && !feof(f))
        {
            int c;
            fprintf(stderr, "c_rules.tsv:%lu: line too long\n", lineno);
            while ((c = fgetc(f)) != EOF && c != '\n')
                ;
            continue;
        }
        while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
            s++;
        if (*s == '\0' || *s == '#')
            continue;
        {
            size_t L = strlen(s);
            while (L > 0 && (s[L - 1] == '\r' || s[L - 1] == '\n'))
                s[--L] = '\0';
        }
        nf = SplitTabs(s, fld, 8);
        if (nf == 0 || fld[0][0] == '\0')
            continue;
        if (nf >= 2 && strcmp(fld[0], "TYPE") == 0 &&
            strcmp(fld[1], "KEY") == 0)
            continue;
        if (nf != 4 || fld[1][0] == '\0')
        {
            fprintf(stderr, "c_rules.tsv:%lu: bad row\n", lineno);
            continue;
        }
        if (!CRuleTypeFromName(fld[0], &type))
        {
            fprintf(stderr, "c_rules.tsv:%lu: unknown TYPE\n", lineno);
            continue;
        }
        if (strlen(fld[1]) >= MAX_NAME_LEN ||
            strlen(fld[2]) >= MAX_EXPR_LEN ||
            strlen(fld[3]) >= MAX_EXPR_LEN)
        {
            fprintf(stderr, "c_rules.tsv:%lu: row too long\n", lineno);
            continue;
        }
        if (tbl->count >= MAX_C_RULES)
        {
            fprintf(stderr, "c_rules.tsv:%lu: table full\n", lineno);
            continue;
        }
        {
            CRule *r = &tbl->rules[tbl->count];
            r->type = type;
            strncpy(r->key, fld[1], MAX_NAME_LEN - 1);
            r->key[MAX_NAME_LEN - 1] = '\0';
            strncpy(r->p1, fld[2], MAX_EXPR_LEN - 1);
            r->p1[MAX_EXPR_LEN - 1] = '\0';
            strncpy(r->p2, fld[3], MAX_EXPR_LEN - 1);
            r->p2[MAX_EXPR_LEN - 1] = '\0';
            DecodeEscapes(r->p1);
            DecodeEscapes(r->p2);
            DecodeEscapes(r->key);
            tbl->count++;
        }
    }
    fclose(f);
    return (int)tbl->count;
}

const CRule *CRulesFind(const CRulesTable *tbl, CRuleType type,
                        const char *key)
{
    size_t i;
    if (tbl == NULL || key == NULL)
        return NULL;
    for (i = 0; i < tbl->count; i++)
        if (tbl->rules[i].type == type &&
            strcmp(tbl->rules[i].key, key) == 0)
            return &tbl->rules[i];
    return NULL;
}

int CRulesInit(void)
{
    if (!g_crules_init_done)
    {
        CRulesLoad(CRULES_PATH, &g_crules);
        g_crules_init_done = 1;
    }
    return (int)g_crules.count;
}

const CRulesTable *CRulesTableGet(void)
{
    return &g_crules;
}
