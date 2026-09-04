#ifndef INGEST_H
#define INGEST_H

#include <stdint.h>
#include "graph.h"

typedef struct
{
    uint64_t lines_read;
    uint64_t lines_parsed;
    uint64_t lines_failed;
    uint64_t relations_inserted;
    uint64_t relations_updated;
} INGEST_STATS;

/*
 * Ingest a TSV file line-by-line (subject\tpredicate\tobject).
 * Each line is parsed and inserted into the graph O(1) via hash table.
 * Skips empty lines, comments (#), and malformed lines.
 * Returns stats about the ingestion.
 */
INGEST_STATS IngestTSV(GRAPH *graph, const char *filepath);

/*
 * Ingest from an already-open FILE handle (streaming).
 * Useful for piped input or large files processed in chunks.
 */
INGEST_STATS IngestTSVStream(GRAPH *graph, FILE *f);

/*
 * Ingest raw text: one triple per line, tab-separated.
 * Trims whitespace, converts to uppercase, skips empty/comments.
 */
int IngestTriple(GRAPH *graph,
                 const char *subject, const char *predicate, const char *object);

#endif
