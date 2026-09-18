/* test_bible_bfs: Fase 2 parity test, exhaustive (no sampling).
    The C BFS (ChatBfsReach / ChatBfsPath) must answer EXACTLY the
    independent Python fixture data/bible/bible_bfs_fixture.tsv
    (2053 novel pairs = 368 at depth 2 + 1685 at depth >= 3,
    regenerable with tools/bible_bfs_fixture.py).

    PART A: for every fixture row (S,O,D), ChatBfsReach from S
    must contain O at EXACT depth D (full coverage, exact depths).

    PART B (anti-false-positive): for EVERY pair (S,O) of the
    whole KB vocabulary NOT in the fixture, ChatBfsPath(S,O) must
    answer 0 (the plain/chain paths own everything else).

    PART C: the census cross-product over ChatBfsReach must
    total exactly 2053 rows, with the depth histogram
    {2:368, 3:368, 4:325, 5:304, 6:243, 7:157, 8:95, 9:63,
     10:49, 11:40, 12:25, 13:13, 14:3} (computed in python).
    ============================================================ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "schema.h"
#include "metaschema.h"
#include "learn.h"
#include "transfer.h"
#include "chat.h"

#define FIX_ROWS 2053

static const char *BibleRelToConn(const char *rel)
{
    if (strcmp(rel, "HIJO_DE") == 0)
        return "isa";
    return NULL;
}

static uint32_t CheckFailures = 0;

static void Check(const char *label, long long got, long long want)
{
    if (got != want)
    {
        printf("FAIL %s (got %lld, want %lld)\n", label, got, want);
        CheckFailures++;
    }
    else
        printf("PASS %s\n", label);
}

int main(void)
{
    CHAT ch;
    ChatInit(&ch, "data/bible/bible_relations.tsv");
    if (ch.kb.num_pairs != 624)
    {
        printf("FAIL ingest %u != 624\n", ch.kb.num_pairs);
        return 1;
    }

    /* ---- load the python fixture ---- */
    FILE *f = fopen("data/bible/bible_bfs_fixture.tsv", "r");
    if (f == NULL)
    {
        printf("FAIL cannot open fixture\n");
        return 1;
    }
    static char fs[FIX_ROWS][SCHEMA_TOKEN_MAX];
    static char fo[FIX_ROWS][SCHEMA_TOKEN_MAX];
    static int fd[FIX_ROWS];
    uint32_t nrows = 0;
    char line[128];
    while (fgets(line, sizeof(line), f) != NULL && nrows < FIX_ROWS)
    {
        char *t1 = strchr(line, '\t');
        if (t1 == NULL)
            continue;
        *t1 = '\0';
        char *obj = t1 + 1;
        char *t2 = strchr(obj, '\t');
        if (t2 == NULL)
            continue;
        *t2 = '\0';
        size_t len = strlen(t2 + 1);
        while (len && (t2[1 + len - 1] == '\n' || t2[1 + len - 1] == '\r'))
            t2[1 + --len] = '\0';
        strncpy(fs[nrows], line, SCHEMA_TOKEN_MAX - 1);
        strncpy(fo[nrows], obj, SCHEMA_TOKEN_MAX - 1);
        fd[nrows] = atoi(t2 + 1);
        nrows++;
    }
    fclose(f);
    Check("fixture rows", (long long)nrows, FIX_ROWS);

    /* ---- PART A: coverage + exact depth, via ChatBfsReach ---- */
    static char names[CHAT_BFS_ROW_MAX][CHAT_TOKEN_MAX];
    static uint32_t depths[CHAT_BFS_ROW_MAX];
    uint32_t covered = 0, wrong_depth = 0, missing = 0;
    for (uint32_t s = 0; s < nrows; s++)
    {
        /* fixture sources are consecutive; avoid re-running the
            census for the same source */
        if (s > 0 && strcmp(fs[s], fs[s - 1]) == 0)
            continue;
        /* mark rows of this source, run the census once, check */
        uint32_t start = s;
        while (s + 1 < nrows && strcmp(fs[s + 1], fs[start]) == 0)
            s++;
        uint32_t n = ChatBfsReach(&ch, fs[start], names, depths,
                                  CHAT_BFS_ROW_MAX);
        for (uint32_t r = start; r <= s; r++)
        {
            int hit = -1;
            for (uint32_t k = 0; k < n; k++)
                if (strcmp(names[k], fo[r]) == 0)
                {
                    hit = (int)k;
                    break;
                }
            if (hit < 0)
            {
                printf("FAIL missing (%s,%s) d=%d\n", fs[r], fo[r],
                       fd[r]);
                CheckFailures++;
                missing++;
                continue;
            }
            covered++;
            if (depths[hit] != (uint32_t)fd[r])
            {
                printf("FAIL wrong depth (%s,%s) got %u want %d\n",
                       fs[r], fo[r], depths[hit], fd[r]);
                CheckFailures++;
                wrong_depth++;
            }
        }
    }
    Check("fixture coverage", (long long)covered, FIX_ROWS);
    Check("exact depths", (long long)wrong_depth, 0);

    /* ---- PART B: anti-FP over the whole cross-product ----
        every (S,O) not in the fixture must get NO bfs path:
        direct pairs, unreachable pairs, self pairs. Fixture
        membership is binary-searched (rows are sorted). */
    long long fp = 0, probed = 0;
    char path[CHAT_BFS_PATH_MAX][CHAT_TOKEN_MAX];
    for (uint32_t i = 0; i < ch.kb.num_vocab; i++)
    {
        for (uint32_t j = 0; j < ch.kb.num_vocab; j++)
        {
            if (i == j)
                continue;
            const char *S = ch.kb.vocab[i];
            const char *O = ch.kb.vocab[j];
            int lo = 0, hi = (int)nrows - 1, in_fix = 0;
            while (lo <= hi)
            {
                int m = (lo + hi) / 2;
                int c = strcmp(fs[m], S);
                if (c == 0)
                    c = strcmp(fo[m], O);
                if (c == 0)
                {
                    in_fix = 1;
                    break;
                }
                if (c < 0)
                    lo = m + 1;
                else
                    hi = m - 1;
            }
            if (in_fix)
                continue;
            probed++;
            if (ChatBfsPath(&ch, S, O, path) > 0)
            {
                printf("FAIL false positive (%s,%s)\n", S, O);
                CheckFailures++;
                fp++;
            }
        }
    }
    printf("anti-FP probed %lld non-fixture pairs\n", probed);
    Check("false positives", fp, 0);

    /* ---- PART C: census totals + depth histogram ---- */
    static long long hist[32];
    long long total = 0;
    for (uint32_t i = 0; i < ch.kb.num_vocab; i++)
    {
        uint32_t n = ChatBfsReach(&ch, ch.kb.vocab[i], names, depths,
                                  CHAT_BFS_ROW_MAX);
        total += n;
        for (uint32_t k = 0; k < n; k++)
        {
            if (depths[k] < 32)
                hist[depths[k]]++;
        }
    }
    Check("census total rows", total, 2053);
    Check("hist 2-hop", hist[2], 368);
    Check("hist 3-hop", hist[3], 368);
    Check("hist 4-hop", hist[4], 325);
    Check("hist 5-hop", hist[5], 304);
    Check("hist 6-hop", hist[6], 243);
    Check("hist 7-hop", hist[7], 157);
    Check("hist 8-hop", hist[8], 95);
    Check("hist 9-hop", hist[9], 63);
    Check("hist 10-hop", hist[10], 49);
    Check("hist 11-hop", hist[11], 40);
    Check("hist 12-hop", hist[12], 25);
    Check("hist 13-hop", hist[13], 13);
    Check("hist 14-hop", hist[14], 3);

    /* ---- PART D: WHY-path spot integrity on a deep pair ----
        (abijah,adaiah) at fixture depth 7: the path must be a
        real edge chain of exactly 7 edges */
    if (ChatBfsPath(&ch, "abijah", "adaiah", path) != 7)
    {
        printf("FAIL deep path steps\n");
        CheckFailures++;
    }
    else
    {
        for (int k = 0; k < 7; k++)
        {
            int edge = 0;
            for (uint32_t i = 0; i < ch.kb.num_pairs && !edge; i++)
            {
                const PAIR_EVID *pp = &ch.kb.pairs[i];
                if (strcmp(pp->family, "taxonomy") == 0 &&
                    strcmp(pp->subject, path[k]) == 0 &&
                    strcmp(pp->object, path[k + 1]) == 0)
                    edge = 1;
            }
            if (!edge)
            {
                printf("FAIL path edge %d not in KB\n", k);
                CheckFailures++;
            }
        }
        printf("PASS deep path edges verified\n");
    }

    if (CheckFailures == 0)
    {
        printf("test_bible_bfs: ALL PASS (fixture parity, anti-FP, "
               "census, deep path)\n");
        return 0;
    }
    printf("test_bible_bfs: %u failures\n", CheckFailures);
    return 1;
}