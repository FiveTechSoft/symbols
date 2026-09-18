/* test_fixtures: external tool fixtures (data/agentic/fixtures.tsv).
   Parser fail-closed (malformed/unknown rows warned + discarded),
   per-table fallback to frozen compiled rows, replica file proving
   the same effective fact set through ToolExecute, idempotence.
   Scratch files only; removed afterwards. */
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

static const char *g_tmp = "test_fixtures_tmp.tsv";
static const char *g_rep = "test_fixtures_replica.tsv";

static void WriteTmp(void)
{
    FILE *f = fopen(g_tmp, "w");
    if (f == NULL)
    {
        printf("FAIL cannot write temp TSV\n");
        exit(1);
    }
    fputs("# temp fixtures (exercises every parser branch)\n", f);
    fputs("\n", f);
    fputs("rel\ttestsubj\ttestrel\ttestobj\n", f);
    fputs("person\ttestname\ttest detail here\n", f);
    fputs("rel\tbadrow\n", f);
    fputs("bogus\ta\tb\n", f);
    fputs("person\tonlyname\n", f);
    fclose(f);
}

static void WriteReplica(void)
{
    FILE *f = fopen(g_rep, "w");
    if (f == NULL)
    {
        printf("FAIL cannot write replica TSV\n");
        exit(1);
    }
    fputs("rel\tbabylonia\trey\tnebuchadnezzar\n", f);
    fputs("rel\tdavid\tmother\tnitzevet\n", f);
    fputs("rel\tsaul\trey\tisrael\n", f);
    fputs("person\tjonas\tnacio en Gathepher\n", f);
    fputs("person\tjesse\tnacio en Bethlehem\n", f);
    fclose(f);
}

static void RunRelation(const char *subj, const char *rel, char *out,
                        size_t size)
{
    ToolRequest req;
    ToolResult res;
    memset(&req, 0, sizeof(req));
    memset(&res, 0, sizeof(res));
    req.tool = TOOL_LOOKUP_RELATION;
    strncpy(req.subject, subj, sizeof(req.subject) - 1);
    strncpy(req.relation, rel, sizeof(req.relation) - 1);
    ToolExecute(&req, &res);
    if (res.ok && res.nitems > 0)
        strncpy(out, res.items[0], size - 1);
    else
        strncpy(out, "(miss)", size - 1);
    out[size - 1] = '\0';
}

static void RunPerson(const char *name, char *out, size_t size)
{
    ToolRequest req;
    ToolResult res;
    memset(&req, 0, sizeof(req));
    memset(&res, 0, sizeof(res));
    req.tool = TOOL_LOOKUP_PERSON;
    strncpy(req.subject, name, sizeof(req.subject) - 1);
    ToolExecute(&req, &res);
    if (res.ok && res.nitems > 0)
        strncpy(out, res.items[0], size - 1);
    else
        strncpy(out, "(miss)", size - 1);
    out[size - 1] = '\0';
}

int main(void)
{
    char got[128], ref[128];

    /* ---- 1. missing file -> frozen compiled fallback ---- */
    FixtureInitFrom("test_fixtures_no_such_file.tsv");
    check("fallback rel rows", FixtureRelCount() == 3);
    check("fallback rel content",
          FixtureRelAt(0) != NULL &&
              strcmp(FixtureRelAt(0)->subject, "babylonia") == 0 &&
              strcmp(FixtureRelAt(2)->object, "israel") == 0);
    check("fallback person rows", FixturePersonCount() == 2);
    check("fallback person content",
          FixturePersonAt(1) != NULL &&
              strcmp(FixturePersonAt(1)->detail,
                     "nacio en Bethlehem") == 0);
    RunRelation("saul", "rey", ref, sizeof(ref));
    RunPerson("jonas", got, sizeof(got));
    check("fallback executes", strcmp(ref, "israel") == 0 &&
                                   strcmp(got, "nacio en Gathepher") ==
                                       0);

    /* ---- 2. temp file: valid rows replace, bad rows die ---- */
    WriteTmp();
    FixtureInitFrom(g_tmp);
    check("file rel replaces", FixtureRelCount() == 1);
    check("file rel content",
          FixtureRelAt(0) != NULL &&
              strcmp(FixtureRelAt(0)->object, "testobj") == 0);
    check("file compiled gone",
          FixtureRelCount() == 1 &&
              strcmp(FixtureRelAt(0)->subject, "testsubj") == 0);
    check("file person replaces", FixturePersonCount() == 1);
    check("file person content",
          FixturePersonAt(0) != NULL &&
              strcmp(FixturePersonAt(0)->detail, "test detail here") ==
                  0);
    RunRelation("saul", "rey", got, sizeof(got));
    check("replaced set misses old fact", strcmp(got, "(miss)") == 0);
    RunRelation("testsubj", "testrel", got, sizeof(got));
    check("replaced set serves new fact",
          strcmp(got, "testobj") == 0);
    remove(g_tmp);

    /* ---- 3. replica of compiled rows -> same effective facts ---- */
    WriteReplica();
    FixtureInitFrom(g_rep);
    check("replica rel rows", FixtureRelCount() == 3);
    check("replica person rows", FixturePersonCount() == 2);
    RunRelation("saul", "rey", got, sizeof(got));
    check("replica same rel fact",
          strcmp(got, ref) == 0 && strcmp(got, "israel") == 0);
    RunPerson("jonas", got, sizeof(got));
    check("replica same person fact",
          strcmp(got, "nacio en Gathepher") == 0);
    FixtureInitFrom(g_rep);
    check("idempotent reload",
          FixtureRelCount() == 3 && FixturePersonCount() == 2);
    remove(g_rep);

    /* ---- 4. shipped file matches compiled charter ---- */
    FixtureInitFrom("data/agentic/fixtures.tsv");
    check("shipped rel rows", FixtureRelCount() == 3);
    check("shipped person rows", FixturePersonCount() == 2);
    RunRelation("david", "mother", got, sizeof(got));
    check("shipped serves fact", strcmp(got, "nitzevet") == 0);

    printf("test_fixtures: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
