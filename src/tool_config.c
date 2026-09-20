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
#define FIXTURE_PATH "data/agentic/fixtures.tsv"
#define SELF_PATH "data/agentic/self.tsv"

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

/* frozen fixture fallbacks (identical literals, moved verbatim from
   tool_executor.c; external stand-ins, never KB). */
static const FixtureRelRow COMPILED_REL[] = {
    {"babylonia", "rey", "nebuchadnezzar"},
    {"david", "mother", "nitzevet"},
    {"saul", "rey", "israel"},
};

static const FixturePersonRow COMPILED_PERSON[] = {
    {"jonas", "nacio en Gathepher"},
    {"jesse", "nacio en Bethlehem"},
};

static const char COMPILED_SELF_SCOPE[] =
    "Soy Symbols, un copiloto y motor de inteligencia artificial simbólica local. Puedo ayudarte a explorar el repositorio, generar y modificar código en C, Python y JavaScript, analizar funciones y dependencias, y responder consultas sobre el conocimiento indexado.";

static const char COMPILED_SELF_GREET[] =
    "¡Hola! Soy Symbols, tu copiloto local de IA y desarrollo. ¿En qué puedo ayudarte hoy con tu proyecto o código?";

typedef struct {
    const char *trigger;
    const char *reply;
} SelfTriggerPair;

static const SelfTriggerPair COMPILED_SELF_TRIG[] = {
    {"quien eres", "scope"},
    {"quien eres tu", "scope"},
    {"who are you", "scope"},
    {"what are you", "scope"},
    {"que eres", "scope"},
    {"que sabes hacer", "scope"},
    {"what can you do", "scope"},
    {"que sabes", "scope"},
    {"que puedo preguntarte", "scope"},
    {"hola", "greeting"},
    {"hello", "greeting"},
    {"hi", "greeting"},
    {"hey", "greeting"},
    {"buenos dias", "greeting"},
    {"buenas tardes", "greeting"},
    {"buenas noches", "greeting"},
    {"buenas", "greeting"},
    {"que tal", "greeting"},
    {"como estas", "greeting"},
    {"how are you", "greeting"},
};

static ShellAllowRow g_shell[TOOLCFG_SHELL_MAX];
static uint32_t g_nshell = 0;
static ToolContractRow g_contract[TOOLCFG_CONTRACT_MAX];
static uint32_t g_ncontract = 0;
static ToolInfoRow g_info[TOOLCFG_INFO_MAX];
static uint32_t g_ninfo = 0;
static FixtureRelRow g_rel[TOOLCFG_REL_MAX];
static uint32_t g_nrel = 0;
static FixturePersonRow g_person[TOOLCFG_PERSON_MAX];
static uint32_t g_nperson = 0;
static char g_self_scope[SELF_SCOPE_MAX];
static char g_self_greet[SELF_GREET_MAX];
static char g_self_trig[TOOLCFG_SELF_TRIG_MAX][64];
static char g_self_reply[TOOLCFG_SELF_TRIG_MAX][16];
static uint32_t g_ntrig = 0;
static int g_tool_init_done = 0;

static void LoadCompiledSelf(void)
{
    strncpy(g_self_scope, COMPILED_SELF_SCOPE, sizeof(g_self_scope) - 1);
    g_self_scope[sizeof(g_self_scope) - 1] = '\0';
    strncpy(g_self_greet, COMPILED_SELF_GREET, sizeof(g_self_greet) - 1);
    g_self_greet[sizeof(g_self_greet) - 1] = '\0';
    g_ntrig = 0;
    for (size_t i = 0;
         i < sizeof(COMPILED_SELF_TRIG) / sizeof(COMPILED_SELF_TRIG[0]) &&
         g_ntrig < TOOLCFG_SELF_TRIG_MAX;
         i++)
    {
        strncpy(g_self_trig[g_ntrig], COMPILED_SELF_TRIG[i].trigger, sizeof(g_self_trig[0]) - 1);
        g_self_trig[g_ntrig][sizeof(g_self_trig[0]) - 1] = '\0';
        strncpy(g_self_reply[g_ntrig], COMPILED_SELF_TRIG[i].reply, sizeof(g_self_reply[0]) - 1);
        g_self_reply[g_ntrig][sizeof(g_self_reply[0]) - 1] = '\0';
        g_ntrig++;
    }
}

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

