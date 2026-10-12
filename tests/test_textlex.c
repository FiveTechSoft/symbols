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
#include "tl_sha256.h"

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

static int WriteCitationFixture(const char *path, const unsigned char *bytes, size_t n)
{
    FILE *f = fopen(path, "wb");
    int ok = f && fwrite(bytes, 1, n, f) == n;
    if (f) fclose(f);
    return ok;
}
static void CheckShaVector(const unsigned char *b, size_t n, const char *gold)
{
    unsigned char hash[32]; char hex[65]; unsigned i;
    TLSha256(b, n, hash);
    for (i = 0; i < 32; i++) sprintf(hex + i * 2, "%02x", hash[i]);
    check("citation SHA-256 known answer", strcmp(hex, gold) == 0);
}
static void CheckCitationFixture(const unsigned char *bytes, size_t len)
{
    const char *path = "textlex_citation_regression.txt";
    GRAPH *g = GraphCreate(512, 256);
    EMBEDDING_TABLE *e = EmbeddingTableCreate(512);
    TEXTLEX *t = TextLexCreate();
    TEXTLEX_STATS a, b;
    uint32_t i;
    GraphSetEmbeddingTable(g, e); TextLexPosReset();
    check("citation fixture written", WriteCitationFixture(path, bytes, len));
    check("citation verified ingest", TextLexIngestVerified(g, t, path, 1, 0, &a) == TL_INGEST_OK);
    check("citation image intact", t->imagelen == len && memcmp(t->image, bytes, len) == 0);
    check("citation unchanged replay", TextLexIngestVerified(g, t, path, 1, 0, &b) == TL_INGEST_OK && b.nsent_new == 0 && b.syms_new == 0 && b.ntok_kept == 0 && b.pair_updates == 0);
    check("citation fixture has sentences", t->nsent > 0);
    for (i = 0; i < t->nsent; i++)
    {
        TL_CITATION c, invalid; size_t n = 0, needed = 0;
        unsigned char quote[4096], sentinel[4096], digest[32];
        check("citation create", TextLexCitation(t, i, &c) == TL_CITE_OK);
        TLSha256(bytes, len, digest);
        check("citation hash/length bind image", c.version == 1 && c.source_bytes == len && memcmp(c.source_sha256, digest, 32) == 0);
        check("citation half-open gold bounds", c.first == t->sents[i].offs[0] && c.end == t->sents[i].offs[t->sents[i].ntok-1] + t->sents[i].lens[t->sents[i].ntok-1]);
        check("citation size query", TextLexCitationCopy(t, &c, NULL, 0, &n) == TL_CITE_OK && n == c.end - c.first);
        check("citation test buffer bound", n < sizeof(quote));
        if (n >= sizeof(quote)) continue;
        memset(quote, 0xa5, sizeof(quote)); memcpy(sentinel, quote, sizeof(quote));
        check("citation short buffer no write", TextLexCitationCopy(t, &c, quote, n-1, &needed) == TL_CITE_BUFFER_SMALL && needed == n && memcmp(quote, sentinel, sizeof(quote)) == 0);
        check("citation exact fit byte copy", TextLexCitationCopy(t, &c, quote, n, &needed) == TL_CITE_OK && needed == n && memcmp(quote, bytes+c.first, n) == 0 && quote[n] == 0xa5);
        invalid = c; invalid.source_sha256[0] ^= 1;
        check("citation wrong hash rejected", TextLexCitationCopy(t, &invalid, quote, sizeof(quote), &needed) == TL_CITE_SOURCE_MISMATCH && needed == 0);
        invalid = c; invalid.source_bytes++;
        check("citation wrong length rejected", TextLexCitationCopy(t, &invalid, quote, sizeof(quote), &needed) == TL_CITE_SOURCE_MISMATCH);
        invalid = c; invalid.first = UINT64_MAX;
        check("citation overflow span rejected", TextLexCitationCopy(t, &invalid, quote, sizeof(quote), &needed) == TL_CITE_INVALID);
        invalid = c; invalid.version = 2;
        check("citation wrong version rejected", TextLexCitationCopy(t, &invalid, quote, sizeof(quote), &needed) == TL_CITE_INVALID);
        {
            uint64_t off = t->sents[i].offs[0];
            t->sents[i].offs[0] = UINT64_MAX;
            memset(&invalid, 0xa5, sizeof(invalid));
            check("citation corrupt token rejected and output zeroed", TextLexCitation(t, i, &invalid) == TL_CITE_INVALID && invalid.version == 0 && invalid.end == 0);
            t->sents[i].offs[0] = off;
        }
    }
    {
        TL_CITATION receipt; size_t n;
        check("citation receipt before unlink", TextLexCitation(t, 0, &receipt) == TL_CITE_OK);
        check("citation source file unlink", remove(path) == 0);
        check("citation resolves owned snapshot after unlink", TextLexCitationCopy(t, &receipt, NULL, 0, &n) == TL_CITE_OK);
        TextLexClear(t);
        check("citation clear invalidates resolver", TextLexCitationCopy(t, &receipt, NULL, 0, &n) == TL_CITE_INVALID);
        WriteCitationFixture(path, (const unsigned char *)"Different source.", 17);
        TextLexIngestVerified(g, t, path, 1, 0, &a);
        check("citation reload mismatch rejected", TextLexCitationCopy(t, &receipt, NULL, 0, &n) == TL_CITE_SOURCE_MISMATCH);
        remove(path);
    }
    TextLexFree(t); EmbeddingTableDestroy(e); GraphDestroy(g);
}
static void CheckCitationMismatch(void)
{
    const char *path = "textlex_citation_mutable.txt";
    const char *original = "Alpha old.\n";
    const char *changed[] = {"Alpha new.\n", "Beta.\n", "Alpha  old.\n"};
    unsigned i;
    for (i = 0; i < 3; i++)
    {
        GRAPH *g = GraphCreate(512, 256); EMBEDDING_TABLE *e = EmbeddingTableCreate(512);
        TEXTLEX *t = TextLexCreate(); TEXTLEX_STATS a, b;
        TL_CITATION c; const char *q[] = {"Alpha"}; uint32_t ix[8], jx[8], n, m, syms, nsent; float sc[8], tc[8];
        GraphSetEmbeddingTable(g, e); TextLexPosReset(); WriteCitationFixture(path, (const unsigned char *)original, strlen(original));
        TextLexIngestVerified(g, t, path, 1, 0, &a);
        n = TextLexRetrieve(t, g, e, q, 1, ix, sc, 8); syms = SymbolCount(g->symbols); nsent = t->nsent;
        TextLexCitation(t, 0, &c);
        WriteCitationFixture(path, (const unsigned char *)changed[i], strlen(changed[i]));
        check("citation changed image rejected", TextLexIngestVerified(g, t, path, 1, 0, &b) == TL_INGEST_SOURCE_MISMATCH);
        check("citation mismatch zero stats", b.bytes_read == 0 && b.nsent_new == 0 && b.syms_new == 0 && b.pair_updates == 0);
        m = TextLexRetrieve(t, g, e, q, 1, jx, tc, 8);
        check("citation mismatch no index/rank/symbol mutation", n == m && nsent == t->nsent && syms == SymbolCount(g->symbols) && memcmp(ix,jx,n*sizeof(*ix)) == 0 && memcmp(sc,tc,n*sizeof(*sc)) == 0);
        b = TextLexIngest(g, t, path, 1, 0);
        check("citation legacy changed image fail closed", b.bytes_read == 0 && b.nsent_new == 0);
        { size_t needed; check("citation old snapshot still resolvable", TextLexCitationCopy(t, &c, NULL, 0, &needed) == TL_CITE_OK); }
        remove(path); TextLexFree(t); EmbeddingTableDestroy(e); GraphDestroy(g);
    }
    {
        GRAPH *g = GraphCreate(512,256); EMBEDDING_TABLE *e = EmbeddingTableCreate(512); TEXTLEX *t = TextLexCreate(); TEXTLEX_STATS a;
        const unsigned char *bytes = (const unsigned char *)"Alpha first.\nBeta second.\n";
        GraphSetEmbeddingTable(g,e);TextLexPosReset();WriteCitationFixture(path,bytes,strlen((const char*)bytes));
        check("citation range ingest first", TextLexIngestVerified(g,t,path,1,1,&a)==TL_INGEST_OK && t->nsent==1);
        check("citation range same image second", TextLexIngestVerified(g,t,path,2,2,&a)==TL_INGEST_OK && t->nsent==2);
        check("citation range replay", TextLexIngestVerified(g,t,path,1,0,&a)==TL_INGEST_OK && a.nsent_new==0);
        remove(path);TextLexFree(t);EmbeddingTableDestroy(e);GraphDestroy(g);
    }
}
static void CheckCitations(void)
{
    unsigned char binary[256]; unsigned i;
    for (i=0;i<256;i++) binary[i]=(unsigned char)i;
    CheckShaVector((const unsigned char *)"",0,"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CheckShaVector((const unsigned char *)"abc",3,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CheckShaVector((const unsigned char *)"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",56,"248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    CheckShaVector(binary,256,"40aff2e9d2d8922e47afd4648e6967497158785fbd1da870e7110266bf944880");
    {
        static const unsigned char a[]="\xc3\x81rbol  caf\xc3\xa9\tlim.\r\n";
        static const unsigned char b[]="Alpha\n\tBeta  Gamma.";
        static const unsigned char c[]="Formula \\lim_{h\\rightarrow 0}\\frac{x^2}{h}.";
        static const unsigned char d[]="Echo same.\nEcho same.";
        static const unsigned char e[]={'A','l','p','h','a',0,'B','e','t','a','.'};
        static const unsigned char f[]={'A','l','p','h','a',' ',0xff,'B','e','t','a','.'};
        CheckCitationFixture(a,sizeof(a)-1);CheckCitationFixture(b,sizeof(b)-1);CheckCitationFixture(c,sizeof(c)-1);
        CheckCitationFixture(d,sizeof(d)-1);CheckCitationFixture(e,sizeof(e));CheckCitationFixture(f,sizeof(f));
    }
    CheckCitationMismatch();
}

static int IsContentByte(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c >= 0x80;
}

/* Explicit evidence gold: sentence boundaries remain legacy; label context is
   separately optional. Unsupported forms and year ambiguity are not solved. */
static void CheckRetrievalSpanFixture(const char *bytes, const char *query,
                                     const char *content, const char *label)
{
    const char *path = "textlex_retrieval_span_regression.txt";
    FILE *f = fopen(path, "wb");
    int written = f && fwrite(bytes, 1, strlen(bytes), f) == strlen(bytes);
    if (f) fclose(f);
    check("retrieval-span fixture written", written);
    if (!written) return;
    GRAPH *g = GraphCreate(512, 256);
    EMBEDDING_TABLE *e = EmbeddingTableCreate(512);
    TEXTLEX *t = TextLexCreate();
    GraphSetEmbeddingTable(g, e); TextLexPosReset();
    TEXTLEX_STATS a = TextLexIngest(g, t, path, 1, 0);
    const char *q[] = {query}; uint32_t ix[8], ix2[8]; float sc[8], sc2[8];
    uint32_t n = TextLexRetrieve(t, g, e, q, 1, ix, sc, 8);
    check("retrieval-span query source hit", n > 0);
    if (n)
    {
        TL_RETRIEVAL_SPANS spans;
        const char *source_content = strstr(bytes, content);
        const char *source_label = label ? strstr(bytes, label) : NULL;
        int valid = TextLexRetrievalSpans(t, ix[0], &spans);
        check("retrieval-span content gold", valid && source_content &&
              spans.content_first == (uint64_t)(source_content - bytes) &&
              spans.content_end - spans.content_first == strlen(content) &&
              memcmp(t->image + spans.content_first, content, strlen(content)) == 0);
        check("retrieval-span label gold", valid &&
              spans.has_label == (label != NULL) &&
              (label ? source_label && spans.label_first == (uint64_t)(source_label - bytes) &&
               spans.label_end - spans.label_first == strlen(label) &&
               memcmp(t->image + spans.label_first, label, strlen(label)) == 0 :
               spans.label_first == 0 && spans.label_end == 0));
        check("retrieval-span combined gold", valid && spans.combined_end == spans.content_end &&
              spans.combined_first == (label ? spans.label_first : spans.content_first) &&
              spans.combined_end <= t->imagelen);
        uint32_t n2 = TextLexRetrieve(t, g, e, q, 1, ix2, sc2, 8);
        check("retrieval-span call leaves ranking exact", n == n2 &&
              memcmp(ix, ix2, n * sizeof(ix[0])) == 0 &&
              memcmp(sc, sc2, n * sizeof(sc[0])) == 0);
    }
    TEXTLEX_STATS b = TextLexIngest(g, t, path, 1, 0);
    check("retrieval-span replay idempotent", b.nsent_new == 0 && b.syms_new == 0 &&
          b.ntok_kept == 0 && b.pair_updates == 0);
    check("retrieval-span image exact and lines covered", t->imagelen == strlen(bytes) &&
          memcmp(t->image, bytes, strlen(bytes)) == 0 && a.lines_content == a.lines_covered);
    const char *absent[] = {"zzyyzzspanabsent20261012"};
    check("retrieval-span absent zero", TextLexRetrieve(t, g, e, absent, 1, ix, sc, 8) == 0);
    TL_RETRIEVAL_SPANS bad;
    memset(&bad, 0xff, sizeof(bad));
    check("retrieval-span invalid idx cleared", !TextLexRetrievalSpans(t, UINT32_MAX, &bad) &&
          bad.content_first == 0 && bad.combined_end == 0 && bad.has_label == 0);
    check("retrieval-span null inputs safe", !TextLexRetrievalSpans(NULL, 0, &bad) &&
          !TextLexRetrievalSpans(t, 0, NULL));
    TextLexFree(t); EmbeddingTableDestroy(e); GraphDestroy(g);
    check("retrieval-span fixture removed", remove(path) == 0);
}

static void CheckRetrievalSpans(void)
{
    CheckRetrievalSpanFixture("Intro:\n1. Alpha content.\n2. Beta content.\n", "Alpha", "Alpha content.", "1.");
    CheckRetrievalSpanFixture("Intro:\n1. Alpha content.\n2. Beta content.\n", "Beta", "Beta content.", "2.");
    CheckRetrievalSpanFixture("12. Alpha content. More detail.\n", "Alpha", "Alpha content.", "12.");
    CheckRetrievalSpanFixture("12. Alpha content. More detail.\n", "More", "More detail.", NULL);
    CheckRetrievalSpanFixture("1. Primer postulado\nContenido completo.\n", "Contenido", "Primer postulado\nContenido completo.", "1.");
    CheckRetrievalSpanFixture("Intro:\r\n1. Alpha content.\r\n", "Alpha", "Alpha content.", "1.");
    CheckRetrievalSpanFixture("Count 7. Next sentence.\n", "Next", "Next sentence.", NULL);
    CheckRetrievalSpanFixture("Value 1.7 seconds.\n", "seconds", "Value 1.7 seconds.", NULL);
    CheckRetrievalSpanFixture("Year 2026. Next year.\n", "Next", "Next year.", NULL);
    CheckRetrievalSpanFixture("Measure 7. Next measure.\n", "Next", "Next measure.", NULL);
    CheckRetrievalSpanFixture("Version 1.2.3 remains.\n", "Version", "Version 1.2.3 remains.", NULL);
    CheckRetrievalSpanFixture("Date 2026-10-12 remains.\n", "Date", "Date 2026-10-12 remains.", NULL);
    CheckRetrievalSpanFixture("1.\nContenido.\n", "Contenido", "Contenido.", NULL);
    CheckRetrievalSpanFixture("  1. Alpha content.\n", "Alpha", "Alpha content.", NULL);
    CheckRetrievalSpanFixture("1. Alpha.\n    2. Beta.\n", "Beta", "Beta.", NULL);
    CheckRetrievalSpanFixture("1) Alpha content.\n", "Alpha", "1) Alpha content.", NULL);
    CheckRetrievalSpanFixture("a) Alpha content.\n", "Alpha", "a) Alpha content.", NULL);
    CheckRetrievalSpanFixture("1.\tAlpha content.\n", "Alpha", "Alpha content.", NULL);
    CheckRetrievalSpanFixture("1.  Alpha content.\n", "Alpha", "Alpha content.", NULL);
    CheckRetrievalSpanFixture("1. Extra. Beta content.\n", "Beta", "Beta content.", NULL);
    CheckRetrievalSpanFixture("7. Next sentence.\n", "Next", "Next sentence.", "7."); /* ambiguous FP */
    CheckRetrievalSpanFixture("Reference:\n1909. This is the work.\n", "This", "This is the work.", "1909."); /* known FP, not semantic success */
    CheckRetrievalSpanFixture("Formula f(x)=x^2 remains.\n", "Formula", "Formula f(x)=x^2 remains.", NULL);
    CheckRetrievalSpanFixture("Code foo(1.7); Count 7. Next sentence.\n", "Next", "Next sentence.", NULL);
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
        EmbeddingTableDestroy(emb2);
        GraphDestroy(g2);
    }

    TextLexFree(tl);
    EmbeddingTableDestroy(emb);
    GraphDestroy(g);

    /* Numeric-science regression: segmentation retains a decimal in
       one retrieved sentence, without changing punctuation display. */
    {
        const char *fixture = "textlex_decimal_regression.txt";
        const char *bytes = "Gamma rays arrived 1.7 seconds later. Count 7. Next sentence.\n";
        FILE *f = fopen(fixture, "wb");
        int written = f && fwrite(bytes, 1, strlen(bytes), f) == strlen(bytes);
        if (f) fclose(f);
        check("decimal fixture written", written);
        if (written)
        {
            GRAPH *dg = GraphCreate(256, 256);
            EMBEDDING_TABLE *de = EmbeddingTableCreate(256);
            TEXTLEX *dt = TextLexCreate();
            GraphSetEmbeddingTable(dg, de);
            TextLexPosReset();
            TextLexIngest(dg, dt, fixture, 1, 0);
            const char *query[] = {"seconds"};
            uint32_t ix[8]; float sc[8]; char display[256];
            uint32_t nr = TextLexRetrieve(dt, dg, de, query, 1, ix, sc, 8);
            check("decimal and integer-ending sentence boundaries", dt->nsent == 3);
            int complete = 0, original = 0, unchanged = 0;
            if (nr)
            {
                const TL_SENT *sent = TextLexSentence(dt, ix[0]);
                uint64_t first = sent->offs[0];
                uint64_t end = sent->offs[sent->ntok - 1] + sent->lens[sent->ntok - 1];
                const char *anchor = strstr(bytes, "1.7 seconds");
                original = anchor && first <= (uint64_t)(anchor - bytes) &&
                    end >= (uint64_t)(anchor - bytes) + strlen("1.7 seconds") &&
                    end <= dt->imagelen &&
                    memcmp(dt->image + first, bytes + first, (size_t)(end - first)) == 0;
                TextLexSentenceText(dt, ix[0], dt->image, dt->imagelen,
                                    display, sizeof(display));
                complete = strstr(display, "Gamma rays arrived") &&
                    strstr(display, "1 . 7 seconds later .");
                unchanged = strstr(display, "1 . 7") != NULL;
            }
            check("seconds retrieves complete decimal sentence", complete);
            check("retrieved source span preserves 1.7 seconds bytes", original);
            check("decimal display remains token-space normalized", unchanged);
            TextLexFree(dt); EmbeddingTableDestroy(de); GraphDestroy(dg);
        }
        check("decimal fixture removed", remove(fixture) == 0);
    }
    CheckRetrievalSpans();
    CheckCitations();
    printf("test_textlex: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
