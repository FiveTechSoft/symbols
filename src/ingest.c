#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "ingest.h"
#include "embedding.h"

/* ============================================================
   Utilities
   ============================================================ */

static void StrToUpperTrim(const char *src, char *dst, size_t max)
{
    size_t j = 0;

    /* Skip leading whitespace */
    while (*src && isspace((unsigned char)*src))
        src++;

    while (*src && j < max - 1)
    {
        char c = (char)toupper((unsigned char)*src);
        dst[j++] = c;
        src++;
    }

    /* Trim trailing whitespace */
    while (j > 0 && dst[j - 1] == ' ')
        j--;

    dst[j] = '\0';
}

/* ============================================================
   Ingest a single triple
   ============================================================ */

int IngestTriple(GRAPH *graph,
                 const char *subject, const char *predicate, const char *object)
{
    if (graph == NULL || subject == NULL || predicate == NULL || object == NULL)
        return 0;

    /* Skip empty fields */
    if (subject[0] == '\0' || predicate[0] == '\0' || object[0] == '\0')
        return 0;

    char s_upper[128], p_upper[128], o_upper[128];
    StrToUpperTrim(subject, s_upper, sizeof(s_upper));
    StrToUpperTrim(predicate, p_upper, sizeof(p_upper));
    StrToUpperTrim(object, o_upper, sizeof(o_upper));

    if (s_upper[0] == '\0' || p_upper[0] == '\0' || o_upper[0] == '\0')
        return 0;

    SYMBOL_ID s_id = GraphAddSymbol(graph, s_upper);
    SYMBOL_ID p_id = GraphAddSymbol(graph, p_upper);
    SYMBOL_ID o_id = GraphAddSymbol(graph, o_upper);

    if (s_id == SYMBOL_INVALID || p_id == SYMBOL_INVALID || o_id == SYMBOL_INVALID)
        return 0;

    int added = GraphAddRelation(graph, s_id, p_id, o_id);

    /* Update embeddings on every co-occurrence (not just new relations) */
    if (graph->embeddings != NULL)
    {
        EMBEDDING_TABLE *emb = graph->embeddings;

        /* Ensure both embeddings are initialized */
        if (EmbeddingGetVector(emb, s_id) == NULL)
        {
            float v[EMBEDDING_DIM];
            EmbeddingRandomInit(v, (uint32_t)s_id * 2654435761u);
            EmbeddingSetVector(emb, s_id, v);
        }
        if (EmbeddingGetVector(emb, o_id) == NULL)
        {
            float v[EMBEDDING_DIM];
            EmbeddingRandomInit(v, (uint32_t)o_id * 2654435761u);
            EmbeddingSetVector(emb, o_id, v);
        }

        /* Hebbian co-occurrence update */
        float *target  = (float *)EmbeddingGetVector(emb, s_id);
        float *context = (float *)EmbeddingGetVector(emb, o_id);
        if (target && context)
        {
            EmbeddingCooccur(target, context, 0.1f);
            EmbeddingCooccur(context, target, 0.1f);
            EmbeddingNormalize(target);
            EmbeddingNormalize(context);
        }
    }

    return added;
}

/* ============================================================
   Parse one TSV line: "subject\tpredicate\tobject"
   ============================================================ */

static int ParseTSVLine(const char *line, char *subj, char *pred, char *obj,
                        size_t field_max)
{
    /* Skip leading whitespace */
    while (*line && isspace((unsigned char)*line))
        line++;

    /* Skip empty lines and comments */
    if (*line == '\0' || *line == '#')
        return 0;

    /* Find first tab */
    const char *tab1 = strchr(line, '\t');
    if (tab1 == NULL)
        return 0;

    size_t len1 = (size_t)(tab1 - line);
    if (len1 >= field_max) len1 = field_max - 1;
    memcpy(subj, line, len1);
    subj[len1] = '\0';

    line = tab1 + 1;

    /* Find second tab */
    const char *tab2 = strchr(line, '\t');
    if (tab2 == NULL)
        return 0;

    size_t len2 = (size_t)(tab2 - line);
    if (len2 >= field_max) len2 = field_max - 1;
    memcpy(pred, line, len2);
    pred[len2] = '\0';

    line = tab2 + 1;

    /* Rest is object (trim trailing newline/whitespace) */
    const char *end = line + strlen(line);
    while (end > line && (*(end - 1) == '\n' || *(end - 1) == '\r' ||
                          *(end - 1) == ' '))
        end--;

    size_t len3 = (size_t)(end - line);
    if (len3 >= field_max) len3 = field_max - 1;
    memcpy(obj, line, len3);
    obj[len3] = '\0';

    return 1;
}

/* ============================================================
   Ingest from FILE stream
   ============================================================ */

INGEST_STATS IngestTSVStream(GRAPH *graph, FILE *f)
{
    INGEST_STATS stats;
    memset(&stats, 0, sizeof(stats));

    if (graph == NULL || f == NULL)
        return stats;

    char line[1024];
    char subj[128], pred[128], obj[128];

    while (fgets(line, sizeof(line), f) != NULL)
    {
        stats.lines_read++;

        /* Strip trailing newline */
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
        {
            line[len - 1] = '\0';
            len--;
        }

        if (!ParseTSVLine(line, subj, pred, obj, sizeof(subj)))
        {
            stats.lines_failed++;
            continue;
        }

        stats.lines_parsed++;

        /* Check if relation already exists to count updates */
        char s_up[128], p_up[128], o_up[128];
        StrToUpperTrim(subj, s_up, sizeof(s_up));
        StrToUpperTrim(pred, p_up, sizeof(p_up));
        StrToUpperTrim(obj, o_up, sizeof(o_up));

        SYMBOL_ID s_id = SymbolFind(graph->symbols, s_up);
        SYMBOL_ID p_id = SymbolFind(graph->symbols, p_up);
        SYMBOL_ID o_id = SymbolFind(graph->symbols, o_up);

        if (s_id != SYMBOL_INVALID && p_id != SYMBOL_INVALID && o_id != SYMBOL_INVALID)
        {
            RELATION *existing = GraphFindRelation(graph, s_id, p_id, o_id);
            if (existing != NULL)
                stats.relations_updated++;
        }

        if (IngestTriple(graph, subj, pred, obj))
            stats.relations_inserted++;
    }

    return stats;
}

/* ============================================================
   Ingest from file path
   ============================================================ */

INGEST_STATS IngestTSV(GRAPH *graph, const char *filepath)
{
    INGEST_STATS stats;
    memset(&stats, 0, sizeof(stats));

    if (graph == NULL || filepath == NULL)
        return stats;

    FILE *f = fopen(filepath, "r");
    if (f == NULL)
        return stats;

    stats = IngestTSVStream(graph, f);
    fclose(f);
    return stats;
}
