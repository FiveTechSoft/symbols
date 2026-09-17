/* test_bible_chat: end-to-end tests for the symbolic conversational
   engine (src/bible_chat.c). The parser and NLG print to stdout; the
   testable core is: ingest contract, direct/2-hop frozen-layer
   queries the chat uses, and the helpers' directionality (pair
   (S,O) = "S isa O" = S is child of O). Tiny scratch corpus, never
   the tracked TSV, so expectations are exact. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "schema.h"
#include "metaschema.h"
#include "learn.h"
#include "transfer.h"
#include "bible_chat.h"

static int g_pass = 0, g_fail = 0;

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

static const char *g_scratch = "test_bible_chat_scratch.tsv";

static void WriteScratch(void)
{
    FILE *f = fopen(g_scratch, "w");
    if (f == NULL)
    {
        printf("FAIL cannot write scratch corpus\n");
        exit(1);
    }
    /* (S,O) = S isa O = S child of O */
    fputs("jaroah\tHIJO_DE\tgilead\n", f);
    fputs("gilead\tHIJO_DE\tmachir\n", f);
    fputs("machir\tHIJO_DE\tmanasseh\n", f);
    fputs("james\tHIJO_DE\tzebedee\n", f);
    fputs("james\tREY_DE\tisrael\n", f); /* now learned (5 rels) */
    fputs("zebedee\tHIJO_DE\tzebedee\n", f); /* self-loop: skipped */
    fclose(f);
}

int main(void)
{
    WriteScratch();
    CHAT ch;
    ChatInit(&ch, g_scratch);
    remove(g_scratch);

    /* ---- 1. ingest contract ----
       5 pairs learned (REY_DE maps to reigns); self-loop filtered */
    check("pairs 5 (6 rows - self-loop)",
          (int)ch.kb.num_pairs, 5);
    check("vocab 7 (5 pairs, israel shared)",
          (int)ch.kb.num_vocab, 7);
    check("deduced kw index (hijo+rey stems)",
          (int)ch.num_kws, 2);
    check("meta transitive licensed",
          (int)MetaHasProperty(&ch.mk, "taxonomy", META_PROP_TRANSITIVE),
          1);

    /* ---- 2. direct (1-hop) queries ---- */
    {
        char out[128];
        check("direct jaroah->gilead",
              (int)TransferDerive(&ch.kb, &ch.mk, "taxonomy", "jaroah",
                                  "gilead", out, sizeof(out)),
              1);
        /* 1-hop only: jaroah->machir needs the chain, not TransferDerive */
        check("direct jaroah->machir refused (2-hop)",
              (int)TransferDerive(&ch.kb, &ch.mk, "taxonomy", "jaroah",
                                  "machir", out, sizeof(out)),
              0);
        check("direct unknown pair refused",
              (int)TransferDerive(&ch.kb, &ch.mk, "taxonomy", "zebedee",
                                  "machir", out, sizeof(out)),
              0);
        check("direct sentence exact",
              (int)(strcmp(out, "jaroah isa gilead.") == 0 ||
                    strcmp(out, "Jaroah isa gilead.") == 0),
              1);
    }

    /* ---- 3. 2-hop chain with proof middle ---- */
    {
        char out[128], mid[32];
        check("chain jaroah->machir",
              (int)TransferDeriveChain(&ch.kb, &ch.mk, "taxonomy", "jaroah",
                                       "machir", out, sizeof(out)),
              1);
        check("explain chain gives middle gilead",
              (int)TransferExplainChain(&ch.kb, &ch.mk, "taxonomy",
                                        "jaroah", "machir", out, sizeof(out),
                                        mid, sizeof(mid)),
              1);
        check("middle is gilead", strcmp(mid, "gilead"), 0);
        check("chain jaroah->zebedee refused (no path)",
              (int)TransferDeriveChain(&ch.kb, &ch.mk, "taxonomy", "jaroah",
                                       "zebedee", out, sizeof(out)),
              0);
    }

    /* ---- 4. helper directionality (the (S,O)=S-child-of-O bug) ---- */
    /* The helpers are static in bible_chat.c; we replicate their
       contract over the same KB to pin the semantics. */
    {
        /* ChatParents(child): parents of X = objects of pairs (X,*) */
        const char *parents_of_jaroah = NULL;
        for (uint32_t i = 0; i < ch.kb.num_pairs; i++)
            if (strcmp(ch.kb.pairs[i].subject, "jaroah") == 0)
                parents_of_jaroah = ch.kb.pairs[i].object;
        check("parents(jaroah) == gilead (subject scan)",
              parents_of_jaroah != NULL &&
                  strcmp(parents_of_jaroah, "gilead"),
              0);

        /* ChatChildren(parent): children of X = subjects of pairs
           (*,X); children(machir) has exactly 1: gilead */
        uint32_t kids = 0, kids_jaroah = 0;
        for (uint32_t i = 0; i < ch.kb.num_pairs; i++)
        {
            if (strcmp(ch.kb.pairs[i].object, "machir") == 0)
                kids++;
            if (strcmp(ch.kb.pairs[i].object, "jaroah") == 0)
                kids_jaroah++;
        }
        check("children(machir) == 1 (gilead)", (int)kids, 1);
        check("children(jaroah) == 0 (leaf)", (int)kids_jaroah, 0);
    }

    /* ---- 5. WHY / ambiguity data shape ----
       james has 1 taxonomy parent here (zebedee); in the real
       corpus james has 2 (alphaeus+zebedee) -> ambiguity branch.
       Family-scoped: james also holds a reigns pair (israel),
       which must NOT count as parent evidence. */
    {
        uint32_t np = 0;
        for (uint32_t i = 0; i < ch.kb.num_pairs; i++)
            if (strcmp(ch.kb.pairs[i].subject, "james") == 0 &&
                strcmp(ch.kb.pairs[i].family, "taxonomy") == 0)
                np++;
        check("parents(james) == 1 (unambiguous in scratch)", (int)np, 1);
    }

    printf("bible chat e2e: %d/%d\n", g_pass, g_pass + g_fail);
    return g_fail == 0 ? 0 : 1;
}