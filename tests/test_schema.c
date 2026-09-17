#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "schema.h"

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

/* exemplars: domain-A math triples + surface sentences (gm_ex analog) */
static const char *ax1_triple[] = {"a", "equivalence", "b"};
static const char *ax1_sent[] = {"a", "cong", "b"};
static const char *ax2_triple[] = {"c", "equivalence", "d"};
static const char *ax2_sent[] = {"c", "cong", "d"};

static const char *ax3_triple[] = {"speed", "proportionality", "distance"};
static const char *ax3_sent[] = {"speed", "proportional", "distance"};

int main(void)
{
    printf("== GEN-META C: discover -> persist -> cold -> novel ==\n\n");

    SCHEMA_KB kb;
    SchemaKBInit(&kb);

    /* role lexicon: derivation-rule entries (never asserted) */
    SchemaDeclareRole(&kb, "speed", 1, 0, 0, 0);
    SchemaDeclareRole(&kb, "distance", 0, 1, 0, 0);
    SchemaDeclareRole(&kb, "temperature", 1, 0, 0, 0);
    SchemaDeclareRole(&kb, "entropy", 0, 1, 0, 0);
    SchemaDeclareRole(&kb, "photon", 1, 0, 0, 0);
    SchemaDeclareRole(&kb, "wave", 0, 1, 0, 0);
    SchemaDeclareRole(&kb, "boost", 0, 0, 1, 0);
    SchemaDeclareRole(&kb, "frame", 0, 0, 0, 1);

    /* 1. discovery from exemplars only */
    CHECK(SchemaObserveExemplar(&kb, "ax1", ax1_triple, 3, ax1_sent, 3),
          "ax1 discovered (cong -> sym)");
    CHECK(SchemaObserveExemplar(&kb, "ax2", ax2_triple, 3, ax2_sent, 3),
          "ax2 discovered (provenance grows)");
    CHECK(SchemaObserveExemplar(&kb, "ax3", ax3_triple, 3, ax3_sent, 3),
          "ax3 discovered (proportional -> dependent_first)");
    CHECK(SchemaCount(&kb) == 2, "exactly 2 schemas (sym + proportionality)");

    const RELATIONAL_SCHEMA *eq = SchemaGet(&kb, 0);
    CHECK(eq && eq->num_prov == 2 &&
              strcmp(eq->prov[0], "ax1") == 0 && strcmp(eq->prov[1], "ax2") == 0,
          "prov equivalence = [ax1, ax2]");
    const RELATIONAL_SCHEMA *pr = SchemaGet(&kb, 1);
    CHECK(pr && pr->order == SCHEMA_ORDER_DEP_FIRST &&
              pr->num_connectives == 1 &&
              strcmp(pr->connectives[0], "proportional") == 0,
          "proportionality: order dependent_first, conn proportional");

    /* idempotency: re-observe same exemplar -> prov unchanged */
    SchemaObserveExemplar(&kb, "ax1", ax1_triple, 3, ax1_sent, 3);
    CHECK(SchemaCount(&kb) == 2 && eq->num_prov == 2, "re-observe idempotent");

    /* 2. persistence: explicit schema facts only */
    uint32_t saved = SchemaKBSave(&kb, "test_schema_kb.txt");
    CHECK(saved == 2, "save 2 schemas");

    /* 3. COLD: wipe ALL working memory, reload schema facts only */
    SCHEMA_KB cold;
    SchemaKBInit(&cold);
    uint32_t n = SchemaKBLoad(&cold, "test_schema_kb.txt");
    CHECK(n == 2, "cold load reads 2 schema+prov facts");
    CHECK(cold.num_vocab == 0 && cold.num_pairs == 0 && cold.num_roles == 0,
          "cold has ZERO working state (vocab/pairs/roles empty)");

    /* cold world re-presentation: roles + vocab as INPUT */
    SchemaDeclareRole(&cold, "speed", 1, 0, 0, 0);
    SchemaDeclareRole(&cold, "distance", 0, 1, 0, 0);
    SchemaDeclareRole(&cold, "temperature", 1, 0, 0, 0);
    SchemaDeclareRole(&cold, "entropy", 0, 1, 0, 0);
    SchemaDeclareRole(&cold, "photon", 1, 0, 0, 0);
    SchemaDeclareRole(&cold, "wave", 0, 1, 0, 0);
    SchemaDeclareRole(&cold, "boost", 0, 0, 1, 0);
    SchemaDeclareRole(&cold, "frame", 0, 0, 0, 1);
    SchemaPresentPair(&cold, "proportionality", "speed", "distance");
    SchemaPresentPair(&cold, "proportionality", "photon", "wave");

    /* 4. NOVEL realization from schemas alone */
    char out[128];
    CHECK(SchemaRealize(&cold, "proportionality", "photon", "wave",
                        out, sizeof(out)) &&
              strcmp(out, "Photon proportional wave.") == 0,
          "novel: 'Photon proportional wave.' (photon presented, never exemplar)");

    /* inverted proportionality: fail-closed UNKNOWN */
    CHECK(!SchemaRealize(&cold, "proportionality", "distance", "photon",
                         out, sizeof(out)),
          "inverted proportionality -> UNKNOWN (no direction evidence)");

    /* unknown vocabulary -> honest UNKNOWN: boost/frame have roles but
       were never PRESENTED into the sym schema's world */
    CHECK(SchemaRealize(&cold, "equivalence", "boost", "frame", out,
                        sizeof(out)) == 0,
          "sym with un-presented vocab -> UNKNOWN");

    /* sym admits any direction once vocab is presented */
    SchemaPresentPair(&cold, "equivalence", "photon", "wave");
    CHECK(SchemaRealize(&cold, "equivalence", "wave", "photon", out,
                        sizeof(out)) &&
              strcmp(out, "Wave cong photon.") == 0,
          "sym admits inversion (Wave cong photon.)");

    /* unknown family -> no schema -> UNKNOWN */
    CHECK(!SchemaRealize(&cold, "nonexistent", "a", "b", out, sizeof(out)),
          "unknown family -> UNKNOWN");

    /* 5. cold idempotency: reload the same file again -> identical */
    {
        SCHEMA_KB cold2;
        SchemaKBInit(&cold);
        uint32_t n2 = SchemaKBLoad(&cold, "test_schema_kb.txt");
        CHECK(n2 == 2 && cold.schemas[0].num_prov == eq->num_prov &&
                  strcmp(cold.schemas[0].family, eq->family) == 0,
              "cold reload idempotent");
    }

    remove("test_schema_kb.txt");

    printf("\n==================================\n");
    if (g_fail == 0)
    {
        printf("GEN-META C pipeline verificado OK.\n");
        return 0;
    }
    printf("FAILURES: %d\n", g_fail);
    return 1;
}