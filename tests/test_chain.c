/* test_chain: Tool Chaining over ExecutionContext (no new words).
   Goal A's result feeds Goal B as argument via positional anaphora
   (subject-after-copula outside vocab/kw/stop/digits): numbers flow
   when the goal mentions digits, else the most recent entity.
   Provenance per goal lands in ch.exec (never in the KB).
   Row 1: calculator -> calculator (391 flows). Row 2: KB -> tool
   (Jesse flows into lookup_person). Row 3: miss path stays echo.
   Real corpus. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>
#include "schema.h"
#include "metaschema.h"
#include "learn.h"
#include "bible_chat.h"
#include "tool_contract.h"

static int g_pass = 0, g_fail = 0;

static const char *g_tmp = "test_chain_out.tmp";
static char g_out[8192];

static void Ask(CHAT *ch, const char *line)
{
    fflush(stdout);
    int saved = _dup(1);
    FILE *cap = fopen(g_tmp, "w");
    if (saved < 0 || cap == NULL)
    {
        printf("FAIL cannot capture stdout\n");
        exit(1);
    }
    fflush(cap);
    _dup2(_fileno(cap), 1);
    ChatHandle(ch, line);
    fflush(stdout);
    _dup2(saved, 1);
    _close(saved);
    fclose(cap);
    FILE *in = fopen(g_tmp, "r");
    size_t n = 0;
    if (in != NULL)
    {
        int c;
        while ((c = fgetc(in)) != EOF && n < sizeof(g_out) - 1)
            g_out[n++] = (char)c;
        fclose(in);
    }
    g_out[n] = '\0';
    for (size_t i = 0; i < n; i++)
        if (g_out[i] == '\n' || g_out[i] == '\r')
            g_out[i] = ' ';
    while (n > 0 && g_out[n - 1] == ' ')
        g_out[--n] = '\0';
    remove(g_tmp);
}

static void check(const char *name, const char *got, const char *want)
{
    if (got != NULL && strcmp(got, want) == 0)
    {
        printf("  PASS %s\n", name);
        g_pass++;
    }
    else
    {
        printf("  FAIL %s\n    got  %.200s\n    want %.200s\n", name,
               got ? got : "(null)", want);
        g_fail++;
    }
}

static int hasprov(CHAT *ch, const char *sub)
{
    for (uint32_t i = 0; i < ch->exec.nprov; i++)
        if (strstr(ch->exec.prov[i], sub) != NULL)
            return 1;
    return 0;
}

static int hasent(CHAT *ch, const char *sub)
{
    for (uint32_t i = 0; i < ch->exec.nent; i++)
        if (strcmp(ch->exec.entities[i], sub) == 0)
            return 1;
    return 0;
}

int main(void)
{
    CHAT ch;
    ChatInit(&ch, "data/bible/bible_relations.tsv");

    Ask(&ch, "Cu\xc3\xa1nto es 23 * 17 y cu\xc3\xa1nto es eso + 9?");
    check("calc chain 391 -> 400", g_out,
          "Segun calculo, 23 * 17 = 391. Segun calculo, 391 + 9 = "
          "400.");
    check("chain number kept", ch.exec.has_number ? ch.exec.number : "?",
          "400");
    check("chain calc provenance", hasprov(&ch, "TOOL calculator") ? "y" : "n", "y");

    Ask(&ch, "Who is the father of David and where was he born?");
    check("KB -> tool chain", g_out,
          "El padre de David es Jesse, segun consta en los registros "
          "directos. Segun fuente externa, Jesse: nacio en "
          "Bethlehem.");
    check("chain entity fed", hasent(&ch, "jesse") ? "y" : "n", "y");
    check("chain tool provenance",
          hasprov(&ch, "TOOL lookup_person(jesse)") ? "y" : "n", "y");

    Ask(&ch, "Who is the father of Babylonia and where was he born?");
    check("miss path stays echo", g_out,
          "No tengo constancia del padre de Babylonia en los textos "
          "cargados. No tengo constancia suficiente para responder a "
          "\"where was he born\".");

    printf("test_chain: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
