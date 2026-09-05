#include <stdio.h>
#include <ctype.h>
#include "export.h"

static void PrintTurtleName(char *dst, size_t max, const char *name)
{
    size_t o = 0;
    for (size_t i = 0; name[i] && o + 3 < max; i++)
    {
        char c = (char)tolower((unsigned char)name[i]);
        if (c == '_' || isalnum((unsigned char)c))
            dst[o++] = c;
        else if (o + 2 < max)
        {
            dst[o++] = '\\';
            dst[o++] = 'u';
        }
    }
    dst[o] = '\0';
}

int GraphExportDot(const GRAPH *graph, const char *filepath)
{
    if (!graph || !filepath || !graph->symbols || !graph->relations)
        return 0;

    FILE *f = fopen(filepath, "w");
    if (!f)
        return 0;

    fprintf(f, "digraph symbols {\n  node [shape=box];\n");

    uint32_t n = RelationCount(graph->relations);
    for (uint32_t i = 0; i < n; i++)
    {
        const RELATION *r = RelationGet(graph->relations, i);
        if (!r)
            continue;
        const SYMBOL *s = SymbolGet(graph->symbols, r->subject);
        const SYMBOL *p = SymbolGet(graph->symbols, r->predicate);
        const SYMBOL *o = SymbolGet(graph->symbols, r->object);
        if (!s || !p || !o)
            continue;
        fprintf(f, "  \"%s\" -> \"%s\" [label=\"%s\"];\n",
                s->name, o->name, p->name);
    }

    fprintf(f, "}\n");
    fclose(f);
    return 1;
}

int GraphExportTurtle(const GRAPH *graph, const char *filepath)
{
    if (!graph || !filepath || !graph->symbols || !graph->relations)
        return 0;

    FILE *f = fopen(filepath, "w");
    if (!f)
        return 0;

    uint32_t n = RelationCount(graph->relations);
    for (uint32_t i = 0; i < n; i++)
    {
        const RELATION *r = RelationGet(graph->relations, i);
        if (!r)
            continue;
        const SYMBOL *s = SymbolGet(graph->symbols, r->subject);
        const SYMBOL *p = SymbolGet(graph->symbols, r->predicate);
        const SYMBOL *o = SymbolGet(graph->symbols, r->object);
        if (!s || !p || !o)
            continue;

        char sn[256], pn[256], on[256];
        PrintTurtleName(sn, sizeof(sn), s->name);
        PrintTurtleName(pn, sizeof(pn), p->name);
        PrintTurtleName(on, sizeof(on), o->name);
        fprintf(f, "<%s> <%s> <%s> .\n", sn, pn, on);
    }

    fclose(f);
    return 1;
}
