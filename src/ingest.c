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
    return IngestTripleSource(graph, subject, predicate, object, NULL);
}

int IngestTripleSource(GRAPH *graph,
                       const char *subject, const char *predicate,
                       const char *object, const char *source)
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

    /* Was this relation already in the graph? (GraphAddRelation returns 1
       for both new inserts and re-strengthening of existing ones) */
    RELATION *existing = GraphFindRelation(graph, s_id, p_id, o_id);

    int added = GraphAddRelation(graph, s_id, p_id, o_id);
    if (!added)
        return 0;

    /* Procedencia: solo en insercion nueva (no pisa la original).
       El simbolo fuente vive en la misma tabla (visible en /find). */
    if (existing == NULL && source != NULL && source[0] != '\0')
    {
        char src_upper[128];
        StrToUpperTrim(source, src_upper, sizeof(src_upper));
        if (src_upper[0] != '\0')
        {
            SYMBOL_ID src_id = GraphAddSymbol(graph, src_upper);
            if (src_id != SYMBOL_INVALID)
            {
                RELATION *r = GraphFindRelation(graph, s_id, p_id, o_id);
                if (r != NULL)
                    RelationSetSource(r, src_id);
            }
        }
    }

    /* Update embeddings on every co-occurrence (not just new relations) */
    if (graph->embeddings != NULL)
    {
        EMBEDDING_TABLE *emb = graph->embeddings;

        /* Ensure all three embeddings are initialized */
        if (EmbeddingGetVector(emb, s_id) == NULL)
        {
            float v[EMBEDDING_DIM];
            EmbeddingRandomInit(v, (uint32_t)s_id * 2654435761u);
            EmbeddingSetVector(emb, s_id, v);
        }
        if (EmbeddingGetVector(emb, p_id) == NULL)
        {
            float v[EMBEDDING_DIM];
            EmbeddingRandomInit(v, (uint32_t)p_id * 2654435761u);
            EmbeddingSetVector(emb, p_id, v);
        }
        if (EmbeddingGetVector(emb, o_id) == NULL)
        {
            float v[EMBEDDING_DIM];
            EmbeddingRandomInit(v, (uint32_t)o_id * 2654435761u);
            EmbeddingSetVector(emb, o_id, v);
        }

        /* Hebbian co-occurrence: all pairs */
        float *s_vec = (float *)EmbeddingGetVector(emb, s_id);
        float *p_vec = (float *)EmbeddingGetVector(emb, p_id);
        float *o_vec = (float *)EmbeddingGetVector(emb, o_id);
        if (s_vec && p_vec)
        {
            EmbeddingCooccur(s_vec, p_vec, 0.1f);
            EmbeddingCooccur(p_vec, s_vec, 0.1f);
        }
        if (p_vec && o_vec)
        {
            EmbeddingCooccur(p_vec, o_vec, 0.1f);
            EmbeddingCooccur(o_vec, p_vec, 0.1f);
        }
        if (s_vec && o_vec)
        {
            EmbeddingCooccur(s_vec, o_vec, 0.1f);
            EmbeddingCooccur(o_vec, s_vec, 0.1f);
        }
        if (s_vec) EmbeddingNormalize(s_vec);
        if (p_vec) EmbeddingNormalize(p_vec);
        if (o_vec) EmbeddingNormalize(o_vec);
    }

    return existing != NULL ? 2 : 1;
}

/* ============================================================
   Parse one TSV line: "subject\tpredicate\tobject"
   ============================================================ */

/* Parse one TSV line: "subject\tpredicate\tobject[\tsource]".
   La 4a columna (opcional) es la procedencia explicita (p.ej. "GEN 1:1");
   sin ella el llamador aporta "fichero:linea". Devuelve 1 y deja src
   vacio ("") si no hay 4a columna. */
static int ParseTSVLineSrc(const char *line, char *subj, char *pred, char *obj,
                           char *src, size_t field_max)
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

    /* Third tab (optional): splits object from explicit source */
    const char *tab3 = strchr(line, '\t');
    const char *obj_end = (tab3 != NULL) ? tab3 : line + strlen(line);
    while (obj_end > line && (*(obj_end - 1) == '\n' || *(obj_end - 1) == '\r' ||
                              *(obj_end - 1) == ' '))
        obj_end--;

    size_t len3 = (size_t)(obj_end - line);
    if (len3 >= field_max) len3 = field_max - 1;
    memcpy(obj, line, len3);
    obj[len3] = '\0';

    src[0] = '\0';
    if (tab3 != NULL)
    {
        const char *sbegin = tab3 + 1;
        while (*sbegin == ' ' || *sbegin == '\t')
            sbegin++;
        const char *send = sbegin + strlen(sbegin);
        while (send > sbegin && (*(send - 1) == '\n' || *(send - 1) == '\r' ||
                                 *(send - 1) == ' ' || *(send - 1) == '\t'))
            send--;
        size_t len4 = (size_t)(send - sbegin);
        if (len4 >= field_max) len4 = field_max - 1;
        memcpy(src, sbegin, len4);
        src[len4] = '\0';
    }

    return 1;
}

static int ParseTSVLine(const char *line, char *subj, char *pred, char *obj,
                        size_t field_max)
{
    char src[128];
    return ParseTSVLineSrc(line, subj, pred, obj, src, field_max);
}

/* ============================================================
   Ingest from FILE stream
   ============================================================ */

INGEST_STATS IngestTSVStream(GRAPH *graph, FILE *f)
{
    return IngestTSVStreamSrc(graph, f, NULL);
}

INGEST_STATS IngestTSVStreamSrc(GRAPH *graph, FILE *f, const char *filepath)
{
    INGEST_STATS stats;
    memset(&stats, 0, sizeof(stats));

    if (graph == NULL || f == NULL)
        return stats;

    char line[1024];
    char subj[128], pred[128], obj[128], src[128];
    char defsrc[256];

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

        if (!ParseTSVLineSrc(line, subj, pred, obj, src, sizeof(subj)))
        {
            stats.lines_failed++;
            continue;
        }

        stats.lines_parsed++;

        /* Procedencia: 4a columna explicita, o "fichero:linea" */
        const char *source = (src[0] != '\0') ? src : NULL;
        defsrc[0] = '\0';
        if (source == NULL && filepath != NULL && filepath[0] != '\0')
        {
            snprintf(defsrc, sizeof(defsrc), "%s:%llu",
                     filepath, (unsigned long long)stats.lines_read);
            source = defsrc;
        }

        int rc = IngestTripleSource(graph, subj, pred, obj, source);
        if (rc == 1)
            stats.relations_inserted++;
        else if (rc == 2)
            stats.relations_updated++;
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

    stats = IngestTSVStreamSrc(graph, f, filepath);
    fclose(f);
    return stats;
}
