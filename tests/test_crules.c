/* test_crules: C language rules (data/c_lang/languagec.tsv).
   Load 19 rows, spot-lookup every TYPE (escape decoding verified),
   miss returns NULL, malformed temp rows warned + discarded,
   init idempotent. Scratch file only; removed afterwards. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>
#include "c_rules.h"
#include "schema.h"
#include "metaschema.h"
#include "learn.h"
#include "bible_chat.h"

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

static const char *g_tmp = "test_crules_tmp.tsv";
static const char *g_cap = "test_crules_cap.tmp";
static const char *g_scratch = "test_crules_scratch.tsv";
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

static void WriteTmp(void)
{
    FILE *f = fopen(g_tmp, "w");
    if (f == NULL)
    {
        printf("FAIL cannot write temp TSV\n");
        exit(1);
    }
    fputs("# temp c_rules (exercises every parser branch)\n", f);
    fputs("\n", f);
    fputs("TYPE\tKEY\tPARAM1\tPARAM2\n", f);
    fputs("header\tmytype\t<my.h>\ttype\n", f);
    fputs("template\tesc\tA{\\nB\\\\C}D\tframe\n", f);
    fputs("only-two-cols\n", f);
    fputs("bogus\tkey\ta\tb\n", f);
    fputs("repair\tshort\tp1\n", f);
    fclose(f);
}

int main(void)
{
    CRulesTable tbl;
    const CRule *r;

    /* ---- 1. shipped file: 19 rows ---- */
    check("load shipped count", CRulesLoad("data/c_lang/languagec.tsv",
                                           &tbl) == 19);
    check("count field", tbl.count == 19);

    /* ---- 2. one lookup per TYPE ---- */
    r = CRulesFind(&tbl, C_RULE_HEADER, "printf");
    check("header printf", r != NULL && strcmp(r->p1, "<stdio.h>") == 0 &&
                               strcmp(r->p2, "function") == 0);
    r = CRulesFind(&tbl, C_RULE_TYPE_SIG, "safe_copy");
    check("type_sig safe_copy",
          r != NULL && strcmp(r->p1, "size_t") == 0 &&
              strcmp(r->p2, "char *dst, const char *src, size_t sz") ==
                  0);
    r = CRulesFind(&tbl, C_RULE_INVARIANT, "bound_chk");
    check("invariant bound_chk",
          r != NULL && strcmp(r->p1, "sz > 0") == 0 &&
              strcmp(r->p2, "capacity_guard") == 0);
    r = CRulesFind(&tbl, C_RULE_TEMPLATE, "for_bounded");
    check("template for_bounded newlines",
          r != NULL && strchr(r->p1, '\n') != NULL &&
              strcmp(r->p2, "bounded_loop") == 0);
    r = CRulesFind(&tbl, C_RULE_REPAIR, "error: 'NULL' undeclared");
    check("repair null",
          r != NULL && strcmp(r->p1, "require_header") == 0 &&
              strcmp(r->p2, "<stddef.h>") == 0);

    /* ---- 3. miss + null guards ---- */
    check("miss unknown key",
          CRulesFind(&tbl, C_RULE_HEADER, "nosuch") == NULL);
    check("miss wrong type",
          CRulesFind(&tbl, C_RULE_TEMPLATE, "printf") == NULL);
    check("null guards",
          CRulesFind(NULL, C_RULE_HEADER, "printf") == NULL &&
              CRulesFind(&tbl, C_RULE_HEADER, NULL) == NULL &&
              CRulesLoad("data/c_lang/c_rules.tsv", NULL) == 0 &&
              CRulesLoad("no_such_file.tsv", &tbl) == 0 &&
              tbl.count == 0);

    /* ---- 4. malformed temp file: 2 valid, rest discarded ---- */
    WriteTmp();
    check("temp valid count",
          CRulesLoad(g_tmp, &tbl) == 2 && tbl.count == 2);
    r = CRulesFind(&tbl, C_RULE_TEMPLATE, "esc");
    check("escape decoding",
          r != NULL && strcmp(r->p1, "A{\nB\\C}D") == 0);
    check("temp unknown gone",
          CRulesFind(&tbl, C_RULE_HEADER, "bogus") == NULL);
    remove(g_tmp);

    /* ---- 5. global init idempotent ---- */
    check("init first", CRulesInit() == 19);
    check("init second", CRulesInit() == 19);
    check("global table live",
          CRulesTableGet() != NULL && CRulesTableGet()->count == 19 &&
              CRulesFind(CRulesTableGet(), C_RULE_HEADER, "memcpy") !=
                  NULL);

    /* ---- 6. ASK-load wire: carga languagec.tsv ---- */
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
        Ask(&ch, "carga languagec.tsv");
        check_str("carga canonical",
                  g_out, "Cargadas 19 reglas de languagec.tsv.");
        Ask(&ch, "Carga LanguageC.TSV");
        check_str("carga case-insensitive",
                  g_out, "Cargadas 19 reglas de languagec.tsv.");
        Ask(&ch, "carga noexiste.tsv");
        check_str("carga missing abstains",
                  g_out, "No entendi la pregunta.");
        Ask(&ch, "carga ../../x.tsv");
        check_str("carga traversal abstains",
                  g_out, "No entendi la pregunta.");
        Ask(&ch, "carga languagec");
        check_str("carga bare noun abstains",
                  g_out, "No entendi la pregunta.");
        Ask(&ch, "quien es el padre de david?");
        check_str("engine intact after loads",
                  g_out, "El padre de David es Jesse, segun consta "
                         "en los registros directos.");
    }

    printf("test_crules: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
