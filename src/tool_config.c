/* tool_config: external agentic tables (data/agentic/tools.tsv).
   English code comments (project rule). One linear pass at init;
   file rows win per table when >= 1 valid row exists, else the
   frozen compiled fallback below (identical literals). Unknown
   TYPEs and malformed rows warn to stderr and are discarded:
   fail-closed, never abort, never invent. Zero malloc. */
#include <stdio.h>
#include <string.h>
#include "tool_contract.h"
#include "c_rules.h"

#define TOOLCFG_LINE_MAX 1024
#define TOOLCFG_PATH "data/agentic/tools.tsv"

static const ShellAllowRow COMPILED_SHELL[] = {
    {"echo", "cmd", ""},
    {"dir", "cmd", ""},
    {"ls", "bash", ""},
    {"pwd", "bash", ""},
    {"powershell", "powershell", "Get-Location"},
    {"bash", "bash", "pwd"},
    {"gcc", "direct", "--version"},
};

static const ToolContractRow COMPILED_CONTRACT[] = {
    {"reigns", TOOL_LOOKUP_RELATION, 0},
    {"taxonomy", TOOL_LOOKUP_RELATION, 1},
    {"father", TOOL_LOOKUP_RELATION, 1},
    {"sibling", TOOL_LOOKUP_RELATION, 1},
    {"wife", TOOL_LOOKUP_RELATION, 1},
    {"parent", TOOL_LOOKUP_RELATION, 1},
    {"children", TOOL_LOOKUP_RELATION, 1},
    {"grandparent", TOOL_LOOKUP_RELATION, 1},
    {"descendant", TOOL_LOOKUP_RELATION, 1},
};

static ShellAllowRow g_shell[TOOLCFG_SHELL_MAX];
static uint32_t g_nshell = 0;
static ToolContractRow g_contract[TOOLCFG_CONTRACT_MAX];
static uint32_t g_ncontract = 0;
static ToolInfoRow g_info[TOOLCFG_INFO_MAX];
static uint32_t g_ninfo = 0;
static int g_tool_init_done = 0;

static int ToolIdFromName(const char *s, ToolId *out)
{
    if (strcmp(s, "lookup_person") == 0)
        *out = TOOL_LOOKUP_PERSON;
    else if (strcmp(s, "lookup_relation") == 0)
        *out = TOOL_LOOKUP_RELATION;
    else if (strcmp(s, "calculator") == 0)
        *out = TOOL_CALCULATOR;
    else if (strcmp(s, "shell") == 0)
        *out = TOOL_SHELL;
    else if (strcmp(s, "fs_read") == 0)
        *out = TOOL_FS_READ;
    else
        return 0;
    return 1;
}

