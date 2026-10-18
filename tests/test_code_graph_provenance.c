/* test_code_graph_provenance.c: per-call provenance, file-aware resolution, macro filter,
   labels, path/explain queries, content-hash skip and blast radius confidence. Deterministic. */
#include <stdio.h>
#include <string.h>
#include "code_graph.h"

static int g_run = 0, g_fail = 0;
#define CHECK(expr, msg) do { g_run++; if (!(expr)) { g_fail++; printf("  [FAIL] %s (line %d)\n", msg, __LINE__); } \
                              else printf("  [PASS] %s\n", msg); } while (0)

/* A project: app.c calls Parse (defined in parse.c, declared in parse.h, included) and fopen/strlen
   (defined as stubs in stubs/c_std_lib.h, NOT included by app.c). */
static const char *PARSE_H = "#ifndef PARSE_H\n#define PARSE_H\nint Parse(const char *s);\n#endif\n";
static const char *PARSE_C =
    "#include \"parse.h\"\n"
    "int Parse(const char *s)\n"
    "{\n"
    "    return Helper(s);\n"
    "}\n"
    "int Helper(const char *s)\n"
    "{\n"
    "    return 1;\n"
    "}\n";
static const char *STUBS_H = "int fopen(const char *p);\nint strlen(const char *p);\n"
                             "int strlen(const char *p) { return 0; }\nint fopen(const char *p) { return 0; }\n";
static const char *APP_C =
    "#include \"parse.h\"\n"
    "\n"
    "int Run(const char *s)\n"
    "{\n"
    "    int n = strlen(s);\n"
    "    fopen(s);\n"
    "    return Parse(s) + n;\n"
    "}\n"
    "\n"
    "TEST(RunWorks)\n"
    "{\n"
    "    Run(\"x\");\n"
    "}\n";

static CODE_GRAPH *build(void)
{
    CODE_GRAPH *cg = CodeGraphCreate(1024, 4096);
    CodeGraphIngestSource(cg, "src/parse.h", PARSE_H);
    CodeGraphIngestSource(cg, "src/parse.c", PARSE_C);
    CodeGraphIngestSource(cg, "stubs/c_std_lib.h", STUBS_H);
    CodeGraphIngestSource(cg, "src/app.c", APP_C);
    return cg;
}

static void test_provenance_and_labels(void)
{
    printf("\n=== provenance + labels ===\n");
    CODE_GRAPH *cg = build();
    CODE_CALL_EVIDENCE ev[8];
    uint32_t n = CodeGraphGetCallEvidence(cg, "Run", "Parse", ev, 8);
    CHECK(n == 1, "Run -> Parse has one recorded site");
    CHECK(n == 1 && strcmp(ev[0].file, "src/app.c") == 0, "site file is src/app.c");
    CHECK(n == 1 && ev[0].line == 7, "site line is 7 (the line of the call)");
    CHECK(n == 1 && ev[0].label == CODE_EDGE_INFERRED, "Parse is INFERRED: declared in an included header");
    CHECK(n == 1 && strcmp(ev[0].target_file, "src/parse.c") == 0, "Parse resolves to src/parse.c");

    n = CodeGraphGetCallEvidence(cg, "Parse", "Helper", ev, 8);
    CHECK(n == 1 && ev[0].label == CODE_EDGE_EXTRACTED && ev[0].line == 4, "Parse -> Helper is EXTRACTED at line 4");

    /* The libc collision: strlen/fopen are 'defined' by a stub header app.c never includes. */
    n = CodeGraphGetCallEvidence(cg, "Run", "strlen", ev, 8);
    CHECK(n == 1 && ev[0].label == CODE_EDGE_UNLINKED && ev[0].line == 5, "strlen call is UNLINKED (stub not included)");
    n = CodeGraphGetCallEvidence(cg, "Run", "fopen", ev, 8);
    CHECK(n == 1 && ev[0].label == CODE_EDGE_UNLINKED && ev[0].line == 6, "fopen call is UNLINKED (stub not included)");

    char out[16][MAX_CODE_NAME];
    uint32_t k = CodeGraphGetResolvedCallees(cg, "Run", out, 16);
    int has_parse = 0, has_strlen = 0;
    for (uint32_t i = 0; i < k; i++) {
        if (!strcmp(out[i], "Parse")) has_parse = 1;
        if (!strcmp(out[i], "strlen")) has_strlen = 1;
    }
    CHECK(has_parse && !has_strlen, "resolved callees keep Parse and drop the libc collision");
    k = CodeGraphGetCallees(cg, "Run", out, 16);
    has_strlen = 0;
    for (uint32_t i = 0; i < k; i++) if (!strcmp(out[i], "strlen")) has_strlen = 1;
    CHECK(has_strlen, "legacy callees still list strlen (unchanged behavior)");

    CODE_EDGE_LABEL lab;
    CHECK(CodeGraphGetEdgeLabel(cg, "Run", "Parse", &lab) && lab == CODE_EDGE_INFERRED, "edge label query");
    CHECK(!CodeGraphGetEdgeLabel(cg, "Run", "Nope", &lab), "no site, no label");
    CHECK(strcmp(CodeGraphEdgeLabelName(CODE_EDGE_EXTRACTED), "EXTRACTED") == 0, "label name");
    CodeGraphDestroy(cg);
}

