/* test_clarify: Fase B wrapper (minimal). AMBIGUOUS arms a pending
   clarification; digits and candidate names resolve it through the
   same goal; failures re-ask once then close UNKNOWN (reused G3
   string); pending never blocks a fresh query. The pure engine is
   untouched: resolution reuses the legacy PARENT template verbatim.
   Scratch corpus, ASCII only. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>
#include "schema.h"
#include "metaschema.h"
#include "learn.h"
#include "bible_chat.h"
#include "chat_clarify.h"

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

static const char *g_scratch = "test_clarify_scratch.tsv";
static const char *g_tmp = "test_clarify_out.tmp";
static char g_out[4096];

static void Ask(CLARIFY *w, const char *line)
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
    ClarifyHandle(w, line);
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
    fputs("james\tHIJO_DE\talphaeus\n", f);
    fputs("james\tHIJO_DE\tzebedee\n", f);
    fclose(f);
}

int main(void)
{
    WriteScratch();
    CLARIFY w;
    ClarifyInit(&w, g_scratch);
    remove(g_scratch);

    Ask(&w, "who is the father of james?");
    check("ambiguous arms clarification", g_out,
          "Hay 2 constancias del padre de James: ?Alphaeus (1) o "
          "Zebedee (2)?");
    Ask(&w, "2");
    check("digit resolves", g_out,
          "El padre de James es Zebedee, segun consta en los registros "
          "directos.");
    Ask(&w, "who is the father of james?");
    check("re-arms", g_out,
          "Hay 2 constancias del padre de James: ?Alphaeus (1) o "
          "Zebedee (2)?");
    Ask(&w, "alphaeus");
    check("name resolves", g_out,
          "El padre de James es Alphaeus, segun consta en los "
          "registros directos.");
    Ask(&w, "who is the father of james?");
    Ask(&w, "9");
    check("out-of-range re-asks", g_out,
          "Hay 2 constancias del padre de James: ?Alphaeus (1) o "
          "Zebedee (2)?");
    Ask(&w, "alphaeus zebedee");
    check("multi-match closes UNKNOWN", g_out,
          "No tengo constancia de a quien te refieres en los textos "
          "cargados.");
    Ask(&w, "el primero");
    check("cold ordinal stays parse-fail", g_out,
          "No entendi la pregunta.");
    Ask(&w, "who is the father of david?");
    check("unambiguous control", g_out,
          "El padre de David es Jesse, segun consta en los registros "
          "directos.");
    Ask(&w, "who is the father of james?");
    Ask(&w, "who is the father of david?");
    check("pending never blocks fresh query", g_out,
          "El padre de David es Jesse, segun consta en los registros "
          "directos.");

    printf("test_clarify: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
