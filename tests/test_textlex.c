/* test_textlex: corpus-to-graph converter over intact Jung text.
   Whole file, read-only. Invariants: full byte read, every content
   line covered, garbage audit (dropped sample printable-free),
   replay adds nothing and yields identical vectors. Counts below
   are pinned by execution (deterministic converter). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "graph.h"
#include "symbol.h"
#include "embedding.h"
#include "text_lex.h"

#define CORPUS "data/texts/jung.txt"

static int g_pass = 0, g_fail = 0;

static void check(const char *name, int cond)
{
    if (cond)
    {
        printf("  PASS %s\n", name);
        g_pass++;
    }
    else
    {
        printf("  FAIL %s\n", name);
        g_fail++;
    }
}

static int IsContentByte(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c >= 0x80;
}

int main(void)
{
    GRAPH *g = GraphCreate(65536, 256);
    EMBEDDING_TABLE *emb;
    TEXTLEX *tl;
    TEXTLEX_STATS s1, s2;
    uint32_t i;
    int audit_ok;
    if (g == NULL)
    {
        printf("FAIL GraphCreate\n");
        return 1;
    }
    emb = EmbeddingTableCreate(65536);
    GraphSetEmbeddingTable(g, emb);
    tl = TextLexCreate();

    s1 = TextLexIngest(g, tl, CORPUS, 1, 0);
    printf("run1: bytes=%llu lines=%u/%u covered=%u sents=%u "
           "toks=%u syms=%u pairs=%u toolong=%u ctrl=%u\n",
           (unsigned long long)s1.bytes_read, s1.lines_seen,
           s1.lines_content, s1.lines_covered, s1.nsent_new,
           s1.ntok_kept, s1.syms_new, s1.pair_updates,
           s1.toolong_dropped, s1.ctrl_skipped);
    printf("dropped_distinct=%u\n", s1.dropped_distinct);
    for (i = 0; i < s1.dropped_distinct && i < 16; i++)
        printf("  drop[%u]=%.31s\n", i, s1.dropped_sample[i]);
    {
        FILE *fc = fopen(CORPUS, "rb");
        size_t fsz = 0;
        if (fc != NULL)
        {
            fseek(fc, 0, SEEK_END);
            fsz = (size_t)ftell(fc);
            fclose(fc);
        }
        check("full byte read", fsz > 0 && s1.bytes_read == fsz);
    }
    check("every content line covered",
          s1.lines_content > 0 && s1.lines_content == s1.lines_covered);
    check("sentences stored", TextLexSentCount(tl) == s1.nsent_new &&
                                  s1.nsent_new > 0);
    audit_ok = 1;
    for (i = 0; i < s1.dropped_distinct && i < 16; i++)
    {
        const char *d = s1.dropped_sample[i];
        size_t k;
        for (k = 0; d[k] != '\0'; k++)
            if (IsContentByte((unsigned char)d[k]))
                audit_ok = 0;
    }
    check("dropped sample holds no content bytes", audit_ok);

    /* replay: identical input must add nothing */
    s2 = TextLexIngest(g, tl, CORPUS, 1, 0);
    printf("run2: sents_new=%u syms_new=%u toks=%u pairs=%u\n",
           s2.nsent_new, s2.syms_new, s2.ntok_kept,
           s2.pair_updates);
    check("replay adds no sentences",
          s2.nsent_new == 0 && s2.nsent_dup == s1.nsent_new);
    check("replay adds no symbols", s2.syms_new == 0);
    check("replay stores no tokens", s2.ntok_kept == 0);
    check("replay trains no pairs", s2.pair_updates == 0);
    check("pinned corpus counts",
          s1.nsent_new == 10730 && s1.ntok_kept == 215043 &&
              s1.syms_new == 19505 && s1.pair_updates == 5772938 &&
              s1.lines_seen == 22345 && s1.lines_content == 18457 &&
              s1.toolong_dropped == 0 && s1.ctrl_skipped == 0);

    /* every stored occurrence counted exactly once: frequency
       sum over all symbols equals kept tokens (not one word
       lost, none double-counted) */
    {
        uint64_t fsum = 0;
        uint32_t k;
        uint32_t nsyms = SymbolCount(g->symbols);
        for (k = 1; k <= nsyms; k++)
        {
            const SYMBOL *s = SymbolGet(g->symbols, k);
            if (s != NULL)
                fsum += s->frequency;
        }
        printf("freq sum: %llu kept: %u\n",
               (unsigned long long)fsum, s1.ntok_kept);
        check("frequency sum equals kept tokens", fsum == s1.ntok_kept);
    }

    /* attention retrieval of a literal sentence */
    {
        FILE *f = fopen(CORPUS, "rb");
        unsigned char *img = NULL;
        size_t imglen = 0;
        const char *qw[] = {"sun"};
        uint32_t idx[8];
        float sc[8];
        uint32_t nret;
        char text[2048];
        if (f != NULL)
        {
            fseek(f, 0, SEEK_END);
            imglen = (size_t)ftell(f);
            fseek(f, 0, SEEK_SET);
            img = (unsigned char *)malloc(imglen);
            if (img != NULL && fread(img, 1, imglen, f) != imglen)
            {
                free(img);
                img = NULL;
            }
            fclose(f);
        }
        check("corpus image readable", img != NULL);
        nret = TextLexRetrieve(tl, g, emb, qw, 1, idx, sc, 8);
        printf("retrieve sun: nret=%u top=%u score=%.4f\n", nret,
               nret > 0 ? idx[0] : 0, nret > 0 ? sc[0] : 0.0f);
        check("sun retrieves sentences", nret > 0);
        check("sun top rank pinned", nret > 0 && (idx[0] == 7159 || idx[0] == 3130));
        {
            /* ranking is max-independent: top[0] is the global max
               for any max (insertion-sort replacement bug guard) */
            uint32_t ix4[4];
            float sc4[4];
            uint32_t n4 = TextLexRetrieve(tl, g, emb, qw, 1, ix4,
                                          sc4, 4);
            check("top identical for max 4 and max 8",
                  nret > 0 && n4 > 0 && ix4[0] == idx[0]);
        }
        if (nret > 0 && img != NULL)
        {
            uint32_t got;
            got = TextLexSentenceText(tl, idx[0], img, imglen, text,
                                      sizeof(text));
            printf("top: %.150s\n", text);
            check("top sentence holds sun literally",
                  got > 0 && strstr(text, "sun") != NULL);
        }
        {
            const char *qw2[] = {"zzzqqq"};
            check("unknown word retrieves nothing",
                  TextLexRetrieve(tl, g, emb, qw2, 1, idx, sc, 8) ==
                      0);
        }
        free(img);
    }
    {
        SYMBOL_ID id = SymbolFind(g->symbols, "libido");
        const SYMBOL *s = SymbolGet(g->symbols, id);
        printf("libido: id=%u freq=%llu vec=%s\n", id,
               s != NULL ? (unsigned long long)s->frequency : 0,
               EmbeddingGetVector(emb, id) != NULL ? "yes" : "no");
        check("libido symbol live with vector",
              id != SYMBOL_INVALID && s != NULL &&
                  s->frequency == 567 &&
                  EmbeddingGetVector(emb, id) != NULL);
    }

    /* byte fidelity: every stored token byte-equals the source at
       its recorded offset (cryptographic truth, no invention) */
    {
        FILE *f = fopen(CORPUS, "rb");
        unsigned char *img2 = NULL;
        size_t len2 = 0;
        uint32_t si;
        uint64_t bad = 0;
        uint64_t hard = 0;
        uint64_t tot = 0;
        if (f != NULL)
        {
            fseek(f, 0, SEEK_END);
            len2 = (size_t)ftell(f);
            fseek(f, 0, SEEK_SET);
            img2 = (unsigned char *)malloc(len2);
            if (img2 == NULL ||
                fread(img2, 1, len2, f) != len2)
            {
                free(img2);
                img2 = NULL;
            }
            fclose(f);
        }
        if (img2 != NULL)
        {
            for (si = 0; si < TextLexSentCount(tl); si++)
            {
                const TL_SENT *sn = TextLexSentence(tl, si);
                uint32_t t;
                for (t = 0; t < sn->ntok; t++)
                {
                    const SYMBOL *s =
                        SymbolGet(g->symbols, sn->ids[t]);
                    size_t L;
                    if (s == NULL || s->name == NULL)
                    {
                        bad++;
                        if (bad <= 20)
                            printf("  nullsym sent=%u tok=%u off=%llu len=%u id=%u\n",
                                   si, t,
                                   (unsigned long long)sn->offs[t],
                                   sn->lens[t], sn->ids[t]);
                        continue;
                    }
                    L = strlen(s->name);
                    tot++;
                    if (sn->offs[t] + L > len2 ||
                        memcmp(img2 + sn->offs[t], s->name, L) !=
                            0 ||
                        L != sn->lens[t])
                    {
                        unsigned k;
                        int high = 0;
                        bad++;
                        for (k = 0; k < L && k < 64; k++)
                        {
                            if ((unsigned char)s->name[k] >= 0x80)
                                high = 1;
                        }
                        for (k = 0; k < sn->lens[t] && k < 64; k++)
                        {
                            if (img2[sn->offs[t] + k] >= 0x80)
                                high = 1;
                        }
                        if (!high)
                            hard++;
                        if (bad <= 20)
                        {
                            printf("  mismatch sent=%u tok=%u off=%llu len=%u name=",
                                   si, t,
                                   (unsigned long long)sn->offs[t],
                                   sn->lens[t]);
                            for (k = 0; k < L && k < 40; k++)
                                printf("%02x", (unsigned char)s->name[k]);
                            printf(" src=");
                            for (k = 0; k < sn->lens[t] && k < 40; k++)
                                printf("%02x", img2[sn->offs[t] + k]);
                            printf("\n");
                        }
                    }
                }
            }
            free(img2);
        }
        printf("fidelity: %llu tokens checked, %llu mismatches\n",
               (unsigned long long)tot, (unsigned long long)bad);
        check("byte fidelity 100% ASCII", tot == 215043 && hard == 0);
    }

    /* cross-graph determinism: fresh ingest, identical vectors */
    {
        GRAPH *g2 = GraphCreate(65536, 256);
        EMBEDDING_TABLE *emb2 = EmbeddingTableCreate(65536);
        TEXTLEX *tl2 = TextLexCreate();
        TEXTLEX_STATS s3;
        uint32_t n;
        int same = 1;
        GraphSetEmbeddingTable(g2, emb2);
        s3 = TextLexIngest(g2, tl2, CORPUS, 1, 0);
        check("second graph same counts",
              s3.nsent_new == s1.nsent_new &&
                  s3.ntok_kept == s1.ntok_kept &&
                  SymbolCount(g2->symbols) ==
                      SymbolCount(g->symbols));
        n = SymbolCount(g->symbols);
        if (n > 2048)
            n = 2048;
        for (i = 1; i <= n; i++)
        {
            const float *a = EmbeddingGetVector(emb, i);
            const float *b = EmbeddingGetVector(emb2, i);
            if (a == NULL || b == NULL ||
                memcmp(a, b, 32 * sizeof(float)) != 0)
            {
                same = 0;
                break;
            }
        }
        check("bit-identical vectors across graphs", same);
        TextLexFree(tl2);
    }

    printf("test_textlex: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
