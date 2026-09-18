/* test_toolconfig: external agentic tables (data/agentic/tools.tsv).
   Parser fail-closed (malformed/unknown rows warned + discarded),
   per-table fallback to frozen compiled rows, ToolInit idempotent.
   Scratch file only; removed afterwards. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tool_contract.h"

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

static const char *g_tmp = "test_toolconfig_tmp.tsv";

static void WriteTmp(void)
{
    FILE *f = fopen(g_tmp, "w");
    if (f == NULL)
    {
        printf("FAIL cannot write temp TSV\n");
        exit(1);
    }
    fputs("# temp config (exercises every parser branch)\n", f);
    fputs("\n", f);
    fputs("TYPE\tKEY\tA\tB\n", f);
    fputs("allow\tshell\tzz1\tcmd\n", f);
    fputs("allow\tshell\tzz2\tbash\tzz-default\n", f);
    fputs("contract\ttestfam\tlookup_person\topen\n", f);
    fputs("tool\ttesttool\tin\tout\n", f);
    fputs("only-two-cols\n", f);
    fputs("bogus\tkey\ta\tb\n", f);
    fputs("contract\tbadfam\tnosuchtool\topen\n", f);
    fputs("contract\tbadpol\tlookup_person\tmaybe\n", f);
    fputs("allow\tnot-shell\tx\ty\n", f);
    fputs("allow\tshell\tx\ttoolongbackendname12345\n", f);
    fclose(f);
}

int main(void)
{
    char be[16], df[64];

    /* ---- 1. missing file -> frozen compiled fallback ---- */
    ToolInitFrom("test_toolconfig_no_such_file.tsv");
    check("fallback shell rows", ShellAllowCount() == 7);
    check("fallback shell first",
          ShellAllowAt(0) != NULL &&
              strcmp(ShellAllowAt(0)->trigger, "echo") == 0);
    check("fallback contract rows", ToolContractCount() == 9);
    check("fallback contract first",
          ToolContractRowAt(0) != NULL &&
              strcmp(ToolContractRowAt(0)->family, "reigns") == 0 &&
              ToolContractRowAt(0)->needs_known == 0);
    check("fallback info empty", ToolInfoCount() == 0);
    check("fallback lookup works",
          ShellLookup("gcc", be, sizeof(be), df, sizeof(df)) == 1 &&
              strcmp(be, "direct") == 0 &&
              strcmp(df, "--version") == 0);

    /* ---- 2. temp file: valid rows replace, bad rows die ---- */
    WriteTmp();
    ToolInitFrom(g_tmp);
    check("file shell rows replace", ShellAllowCount() == 2);
    check("file shell content",
          ShellAllowAt(0) != NULL &&
              strcmp(ShellAllowAt(0)->trigger, "zz1") == 0 &&
              ShellAllowAt(1) != NULL &&
              strcmp(ShellAllowAt(1)->dflt, "zz-default") == 0);
    check("file shell compiled gone",
          ShellLookup("echo", NULL, 0, NULL, 0) == 0);
    check("file contract replaces", ToolContractCount() == 1);
    check("file contract content",
          ToolContractRowAt(0) != NULL &&
              strcmp(ToolContractRowAt(0)->family, "testfam") == 0 &&
              ToolContractRowAt(0)->tool == TOOL_LOOKUP_PERSON &&
              ToolContractRowAt(0)->needs_known == 0);
    check("file tool info row", ToolInfoCount() == 1);
    check("file tool info content",
          ToolInfoRowAt(0) != NULL &&
              strcmp(ToolInfoRowAt(0)->name, "testtool") == 0);
    check("out of range guards",
          ShellAllowAt(99) == NULL && ToolContractRowAt(99) == NULL &&
              ToolInfoRowAt(99) == NULL);
    remove(g_tmp);

    /* ---- 3. ToolInit default path + idempotence ---- */
    ToolInit();
    ToolInit();
    check("shipped file shell rows", ShellAllowCount() == 7);
    check("shipped file contract rows", ToolContractCount() == 9);
    check("shipped gcc row intact",
          ShellLookup("gcc", be, sizeof(be), df, sizeof(df)) == 1 &&
              strcmp(df, "--version") == 0);

    printf("test_toolconfig: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
