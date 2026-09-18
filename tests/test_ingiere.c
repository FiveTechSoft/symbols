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
#include "chat.h"

static int g_pass = 0, g_fail = 0;

static void check(const char *name, int cond)
{
    if (cond)
    {
        printf("  PASS %s\n", name);
        g_pass++;
    }
    else
    {
        printf("  FAIL %s\n", name);
        g_fail++;
    }
}

static void check_str(const char *name, const char *got,                      const char *want)
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

/* prefix pin for long literal answers (heads are ASCII-stable) */
static void check_prefix(const char *name, const char *got,
                         const char *want)
{
    size_t L = strlen(want);
    if (got != NULL && strncmp(got, want, L) == 0)
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
    /* ---- dynamic text loads: census never hardcoded. Receipt
       numbers must agree with the estado inventory; replays add
       zero; shared symbols add up. ---- */
    {
        unsigned N1 = 0, M1 = 0, N2 = 0, T = 0, N3 = 0, M3 = 0;
        unsigned N3b = 0, T2 = 0;
        char nm[64];
        char exp[256];
        Ask(&ch, "estado");
        check_str("estado empty session",
                  g_out, "No tengo ningun texto cargado.");
        Ask(&ch, "load jung.txt");
        check("load jung receipt parses",
              sscanf(g_out,
                     "Incorporadas %u frases y %u simbolos de jung.txt.",
                     &N1, &M1) == 2 && N1 > 0 && M1 > 0);
        Ask(&ch, "load jung.txt");
        check_str("load jung replay adds nothing",
                  g_out,
                  "Incorporadas 0 frases y 0 simbolos de jung.txt.");
        Ask(&ch, "estado");
        check("estado agrees with jung receipt",
              sscanf(g_out,
                     "Tengo cargado: %63[^ ] (%u frases). "
                     "Simbolos en sesion: %u.",
                     nm, &N2, &T) == 3 &&
                  strcmp(nm, "jung.txt") == 0 && N2 == N1 && T == M1);
    Ask(&ch, "what does the sun mean");
    check_prefix("ask jung sun",
                 g_out, "Segun el texto: This does not mean that men "
                        "loved the visible God ; they love hi");
    Ask(&ch, "what is the mother");
    check_prefix("ask jung mother",
                 g_out, "Segun el texto: And the boy looks up familiarly "
                        "To his Father , Helios , ");
    Ask(&ch, "who is the hero");
    check_prefix("ask jung hero long answer",
                 g_out, "Segun el texto: — THE UNCONSCIOUS ORIGIN");
    Ask(&ch, "what is libido");
    check_prefix("ask jung libido",
                 g_out, "Segun el texto: Briefly , we may designate "
                        "this amount of libido as ");
    Ask(&ch, "explicamelo");
    check_prefix("follow-up advances past top",
                 g_out, "Segun el texto: What this signifies we already "
                        "know");
    Ask(&ch, "explicamelo");
    check_prefix("follow-up advances again",
                 g_out, "Segun el texto: This confirmation is parallel "
                        "to the postulate ");
    Ask(&ch, "xyzqqq");
    check_str("bare unknown abstains despite cache",
              g_out, "No entendi la pregunta.");
        Ask(&ch, "load noexiste.txt");
        check_str("load txt missing abstains",
                  g_out, "No entendi la pregunta.");
        Ask(&ch, "load ../../x.txt");
        check_str("load txt traversal abstains",
                  g_out, "No entendi la pregunta.");
        Ask(&ch, "load bible.txt");
        check("load bible receipt parses",
              sscanf(g_out,
                     "Incorporadas %u frases y %u simbolos de bible.txt.",
                     &N3, &M3) == 2 && N3 > 0 && M3 > 0);
        Ask(&ch, "estado");
        snprintf(exp, sizeof(exp), "jung.txt (%u frases)", N1);
        check("estado keeps jung entry",
              strstr(g_out, exp) != NULL);
        snprintf(exp, sizeof(exp), "bible.txt (%u frases)", N3);
        check("estado lists bible entry",
              strstr(g_out, exp) != NULL);
        check("estado shared symbols add up",
              sscanf(g_out,
                     "Tengo cargado: %63[^ ] (%u frases); "
                     "bible.txt (%u frases). Simbolos en sesion: %u.",
                     nm, &N2, &N3b, &T2) == 4 && N2 == N1 &&
                  N3b == N3 && T2 == M1 + M3);
    }
    Ask(&ch, "quien es el padre de david?");
    check_str("engine intact after ingests",
              g_out, "El padre de David es Jesse, segun consta "
                     "en los registros directos.");

    printf("test_ingiere: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
