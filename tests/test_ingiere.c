/* test_ingiere: hot knowledge loads (load <file.tsv>).
   Transport + measurement only: resolve sandbox (data/samples/ open),
   scanned-vs-admitted counts, missing/traversal abstain, session
   intact afterwards. jung pins the lexicon gate: 40 scanned, 0
   admitted (SIGNIFICA/ES unmapped). Scratch corpus only. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>
#include "bible_chat.h"

static int g_pass = 0, g_fail = 0;

static void check_str(const char *name, const char *got,
                      const char *want)
{
    if (got != NULL && strcmp(got, want) == 0)
    {
        printf("  PASS %s\n", name);
        g_pass++;
    }
    else
    {
        printf("  FAIL %s\n    got  %.120s\n    want %.120s\n", name,
               got ? got : "(null)", want);
        g_fail++;
    }
}

static const char *g_cap = "test_ingiere_cap.tmp";
static const char *g_scratch = "test_ingiere_scratch.tsv";
static char g_out[4096];

static void Ask(CHAT *ch, const char *line)
{
    fflush(stdout);
    int saved = _dup(1);
    FILE *cap = fopen(g_cap, "w");
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
    FILE *in = fopen(g_cap, "r");
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
    remove(g_cap);
}

int main(void)
{
    CHAT ch;
    FILE *sf = fopen(g_scratch, "w");
    if (sf == NULL)
    {
        printf("FAIL cannot write scratch corpus\n");
        exit(1);
    }
    fputs("david\tHIJO_DE\tjesse\n", sf);
    fclose(sf);
    ChatInit(&ch, g_scratch);
    remove(g_scratch);

    Ask(&ch, "load jung.tsv");
    check_str("load jung counts",
              g_out, "Incorporadas 0 de 40 afirmaciones de jung.tsv.");
    Ask(&ch, "Load JUNG.TSV");
    check_str("load case-insensitive",
              g_out, "Incorporadas 0 de 40 afirmaciones de jung.tsv.");
    Ask(&ch, "load languagec.tsv");
    check_str("load rules file admits none",
              g_out,
              "Incorporadas 0 de 20 afirmaciones de languagec.tsv.");
    Ask(&ch, "load noexiste.tsv");
    check_str("load missing abstains",
              g_out, "No entendi la pregunta.");
    Ask(&ch, "load ../../x.tsv");
    check_str("load traversal abstains",
              g_out, "No entendi la pregunta.");
    Ask(&ch, "quien es el padre de david?");
    check_str("engine intact after ingests",
              g_out, "El padre de David es Jesse, segun consta "
                     "en los registros directos.");

    printf("test_ingiere: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