static void LoadCompiledFixtures(void)
{
    size_t i;
    g_nrel = 0;
    for (i = 0;
         i < sizeof(COMPILED_REL) / sizeof(COMPILED_REL[0]) &&
         g_nrel < TOOLCFG_REL_MAX;
         i++)
        g_rel[g_nrel++] = COMPILED_REL[i];
    g_nperson = 0;
    for (i = 0;
         i < sizeof(COMPILED_PERSON) / sizeof(COMPILED_PERSON[0]) &&
         g_nperson < TOOLCFG_PERSON_MAX;
         i++)
        g_person[g_nperson++] = COMPILED_PERSON[i];
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
    LoadCompiledFixtures();
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

void FixtureInitFrom(const char *path)
{
    FILE *f;
    char line[TOOLCFG_LINE_MAX];
    unsigned long lineno = 0;
    FixtureRelRow rl[TOOLCFG_REL_MAX];
    FixturePersonRow pl[TOOLCFG_PERSON_MAX];
    uint32_t nrl = 0, npl = 0;
    LoadCompiledFixtures();
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
            fprintf(stderr, "fixtures.tsv:%lu: line too long\n",
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
        if (strcmp(fld[0], "rel") == 0)
        {
            if (nf != 4 || fld[1][0] == '\0')
            {
                fprintf(stderr, "fixtures.tsv:%lu: bad rel row\n",
                        lineno);
                continue;
            }
            if (!Fits(fld[1], sizeof(rl[0].subject)) ||
                !Fits(fld[2], sizeof(rl[0].rel)) ||
                !Fits(fld[3], sizeof(rl[0].object)))
            {
                fprintf(stderr,
                        "fixtures.tsv:%lu: rel row too long\n", lineno);
                continue;
            }
            if (nrl >= TOOLCFG_REL_MAX)
            {
                fprintf(stderr,
                        "fixtures.tsv:%lu: rel table full\n", lineno);
                continue;
            }
            strncpy(rl[nrl].subject, fld[1],
                    sizeof(rl[0].subject) - 1);
            strncpy(rl[nrl].rel, fld[2], sizeof(rl[0].rel) - 1);
            strncpy(rl[nrl].object, fld[3],
                    sizeof(rl[0].object) - 1);
            nrl++;
        }
        else if (strcmp(fld[0], "person") == 0)
        {
            if (nf != 3 || fld[1][0] == '\0')
            {
                fprintf(stderr,
                        "fixtures.tsv:%lu: bad person row\n", lineno);
                continue;
            }
            if (!Fits(fld[1], sizeof(pl[0].name)) ||
                !Fits(fld[2], sizeof(pl[0].detail)))
            {
                fprintf(stderr,
                        "fixtures.tsv:%lu: person row too long\n",
                        lineno);
                continue;
            }
            if (npl >= TOOLCFG_PERSON_MAX)
            {
                fprintf(stderr,
                        "fixtures.tsv:%lu: person table full\n",
                        lineno);
                continue;
            }
            strncpy(pl[npl].name, fld[1], sizeof(pl[0].name) - 1);
            strncpy(pl[npl].detail, fld[2],
                    sizeof(pl[0].detail) - 1);
            npl++;
        }
        else
        {
            fprintf(stderr, "fixtures.tsv:%lu: unknown TYPE\n",
                    lineno);
            continue;
        }
    }
    fclose(f);
    if (nrl > 0)
    {
        memcpy(g_rel, rl, nrl * sizeof(rl[0]));
        g_nrel = nrl;
    }
    if (npl > 0)
    {
        memcpy(g_person, pl, npl * sizeof(pl[0]));
        g_nperson = npl;
    }
}

void ToolInit(void)
{
    if (!g_tool_init_done)
    {
        ToolInitFrom(TOOLCFG_PATH);
        FixtureInitFrom(FIXTURE_PATH);
        LoadCompiledSelf();
        SelfInitFrom(SELF_PATH);
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

uint32_t FixtureRelCount(void)
{
    return g_nrel;
}

const FixtureRelRow *FixtureRelAt(uint32_t i)
{
    if (i >= g_nrel)
        return NULL;
    return &g_rel[i];
}

uint32_t FixturePersonCount(void)
{
    return g_nperson;
}

const FixturePersonRow *FixturePersonAt(uint32_t i)
{
    if (i >= g_nperson)
        return NULL;
    return &g_person[i];
}

/* normalize a trigger line the same way queries normalize
   (fold + lowercase per token, single spaces): matching is then an
   exact string compare, no word lists anywhere. */
static void NormLine(const char *in, char *out, size_t size)
{
    size_t pos = 0;
    const char *p = in;
    out[0] = '\0';
    if (size == 0 || in == NULL)
        return;
    while (*p != '\0')
    {
        char tok[CHAT_TOKEN_MAX];
        char norm[CHAT_TOKEN_MAX];
        size_t L = 0;
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '\0')
            break;
        while (p[L] != '\0' && p[L] != ' ' && p[L] != '\t' &&
               L + 1 < sizeof(tok))
            L++;
        if (L >= sizeof(tok) - 1)
            break;
        memcpy(tok, p, L);
        tok[L] = '\0';
        p += L;
        while (*p != '\0' && *p != ' ' && *p != '\t')
            p++;
        ChatNormTok(tok, norm, sizeof(norm));
        if (norm[0] == '\0')
            continue;
        L = strlen(norm);
        if (pos > 0 && pos + 1 < size)
            out[pos++] = ' ';
        if (pos + L >= size)
            break;
        memcpy(out + pos, norm, L);
        pos += L;
    }
    out[pos < size ? pos : size - 1] = '\0';
}

void SelfInitFrom(const char *path)
{
    FILE *f;
    char line[TOOLCFG_LINE_MAX];
    unsigned long lineno = 0;
    char scope[SELF_SCOPE_MAX];
    char greet[SELF_GREET_MAX];
    char trig[TOOLCFG_SELF_TRIG_MAX][64];
    char treply[TOOLCFG_SELF_TRIG_MAX][16];
    uint32_t ntrig = 0;
    scope[0] = '\0';
    greet[0] = '\0';
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
            fprintf(stderr, "self.tsv:%lu: line too long\n", lineno);
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
        if (strcmp(fld[0], "scope") == 0 ||
            strcmp(fld[0], "greeting") == 0)
        {
            int is_greet = strcmp(fld[0], "greeting") == 0;
            char *dst = is_greet ? greet : scope;
            size_t cap = is_greet ? sizeof(greet) : sizeof(scope);
            if (nf != 3 || fld[1][0] == '\0' || fld[2][0] == '\0')
            {
                fprintf(stderr, "self.tsv:%lu: bad reply row\n",
                        lineno);
                continue;
            }
            if (dst[0] != '\0')
                continue;
            strncpy(dst, fld[2], cap - 1);
            dst[cap - 1] = '\0';
        }
        else if (strcmp(fld[0], "trigger") == 0)
        {
            char norm[64];
            if (nf != 3 || fld[1][0] == '\0' ||
                (strcmp(fld[2], "scope") != 0 &&
                 strcmp(fld[2], "greeting") != 0))
            {
                fprintf(stderr, "self.tsv:%lu: bad trigger row\n",
                        lineno);
                continue;
            }
            NormLine(fld[1], norm, sizeof(norm));
            if (norm[0] == '\0')
            {
                fprintf(stderr, "self.tsv:%lu: empty trigger\n",
                        lineno);
                continue;
            }
            if (ntrig >= TOOLCFG_SELF_TRIG_MAX)
            {
                fprintf(stderr, "self.tsv:%lu: trigger table full\n",
                        lineno);
                continue;
            }
            strncpy(trig[ntrig], norm, sizeof(trig[0]) - 1);
            trig[ntrig][sizeof(trig[0]) - 1] = '\0';
            strncpy(treply[ntrig], fld[2], sizeof(treply[0]) - 1);
            treply[ntrig][sizeof(treply[0]) - 1] = '\0';
            ntrig++;
        }
        else
        {
            fprintf(stderr, "self.tsv:%lu: unknown TYPE\n", lineno);
            continue;
        }
    }
    fclose(f);
    if (scope[0] != '\0')
    {
        strncpy(g_self_scope, scope, sizeof(g_self_scope) - 1);
        g_self_scope[sizeof(g_self_scope) - 1] = '\0';
    }
    if (greet[0] != '\0')
    {
        strncpy(g_self_greet, greet, sizeof(g_self_greet) - 1);
        g_self_greet[sizeof(g_self_greet) - 1] = '\0';
    }
    if (ntrig > 0)
    {
        memcpy(g_self_trig, trig, ntrig * sizeof(trig[0]));
        memcpy(g_self_reply, treply, ntrig * sizeof(treply[0]));
        g_ntrig = ntrig;
    }
}

const char *SelfScopeText(void)
{
    return g_self_scope;
}

const char *SelfGreetText(void)
{
    return g_self_greet;
}

uint32_t SelfTriggerCount(void)
{
    return g_ntrig;
}

const char *SelfTriggerAt(uint32_t i)
{
    if (i >= g_ntrig)
        return NULL;
    return g_self_trig[i];
}

const char *SelfTriggerReplyAt(uint32_t i)
{
    if (i >= g_ntrig)
        return NULL;
    return g_self_reply[i];
}
