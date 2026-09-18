/* test_composite: Fase A Pasos 2+3 (dispatcher + CompositeResponse).
   Multi-goal lines evaluate every QueryGoal in order over the shared
   dialogue state and figure each one: ANSWER / constancia / span-echo
   UNKNOWN / AMBIGUOUS. Anti-silent-loss invariant: no goal ever
   disappears. Single-goal lines keep byte-identical legacy outputs
   (G1/G2/G5 pins). Scratch corpus, ASCII only. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>
#include "schema.h"
#include "metaschema.h"
#include "learn.h"
#include "chat.h"

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
        printf("  FAIL %s\n    got  %.150s\n    want %.150s\n", name,
               got ? got : "(null)", want);
        g_fail++;
    }
}

static const char *g_scratch = "test_composite_scratch.tsv";
static const char *g_tmp = "test_composite_out.tmp";
static char g_out[4096];

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

static void WriteScratch(void)
{
    FILE *f = fopen(g_scratch, "w");
    if (f == NULL)
    {
        printf("FAIL cannot write scratch corpus\n");
        exit(1);
    }
    fputs("david\tHIJO_DE\tjesse\n", f);
    fputs("jesse\tPADRE_DE\tdavid\n", f);
    fputs("solomon\tHIJO_DE\tdavid\n", f);
    fputs("david\tREY_DE\tisrael\n", f);
    fputs("james\tHIJO_DE\talphaeus\n", f);
    fputs("james\tHIJO_DE\tzebedee\n", f);
    fputs("rehoboam\tHIJO_DE\tsolomon\n", f);
    fputs("abigail\tESPOSA_DE\tnabal\n", f);
    fputs("adah\tESPOSA_DE\tesau\n", f);
    fclose(f);
}

int main(void)
{
    WriteScratch();
    CHAT ch;
    ChatInit(&ch, g_scratch);
    remove(g_scratch);

    /* single-goal pins (legacy path, byte-identical) */
    Ask(&ch, "who is the father of david?");
    check("single PARENT answer", g_out,
          "El padre de David es Jesse, segun consta en los registros "
          "directos.");
    Ask(&ch, "the father of david");
    check("single G1 veto intact", g_out, "No entendi la pregunta.");
    Ask(&ch, "the king of");
    check("single G2 veto intact", g_out, "No entendi la pregunta.");

    /* arg coordination: both figured */
    Ask(&ch, "who is the father of david and solomon?");
    check("arg-coord two answers", g_out,
          "El padre de David es Jesse, segun consta en los registros "
          "directos. El padre de Solomon es David, segun consta en los "
          "registros directos.");

    /* known + unknown-vocab: ANSWER + G4, never silent loss */
    Ask(&ch, "who is the father of david and babylonia?");
    check("known + G4", g_out,
          "El padre de David es Jesse, segun consta en los registros "
          "directos. No tengo constancia del padre de Babylonia en los "
          "textos cargados.");

    /* known + ambiguous: ANSWER + AMBIGUOUS */
    Ask(&ch, "who is the father of david and james?");
    check("known + ambiguous", g_out,
          "El padre de David es Jesse, segun consta en los registros "
          "directos. Hay 2 constancias del padre de James: ambiguo, "
          "necesito desambiguar.");

    /* bare fragment coordination: whole-plan ABSTAIN (Fase 4 G1) */
    Ask(&ch, "el padre de david y de salomon");
    check("bare coord abstains", g_out, "No entendi la pregunta.");

    /* goal coordination with unsafe anaphora: ANSWER + span-echo */
    Ask(&ch, "who was david's father and who was he king of?");
    check("anaphora goal figured as UNKNOWN", g_out,
          "El padre de David es Jesse, segun consta en los registros "
          "directos. No tengo constancia suficiente para responder a "
          "\"who was he king of\".");

    /* two full goals, second elliptical over wife frame */
    Ask(&ch, "who is the wife of nabal and esau?");
    check("wife arg-coord", g_out,
          "esposa de Nabal: Abigail. esposa de Esau: Adah.");

    printf("test_composite: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
