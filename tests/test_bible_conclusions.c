/* test_bible_conclusions: exhaustive 2-hop conclusion census.
    For EVERY pair (S,O) in the re-presented KB, TransferDeriveChain
    must answer 1 exactly for the 368 novel 2-hop conclusions the
    independent Python fixture counted over the same TSV (381 2-hop
    path instances dedup to 368). No sampling: full cross-product.
    ============================================================ */
#include <stdio.h>
#include <string.h>
#include "schema.h"
#include "metaschema.h"
#include "learn.h"
#include "transfer.h"

static const char *BibleRelToConn(const char *rel)
{
    if (strcmp(rel, "HIJO_DE") == 0)
        return "isa";
    return NULL;
}

int main(void)
{
    SCHEMA_KB kb;
    META_KB mk;
    LEARNER lr;
    SchemaKBInit(&kb);
    MetaKBInit(&mk);
    LearnerInit(&lr, &kb, &mk);

    FILE *f = fopen("data/bible/bible_relations.tsv", "r");
    if (f == NULL)
    {
        printf("FAIL cannot open corpus\n");
        return 1;
    }
    char line[LEARN_MAX_LINE];
    uint32_t pairs = 0;
    while (fgets(line, sizeof(line), f) != NULL)
    {
        size_t len = strlen(line);
        while (len && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        if (len == 0)
            continue;
        char buf[LEARN_MAX_LINE];
        strncpy(buf, line, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        char *t1 = strchr(buf, '\t');
        if (t1 == NULL)
            continue;
        *t1 = '\0';
        char *rel = t1 + 1;
        char *t2 = strchr(rel, '\t');
        if (t2 == NULL)
            continue;
        *t2 = '\0';
        char *obj = t2 + 1;
        char *eol = strpbrk(obj, "\t\r\n");
        if (eol != NULL)
            *eol = '\0';
        if (strcmp(buf, obj) == 0)
            continue; /* self-loop */
        const char *conn = BibleRelToConn(rel);
        if (conn == NULL)
            continue;
        char sent[LEARN_MAX_LINE];
        snprintf(sent, sizeof(sent), "%s %s %s", buf, conn, obj);
        if (LearnerLearnLine(&lr, sent))
            pairs++;
    }
    fclose(f);
    if (pairs != 913)
    {
        printf("FAIL ingest %u != 913\n", pairs);
        return 1;
    }
    MetaDiscover(&mk);
    if (!MetaHasProperty(&mk, "taxonomy", META_PROP_TRANSITIVE))
    {
        printf("FAIL no transitive meta\n");
        return 1;
    }

    /* exhaustive census over every (S,O) pair combination,
        deduped: TransferDeriveChain answers per (S,O), but the
        python fixture counts DISTINCT conclusions, so the census
        records each (S,O) once (any middle proving it is fine). */
    char seen[SCHEMA_PAIR_MAX][SCHEMA_TOKEN_MAX * 2 + 4];
    uint32_t nseen = 0, derived = 0, dup = 0, i, j;
    for (i = 0; i < kb.num_pairs; i++)
    {
        for (j = 0; j < kb.num_pairs; j++)
        {
            char out[128], key[SCHEMA_TOKEN_MAX * 2 + 4];
            if (!TransferDeriveChain(&kb, &mk, "taxonomy",
                                     kb.pairs[i].subject,
                                     kb.pairs[j].object,
                                     out, sizeof(out)))
                continue;
            snprintf(key, sizeof(key), "%s|%s",
                     kb.pairs[i].subject, kb.pairs[j].object);
            int k, dupk = 0;
            for (k = 0; k < (int)nseen; k++)
            {
                if (strcmp(seen[k], key) == 0)
                {
                    dupk = 1;
                    break;
                }
            }
            if (dupk)
            {
                dup++;
                continue;
            }
            if (nseen < SCHEMA_PAIR_MAX)
            {
                strncpy(seen[nseen], key, sizeof(seen[nseen]) - 1);
                seen[nseen][sizeof(seen[nseen]) - 1] = '\0';
                nseen++;
            }
            derived++;
        }
    }
    printf("derived distinct 2-hop conclusions: %u (dup answers %u)\n",
           derived, dup);
    printf("expect (python fixture): 368\n");
    if (derived == 368)
    {
        printf("PASS exhaustive census matches python fixture 368\n");
        return 0;
    }
    printf("FAIL census mismatch\n");
    return 1;
}