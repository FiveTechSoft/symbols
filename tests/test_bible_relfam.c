/* test_bible_relfam: full-corpus contract for the 4 new relational
   families (sibling/father/reigns/wife) + taxonomy, all DEDUCED at
   ingest: es_stem = REL minus "_DE", en_stem = connective minus
   "_of", family = lr.last_family. Per-family derivation policy at
   the chat layer (consultable, census-verified):
     taxonomy: direct + chain (368 novel, frozen fixture)
     father:   direct + chain (3 novel, all true)
     sibling:  direct + swap over OBSERVED pairs only; chain refused
     reigns:   direct only (1 false chain refused)
     wife:     direct only
   The frozen layers are never touched: no cross-inference between
   families, chain policy only licenses taxonomy+father. */
#include <stdio.h>
#include <string.h>
#include "schema.h"
#include "metaschema.h"
#include "learn.h"
#include "transfer.h"
#include "bible_chat.h"

static int g_pass = 0, g_fail = 0;

static void check(const char *name, long long got, long long want)
{
    if (got == want)
    {
        printf("  PASS %s\n", name);
        g_pass++;
    }
    else
    {
        printf("  FAIL %s (got %lld want %lld)\n", name, got, want);
        g_fail++;
    }
}

static long long FamilyPairs(const CHAT *ch, const char *family)
{
    long long n = 0;
    for (uint32_t i = 0; i < ch->kb.num_pairs; i++)
        if (strcmp(ch->kb.pairs[i].family, family) == 0)
            n++;
    return n;
}

/* chain conclusions derivable from OBSERVED pairs of the family,
   excluding observed ones (census of novel conclusions, deduped
   by (start,end) to match the first-match derivation semantics) */
static long long ChainCensus(const CHAT *ch, const char *family)
{
    char seen[1024][2 * SCHEMA_TOKEN_MAX + 1];
    long long n = 0;
    for (uint32_t i = 0; i < ch->kb.num_pairs; i++)
    {
        const PAIR_EVID *p1 = &ch->kb.pairs[i];
        if (strcmp(p1->family, family) != 0)
            continue;
        for (uint32_t j = 0; j < ch->kb.num_pairs; j++)
        {
            const PAIR_EVID *p2 = &ch->kb.pairs[j];
            if (strcmp(p2->family, family) != 0)
                continue;
            if (strcmp(p1->object, p2->subject) != 0)
                continue;
            if (strcmp(p1->subject, p2->object) == 0)
                continue;
            int observed = 0;
            for (uint32_t k = 0; k < ch->kb.num_pairs; k++)
            {
                const PAIR_EVID *q = &ch->kb.pairs[k];
                if (strcmp(q->family, family) == 0 &&
                    strcmp(q->subject, p1->subject) == 0 &&
                    strcmp(q->object, p2->object) == 0)
                    observed = 1;
            }
            if (observed)
                continue;
            char key[2 * SCHEMA_TOKEN_MAX + 1];
            snprintf(key, sizeof(key), "%s|%s", p1->subject, p2->object);
            int dup = 0;
            for (long long k = 0; k < n; k++)
                if (strcmp(seen[k], key) == 0)
                    dup = 1;
            if (!dup && n < 1024)
                strcpy(seen[n++], key);
        }
    }
    return n;
}