static void test_macro_filter(void)
{
    printf("\n=== macro filter ===\n");
    CODE_GRAPH *cg = build();
    CHECK(!CodeGraphHasSymbol(cg, "TEST") || CodeGraphGetFunctionFile(cg, "TEST") == NULL,
          "TEST(...) is not a function definition");
    CHECK(CodeGraphGetFunctionFile(cg, "Run") != NULL, "Run is still a function");
    char out[8][MAX_CODE_NAME];
    CHECK(CodeGraphGetCallers(cg, "TEST", out, 8) == 0, "TEST has no callers");
    CHECK(cg->macro_names_skipped > 0, "skipped macro names are counted");
    CodeGraphDestroy(cg);
}

static void test_path_and_explain(void)
{
    printf("\n=== path + explain ===\n");
    CODE_GRAPH *cg = build();
    char path[8][MAX_CODE_NAME];
    uint32_t n = CodeGraphFindCallPath(cg, "Run", "Helper", 4, path, 8);
    CHECK(n == 3 && !strcmp(path[0], "Run") && !strcmp(path[1], "Parse") && !strcmp(path[2], "Helper"),
          "path Run -> Parse -> Helper");
    CHECK(CodeGraphFindCallPath(cg, "Helper", "Run", 4, path, 8) == 0, "no reverse path");
    CHECK(CodeGraphFindCallPath(cg, "Run", "strlen", 4, path, 8) == 0, "no path through an UNLINKED edge");
    char buf[512];
    int w = CodeGraphExplainEdge(cg, "Run", "Parse", buf, sizeof buf);
    CHECK(w > 0 && strstr(buf, "INFERRED") && strstr(buf, "src/app.c:7") && strstr(buf, "src/parse.c"),
          "explain shows label, file:line and target");
    CodeGraphDestroy(cg);
}

static void test_line_numbers(void)
{
    printf("\n=== line accuracy ===\n");
    /* Multi-line comment, string with a paren and a prototype before the call: lines must not drift. */
    const char *src =
        "/* a\n"
        "   b */\n"
        "int Foo(int a);\n"
        "int Bar(void)\n"
        "{\n"
        "    const char *s = \"x(\";\n"
        "    return Foo(1);\n"
        "}\n";
    CODE_GRAPH *cg = CodeGraphCreate(256, 512);
    CodeGraphIngestSource(cg, "x.c", src);
    CODE_CALL_EVIDENCE ev[4];
    uint32_t n = CodeGraphGetCallEvidence(cg, "Bar", "Foo", ev, 4);
    CHECK(n == 1 && ev[0].line == 7, "call to Foo reported at line 7");
    CodeGraphDestroy(cg);
}

