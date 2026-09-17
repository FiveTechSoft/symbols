#include <stdio.h>
#include <string.h>
#include "schema.h"
#include "metaschema.h"
#include "transfer.h"

static int g_fail = 0;

#define CHECK(cond, name)                                                  \
    do                                                                     \
    {                                                                      \
        if (cond)                                                          \
            printf("  PASS %s\n", name);                                   \
        else                                                               \
        {                                                                  \
            printf("  FAIL %s\n", name);                                   \
            g_fail++;                                                      \
        }                                                                  \
    } while (0)

/* taxonomy exemplars: discrete domain (animal classification) */
static const char *tx_t1[] = {"robin", "taxonomy", "bird"};
static const char *tx_s1[] = {"robin", "isa", "bird"};
static const char *tx_t2[] = {"bird", "taxonomy", "animal"};
static const char *tx_s2[] = {"bird", "isa", "animal"};

/* adversary 1: only ONE link -> nothing discovered */
static const char *ux_t1[] = {"x1", "ux", "x2"};
static const char *ux_s1[] = {"x1", "isa", "x2"};

/* adversary 2: (A,B)+(B,A) = SYMMETRY evidence, NOT a chain */
static const char *sx_t1[] = {"yin", "sx", "yang"};
static const char *sx_t2[] = {"yang", "sx", "yin"};
static const char *sx_s1[] = {"yin", "isa", "yang"};
static const char *sx_s2[] = {"yang", "isa", "yin"};

