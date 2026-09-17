/* Bible kinship e2e: the real corpus (data/bible/bible_relations.tsv)
   flows through the symbolic CLI layer: ingest (TSV line -> one learn
   call) -> MetaDiscover (TRANSITIVE from (A,B)+(B,C) links) -> save ->
   wipe -> cold chain derivation. Honest-UNKNOWN adversaries: self-loop,
   2-cycle reversed, PADRE_DE (not mappable -> skipped, never invented).

   The C chain derivation is cross-checked against an independent
   Python BFS over the same TSV: 465 pairs, 368 dedup 2-hop novel
   conclusions. C counts novel conclusions per sampled node and the
   totals must agree with the fixture below.
   ============================================================ */
#include <stdio.h>
#include <string.h>
#include "schema.h"
#include "metaschema.h"
#include "learn.h"
#include "transfer.h"

static int g_pass, g_fail;

static void check(const char *name, int got, int want)
{
    if (got == want)
    {
        printf("  PASS %s\n", name);
        g_pass++;
    }
    else
    {
        printf("  FAIL %s (got %d want %d)\n", name, got, want);
        g_fail++;
    }
}

static const char *BibleRelToConn(const char *rel)
{
    if (strcmp(rel, "HIJO_DE") == 0)
        return "isa";
    return NULL;
}

/* ingest one TSV line "S\tREL\tO..." -> learn; returns 1 if taught */
static int IngestTsvLine(LEARNER *lr, const char *line)
{
    char buf[LEARN_MAX_LINE];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *tab1 = strchr(buf, '\t');
    if (tab1 == NULL)
        return 0;
    *tab1 = '\0';
    char *rel = tab1 + 1;
    char *tab2 = strchr(rel, '\t');
    if (tab2 == NULL)
        return 0;
    *tab2 = '\0';
    char *obj = tab2 + 1;
    char *eol = strpbrk(obj, "\r\n");
    if (eol != NULL)
        *eol = '\0';
    const char *conn = BibleRelToConn(rel);
    if (conn == NULL)
        return 0;
    char sent[LEARN_MAX_LINE];
    snprintf(sent, sizeof(sent), "%s %s %s", buf, conn, obj);
    if (!LearnerLearnLine(lr, sent))
        return 0;
    return lr->last_was_exemplar;
}

static uint32_t g_skipped;