static void test_extern_c_and_declares(void)
{
    printf("\n=== extern \"C\" ===\n");
    const char *hdr = "#ifdef __cplusplus\nextern \"C\" {\n#endif\nint Ext(int a);\n#ifdef __cplusplus\n}\n#endif\n";
    const char *impl = "#include \"e.h\"\nint Ext(int a)\n{\n    return a;\n}\n";
    const char *use = "#include \"e.h\"\nint Use(void)\n{\n    return Ext(2);\n}\n";
    CODE_GRAPH *cg = CodeGraphCreate(256, 512);
    CodeGraphIngestSource(cg, "e.h", hdr);
    CodeGraphIngestSource(cg, "e.c", impl);
    CodeGraphIngestSource(cg, "u.c", use);
    CODE_EDGE_LABEL lab;
    CHECK(CodeGraphGetEdgeLabel(cg, "Use", "Ext", &lab) && lab == CODE_EDGE_INFERRED,
          "prototype inside extern \"C\" links the call");
    CodeGraphDestroy(cg);
}

static void test_hash_skip(void)
{
    printf("\n=== content hash skip ===\n");
    CODE_GRAPH *cg = build();
    uint32_t sites = cg->site_count, calls = cg->total_calls;
    CodeGraphIngestSource(cg, "src/app.c", APP_C);
    CHECK(cg->files_skipped_unchanged == 1, "identical re-ingest is skipped");
    CHECK(cg->site_count == sites && cg->total_calls == calls, "skipped re-ingest adds nothing");
    CodeGraphIngestSource(cg, "src/app.c", "int Run(const char *s)\n{\n    return Parse(s);\n}\n");
    CODE_CALL_EVIDENCE ev[4];
    uint32_t n = CodeGraphGetCallEvidence(cg, "Run", "Parse", ev, 4);
    CHECK(n == 1 && ev[0].line == 3, "changed file replaces its sites (new line 3)");
    CodeGraphDestroy(cg);
}

static void test_ambiguous_and_blast(void)
{
    printf("\n=== ambiguous + blast radius ===\n");
    CODE_GRAPH *cg = CodeGraphCreate(512, 1024);
    CodeGraphIngestSource(cg, "a.c", "int Dup(void)\n{\n    return 1;\n}\n");
    CodeGraphIngestSource(cg, "b.c", "int Dup(void)\n{\n    return 2;\n}\n");
    CodeGraphIngestSource(cg, "m.c", "int Main(void)\n{\n    return Dup();\n}\n");
    CODE_CALL_EVIDENCE ev[4];
    uint32_t n = CodeGraphGetCallEvidence(cg, "Main", "Dup", ev, 4);
    CHECK(n == 1 && ev[0].label == CODE_EDGE_UNLINKED, "no include reaches either Dup: UNLINKED, not guessed");
    CodeGraphDestroy(cg);

    cg = build();
    CODE_BLAST_RADIUS br;
    CHECK(CodeGraphComputeBlastRadius(cg, "Parse", 3, &br) == 1, "blast radius computed");
    int run_found = 0, run_low = 1;
    for (uint32_t i = 0; i < br.entry_count; i++)
        if (!strcmp(br.entries[i].symbol_name, "Run")) { run_found = 1; run_low = br.entries[i].low_confidence; }
    CHECK(run_found && !run_low, "Run reaches Parse through a resolved edge: not low confidence");
    CHECK(CodeGraphComputeBlastRadius(cg, "strlen", 3, &br) == 1, "blast radius of strlen computed");
    run_found = 0; run_low = 0;
    for (uint32_t i = 0; i < br.entry_count; i++)
        if (!strcmp(br.entries[i].symbol_name, "Run")) { run_found = 1; run_low = br.entries[i].low_confidence; }
    CHECK(run_found && run_low, "Run reaches strlen only by name: kept, flagged low confidence");
    char buf[2048];
    CodeGraphFormatBlastRadius(cg, &br, buf, sizeof buf);
    CHECK(strstr(buf, "low confidence") != NULL, "report marks the low-confidence entry");
    CodeGraphDestroy(cg);
}

int main(void)
{
    test_provenance_and_labels();
    test_macro_filter();
    test_path_and_explain();
    test_line_numbers();
    test_extern_c_and_declares();
    test_hash_skip();
    test_ambiguous_and_blast();
    printf("\n%d checks, %d failed\n", g_run, g_fail);
    return g_fail ? 1 : 0;
}
