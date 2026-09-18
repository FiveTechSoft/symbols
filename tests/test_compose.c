/* test_compose: heterogeneous composition contract (Phase 3).
   Structural world (no semantics claimed): wife(A,B) observed with
   father(B,C) observed licenses taxonomy(A,C) ONLY when every
   premise instance confirms (rate 1.0) at support >= 2. Covers:
   - MetaRuleDiscover: licensed rule (support 2), idempotent; gate
     adversaries (support 1, counter-instance) license 0 rules.
   - TransferCompose: novel conclusion derived from re-presented
     premises; veto when the conclusion is already pair evidence.
   - TransferExplainCompose: the mirror — refuses when the
     conclusion is NOT observed (that is derivation's territory).
   - Fail-closed: unlicensed rule order (father,wife), missing
     premise link, A == C cycle -> 0 (UNKNOWN, never a hypothesis).
   - Persistence: compose(r1,r2,r3,support) survives a cold load
     and still derives with re-presented premises.
   - Chat end-to-end: the compose-why question answers with the r3
     stem DEDUCED from the corpus lexicon (not the question's own
     word); a wrong rule order and the real KJV corpus (0 licensed
     rules, census-measured in tools/bible_compose_census.py) stay
     UNKNOWN. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>
#include "schema.h"
#include "metaschema.h"
#include "learn.h"
#include "transfer.h"
#include "chat.h"

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

/* stdout capture (same pattern as test_bible_chat_lex) */
static const char *g_tmp = "test_compose_out.tmp";
static char g_out[2048];

static void Ask(CHAT *ch, const char *line)
{
    fflush(stdout);
    int saved = _dup(1);
    FILE *cap = fopen(g_tmp, "w");
    if (saved < 0 || cap == NULL)
    {
        printf("FAIL cannot capture stdout\n");
        exit(1);
    }
    fflush(cap);
    _dup2(_fileno(cap), 1);
    ChatHandle(ch, line);
    fflush(stdout);
    _dup2(saved, 1);
    _close(saved);
    fclose(cap);
    FILE *in = fopen(g_tmp, "r");
    size_t n = 0;
    if (in != NULL)
    {
        int c;
        while ((c = fgetc(in)) != EOF && n < sizeof(g_out) - 1)
            g_out[n++] = (char)c;
        fclose(in);
    }
    g_out[n] = '\0';
    for (size_t i = 0; i < n; i++)
        if (g_out[i] == '\n')
            g_out[i] = ' ';
    while (n > 0 && g_out[n - 1] == ' ')
        g_out[--n] = '\0';
    remove(g_tmp);
}

static const char *g_scratch = "test_compose_scratch.tsv";

static void WriteScratch(void)
{
    FILE *f = fopen(g_scratch, "w");
    if (f == NULL)
    {
        printf("FAIL cannot write scratch corpus\n");
        exit(1);
    }
    /* the 7-row structural world: wife(A,B) + father(B,C) with
       taxonomy(A,C) observed for every instance (rate 1.0); the
       (beto,ana) father row adds an A == C instance that the
       enumeration must exclude */
    fputs("ana\tESPOSA_DE\tbeto\n", f);
    fputs("eva\tESPOSA_DE\tflor\n", f);
    fputs("beto\tPADRE_DE\tcaro\n", f);
    fputs("beto\tPADRE_DE\tana\n", f);
    fputs("flor\tPADRE_DE\tdil\n", f);
    fputs("ana\tHIJO_DE\tcaro\n", f);
    fputs("eva\tHIJO_DE\tdil\n", f);
    fclose(f);
}