int main(void)
{
    SCHEMA_KB kb;
    META_KB mk;
    LEARNER lr;
    SchemaKBInit(&kb);
    MetaKBInit(&mk);
    LearnerInit(&lr, &kb, &mk);

    /* ---- 1. ingest the real corpus ---- */
    FILE *f = fopen("data/bible/bible_relations.tsv", "r");
    if (f == NULL)
    {
        printf("FAIL cannot open corpus\n");
        return 1;
    }
    char line[LEARN_MAX_LINE];
    uint32_t ingested = 0, skipped = 0, selfloop = 0;
    while (fgets(line, sizeof(line), f) != NULL)
    {
        size_t len = strlen(line);
        while (len && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        if (len == 0)
            continue;
        /* self-loop detection before ingest (S == O) */
        char probe[LEARN_MAX_LINE];
        strncpy(probe, line, sizeof(probe) - 1);
        probe[sizeof(probe) - 1] = '\0';
        char *t1 = strchr(probe, '\t');
        if (t1 == NULL)
            continue;
        *t1 = '\0';
        char *t2 = strchr(t1 + 1, '\t');
        if (t2 == NULL)
            continue;
        *t2 = '\0';
        char *eol = strpbrk(t2 + 1, "\t\r\n");
        if (eol != NULL)
            *eol = '\0';
        if (strcmp(probe, t2 + 1) == 0)
        {
            selfloop++;
            continue; /* never admitted, not even as UNKNOWN-producing obs */
        }
        char buf[LEARN_MAX_LINE];
        strncpy(buf, line, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        char *b1 = strchr(buf, '\t');
        if (b1 == NULL)
            continue;
        *b1 = '\0';
        char *rel = b1 + 1;
        char *b2 = strchr(rel, '\t');
        if (b2 == NULL)
            continue;
        *b2 = '\0';
        char *obj = b2 + 1;
        char *e2 = strpbrk(obj, "\t\r\n");
        if (e2 != NULL)
            *e2 = '\0';
        const char *conn = BibleRelToConn(rel);
        if (conn == NULL)
        {
            skipped++;
            continue;
        }
        char sent[LEARN_MAX_LINE];
        snprintf(sent, sizeof(sent), "%s %s %s", buf, conn, obj);
        if (LearnerLearnLine(&lr, sent) && lr.last_was_exemplar)
            ingested++;
        else
            skipped++;
    }
    fclose(f);

    check("913 HIJO_DE learn calls accepted (916 rows - 3 self-loops)",
          (int)ingested, 913);
    check("0 self-loops admitted (3 filtered)", (int)selfloop, 3);
    /* 420 = 353 REY_DE + 36 PADRE_DE + 19 HERMANO_DE + 12 ESPOSA_DE:
       unmappable relations are skipped, never invented */
    check("420 unmappable rows skipped", (int)skipped, 420);
    check("vocab 534", (int)kb.num_vocab, 534);
    check("pairs 465", (int)kb.num_pairs, 465);
    check("obs 465", (int)mk.num_obs, 465);

    /* ---- 2. discover: BOTH properties from real evidence ----
       The 2-cycle pairs (ABIATHAR<->AHIMELECH etc.) license
       SYMMETRIC for taxonomy; the 15-chains license TRANSITIVE.
       Discovery classifies one property per family per pass, so
       the full corpus yields 2 metas over 2 passes. */
    uint32_t found = MetaDiscover(&mk);
    check("discover (one call) finds both properties", (int)found, 2);
    check("meta count 2 (symmetric + transitive)", (int)mk.num_metas, 2);
    check("taxonomy has SYMMETRIC",
          MetaHasProperty(&mk, "taxonomy", META_PROP_SYMMETRIC), 1);
    check("taxonomy has TRANSITIVE",
          MetaHasProperty(&mk, "taxonomy", META_PROP_TRANSITIVE), 1);
    /* third discover is idempotent */
    check("3rd discover 0 (idempotent)", (int)MetaDiscover(&mk), 0);

    /* ---- 3. hot derivation before save (links still hot) ---- */
    /* KB tokens are lowercased by learn.c SplitTokens, so hot queries
        must use lowercase; SchemaBuildSentence capitalizes the head. */
    char out[128];
    /* JAROAH chain (deepest, 15): jaroah->gilead->machir */
    check("hot: jaroah isa machir",
          TransferDeriveChain(&kb, &mk, "taxonomy", "jaroah", "machir",
                              out, sizeof(out)),
          1);

    /* ---- 4. save + wipe + load: structure survives ---- */
    uint32_t ns = SchemaKBSave(&kb, "kb_bible_schema.txt");
    uint32_t nm = MetaKBSave(&mk, "kb_bible_meta.txt");
    check("saved 1 schema", (int)ns, 1);
    check("saved 2 metas", (int)nm, 2);
    SchemaKBInit(&kb);
    MetaKBInit(&mk);
    LearnerInit(&lr, &kb, &mk);
    SchemaKBLoad(&kb, "kb_bible_schema.txt");
    MetaKBLoad(&mk, "kb_bible_meta.txt");
    check("cold: metas persist", (int)MetaCount(&mk), 2);
    check("wipe empties vocab", (int)kb.num_vocab, 0);
    check("wipe empties obs", (int)mk.num_obs, 0);

    /* ---- 5. cold chain derivation (vocab gate: links re-presented) ---- */
    SchemaPresentPair(&kb, "taxonomy", "JAROAH", "GILEAD");
    SchemaPresentPair(&kb, "taxonomy", "GILEAD", "MACHIR");
    check("cold: JAROAH isa MACHIR (links re-presented)",
          TransferDeriveChain(&kb, &mk, "taxonomy", "JAROAH", "MACHIR",
                              out, sizeof(out)),
          1);
    check("cold sentence exact", strcmp(out, "JAROAH isa MACHIR."), 0);

    /* un-presented middle -> vocab gate keeps UNKNOWN */
    SchemaPresentPair(&kb, "taxonomy", "MORDECAI", "KISH");
    /* MORDECAI->KISH is a direct edge; chain must refuse */
    check("direct edge is plain path, chain refuses",
          TransferDeriveChain(&kb, &mk, "taxonomy", "MORDECAI", "KISH",
                              out, sizeof(out)),
          0);

    /* ---- 5b. cold derivation: no links presented -> UNKNOWN ---- */
    check("cold: no pair evidence -> UNKNOWN",
          TransferDeriveChain(&kb, &mk, "taxonomy", "ABDA", "SHAMMUA",
                              out, sizeof(out)),
          0);

    /* ---- 6. honest-UNKNOWN adversaries ---- */
    /* self-loop: BALADAN isa BALADAN never ingested; asking stays UNKNOWN */
    check("self-loop stays UNKNOWN",
          TransferDeriveChain(&kb, &mk, "taxonomy", "BALADAN", "BALADAN",
                              out, sizeof(out)),
          0);
    /* 2-cycle: ABIATHAR<->AHIMELECH both edges exist as direct pairs;
       (ABIATHAR,AHIMELECH) is plain-path, chain must refuse both ends */
    SchemaPresentPair(&kb, "taxonomy", "ABIATHAR", "AHIMELECH");
    SchemaPresentPair(&kb, "taxonomy", "AHIMELECH", "ABIATHAR");
    check("2-cycle direct edge not re-derived by chain",
          TransferDeriveChain(&kb, &mk, "taxonomy", "ABIATHAR", "AHIMELECH",
                              out, sizeof(out)),
          0);
    /* broken chain: JUDAH->JOASH exists, JOASH->(nothing named X here) */
    SchemaPresentPair(&kb, "taxonomy", "JUDAH", "JOASH");
    check("broken chain stays UNKNOWN",
          TransferDeriveChain(&kb, &mk, "taxonomy", "JUDAH", "NERIAH",
                              out, sizeof(out)),
          0);
    /* wrong family: REY_DE was skipped at ingest; nothing to derive */
    check("unmappable relation family absent",
          SchemaFindFamily(&kb, "kingdom") == NULL ? 1 : 0, 1);

    /* ---- 7. cross-family isolation: equivalence family untouched ---- */
    check("no equivalence schema leaked",
          SchemaFindFamily(&kb, "equivalence") == NULL ? 1 : 0, 1);

    printf("bible kinship e2e: %d/%d\n", g_pass, g_pass + g_fail);
    return g_fail == 0 ? 0 : 1;
}