#ifndef EXPORT_H
#define EXPORT_H

#include <stdint.h>
#include "graph.h"

/* Write the graph as a Graphviz DOT file (nodes = symbols used in
   relations, edges labelled with predicate names). */
int GraphExportDot(const GRAPH *graph, const char *filepath);

/* Write the graph as Turtle (.ttl): <S> <p> <O> triples, lowercase
   predicates used as relative IRIs. */
int GraphExportTurtle(const GRAPH *graph, const char *filepath);

#endif