int main(void)
{
    printf("== COMPOSE (Phase 3): licensed heterogeneous rules ==\n\n");

    /* ---- 1. discovery over the structural world ---- */
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
    checkn("licensed rules", (long long)MetaRuleDiscover(&mk1), 1);
    checkn("idempotent rediscovery", (long long)MetaRuleDiscover(&mk1), 0);
    checkn("rule count 1", (long long)MetaRuleCount(&mk1), 1);
    META_RULE rule;
    checkn("rule (wife,father) licensed",
           (long long)MetaFindRule(&mk1, "wife", "father", &rule), 1);
    checkn("rule r3 is taxonomy",
           strcmp(rule.r3, "taxonomy") == 0, 1);
    checkn("rule support 2", (long long)rule.support, 2);
    checkn("reversed order (father,wife) NOT licensed",
           (long long)MetaFindRule(&mk1, "father", "wife", &rule), 0);

    /* ---- 2. TransferCompose: novel only, veto on observed ---- */
    char out[128];
    checkn("compose vetoed: conclusion already observed",
           (long long)TransferCompose(&kb1, &mk1, "wife", "father", "ana",
                                      "beto", "caro", out, sizeof(out)),
           0);
    checkn("explain accepts: conclusion IS observed fact",
           (long long)TransferExplainCompose(&kb1, &mk1, "wife", "father",
                                             "ana", "beto", "caro", out,
                                             sizeof(out)),
           1);
    checks("explain sentence", out, "Ana isa caro.");

    /* premises re-presented WITHOUT the conclusion: novel derivation.
       The r3 schema itself must exist to build sentences, so the
       taxonomy family is taught via an unrelated exemplar (x isa y)
       — the ana/caro conclusion stays unobserved. */
    SCHEMA_KB kb2;
    LEARNER lr2;
    SchemaKBInit(&kb2);
    LearnerInit(&lr2, &kb2, &mk1);
    Teach(&lr2, "x isa y");
    Teach(&lr2, "ana wife_of beto");
    Teach(&lr2, "beto father_of caro");
    checkn("novel compose from re-presented premises",
           (long long)TransferCompose(&kb2, &mk1, "wife", "father", "ana",
                                      "beto", "caro", out, sizeof(out)),
           1);
    checks("novel compose sentence", out, "Ana isa caro.");
    checkn("explain refuses: conclusion NOT observed",
           (long long)TransferExplainCompose(&kb2, &mk1, "wife", "father",
                                             "ana", "beto", "caro", out,
                                             sizeof(out)),
           0);

    /* ---- 3. fail-closed adversaries ---- */
    checkn("unlicensed rule order -> 0",
           (long long)TransferCompose(&kb1, &mk1, "father", "wife", "beto",
                                      "ana", "beto", out, sizeof(out)),
           0);
    checkn("missing premise link (A,B) -> 0",
           (long long)TransferCompose(&kb2, &mk1, "wife", "father", "eva",
                                      "flor", "dil", out, sizeof(out)),
           0);
    checkn("missing premise link (B,C) -> 0",
           (long long)TransferCompose(&kb2, &mk1, "wife", "father", "ana",
                                      "beto", "dil", out, sizeof(out)),
           0);
    checkn("cycle A == C -> 0",
           (long long)TransferCompose(&kb1, &mk1, "wife", "father", "ana",
                                      "beto", "ana", out, sizeof(out)),
           0);

    /* ---- 4. gate adversaries: thin support and counter-instance ---- */
    SCHEMA_KB kbg;
    META_KB mkg;
    LEARNER lrg;
    SchemaKBInit(&kbg);
    MetaKBInit(&mkg);
    LearnerInit(&lrg, &kbg, &mkg);
    Teach(&lrg, "p wife_of q");
    Teach(&lrg, "q father_of r");
    Teach(&lrg, "p isa r");    /* the only confirming instance */
    Teach(&lrg, "m wife_of n");
    Teach(&lrg, "n father_of x"); /* counter-instance: no isa(m,x) */
    checkn("counter-instance blocks licensing",
           (long long)MetaRuleDiscover(&mkg), 0);
    checkn("gate blocks transfer too",
           (long long)TransferCompose(&kbg, &mkg, "wife", "father", "p",
                                      "q", "r", out, sizeof(out)),
           0);

    /* ---- 5. persistence: the rule survives exemplar loss ---- */
    checkn("save writes 1 compose fact",
           (long long)MetaKBSave(&mk1, "test_compose_mk.txt"), 1);
    META_KB mkc;
    checkn("cold load", (long long)MetaKBLoad(&mkc, "test_compose_mk.txt"),
           1);
    checkn("cold rule count", (long long)MetaRuleCount(&mkc), 1);
    checkn("cold rule lookup",
           (long long)MetaFindRule(&mkc, "wife", "father", &rule), 1);
    checkn("cold support survives", (long long)rule.support, 2);
    SCHEMA_KB kbc;
    LEARNER lrc;
    SchemaKBInit(&kbc);
    LearnerInit(&lrc, &kbc, &mkc);
    Teach(&lrc, "x isa y");
    Teach(&lrc, "ana wife_of beto");
    Teach(&lrc, "beto father_of caro");
    checkn("cold compose with re-presented premises",
           (long long)TransferCompose(&kbc, &mkc, "wife", "father", "ana",
                                      "beto", "caro", out, sizeof(out)),
           1);
    checks("cold compose sentence", out, "Ana isa caro.");
    remove("test_compose_mk.txt");

    /* ---- 6. chat end-to-end over the scratch world ---- */
    WriteScratch();
    CHAT ch;
    ChatInit(&ch, g_scratch);
    remove(g_scratch);
    checkn("chat licensed rules", (long long)MetaRuleCount(&ch.mk), 1);
    Ask(&ch, "por que ana es esposa de beto siendo beto padre de caro");
    checks("compose-why answer with r3 stem", g_out,
           "Lo se porque Ana es esposa de Beto, y Beto es padre de Caro. "
           "Por tanto Ana es hijo de Caro.");
    Ask(&ch, "por que ana es hijo de caro siendo beto padre de caro");
    checks("wrong rule order stays UNKNOWN", g_out,
           "No tengo constancia de una relacion entre Ana y Caro por "
           "ahi.");

    /* ---- 7. anti-FP: the real corpus licenses 0 rules ---- */
    CHAT ch2;
    ChatInit(&ch2, "data/bible/bible_relations.tsv");
    checkn("real corpus licensed rules", (long long)MetaRuleCount(&ch2.mk),
           0);
    Ask(&ch2,
        "por que abijah es hijo de adaiah siendo adaiah rey de jehu");
    checks("real corpus compose stays UNKNOWN", g_out,
           "No tengo constancia de una relacion entre Abijah y Adaiah "
           "por ahi.");

    printf("\n==================================\n");
    if (g_fail == 0)
    {
        printf("COMPOSE contract OK (%d checks).\n", g_pass);
        return 0;
    }
    printf("FAILURES: %d (%d passed)\n", g_fail, g_pass);
    return 1;
}