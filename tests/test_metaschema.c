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

/* ax3-style exemplar: the two observed directions of proportional */
static const char *ax_t1[] = {"speed", "proportionality", "distance"};
static const char *ax_s1[] = {"speed", "proportional", "distance"};

static const char *ax_t2[] = {"distance", "proportionality", "speed"};
static const char *ax_s2[] = {"distance", "proportional", "speed"};

/* adversarial families: must NOT become symmetric */
static const char *rx_t1[] = {"x1", "rr", "x2"};
static const char *rx_s1[] = {"x1", "after", "x2"};
static const char *sx_t1[] = {"y1", "ss", "y2"};
static const char *sx_s1[] = {"y1", "after", "y2"};

int main(void)
{
    printf("== META-SCHEMA C: structural property survives exemplars ==\n\n");

    SCHEMA_KB kb;
    META_KB mk;
    SchemaKBInit(&kb);
    MetaKBInit(&mk);

    SchemaDeclareRole(&kb, "speed", 1, 0, 0, 0);
    SchemaDeclareRole(&kb, "distance", 0, 1, 0, 0);
    SchemaDeclareRole(&kb, "temperature", 1, 0, 0, 0);
    SchemaDeclareRole(&kb, "entropy", 0, 1, 0, 0);
    SchemaDeclareRole(&kb, "photon", 1, 0, 0, 0);
    SchemaDeclareRole(&kb, "wave", 0, 1, 0, 0);
    SchemaDeclareRole(&kb, "electron", 1, 0, 0, 0);
    SchemaDeclareRole(&kb, "mass", 0, 1, 0, 0);

    /* 1. learn: both directions observed for proportionality */
    CHECK(SchemaObserveExemplar(&kb, "ax3", ax_t1, 3, ax_s1, 3),
          "ax3 learned (proportional)");
    CHECK(SchemaObserveExemplar(&kb, "ax4", ax_t2, 3, ax_s2, 3),
          "ax3b learned (inverted observation)");
    CHECK(SchemaCount(&kb) == 1, "one family, two exemplars");

    /* adversarial: single-direction and mixed-family must teach nothing */
    CHECK(SchemaObserveExemplar(&kb, "rx1", rx_t1, 3, rx_s1, 3),
          "rx after-family learned (directional)");
    CHECK(SchemaObserveExemplar(&kb, "sx1", sx_t1, 3, sx_s1, 3),
          "sx after-family learned (single direction)");
    CHECK(SchemaCount(&kb) == 3, "3 schemas total");

    /* 2. meta-observe + discover: only R-with-both-directions -> symmetric */
    CHECK(MetaObserve(&mk, "proportionality", "speed", "distance", "o1"),
          "observe o1: proportional(speed,distance)");
    CHECK(MetaObserve(&mk, "proportionality", "distance", "speed", "o2"),
          "observe o2: proportional(distance,speed)");
    CHECK(MetaObserve(&mk, "rr", "x1", "x2", "o3"),
          "observe o3: after(x1,x2) family rr");
    CHECK(MetaDiscover(&mk) == 1, "exactly 1 meta property discovered");
    CHECK(MetaHasProperty(&mk, "proportionality", META_PROP_SYMMETRIC),
          "proportionality -> SYMMETRIC");
    CHECK(!MetaHasProperty(&mk, "rr", META_PROP_SYMMETRIC),
          "ADVERSARY rr (one direction only) -> no property");
    CHECK(!MetaHasProperty(&mk, "ss", META_PROP_SYMMETRIC),
          "ADVERSARY ss (never observed) -> no property");
    CHECK(MetaCount(&mk) == 1, "exactly 1 meta schema");

    /* 3. persistence: meta facts only */
    CHECK(MetaKBSave(&mk, "test_metaschema.txt") == 1, "save 1 meta schema");
    META_KB cold;
    CHECK(MetaKBLoad(&cold, "test_metaschema.txt") == 1,
          "cold meta load");
    CHECK(MetaHasProperty(&cold, "proportionality", META_PROP_SYMMETRIC),
          "SYMMETRIC survives exemplar disappearance");
    CHECK(cold.num_obs == 0, "cold meta has ZERO observations");

    /* 4. wipe the schema KB's exemplars too: rebuild cold from
       schema facts (schema_kb analog), then present world vocab */
    SCHEMA_KB scold;
    SchemaKBInit(&scold);
    CHECK(SchemaKBSave(&kb, "test_schema_kb.txt") == 3, "save 3 schemas");
    CHECK(SchemaKBLoad(&scold, "test_schema_kb.txt") == 3,
          "cold schema load (3 schema+prov facts)");

    SchemaDeclareRole(&scold, "speed", 1, 0, 0, 0);
    SchemaDeclareRole(&scold, "distance", 0, 1, 0, 0);
    SchemaDeclareRole(&scold, "temperature", 1, 0, 0, 0);
    SchemaDeclareRole(&scold, "entropy", 0, 1, 0, 0);
    SchemaDeclareRole(&scold, "photon", 1, 0, 0, 0);
    SchemaDeclareRole(&scold, "wave", 0, 1, 0, 0);
    SchemaDeclareRole(&scold, "electron", 1, 0, 0, 0);
    SchemaDeclareRole(&scold, "mass", 0, 1, 0, 0);
    SchemaPresentPair(&scold, "proportionality", "speed", "distance");
    SchemaPresentPair(&scold, "proportionality", "photon", "wave");

    char out[128];

    /* 5. NOVEL derivations in BOTH directions from structure alone */
    CHECK(TransferDerive(&scold, &cold, "proportionality", "photon", "wave",
                         out, sizeof(out)) &&
              strcmp(out, "Photon proportional wave.") == 0,
          "novel direct: 'Photon proportional wave.'");
    CHECK(TransferDeriveSwapped(&scold, &cold, "proportionality", "wave",
                                "photon", out, sizeof(out)) &&
              strcmp(out, "Wave proportional photon.") == 0,
          "novel swapped (meta-licensed): 'Wave proportional photon.'");

    /* 6. vocabulary gate: electron/mass have roles but NEVER presented */
    CHECK(!TransferDerive(&scold, &cold, "proportionality", "electron",
                          "mass", out, sizeof(out)),
          "unpresented vocab direct -> UNKNOWN");
    CHECK(!TransferDeriveSwapped(&scold, &cold, "proportionality", "mass",
                                 "electron", out, sizeof(out)),
          "unpresented vocab swapped -> UNKNOWN");

    /* 7. no meta evidence -> no swap license even with vocab+roles */
    CHECK(!TransferDeriveSwapped(&scold, &cold, "rr", "x1", "x2", out,
                                 sizeof(out)),
          "family without meta property -> swap UNKNOWN");

    /* 8. chain family stays fail-closed even if someone asserts
       symmetric on it (defense in depth) */
    CHECK(!TransferDeriveSwapped(&scold, &cold, "ss", "y1", "y2", out,
                                 sizeof(out)),
          "chain family -> swap UNKNOWN even if symmetric existed");

    remove("test_metaschema.txt");
    remove("test_schema_kb.txt");

    printf("\n==================================\n");
    if (g_fail == 0)
    {
        printf("META-SCHEMA + TRANSFER C pipeline verificado OK.\n");
        return 0;
    }
    printf("FAILURES: %d\n", g_fail);
    return 1;
}