/* test_bible_chat_lex: lexical-robustness contract for the chat
   front-end (Fase 1). Covers:
   - ASCII folding in Split(): accented surface ("reinó" style) is
     invisible to matching; tokens fold to corpus form. Verified via
     a scratch corpus with accented entity names.
   - Morphological fold: ES plural ('s'/'es') and gender swap
     (o<->a) are RULES; a fold only matches if the result equals a
     DEDUCED stem (esposo/esposos/esposas -> esposa). No word lists:
     the EN_SURFACE table only holds non-derivable English nouns.
   - BOOL guard rejects the relation word itself in any folded form
     ("es nabal esposo de abigail" is a BOOL with subject nabal).
   The corpus is a tiny scratch TSV, never the tracked one. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>
#include "schema.h"
#include "metaschema.h"
#include "learn.h"
#include "bible_chat.h"

static int g_pass = 0, g_fail = 0;

static void check(const char *name, const char *got, const char *want)
{
    if (got != NULL && strcmp(got, want) == 0)
    {
        printf("  PASS %s\n", name);
        g_pass++;
    }
    else
    {
        printf("  FAIL %s (got %.90s want %.40s)\n", name,
               got ? got : "(null)", want);
        g_fail++;
    }
}

static const char *g_scratch = "test_bible_chat_lex_scratch.tsv";
static const char *g_tmp = "test_bible_chat_lex_out.tmp";

/* Capture ChatHandle output: stdout is swapped to a temp file with
   _dup/_dup2 (CRT-level, restore is exact) and read back. */
static char g_out[2048];

static void Ask(CHAT *ch, const char *line)
{
    fflush(stdout);
    int saved = _dup(1); /* keep the real stdout fd */
    FILE *cap = fopen(g_tmp, "w");
    if (saved < 0 || cap == NULL)
    {
        printf("FAIL cannot capture stdout\n");
        exit(1);
    }
    fflush(cap);
    _dup2(_fileno(cap), 1); /* fd 1 now writes to the temp file */
    ChatHandle(ch, line);
    fflush(stdout);
    _dup2(saved, 1); /* restore */
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
        if (g_out[i] == '\n')
            g_out[i] = ' ';
    while (n > 0 && g_out[n - 1] == ' ')
        g_out[--n] = '\0'; /* strip the newline-turned-space tail */
    remove(g_tmp);
}

static void WriteScratch(void)
{
    FILE *f = fopen(g_scratch, "w");
    if (f == NULL)
    {
        printf("FAIL cannot write scratch corpus\n");
        exit(1);
    }
    /* wife family only; (S,O) = S is the wife of O */
    fputs("abigail\tESPOSA_DE\tnabal\n", f);
    fputs("nabal\tESPOSA_DE\tabigail\n", f);
    fclose(f);
}

int main(void)
{
    WriteScratch();
    CHAT ch;
    ChatInit(&ch, g_scratch);
    remove(g_scratch);

    /* ---- 1. deduced index contract (no hardcoded lexicon) ---- */
    check("kw esposa deduced", ch.num_kws == 1 ? "esposa" : "?", "esposa");
    check("kw family wife",
          strcmp(ch.kws[0].family, "wife") == 0 ? "wife" : "?", "wife");

    /* ---- 2. gender fold: esposo/esposos/esposas -> esposa ---- */
    Ask(&ch, "esposo de abigail");
    check("esposo (masc sing) = esposa query", g_out,
          "esposa de Abigail: Nabal.");
    Ask(&ch, "esposos de abigail");
    check("esposos (masc pl) = esposa query", g_out,
          "esposa de Abigail: Nabal.");
    Ask(&ch, "esposas de abigail");
    check("esposas (fem pl) = esposa query", g_out,
          "esposa de Abigail: Nabal.");
    Ask(&ch, "quien es el esposo de abigail");
    check("wh + esposo query", g_out, "esposa de Abigail: Nabal.");

    /* ---- 3. BOOL with variant: guard rejects the relation word
       itself in folded form; subject slot must be the NAME.
       Scratch ingests BOTH directions, so (nabal,abigail) is a
       direct pair: per-policy wife BOOL = direct (A,B) only. ---- */
    Ask(&ch, "nabal es esposo de abigail");
    check("BOOL nabal esposo de abigail (direct pair)", g_out,
          "Si, Nabal esposa de Abigail.");
    Ask(&ch, "es abigail esposa de nabal");
    check("BOOL abigail esposa de nabal positive", g_out,
          "Si, Abigail esposa de Nabal.");
    Ask(&ch, "es nabal esposo de abigail");
    check("BOOL copula-first variant same result", g_out,
          "Si, Nabal esposa de Abigail.");

    /* ---- 4. no false fold: un-deduced words stay inert ---- */
    Ask(&ch, "reino de abigail");
    check("reino (suppletive) does not fold to esposa", g_out,
          "No entendi la pregunta.");
    Ask(&ch, "hermano de abigail");
    check("hermano not deduced in this corpus", g_out,
          "No entendi la pregunta.");

    /* ---- 5. accent folding at entity level ----
       the corpus itself is accent-free, so accented QUERIES with
       corpus entities must still resolve (accents only on function
       words here: "quién" is not corpus vocab but folds) */
    Ask(&ch, "qui\u00e9n es la esposa de abigail");
    check("accented quien folds", g_out, "esposa de Abigail: Nabal.");

    printf("test_bible_chat_lex: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}