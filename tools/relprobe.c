/* relprobe: inspect a symbol's relations in a converted corpus.
   Scratch diagnostic (Temp build, not part of ctest).
   Usage: relprobe <corpus.txt> <word>
   Prints: variant lookup + frequency; top co-occurring symbols
   (exact counts over stored sentences); top retrieval sentence
   (the established sentence joining them). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    /* ---- compare mode: scoring variants x questions ---- */
    if (strcmp(word, "--compare") == 0)
    {
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
        static const char *STOPW[] = {
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
        FILE *ff = fopen(path, "rb");
        unsigned char *img = NULL;
        size_t imglen = 0;
        uint32_t qi, vi;
        if (ff != NULL)
        {
            fseek(ff, 0, SEEK_END);
            imglen = (size_t)ftell(ff);
            fseek(ff, 0, SEEK_SET);
            img = (unsigned char *)malloc(imglen);
            if (img == NULL ||
                fread(img, 1, imglen, ff) != imglen)
            {
                free(img);
                img = NULL;
            }
            fclose(ff);
        }
        for (vi = 0; vi < 7; vi++)
        {
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
                tok = strtok(qb, " ");
                while (tok != NULL && nqw < 16)
                {
                    qw[nqw++] = tok;
                    tok = strtok(NULL, " ");
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
                            while (STOPW[si] != NULL)
                            {
                                if (strcmp(lw, STOPW[si]) == 0)
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
                                while (*p != '\0')
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
                                    {
                                        found = 1;
                                        break;
                                    }
                                }
                                if (!found)
                                    all = 0;
                            }
                        }
                        if (ncontent > 0 && all)
                            complete = 1;
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
        }
        free(img);
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
    /* co-occurrence census over stored sentences */
    nsent = TextLexSentCount(tl);
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
            SYMBOL_ID id = sn->ids[j];
            uint32_t m;
            int found = 0;
            if (id == target)
                continue;
            for (m = 0; m < nbn; m++)
            {
                if (nb[m].id == id)
                {
                    nb[m].count++;
                    found = 1;
                    break;
                }
            }
            if (!found)
            {
                if (nbn >= bncap)
                {
                    uint32_t nc = bncap == 0 ? 1024 : bncap * 2;
                    NEIGH *nn = (NEIGH *)realloc(nb, nc * sizeof(NEIGH));
                    if (nn == NULL)
                        break;
                    nb = nn;
                    bncap = nc;
                }
                nb[nbn].id = id;
                nb[nbn].count = 1;
                nbn++;
            }
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
            if (ss == NULL || ss->frequency < 5 || ss->name == NULL)
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
    /* established sentence: top retrieval for the word */
    {
        const char *qw[1];
        uint32_t idx[8];
        float sc[8];
        uint32_t nret;
        qw[0] = word;
        nret = TextLexRetrieve(tl, g, emb, qw, 1, idx, sc, 8);
        printf("retrieve nret=%u\n", nret);
        if (nret > 0)
        {
            const TL_SENT *sn = TextLexSentence(tl, idx[0]);
            FILE *f = fopen(path, "rb");
            if (f != NULL && sn != NULL)
            {
                fseek(f, 0, SEEK_END);
                long sz = ftell(f);
                unsigned char *img;
                fseek(f, 0, SEEK_SET);
                img = (unsigned char *)malloc((size_t)sz);
                if (img != NULL && fread(img, 1, (size_t)sz, f) == (size_t)sz)
                {
                    char out[2048];
                    if (TextLexSentenceText(tl, idx[0], img, (size_t)sz,
                                            out, sizeof(out)) > 0)
                        printf("top[%u] score=%.4f: %.300s\n", idx[0],
                               sc[0], out);
                    free(img);
                }
                fclose(f);
            }
        }
    }
    return 0;
}