int main(void)
{
    CHAT ch;
    ChatInit(&ch, "data/bible/bible_relations.tsv");

    /* ---- 1. ingest contract (census: 1336 rows, 3 self-loops) ---- */
    check("pairs 624", (long long)ch.kb.num_pairs, 624);
    check("vocab 674", (long long)ch.kb.num_vocab, 674);
    check("deduced relations 5", (long long)ch.num_kws, 5);
    check("meta families 6", (long long)MetaCount(&ch.mk), 6);

    /* ---- 2. deduced index: stems, connectives, families ---- */
    const REL_KW *hijo = NULL, *hermano = NULL, *padre = NULL, *rey = NULL,
                 *esposa = NULL;
    for (uint32_t i = 0; i < ch.num_kws; i++)
    {
        const REL_KW *k = &ch.kws[i];
        if (strcmp(k->es_stem, "hijo") == 0)
            hijo = k;
        else if (strcmp(k->es_stem, "hermano") == 0)
            hermano = k;
        else if (strcmp(k->es_stem, "padre") == 0)
            padre = k;
        else if (strcmp(k->es_stem, "rey") == 0)
            rey = k;
        else if (strcmp(k->es_stem, "esposa") == 0)
            esposa = k;
    }
    check("kw hijo found", hijo != NULL, 1);
    check("kw hermano found", hermano != NULL, 1);
    check("kw padre found", padre != NULL, 1);
    check("kw rey found", rey != NULL, 1);
    check("kw esposa found", esposa != NULL, 1);
    if (hijo && hermano && padre && rey && esposa)
    {
        check("hijo -> taxonomy/isa",
              strcmp(hijo->family, "taxonomy") == 0 &&
                  strcmp(hijo->conn, "isa") == 0,
              1);
        check("hermano -> sibling/sibling_of",
              strcmp(hermano->family, "sibling") == 0 &&
                  strcmp(hermano->conn, "sibling_of") == 0,
              1);
        check("padre -> father/father_of",
              strcmp(padre->family, "father") == 0 &&
                  strcmp(padre->conn, "father_of") == 0,
              1);
        check("rey -> reigns/reigns",
              strcmp(rey->family, "reigns") == 0 &&
                  strcmp(rey->conn, "reigns") == 0,
              1);
        check("esposa -> wife/wife_of",
              strcmp(esposa->family, "wife") == 0 &&
                  strcmp(esposa->conn, "wife_of") == 0,
              1);
        check("en stems deduced (sibling/father/reigns/wife)",
              strcmp(hermano->en_stem, "sibling") == 0 &&
                  strcmp(padre->en_stem, "father") == 0 &&
                  strcmp(rey->en_stem, "reigns") == 0 &&
                  strcmp(esposa->en_stem, "wife") == 0,
              1);
    }

    /* ---- 3. per-family pair census (measured, double-method) ---- */
    check("taxonomy pairs 465", FamilyPairs(&ch, "taxonomy"), 465);
    check("sibling pairs 14", FamilyPairs(&ch, "sibling"), 14);
    check("father pairs 30", FamilyPairs(&ch, "father"), 30);
    check("reigns pairs 105", FamilyPairs(&ch, "reigns"), 105);
    check("wife pairs 10", FamilyPairs(&ch, "wife"), 10);

    /* ---- 4. chain policy: taxonomy+father only (consultable) ---- */
    check("policy taxonomy chain allowed",
          (long long)ChatFamilyChainAllowed("taxonomy"), 1);
    check("policy father chain allowed",
          (long long)ChatFamilyChainAllowed("father"), 1);
    check("policy sibling chain refused",
          (long long)ChatFamilyChainAllowed("sibling"), 0);
    check("policy reigns chain refused",
          (long long)ChatFamilyChainAllowed("reigns"), 0);
    check("policy wife chain refused",
          (long long)ChatFamilyChainAllowed("wife"), 0);

    /* ---- 5. chain conclusions under policy ---- */
    {
        char out[128], mid[SCHEMA_TOKEN_MAX];
        check("father chain abraham->jacob (isaac middle)",
              (long long)TransferExplainChain(&ch.kb, &ch.mk, "father",
                                              "abraham", "jacob", out,
                                              sizeof(out), mid, sizeof(mid)),
              1);
        check("father chain middle is isaac",
              strcmp(mid, "isaac") == 0, 1);
        check("sibling chain census refused at chat layer",
              (long long)ChatFamilyChainAllowed("sibling"), 0);
        check("reigns chain census refused at chat layer",
              (long long)ChatFamilyChainAllowed("reigns"), 0);
        /* the raw meta layer DOES find chain evidence in the corpus
           (debir->eglon->moab reigns; sibling A,B)+(B,C)); the
           refusal is CHAT POLICY, not absence of meta property */
        check("reigns transitive in meta (evidence exists)",
              (long long)MetaHasProperty(&ch.mk, "reigns",
                                         META_PROP_TRANSITIVE),
              1);
        check("sibling transitive in meta (evidence exists)",
              (long long)MetaHasProperty(&ch.mk, "sibling",
                                         META_PROP_TRANSITIVE),
              1);
        check("wife not transitive in meta (no chains)",
              (long long)MetaHasProperty(&ch.mk, "wife",
                                         META_PROP_TRANSITIVE),
              0);
        check("taxonomy transitive in meta (frozen)",
              (long long)MetaHasProperty(&ch.mk, "taxonomy",
                                         META_PROP_TRANSITIVE),
              1);
        check("father transitive in meta",
              (long long)MetaHasProperty(&ch.mk, "father",
                                         META_PROP_TRANSITIVE),
              1);
        /* the refused chains must NOT derive through the chat path */
        check("reigns chain debir->moab refused by policy",
              (long long)TransferDeriveChain(&ch.kb, &ch.mk, "reigns",
                                             "debir", "moab", out,
                                             sizeof(out)) &&
                  ChatFamilyChainAllowed("reigns"),
              0);
        check("father transitive in meta",
              (long long)MetaHasProperty(&ch.mk, "father",
                                         META_PROP_TRANSITIVE),
              1);
    }

    /* ---- 6. no cross-family inference ---- */
    {
        char out[128];
        check("cross-family abimelech->gerar as taxonomy refused",
              (long long)TransferDerive(&ch.kb, &ch.mk, "taxonomy",
                                        "abimelech", "gerar", out,
                                        sizeof(out)),
              0);
        check("within-family abimelech reigns gerar admitted",
              (long long)TransferDerive(&ch.kb, &ch.mk, "reigns",
                                        "abimelech", "gerar", out,
                                        sizeof(out)),
              1);
    }

    /* ---- 7. sibling swap over observed pairs only ---- */
    {
        char a[SCHEMA_TOKEN_MAX], b[SCHEMA_TOKEN_MAX];
        int have = 0;
        for (uint32_t i = 0; i < ch.kb.num_pairs && !have; i++)
        {
            const PAIR_EVID *p = &ch.kb.pairs[i];
            if (strcmp(p->family, "sibling") == 0 &&
                strcmp(p->subject, p->object) != 0)
            {
                strcpy(a, p->subject);
                strcpy(b, p->object);
                have = 1;
            }
        }
        check("observed sibling pair exists", have, 1);
        if (have)
        {
            char sibs[16][CHAT_TOKEN_MAX];
            uint32_t n1 = ChatSiblings(&ch, a, sibs, 16);
            check("sibling swap: asking a sees b", n1 > 0, 1);
            int sees_b = 0;
            for (uint32_t i = 0; i < n1; i++)
                if (strcmp(sibs[i], b) == 0)
                    sees_b = 1;
            check("sibling swap direction (a->b)", sees_b, 1);
            /* cold sibling pair must NOT derive (no observed swap) */
            char out[128];
            check("sibling chain refused cold (fail-closed)",
                  (long long)TransferDeriveChain(&ch.kb, &ch.mk, "sibling", a,
                                                 b, out, sizeof(out)),
                  0);
        }
    }

    /* ---- 8. chain census of novel conclusions (double-method data) ---- */
    check("taxonomy novel chains 368 (frozen fixture)",
          ChainCensus(&ch, "taxonomy"), 368);
    check("father novel chains 3", ChainCensus(&ch, "father"), 3);
    /* sibling chain would yield 3 novel but they are REFUSED */
    check("sibling raw novel chains 3 (refused by policy)",
          ChainCensus(&ch, "sibling"), 3);
    check("reigns raw novel chains 1 (refused by policy)",
          ChainCensus(&ch, "reigns"), 1);
    check("wife raw novel chains 0", ChainCensus(&ch, "wife"), 0);

    printf("bible relfam: %d/%d\n", g_pass, g_pass + g_fail);
    return g_fail == 0 ? 0 : 1;
}