static void LoadCompiled(void)
{
    size_t i;
    g_nshell = 0;
    for (i = 0;
         i < sizeof(COMPILED_SHELL) / sizeof(COMPILED_SHELL[0]) &&
         g_nshell < TOOLCFG_SHELL_MAX;
         i++)
        g_shell[g_nshell++] = COMPILED_SHELL[i];
    g_ncontract = 0;
    for (i = 0;
         i < sizeof(COMPILED_CONTRACT) / sizeof(COMPILED_CONTRACT[0]) &&
         g_ncontract < TOOLCFG_CONTRACT_MAX;
         i++)
        g_contract[g_ncontract++] = COMPILED_CONTRACT[i];
    g_ninfo = 0;
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

static int Fits(const char *s, size_t cap)
{
    return strlen(s) < cap;
}

void ToolInitFrom(const char *path)
{
    FILE *f;
    char line[TOOLCFG_LINE_MAX];
    unsigned long lineno = 0;
    ShellAllowRow sh[TOOLCFG_SHELL_MAX];
    ToolContractRow co[TOOLCFG_CONTRACT_MAX];
    ToolInfoRow in[TOOLCFG_INFO_MAX];
    uint32_t nsh = 0, nco = 0, nin = 0;
    LoadCompiled();
    if (path == NULL)
        return;
    f = fopen(path, "r");
    if (f == NULL)
        return;
    while (fgets(line, sizeof(line), f) != NULL)
    {
        char *fld[8];
        uint32_t nf;
        char *s = line;
        lineno++;
        if (strchr(line, '\n') == NULL && !feof(f))
        {
            int c;
            fprintf(stderr, "tools.tsv:%lu: line too long, discarded\n",
                    lineno);
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
        if (strcmp(fld[0], "tool") == 0)
        {
            if (nf != 4 || fld[1][0] == '\0')
            {
                fprintf(stderr, "tools.tsv:%lu: bad tool row\n",
                        lineno);
                continue;
            }
            if (!Fits(fld[1], sizeof(in[0].name)) ||
                !Fits(fld[2], sizeof(in[0].inputs)) ||
                !Fits(fld[3], sizeof(in[0].outputs)))
            {
                fprintf(stderr, "tools.tsv:%lu: tool row too long\n",
                        lineno);
                continue;
            }
            if (nin >= TOOLCFG_INFO_MAX)
            {
                fprintf(stderr, "tools.tsv:%lu: info table full\n",
                        lineno);
                continue;
            }
            strncpy(in[nin].name, fld[1], sizeof(in[0].name) - 1);
            strncpy(in[nin].inputs, fld[2], sizeof(in[0].inputs) - 1);
            strncpy(in[nin].outputs, fld[3],
                    sizeof(in[0].outputs) - 1);
            nin++;
        }
        else if (strcmp(fld[0], "allow") == 0)
        {
            const char *dflt = "";
            if ((nf != 4 && nf != 5) || fld[1][0] == '\0')
            {
                fprintf(stderr, "tools.tsv:%lu: bad allow row\n",
                        lineno);
                continue;
            }
            if (strcmp(fld[1], "shell") != 0)
            {
                fprintf(stderr, "tools.tsv:%lu: unknown allow key\n",
                        lineno);
                continue;
            }
            if (nf == 5)
                dflt = fld[4];
            if (!Fits(fld[2], sizeof(sh[0].trigger)) ||
                !Fits(fld[3], sizeof(sh[0].backend)) ||
                !Fits(dflt, sizeof(sh[0].dflt)))
            {
                fprintf(stderr, "tools.tsv:%lu: allow row too long\n",
                        lineno);
                continue;
            }
            if (nsh >= TOOLCFG_SHELL_MAX)
            {
                fprintf(stderr, "tools.tsv:%lu: shell table full\n",
                        lineno);
                continue;
            }
            strncpy(sh[nsh].trigger, fld[2],
                    sizeof(sh[0].trigger) - 1);
            strncpy(sh[nsh].backend, fld[3],
                    sizeof(sh[0].backend) - 1);
            strncpy(sh[nsh].dflt, dflt, sizeof(sh[0].dflt) - 1);
            nsh++;
        }
        else if (strcmp(fld[0], "contract") == 0)
        {
            ToolId tool = TOOL_NONE;
            int needs_known = -1;
            if (nf != 4 || fld[1][0] == '\0')
            {
                fprintf(stderr,
                        "tools.tsv:%lu: bad contract row\n", lineno);
                continue;
            }
            if (!ToolIdFromName(fld[2], &tool))
            {
                fprintf(stderr,
                        "tools.tsv:%lu: unknown contract tool\n",
                        lineno);
                continue;
            }
            if (strcmp(fld[3], "open") == 0)
                needs_known = 0;
            else if (strcmp(fld[3], "known-only") == 0)
                needs_known = 1;
            else
            {
                fprintf(stderr,
                        "tools.tsv:%lu: bad contract policy\n",
                        lineno);
                continue;
            }
            if (!Fits(fld[1], sizeof(co[0].family)))
            {
                fprintf(stderr,
                        "tools.tsv:%lu: contract row too long\n",
                        lineno);
                continue;
            }
            if (nco >= TOOLCFG_CONTRACT_MAX)
            {
                fprintf(stderr,
                        "tools.tsv:%lu: contract table full\n", lineno);
                continue;
            }
            strncpy(co[nco].family, fld[1], sizeof(co[0].family) - 1);
            co[nco].tool = tool;
            co[nco].needs_known = needs_known;
            nco++;
        }
        else
        {
            fprintf(stderr, "tools.tsv:%lu: unknown TYPE\n", lineno);
            continue;
        }
    }
    fclose(f);
    if (nsh > 0)
    {
        memcpy(g_shell, sh, nsh * sizeof(sh[0]));
        g_nshell = nsh;
    }
    if (nco > 0)
    {
        memcpy(g_contract, co, nco * sizeof(co[0]));
        g_ncontract = nco;
    }
    if (nin > 0)
    {
        memcpy(g_info, in, nin * sizeof(in[0]));
        g_ninfo = nin;
    }
}

void ToolInit(void)
{
    if (!g_tool_init_done)
    {
        ToolInitFrom(TOOLCFG_PATH);
        CRulesInit();
        g_tool_init_done = 1;
    }
}

uint32_t ShellAllowCount(void)
{
    return g_nshell;
}

const ShellAllowRow *ShellAllowAt(uint32_t i)
{
    if (i >= g_nshell)
        return NULL;
    return &g_shell[i];
}

uint32_t ToolContractCount(void)
{
    return g_ncontract;
}

const ToolContractRow *ToolContractRowAt(uint32_t i)
{
    if (i >= g_ncontract)
        return NULL;
    return &g_contract[i];
}

uint32_t ToolInfoCount(void)
{
    return g_ninfo;
}

const ToolInfoRow *ToolInfoRowAt(uint32_t i)
{
    if (i >= g_ninfo)
        return NULL;
    return &g_info[i];
}
