/* test_learn: full learning cycle (Phase 4) — INPUT -> observations
   -> LEARN -> SCHEMA -> METASCHEMA -> COMPOSITION -> TRANSFER ->
   QUERY -> ANSWER / honest UNKNOWN, with ZERO word semantics: the
   connective -> family table is the only lexicon, consulted, never
   asserted. Covers:
   1. examples feed: mixed exemplars teach schemas (learn.c is the
      only text parser); unknown connectives are REJECTED, not
      guessed.
   2. structure: exemplar counts + rejection of malformed lines.
   3. schema: discovered families with template-registry order and
      connectives; provenance = exemplar ids.
   4. meta: SYMMETRIC/TRANSITIVE from two-direction / link evidence
      only (single direction proves nothing: fail-closed).
   5. composition: rule wife o father => taxonomy licensed by the
      rate-1.0 / support>=2 gate, from exemplar observations.
   6. transfer: plain / swapped / chain / compose derivation paths
      with fail-closed adversaries.
   7. query on entities NEVER seen in the exemplars (novel tokens
      presented into a fresh world): learning transfers because the
      structure is entity-agnostic; foo/bar arbitrary symbols work
      identically (HARDCODING=0 probe: no word knowledge involved).
   8. answer: realized sentence capitalizes only the subject.
   9. UNKNOWN without license: missing link, unlicensed rule order,
      cycle, cold chain, untaught family — never a hypothesis. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "schema.h"
#include "metaschema.h"
#include "learn.h"
#include "transfer.h"

static int g_pass = 0, g_fail = 0;

static void checkn(const char *name, long long got, long long want)
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

static void checks(const char *name, const char *got, const char *want)
{
    if (got != NULL && strcmp(got, want) == 0)
    {
        printf("  PASS %s\n", name);
        g_pass++;
    }
    else
    {
        printf("  FAIL %s (got %.90s want %.60s)\n", name,
               got ? got : "(null)", want);
        g_fail++;
    }
}

static void Teach(LEARNER *lr, const char *line)
{
    if (!LearnerLearnLine(lr, line))
    {
        printf("FAIL cannot learn: %s\n", line);
        exit(1);
    }
}

static void TeachCheck(LEARNER *lr, const char *line, int expect_ok,
                       const char *name)
{
    int ok = LearnerLearnLine(lr, line);
    checkn(name, ok, expect_ok);
}

int main(void)
{
    printf("== LEARN (Phase 4): full learning cycle ==\n\n");

    /* ---- 1-2. examples + structure: mixed exemplar feed ---- */
    SCHEMA_KB kb1;
    META_KB mk1;
    LEARNER lr1;
    SchemaKBInit(&kb1);
    MetaKBInit(&mk1);
    LearnerInit(&lr1, &kb1, &mk1);

    Teach(&lr1, "ana wife_of beto");
    Teach(&lr1, "eva wife_of flor");
    Teach(&lr1, "beto father_of caro");
    Teach(&lr1, "beto father_of ana");
    Teach(&lr1, "flor father_of dil");
    Teach(&lr1, "ana isa caro");
    Teach(&lr1, "eva isa dil");
    /* symmetric evidence for the wife family (two directions) */
    Teach(&lr1, "beto wife_of ana");
    /* transitive evidence for the isa family (links ana>caro>dil) */
    Teach(&lr1, "caro isa dil");

    /* malformed / unknown-connective lines are REJECTED, never
       guessed (structure is the parser's contract) */
    TeachCheck(&lr1, "ana loves beto", 0, "unknown connective rejected");
    TeachCheck(&lr1, "ana wife_of", 0, "two-token line rejected");
    TeachCheck(&lr1, "", 0, "empty line rejected");
    checkn("observations accumulated", (long long)lr1.next_obs, 9);

    /* ---- 3. schema: families + template registry ---- */
    const RELATIONAL_SCHEMA *sw = SchemaFindFamily(&kb1, "wife");
    checkn("wife schema exists", sw != NULL, 1);
    checkn("wife order CHAIN (bible family: direction by evidence only)",
           sw != NULL && sw->order == SCHEMA_ORDER_CHAIN, 1);
    const RELATIONAL_SCHEMA *st = SchemaFindFamily(&kb1, "taxonomy");
    checkn("taxonomy schema exists", st != NULL, 1);
    checkn("taxonomy connective isa",
           st != NULL && st->num_connectives == 1 &&
               strcmp(st->connectives[0], "isa") == 0,
           1);
    checkn("provenance records exemplar ids",
           sw != NULL && sw->num_prov >= 2 &&
               strcmp(sw->prov[0], "obs-0") == 0,
           1);
    checkn("untaught family absent",
           (long long)(SchemaFindFamily(&kb1, "nonsense") == NULL), 1);

    /* ---- 4. meta: properties only from real evidence ---- */
    checkn("meta discovery new props", (long long)LearnerDiscoverMeta(&lr1),
           2); /* wife SYMMETRIC + taxonomy TRANSITIVE */
    checkn("meta discovery idempotent",
           (long long)LearnerDiscoverMeta(&lr1), 0);
    checkn("wife symmetric licensed",
           (long long)MetaHasProperty(&mk1, "wife", META_PROP_SYMMETRIC), 1);
    checkn("taxonomy transitive licensed",
           (long long)MetaHasProperty(&mk1, "taxonomy",
                                      META_PROP_TRANSITIVE),
           1);
    checkn("father NOT symmetric (single direction only)",
           (long long)MetaHasProperty(&mk1, "father", META_PROP_SYMMETRIC),
           0);
    checkn("wife NOT transitive (no links observed)",
           (long long)MetaHasProperty(&mk1, "wife", META_PROP_TRANSITIVE),
           0);

    /* ---- 5. composition: licensed rule from observations ---- */
    checkn("compose rules licensed", (long long)MetaRuleDiscover(&mk1), 1);
    checkn("compose idempotent", (long long)MetaRuleDiscover(&mk1), 0);
    checkn("rule count 1", (long long)MetaRuleCount(&mk1), 1);
    META_RULE rule;
    checkn("rule (wife,father)=>taxonomy",
           (long long)MetaFindRule(&mk1, "wife", "father", &rule), 1);
    checkn("rule support 2", (long long)rule.support, 2);
    checkn("reversed order NOT licensed",
           (long long)MetaFindRule(&mk1, "father", "wife", &rule), 0);

    /* ---- 6. transfer: all four derivation paths ---- */
    char out[128];
    /* plain: observed direction */
    checkn("plain derive",
           (long long)TransferDerive(&kb1, &mk1, "wife", "ana", "beto", out,
                                     sizeof(out)),
           1);
    checks("plain sentence", out, "Ana wife_of beto.");
    /* swapped: CHAIN families stay fail-closed even under SYMMETRIC
       (no role evidence: the swap cannot be licensed) */
    checkn("swapped refuses CHAIN family (fail-closed)",
           (long long)TransferDeriveSwapped(&kb1, &mk1, "wife", "beto",
                                            "ana", out, sizeof(out)),
           0);
    /* chain: taxonomy links ana->caro->dil observed */
    checkn("chain derive",
           (long long)TransferDeriveChain(&kb1, &mk1, "taxonomy", "ana",
                                          "dil", out, sizeof(out)),
           1);
    checks("chain sentence", out, "Ana isa dil.");
    checkn("chain refuses direct evidence",
           (long long)TransferDeriveChain(&kb1, &mk1, "taxonomy", "ana",
                                          "caro", out, sizeof(out)),
           0);
    /* compose: novel conclusion from re-presented premises */
    checkn("compose vetoed: conclusion observed",
           (long long)TransferCompose(&kb1, &mk1, "wife", "father", "ana",
                                      "beto", "caro", out, sizeof(out)),
           0);
    SCHEMA_KB kb2;
    LEARNER lr2;
    SchemaKBInit(&kb2);
    LearnerInit(&lr2, &kb2, &mk1); /* same meta rules */
    Teach(&lr2, "x isa y");        /* r3 schema exists in this world */
    Teach(&lr2, "ana wife_of beto");
    Teach(&lr2, "beto father_of caro");
    checkn("compose novel conclusion",
           (long long)TransferCompose(&kb2, &mk1, "wife", "father", "ana",
                                      "beto", "caro", out, sizeof(out)),
           1);
    checks("compose sentence", out, "Ana isa caro.");

    /* ---- 7-9. query novel entities in a fresh world; foo/bar
       arbitrary symbols; UNKNOWN without license ---- */
    SCHEMA_KB kb3;
    META_KB mk3;
    LEARNER lr3;
    SchemaKBInit(&kb3);
    MetaKBInit(&mk3);
    LearnerInit(&lr3, &kb3, &mk3);
    /* teach ONLY structure with the arbitrary symbols foo/bar:
       if any word semantics leaked into the pipeline, this world
       could not answer anything. World is consistent by design:
       every wife o father premise instance confirms its taxonomy
       conclusion (rate 1.0, support 2). */
    Teach(&lr3, "foo wife_of bar");
    Teach(&lr3, "bar wife_of foo");
    Teach(&lr3, "bar father_of baz");
    Teach(&lr3, "foo father_of qux");
    Teach(&lr3, "baz father_of qux");
    Teach(&lr3, "bar isa qux");    /* confirms bar w foo & foo f qux */
    Teach(&lr3, "baz isa qux");    /* confirms foo f baz instance */
    Teach(&lr3, "baz isa waldo");
    /* the conclusion of the (foo,bar,baz) instance enters as a META
       OBSERVATION ONLY (domain-A evidence licenses the rule; the
       domain-B pair stays un-presented so the compose conclusion
       remains NOVEL for TransferCompose) */
    checkn("meta-only conclusion observation",
           (long long)MetaObserve(&mk3, "taxonomy", "foo", "baz",
                                  "obs-meta-1"),
           1);
    /* two symmetric rules license (the world is consistent in both
       directions: wife o father => taxonomy AND wife o taxonomy =>
       father, each support 2, rate 1.0) */
    checkn("foo-world compose rules", (long long)MetaRuleDiscover(&mk3), 2);
    /* wife SYMMETRIC (both directions) + taxonomy TRANSITIVE
       (foo>baz>waldo chain) + father TRANSITIVE (bar>baz>qux) */
    checkn("foo-world meta props", (long long)LearnerDiscoverMeta(&lr3), 3);

    /* plain answer with novel entities (never exemplars) */
    LearnerPresentPair(&lr3, "wife", "waldo", "foo");
    checkn("novel entity plain derive",
           (long long)TransferDerive(&kb3, &mk3, "wife", "waldo", "foo", out,
                                     sizeof(out)),
           1);
    checks("novel entity sentence", out, "Waldo wife_of foo.");
    /* composition transfers to foo/bar world premises; the r3
       schema must exist in THIS kb to build sentences (taught via
       the bar/baz exemplars above) */
    checkn("foo-world compose novel",
           (long long)TransferCompose(&kb3, &mk3, "wife", "father", "foo",
                                      "bar", "baz", out, sizeof(out)),
           1);
    checks("foo-world compose sentence", out, "Foo isa baz.");
    /* chain transfer: bar>qux is presented but no qux>* link exists:
       foo-world keeps chain in the honest-UNKNOWN zone */
    checkn("foo-world chain without middle refuses",
           (long long)TransferDeriveChain(&kb3, &mk3, "taxonomy", "bar",
                                          "waldo", out, sizeof(out)),
           0);
    checkn("UNKNOWN: cold chain without links",
           (long long)TransferDeriveChain(&kb3, &mk3, "taxonomy", "foo",
                                          "waldo", out, sizeof(out)),
           0);
    /* swapped: needs DEP_FIRST/OPER_FIRST order + declared roles +
       SYMMETRIC meta; CHAIN families (bible + isa) never license
       it. The swap is exercised on a proportional world whose
       reverse direction is META-OBSERVED but not presented as pair
       evidence (otherwise the plain path owns it and vetoes). */
    SCHEMA_KB kb4;
    META_KB mk4;
    LEARNER lr4;
    SchemaKBInit(&kb4);
    MetaKBInit(&mk4);
    LearnerInit(&lr4, &kb4, &mk4);
    Teach(&lr4, "foo proportional bar");
    Teach(&lr4, "baz proportional qux");
    /* reverse direction seen at the meta layer only (domain-A
       evidence): no pair presentation in this world */
    checkn("meta reverse observation accepted",
           (long long)MetaObserve(&mk4, "proportionality", "bar", "foo",
                                  "obs-meta-0"),
           1);
    SchemaDeclareRole(&kb4, "foo", 1, 0, 0, 0); /* dependent */
    SchemaDeclareRole(&kb4, "bar", 0, 1, 0, 0); /* independent */
    checkn("proportional meta symmetric",
           (long long)LearnerDiscoverMeta(&lr4), 1);
    checkn("proportional swapped licensed",
           (long long)TransferDeriveSwapped(&kb4, &mk4, "proportionality",
                                            "bar", "foo", out, sizeof(out)),
           1);
    checks("proportional swapped sentence", out, "Bar proportional foo.");
    checkn("proportional swapped WITHOUT roles refuses",
           (long long)TransferDeriveSwapped(&kb4, &mk4, "proportionality",
                                            "qux", "baz", out, sizeof(out)),
           0);

    /* ---- UNKNOWN without license (fail-closed adversaries) ---- */
    /* missing premise link: (baz,wife,waldo) does not exist */
    checkn("UNKNOWN: missing wife link",
           (long long)TransferCompose(&kb3, &mk3, "wife", "father", "baz",
                                      "waldo", "foo", out, sizeof(out)),
           0);
    /* unlicensed rule order (father,wife) */
    checkn("UNKNOWN: unlicensed rule order",
           (long long)TransferCompose(&kb3, &mk3, "father", "wife", "bar",
                                      "foo", "baz", out, sizeof(out)),
           0);
    /* cycle: conclusion ends coincide */
    checkn("UNKNOWN: cycle A == C",
           (long long)TransferCompose(&kb3, &mk3, "wife", "father", "bar",
                                      "foo", "bar", out, sizeof(out)),
           0);
    /* untaught family: query path must refuse, not guess */
    checkn("UNKNOWN: untaught family",
           (long long)TransferDerive(&kb3, &mk3, "nonsense", "foo", "bar",
                                     out, sizeof(out)),
           0);
    /* vocabulary gate: token never presented in this world */
    checkn("UNKNOWN: token outside vocabulary",
           (long long)TransferDerive(&kb3, &mk3, "wife", "zebu", "foo", out,
                                     sizeof(out)),
           0);

    /* ---- consultable-table contract (HARDCODING=0) ---- */
    checkn("table: wife_of is a connective",
           (long long)LearnerIsConnective("wife_of"), 1);
    checkn("table: loves is NOT a connective",
           (long long)LearnerIsConnective("loves"), 0);
    checks("table: wife_of maps to family wife",
           LearnerConnFamily("wife_of"), "wife");
    checkn("table: unknown maps to NULL",
           (long long)(LearnerConnFamily("loves") == NULL), 1);

    printf("\n==================================\n");
    if (g_fail == 0)
    {
        printf("LEARN cycle OK (%d checks).\n", g_pass);
        return 0;
    }
    printf("FAILURES: %d (%d passed)\n", g_fail, g_pass);
    return 1;
}