int main(void)
{
    printf("== SECOND FAMILY: discrete taxonomy, TRANSITIVE meta ==\n\n");

    SCHEMA_KB kb;
    META_KB mk;
    SchemaKBInit(&kb);
    MetaKBInit(&mk);

    /* 1. learn two taxonomy links from raw input (learn.c analog) */
    CHECK(SchemaObserveExemplar(&kb, "tx1", tx_t1, 3, tx_s1, 3),
          "tx1 learned (robin isa bird -> taxonomy/chain)");
    CHECK(SchemaObserveExemplar(&kb, "tx2", tx_t2, 3, tx_s2, 3),
          "tx2 learned (bird isa animal)");
    CHECK(MetaObserve(&mk, "taxonomy", "robin", "bird", "o1"),
          "observe o1: isa(robin,bird)");
    CHECK(MetaObserve(&mk, "taxonomy", "bird", "animal", "o2"),
          "observe o2: isa(bird,animal)");

    /* 2. adversarial observations in the SAME discovery pass */
    CHECK(SchemaObserveExemplar(&kb, "ux1", ux_t1, 3, ux_s1, 3),
          "ux single-link family learned (schema yes, property NO)");
    CHECK(MetaObserve(&mk, "ux", "x1", "x2", "o3"),
          "observe o3: ux(x1,x2) SINGLE link");
    CHECK(SchemaObserveExemplar(&kb, "sx1", sx_t1, 3, sx_s1, 3),
          "sx both-directions exemplars (would look symmetric)");
    CHECK(SchemaObserveExemplar(&kb, "sx2", sx_t2, 3, sx_s2, 3),
          "sx learned");
    CHECK(MetaObserve(&mk, "sx", "yin", "yang", "o4"),
          "observe o4: sx(yin,yang)");
    CHECK(MetaObserve(&mk, "sx", "yang", "yin", "o5"),
          "observe o5: sx(yang,yin) -> SYMMETRIC candidate, not chain");

    CHECK(MetaDiscover(&mk) == 2,
          "exactly 2 meta properties (taxonomy TRANSITIVE + sx SYMMETRIC)");
    CHECK(MetaHasProperty(&mk, "taxonomy", META_PROP_TRANSITIVE),
          "taxonomy -> TRANSITIVE (chain evidence)");
    CHECK(MetaHasProperty(&mk, "sx", META_PROP_SYMMETRIC),
          "sx -> SYMMETRIC (both directions, NOT transitive)");
    CHECK(!MetaHasProperty(&mk, "sx", META_PROP_TRANSITIVE),
          "sx must NOT be TRANSITIVE ((A,B)+(B,A) is symmetry evidence)");
    CHECK(!MetaHasProperty(&mk, "ux", META_PROP_TRANSITIVE),
          "ADVERSARY ux (one link only) -> no property");
    CHECK(MetaCount(&mk) == 2, "exactly 2 meta schemas");

    /* 3. persistence: both properties survive exemplar death */
    CHECK(SchemaKBSave(&kb, "test_family2_schema.txt") == 3,
          "save 3 schemas (taxonomy, ux, sx)");
    CHECK(MetaKBSave(&mk, "test_family2_meta.txt") == 2,
          "save 2 meta properties");
    SCHEMA_KB scold;
    META_KB mcold;
    SchemaKBInit(&scold);
    MetaKBInit(&mcold);
    CHECK(SchemaKBLoad(&scold, "test_family2_schema.txt") == 3,
          "cold schema load");
    CHECK(MetaKBLoad(&mcold, "test_family2_meta.txt") == 2,
          "cold meta load");
    CHECK(MetaHasProperty(&mcold, "taxonomy", META_PROP_TRANSITIVE),
          "TRANSITIVE survives exemplar disappearance");
    CHECK(mcold.num_obs == 0 && scold.num_pairs == 0,
          "cold KBs have ZERO observations/pairs");

    /* 4. cold chain derivation: links re-presented as INPUT */
    SchemaPresentPair(&scold, "taxonomy", "robin", "bird");
    SchemaPresentPair(&scold, "taxonomy", "bird", "animal");
    SchemaPresentPair(&scold, "taxonomy", "sparrow", "bird");

    char out[128];
    CHECK(TransferDeriveChain(&scold, &mcold, "taxonomy", "robin",
                              "animal", out, sizeof(out)) &&
              strcmp(out, "Robin isa animal.") == 0,
          "NOVEL chain: 'Robin isa animal.' (conclusion never presented)");
    CHECK(!TransferDeriveChain(&scold, &mcold, "taxonomy", "sparrow",
                               "bird", out, sizeof(out)),
          "direct pair NOT re-derived through chain path (plain path owns it)");
    CHECK(TransferDerive(&scold, &mcold, "taxonomy", "sparrow", "bird",
                         out, sizeof(out)) &&
              strcmp(out, "Sparrow isa bird.") == 0,
          "direct presented pair via plain path");

    /* 5. cross-family contamination must NOT happen */
    CHECK(!TransferDeriveChain(&scold, &mcold, "ux", "x1", "x2", out,
                               sizeof(out)),
          "ux family: no transitive property -> chain UNKNOWN");
    CHECK(!TransferDeriveSwapped(&scold, &mcold, "taxonomy", "bird",
                                 "robin", out, sizeof(out)),
          "taxonomy is NOT symmetric -> swap UNKNOWN");
    CHECK(!TransferDeriveChain(&scold, &mcold, "sx", "yin", "yang", out,
                               sizeof(out)),
          "sx symmetric family: chain path requires links (absent) -> UNKNOWN");

    /* 6. vocabulary gate on the chain conclusion */
    CHECK(!TransferDeriveChain(&scold, &mcold, "taxonomy", "robin", "worm",
                               out, sizeof(out)),
          "conclusion vocab unpresented -> UNKNOWN (worm never given)");
    SchemaPresentPair(&scold, "taxonomy", "animal", "worm");
    /* transitividad ENCADENA: bird->animal + animal->worm = 2 eslabones
       -> bird isa worm es una conclusion LEGITIMA de la propiedad */
    CHECK(TransferDeriveChain(&scold, &mcold, "taxonomy", "bird", "worm",
                              out, sizeof(out)) &&
              strcmp(out, "Bird isa worm.") == 0,
          "chained transitivity: bird->animal + animal->worm -> 'Bird isa worm.'");
    /* adversario real: eslabon roto (sin (M,O) que ligue) */
    SchemaPresentPair(&scold, "taxonomy", "stone", "mineral");
    CHECK(!TransferDeriveChain(&scold, &mcold, "taxonomy", "bird", "mineral",
                               out, sizeof(out)),
          "broken chain (bird->animal vs stone->mineral, no shared middle) -> UNKNOWN");

    remove("test_family2_schema.txt");
    remove("test_family2_meta.txt");

    printf("\n==================================\n");
    if (g_fail == 0)
    {
        printf("SECOND FAMILY (taxonomy/TRANSITIVE) verificado OK.\n");
        return 0;
    }
    printf("FAILURES: %d\n", g_fail);
    return 1;
}