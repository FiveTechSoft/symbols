/* One-TU, read-only lexical baseline for the optional impact comparator. */
#include <stdio.h>
#include <string.h>
#include "code_graph.h"

static void json_string(const char *s)
{
    putchar('"');
    for (; *s; ++s) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') { putchar('\\'); putchar(c); }
        else if (c < 32) printf("\\u%04x", c);
        else putchar(c);
    }
    putchar('"');
}

int main(int argc, char **argv)
{
    if (argc != 3 || !argv[1][0] || !argv[2][0]) {
        fprintf(stderr, "usage: lexical_impact SOURCE.c SYMBOL\n");
        return 2;
    }
    CODE_GRAPH *cg = CodeGraphCreate(1024, 2048);
    if (!cg || !CodeGraphIngestFile(cg, argv[1])) {
        fprintf(stderr, "lexical ingestion unavailable\n");
        CodeGraphDestroy(cg);
        return 2;
    }
    CODE_BLAST_RADIUS radius;
    int found = CodeGraphComputeBlastRadius(cg, argv[2], 1, &radius);
    printf("{\"symbol\":"); json_string(argv[2]);
    printf(",\"source\":"); json_string(argv[1]);
    printf(",\"direct_callers\":[");
    int first = 1;
    for (unsigned i = 0; found && i < radius.entry_count; ++i) {
        BLAST_RADIUS_ENTRY *e = &radius.entries[i];
        if (e->depth != 1 || e->kind != CODE_SYM_FUNCTION) continue;
        if (!first) putchar(',');
        json_string(e->symbol_name);
        first = 0;
    }
    puts("],\"limitations\":[\"lexical_names_not_bindings\",\"one_tu\",\"calls_only\"]}");
    CodeGraphDestroy(cg);
    return 0;
}
