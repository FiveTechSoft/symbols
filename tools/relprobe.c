/* relprobe: inspect a symbol's relations in a converted corpus.
   Scratch diagnostic (Temp build, not part of ctest).
   Usage: relprobe <corpus.txt> <word>
   Prints: variant lookup + frequency; top co-occurring symbols
   (exact counts over stored sentences); top retrieval sentence
   (the established sentence joining them). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "graph.h"
#include "symbol.h"
#include "embedding.h"
#include "text_lex.h"

typedef struct
{
    SYMBOL_ID id;
    uint32_t count;
} NEIGH;

static int CmpNeigh(const void *a, const void *b)
{
    const NEIGH *x = (const NEIGH *)a;
    const NEIGH *y = (const NEIGH *)b;
    if (x->count != y->count)
        return (x->count > y->count) ? -1 : 1;
    return (x->id < y->id) ? -1 : (x->id > y->id);
}

int main(int argc, char **argv)
{
    GRAPH *g;
    EMBEDDING_TABLE *emb;
    TEXTLEX *tl;
    TEXTLEX_STATS st;
    const char *path;
    const char *word;
    SYMBOL_ID target;
    const SYMBOL *s;
    NEIGH *nb = NULL;
    uint32_t nbn = 0, bncap = 0;
    uint32_t i, k;
    uint32_t nsent;
    const char *variants[4];
    char vbuf[4][64];
    size_t L;
    unsigned char *img = NULL;
    size_t imglen = 0;
    /* measurement stopwords (harness only; mirrors serving) */
    static const char *HSTOPW[] = {
        "the", "a", "an", "of", "to", "in", "is", "are",
        "was", "were", "be", "do", "does", "did", "what",
        "who", "whom", "which", "where", "when", "how",
        "tell", "me", "about", "de", "el", "la", "los",
        "las", "un", "una", "en", "y", "e", "o", "que",
        "quien", "es", "son", "por", "mi", "tu", "su",
        "no", "si", "and", "or", "for", "on", "at", "by",
        "with", "from", "as", "it", "its", "this", "that",
        NULL,
    };
    if (argc < 3)
    {
        printf("usage: relprobe <corpus.txt> <word>\n");
        return 2;
    }
    path = argv[1];
    word = argv[2];
    g = GraphCreate(65536, 256);
    emb = EmbeddingTableCreate(65536);
    GraphSetEmbeddingTable(g, emb);
    tl = TextLexCreate();
    st = TextLexIngest(g, tl, path, 1, 0);
    printf("ingest: bytes=%llu sents=%u syms=%u\n",
           (unsigned long long)st.bytes_read, st.nsent_new,
           st.syms_new);
    /* single image mapping shared by every mode below */
    {
        FILE *fi = fopen(path, "rb");
        img = NULL;
        imglen = 0;
        if (fi != NULL)
        {
            fseek(fi, 0, SEEK_END);
            imglen = (size_t)ftell(fi);
            fseek(fi, 0, SEEK_SET);
            img = (unsigned char *)malloc(imglen);
            if (img == NULL ||
                fread(img, 1, imglen, fi) != imglen)
            {
                free(img);
                img = NULL;
                imglen = 0;
            }
            fclose(fi);
        }
    }
    /* ---- eval mode: formal benchmark over tests/eval_jung.txt.
       P@1 (top == EXP), MRR over top-8 keyword hits, abstention
       rate, coverage rate, mean latency. */
    if (strcmp(word, "--eval") == 0)
    {
        FILE *ef = fopen("tests/eval_jung.txt", "r");
        char line[1024];
        uint32_t nq = 0, p1 = 0, abok = 0, abtot = 0, cov = 0;
        double mrr = 0.0, ms = 0.0;
        uint32_t nexp = 0, nranked = 0;
        if (ef == NULL)
        {
            printf("EVAL: cannot open tests/eval_jung.txt\n");
            return 1;
        }
        while (fgets(line, sizeof(line), ef) != NULL)
        {
            char *q, *kw, *ex;
            char qb[512];
            const char *qw[16];
            uint32_t nqw = 0;
            char *tok;
            uint32_t idx[8];
            float sc[8];
            uint32_t nret;
            clock_t t0;
            if (line[0] == '#' || line[0] == '\n' || line[0] == '\r' ||
                strncmp(line, "Q: ", 3) != 0)
                continue;
            q = line + 3;
            kw = strstr(q, " | KW: ");
            ex = strstr(q, " | EXP: ");
            if (kw == NULL || ex == NULL)
                continue;
            *kw = '\0';
            *ex = '\0';
            kw += 7;
            ex += 8;
            {
                size_t L = strlen(ex);
                while (L > 0 && (ex[L - 1] == '\n' || ex[L - 1] == '\r' ||
                                ex[L - 1] == ' '))
                    ex[--L] = '\0';
            }
            strncpy(qb, q, sizeof(qb) - 1);
            qb[sizeof(qb) - 1] = '\0';
            {
                size_t L = strlen(qb);
                while (L > 0 && (qb[L - 1] == ' ' || qb[L - 1] == '\t'))
                    qb[--L] = '\0';
            }
            tok = strtok(qb, " \t\r\n?,.;:");
            while (tok != NULL && nqw < 16)
            {
                uint32_t si = 0;
                int isstop = 0;
                char lw[64];
                size_t li;
                for (li = 0; li < strlen(tok) && li < 63; li++)
                {
                    char c = tok[li];
                    lw[li] = (c >= 'A' && c <= 'Z') ? (char)(c + 32)
                                                   : c;
                }
                lw[li] = '\0';
                while (HSTOPW[si] != NULL)
                {
                    if (strcmp(lw, HSTOPW[si]) == 0)
                    {
                        isstop = 1;
                        break;
                    }
                    si++;
                }
                if (!isstop)
                    qw[nqw++] = tok;
                tok = strtok(NULL, " \t\r\n?,.;:");
            }
            if (nqw == 0)
            {
                printf("EVAL %s | top=none 0 rank=0\n", q);
                nq++;
                continue;
            }
            t0 = clock();
            nret = TextLexRetrieve(tl, g, emb, qw, nqw, idx, sc, 8);
            ms += 1000.0 * (double)(clock() - t0) /
                  (double)CLOCKS_PER_SEC;
            nq++;
            if (strcmp(ex, "ABSTAIN") == 0)
            {
                abtot++;
                if (nret == 0)
                    abok++;
                else
                    printf("EVAL-FAIL abstain [%s] got %u\n", q,
                           idx[0]);
                continue;
            }
            /* MRR + coverage over top-8 keyword hits */
            {
                uint32_t r;
                int rank = 0;
                nranked++;
                for (r = 0; r < nret; r++)
                {
                    char out[2048];
                    if (TextLexSentenceText(tl, idx[r], img,
                                            imglen, out,
                                            sizeof(out)) > 0)
                    {
                        char kb[256];
                        char *k2;
                        int all = 1;
                        strncpy(kb, kw, sizeof(kb) - 1);
                        kb[sizeof(kb) - 1] = '\0';
                        for (k2 = strtok(kb, ",");
                             k2 != NULL;
                             k2 = strtok(NULL, ","))
                        {
                            while (*k2 == ' ')
                                k2++;
                            {
                                char *p = out;
                                size_t wl = strlen(k2);
                                int found = 0;
                                while (*p != '\0' && !found)
                                {
                                    size_t k3;
                                    for (k3 = 0; k3 < wl; k3++)
                                    {
                                        char c = p[k3];
                                        if (c >= 'A' && c <= 'Z')
                                            c = (char)(c + 32);
                                        if (c != k2[k3] ||
                                            p[k3] == '\0')
                                            break;
                                    }
                                    if (k3 == wl)
                                        found = 1;
                                    else
                                        p++;
                                }
                                if (!found)
                                    all = 0;
                            }
                        }
                        if (all)
                        {
                            rank = (int)r + 1;
                            if (r == 0)
                                cov++;
                            break;
                        }
                    }
                }
                if (rank > 0)
                    mrr += 1.0 / (double)rank;
                if (strcmp(ex, "-") != 0)
                {
                    nexp++;
                    if (nret > 0 && idx[0] == (uint32_t)atoi(ex))
                        p1++;
                    else
                        printf("EVAL-MISS p1 [%s] want %s got %s%u\n",
                               q, ex, nret > 0 ? "" : "none ",
                               nret > 0 ? idx[0] : 0);
                }
                printf("EVAL %s | top=%s%u rank=%d\n", q,
                       nret > 0 ? "" : "none ",
                       nret > 0 ? idx[0] : 0, rank);
            }
        }
        fclose(ef);
        printf("EVAL-SUMMARY n=%u p1=%u/%u mrr=%.3f abstain=%u/%u "
               "coverage=%.3f ms=%.1f\n",
               nq, p1, nexp,
               nranked > 0 ? mrr / (double)nranked : 0.0, abok,
               abtot,
               nranked > 0 ? (double)cov / (double)nranked : 0.0,
               ms);
        free(img);
        return 0;
    }
    /* ---- compare mode: scoring variants x questions ---- */
    if (strcmp(word, "--compare") == 0)    {
        static const char *QS[] = {
            "sun", "mother", "libido", "sacrifice", "rebirth",
            "hero", "tree", "serpent", "dreams", "god", "water",
            "incest", "what does the sun mean", "que significa sol",
            "tell me about fire", "who is christ",
        };
        static const struct
        {
            const char *name;
            unsigned flags;
        } VS[] = {
            {"DEFAULT", 15}, {"NO_DROP", 7}, {"NO_ORDER", 11},
            {"NO_INTER", 13}, {"NO_RARITY", 14}, {"CENTROID", 24},
            {"HAMMING", 40},
        };
        uint32_t qi, vi;
        /* image shared from main scope (single disk read) */
        for (vi = 0; vi < 7; vi++)
        {
            uint32_t vcomp = 0;
            double vscore = 0.0;
            clock_t t0 = clock();
            for (qi = 0; qi < 16; qi++)
            {
                char qb[512];
                const char *qw[16];
                uint32_t nqw = 0;
                char *tok;
                uint32_t idx[2];
                float sc[2];
                uint32_t nret;
                uint32_t complete = 0;
                uint32_t ncontent = 0;
                strncpy(qb, QS[qi], sizeof(qb) - 1);
                qb[sizeof(qb) - 1] = '\0';
                tok = strtok(qb, " \t\r\n?,.;:");
                while (tok != NULL && nqw < 16)
                {
                    uint32_t si = 0;
                    int isstop = 0;
                    char lw[64];
                    size_t li;
                    for (li = 0; li < strlen(tok) && li < 63; li++)
                    {
                        char c = tok[li];
                        lw[li] = (c >= 'A' && c <= 'Z') ? (char)(c + 32)
                                                       : c;
                    }
                    lw[li] = '\0';
                    while (HSTOPW[si] != NULL)
                    {
                        if (strcmp(lw, HSTOPW[si]) == 0)
                        {
                            isstop = 1;
                            break;
                        }
                        si++;
                    }
                    if (!isstop)
                        qw[nqw++] = tok;
                    tok = strtok(NULL, " \t\r\n?,.;:");
                }
                nret = TextLexRetrieveV(tl, g, emb, qw, nqw, idx,
                                        sc, 1, VS[vi].flags);
                if (nret > 0 && img != NULL)
                {
                    char out[2048];
                    uint32_t wi;
                    if (TextLexSentenceText(tl, idx[0], img,
                                            imglen, out,
                                            sizeof(out)) > 0)
                    {
                        int all = 1;
                        for (wi = 0; wi < nqw; wi++)
                        {
                            uint32_t si = 0;
                            int isstop = 0;
                            char lw[64];
                            size_t li;
                            for (li = 0; li < strlen(qw[wi]) &&
                                        li < 63;
                                 li++)
                            {
                                char c = qw[wi][li];
                                lw[li] = (c >= 'A' && c <= 'Z')
                                             ? (char)(c + 32)
                                             : c;
                            }
                            lw[li] = '\0';
                            while (HSTOPW[si] != NULL)
                            {
                                if (strcmp(lw, HSTOPW[si]) == 0)
                                {
                                    isstop = 1;
                                    break;
                                }
                                si++;
                            }
                            if (isstop)
                                continue;
                            ncontent++;
                            {
                                char *p = out;
                                int found = 0;
                                size_t wl = strlen(lw);
                                while (*p != '\0' && !found)
                                {
                                    size_t k;
                                    for (k = 0; k < wl; k++)
                                    {
                                        char c = p[k];
                                        if (c >= 'A' && c <= 'Z')
                                            c = (char)(c + 32);
                                        if (c != lw[k] || p[k] == '\0')
                                            break;
                                    }
                                    if (k == wl)
                                        found = 1;
                                    else
                                        p++;
                                }
                                if (!found)
                                    all = 0;
                            }
                        }
                        if (ncontent > 0 && all)
                            complete = 1;
                        vcomp += complete;
                        vscore += (double)sc[0];
                        printf("%s\t%s\t%u\t%.2f\t%u\t%.80s\n",
                               VS[vi].name, QS[qi], idx[0], sc[0],
                               complete, out);
                    }
                    else
                        printf("%s\t%s\t-\t0\t0\tDISPLAY-FAIL\n",
                               VS[vi].name, QS[qi]);
                }
                else
                    printf("%s\t%s\t-\t0\t0\tABSTAIN\n",
                           VS[vi].name, QS[qi]);
            }
            {
                double ms = 1000.0 * (double)(clock() - t0) /
                            (double)CLOCKS_PER_SEC;
                printf("AGG\t%s\tcomplete=%u/16\tavg=%.2f\tms=%.1f\n",
                       VS[vi].name, vcomp, vscore / 16.0, ms);
            }
        }
        return 0;
    }
    /* variant lookup */
    L = strlen(word);
    target = SYMBOL_INVALID;
    if (L > 0 && L < 64)
    {
        uint32_t v;
        strcpy(vbuf[0], word);
        strcpy(vbuf[1], word);
        strcpy(vbuf[2], word);
        strcpy(vbuf[3], word);
        if (vbuf[1][0] >= 'a' && vbuf[1][0] <= 'z')
            vbuf[1][0] = (char)(vbuf[1][0] - 32);
        for (i = 0; i < L; i++)
        {
            if (vbuf[2][i] >= 'a' && vbuf[2][i] <= 'z')
                vbuf[2][i] = (char)(vbuf[2][i] - 32);
            if (vbuf[3][i] >= 'A' && vbuf[3][i] <= 'Z')
                vbuf[3][i] = (char)(vbuf[3][i] + 32);
        }
        for (v = 0; v < 4; v++)
        {
            SYMBOL_ID id = SymbolFind(g->symbols, vbuf[v]);
            printf("variant %-12s -> %s", vbuf[v],
                   id == SYMBOL_INVALID ? "ABSENT" : "present");
            if (id != SYMBOL_INVALID)
            {
                const SYMBOL *ss = SymbolGet(g->symbols, id);
                printf(" id=%u freq=%llu", id,
                       ss ? (unsigned long long)ss->frequency : 0);
                if (target == SYMBOL_INVALID)
                    target = id;
            }
            printf("\n");
        }
        variants[0] = vbuf[0];
    }
    (void)variants;
    if (target == SYMBOL_INVALID)
    {
        printf("symbol '%s' not in graph: nothing to relate.\n",
               word);
        return 0;
    }
    s = SymbolGet(g->symbols, target);
    printf("target id=%u freq=%llu\n", target,
           s ? (unsigned long long)s->frequency : 0);
    {
        const float *v = EmbeddingGetVector(emb, target);
        if (v != NULL)
        {
            float tot = 0.0f, mx = 0.0f;
            uint32_t d;
            for (d = 0; d < 32; d++)
            {
                tot += v[d];
                if (v[d] > mx)
                    mx = v[d];
            }
            if (tot > 0.0f)
                printf("target conc=%.3f\n", mx / tot);
        }
    }
    /* co-occurrence census over stored sentences, O(1) per
       token via direct id-indexed counters */
    nsent = TextLexSentCount(tl);
    {
        uint32_t nsyms = SymbolCount(g->symbols);
        uint32_t *counts =
            (uint32_t *)calloc(nsyms + 1, sizeof(uint32_t));
        if (counts != NULL)
        {
            for (i = 0; i < nsent; i++)
            {
                const TL_SENT *sn = TextLexSentence(tl, i);
                uint32_t j;
                int has = 0;
                for (j = 0; j < sn->ntok; j++)
                {
                    if (sn->ids[j] == target)
                    {
                        has = 1;
                        break;
                    }
                }
                if (!has)
                    continue;
                for (j = 0; j < sn->ntok; j++)
                {
                    if (sn->ids[j] != target &&
                        sn->ids[j] <= nsyms)
                        counts[sn->ids[j]]++;
                }
            }
            for (i = 1; i <= nsyms; i++)
            {
                if (counts[i] > 0)
                {
                    if (nbn >= bncap)
                    {
                        uint32_t nc = bncap == 0 ? 1024 : bncap * 2;
                        NEIGH *nn = (NEIGH *)realloc(
                            nb, nc * sizeof(NEIGH));
                        if (nn == NULL)
                            break;
                        nb = nn;
                        bncap = nc;
                    }
                    nb[nbn].id = i;
                    nb[nbn].count = counts[i];
                    nbn++;
                }
            }
            free(counts);
        }
    }
    qsort(nb, nbn, sizeof(NEIGH), CmpNeigh);
    printf("neighbors=%u top:\n", nbn);
    for (k = 0; k < nbn && k < 15; k++)
    {
        const SYMBOL *ss = SymbolGet(g->symbols, nb[k].id);
        printf("  %-20s x%u\n", ss ? ss->name : "?>!", nb[k].count);
    }
    free(nb);
    /* key concepts: concentration = max bucket share of the
       count vector (peaked = focused topic; flat = glue).
       Reads the relation vectors directly. */
    {
        typedef struct
        {
            SYMBOL_ID id;
            float conc;
            uint64_t freq;
        } CONC;
        CONC *cs = NULL;
        uint32_t ncs = 0, ccap = 0;
        uint32_t nsyms = SymbolCount(g->symbols);
        for (i = 1; i <= nsyms; i++)
        {
            const SYMBOL *ss = SymbolGet(g->symbols, i);
            const float *v;
            float tot = 0.0f, mx = 0.0f;
            uint32_t d;
            char c0;
            if (ss == NULL || ss->frequency < 5 || ss->name == NULL)
                continue;
            /* lexical filter: genuine words only (letter start,
               length 3+). Reference marks, digits and initials
               stay in storage, out of concept ranking. */
            c0 = ss->name[0];
            if (!((c0 >= 'a' && c0 <= 'z') ||
                  (c0 >= 'A' && c0 <= 'Z')))
                continue;
            if (strlen(ss->name) < 3)
                continue;
            v = EmbeddingGetVector(emb, i);
            if (v == NULL)
                continue;
            for (d = 0; d < 32; d++)
            {
                tot += v[d];
                if (v[d] > mx)
                    mx = v[d];
            }
            if (tot <= 0.0f)
                continue;
            if (ncs >= ccap)
            {
                uint32_t nc = ccap == 0 ? 4096 : ccap * 2;
                void *nn = realloc(cs, nc * sizeof(CONC));
                if (nn == NULL)
                    break;
                cs = (CONC *)nn;
                ccap = nc;
            }
            cs[ncs].id = i;
            cs[ncs].conc = mx / tot;
            cs[ncs].freq = ss->frequency;
            ncs++;
        }
        /* insertion rank top 25 by concentration (n small after
           floor; simple selection) */
        printf("concepts=%u top by concentration:\n", ncs);
        for (k = 0; k < ncs && k < 25; k++)
        {
            uint32_t m = k;
            uint32_t j;
            CONC t;
            for (j = k + 1; j < ncs; j++)
            {
                if (cs[j].conc > cs[m].conc)
                    m = j;
            }
            t = cs[k];
            cs[k] = cs[m];
            cs[m] = t;
            {
                const SYMBOL *ss = SymbolGet(g->symbols, cs[k].id);
                printf("  %-20s conc=%.3f freq=%llu\n",
                       ss ? ss->name : "?>!", cs[k].conc,
                       (unsigned long long)cs[k].freq);
            }
        }
        free(cs);
    }
    /* established sentence: top retrieval for the word
       (shared image, single disk read at startup) */
    {
        const char *qw[1];
        uint32_t idx[8];
        float sc[8];
        uint32_t nret;
        qw[0] = word;
        nret = TextLexRetrieve(tl, g, emb, qw, 1, idx, sc, 8);
        printf("retrieve nret=%u\n", nret);
        if (nret > 0 && img != NULL)
        {
            const TL_SENT *sn = TextLexSentence(tl, idx[0]);
            if (sn != NULL)
            {
                char out[2048];
                if (TextLexSentenceText(tl, idx[0], img, imglen,
                                        out, sizeof(out)) > 0)
                    printf("top[%u] score=%.4f: %.300s\n", idx[0],
                           sc[0], out);
            }
        }
    }
    free(img);
    return 0;
}
