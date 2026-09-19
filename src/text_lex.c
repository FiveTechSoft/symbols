/* text_lex.c — corpus-to-graph converter. See text_lex.h doctrine. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "text_lex.h"
#include "symbol.h"
#include "embedding.h"

#define TL_POS_MAX 4096

static void LayerNorm(float *vec, uint32_t dim);
static float g_temperature = 1.0f;
static float CrossAttentionScore(const char *query, const char *symbol);

typedef struct
{
    uint64_t off;
    uint32_t len;
    uint32_t line;
} TL_RAWTOK;

typedef struct
{
    TL_RAWTOK *toks;
    uint32_t ntok;
    uint32_t cap;
} TL_TOKBUF;

/* Phase 2: QKV separation — inverted index + novelty cache.
   Keys: symbol → sentences containing it (sparse, for fast lookup).
   Values: per-symbol novelty (precomputed TF-IDF weight). */
typedef struct
{
    SYMBOL_ID sym;
    uint32_t *sents;    /* sentence indices containing this symbol */
    uint32_t nsents;    /* count */
    float novelty;      /* precomputed 1/(1+freq) */
    uint32_t cap;
} TL_INVENTRY;

typedef struct
{
    TL_INVENTRY *entries;
    uint32_t nent;
    uint32_t cap;
    int ready;
} TL_INVINDEX;

static TL_INVINDEX g_invindex = {NULL, 0, 0, 0};

/* Phase 4: KV-cache — per-session cache for query token novelty.
   Avoids recomputing 1/(1+freq) for repeated query tokens. */
typedef struct
{
    SYMBOL_ID sym;
    float novelty;
    uint32_t pos;
} TL_KVCACHE_ENTRY;

typedef struct
{
    TL_KVCACHE_ENTRY *entries;
    uint32_t nent;
    uint32_t cap;
} TL_KVCACHE;

static TL_KVCACHE g_kvcache = {NULL, 0, 0};

static float KVCacheLookup(SYMBOL_ID sym, const GRAPH *graph)
{
    uint32_t i;
    for (i = 0; i < g_kvcache.nent; i++)
    {
        if (g_kvcache.entries[i].sym == sym)
            return g_kvcache.entries[i].novelty;
    }
    /* miss: compute and store */
    {
        const SYMBOL *s = SymbolGet(graph ? graph->symbols : NULL, sym);
        float nv = (s == NULL) ? 1.0f : 1.0f / (1.0f + (float)s->frequency);
        if (g_kvcache.nent >= g_kvcache.cap)
        {
            g_kvcache.cap = g_kvcache.cap ? g_kvcache.cap * 2 : 256;
            g_kvcache.entries = (TL_KVCACHE_ENTRY *)realloc(
                g_kvcache.entries, g_kvcache.cap * sizeof(TL_KVCACHE_ENTRY));
        }
        g_kvcache.entries[g_kvcache.nent].sym = sym;
        g_kvcache.entries[g_kvcache.nent].novelty = nv;
        g_kvcache.nent++;
        return nv;
    }
}

static void KVCacheReset(void)
{
    g_kvcache.nent = 0;
}

TEXTLEX *TextLexCreate(void)
{
    TEXTLEX *tl = (TEXTLEX *)calloc(1, sizeof(TEXTLEX));
    return tl;
}

static void SentFree(TL_SENT *s)
{
    if (s == NULL)
        return;
    free(s->offs);
    free(s->lens);
    free(s->ids);
    free(s->lines);
    s->offs = NULL;
    s->lens = NULL;
    s->ids = NULL;
    s->lines = NULL;
    s->ntok = 0;
    s->cap = 0;
}

void TextLexClear(TEXTLEX *tl){
    uint32_t i;
    if (tl == NULL)
        return;
    for (i = 0; i < tl->nsent; i++)
        SentFree(&tl->sents[i]);
    free(tl->sents);
    tl->sents = NULL;
    tl->nsent = 0;
    tl->cap = 0;
    free(tl->image);
    tl->image = NULL;
    tl->imagelen = 0;
}

void TextLexFree(TEXTLEX *tl)
{
    if (tl == NULL)
        return;
    TextLexClear(tl);
    free(tl);
}

uint32_t TextLexSentCount(const TEXTLEX *tl)
{
    return tl == NULL ? 0 : tl->nsent;
}

const TL_SENT *TextLexSentence(const TEXTLEX *tl, uint32_t idx)
{
    if (tl == NULL || idx >= tl->nsent)
        return NULL;
    return &tl->sents[idx];
}

/* Literal display: join token byte slices with single spaces
   (words byte-exact; wrap whitespace normalized, source intact).
   Over-long sentences truncate with a marked tail (display-only;
   the store keeps every token). Returns token count, 0 on bad
   span. */
uint32_t TextLexSentenceText(const TEXTLEX *tl, uint32_t idx,
                             const unsigned char *img, size_t imglen,
                             char *out, size_t size)
{
    const TL_SENT *s;
    uint32_t i;
    size_t pos = 0;
    static const char *cut = " [...]";
    if (tl == NULL || idx >= tl->nsent || img == NULL || out == NULL ||
        size == 0)
        return 0;
    s = &tl->sents[idx];
    for (i = 0; i < s->ntok; i++)
    {
        uint64_t o = s->offs[i];
        uint32_t L = s->lens[i];
        if (o + L > imglen)
            return 0;
        if (i > 0 && pos + 1 < size)
            out[pos++] = ' ';
        if (pos + L >= size)
            break;
        memcpy(out + pos, img + o, L);
        pos += L;
    }
    if (i < s->ntok)
    {
        size_t k = strlen(cut);
        if (pos + k >= size)
            pos = size > k + 1 ? size - k - 1 : 0;
        memcpy(out + pos, cut, k);
        pos += k;
    }
    if (pos < size)
        out[pos] = '\0';
    else if (size > 0)
        out[size - 1] = '\0';
    return s->ntok;
}

/* positional embeddings: separate table (not symbols), one
   vector per sentence position. Co-counted with the words that
   occur there. Session-global; replay-safe (dups never
   re-accumulate). */
static float g_posvec[TL_POS_MAX][EMBEDDING_DIM];

#define POSBUCKET(k) (0x80000000u | ((uint32_t)(k) % TL_POS_MAX))

void TextLexPosReset(void)
{
    memset(g_posvec, 0, sizeof(g_posvec));
    KVCacheReset();
}

/* matmul-free compatibility: top-m active-dimension overlap.
   Signature = indices of the m largest centroid dims (partial
   selection by comparison only). Score = intersection size
   (integer count). No multiplications, no divisions, no
   softmax, no gradients anywhere. */
#define TL_TOPM 8

static void TopDims(const float *v, uint32_t *out, uint32_t m)
{
    uint32_t used[EMBEDDING_DIM];
    uint32_t i;
    uint32_t k;
    for (i = 0; i < EMBEDDING_DIM; i++)
        used[i] = 0;
    for (k = 0; k < m; k++)
    {
        uint32_t best = 0;
        uint32_t bi;
        for (bi = 0; bi < EMBEDDING_DIM; bi++)
        {
            if (!used[bi] && v[bi] > v[best])
                best = bi;
        }
        if (used[best])
        {
            for (bi = 0; bi < EMBEDDING_DIM; bi++)
            {
                if (!used[bi])
                {
                    best = bi;
                    break;
                }
            }
        }
        used[best] = 1;
        out[k] = best;
    }
}

/* scoring ablation flags (experiment harness only; serving
   uses SF_DEFAULT, pinned by tests). All branches matmul-free:
   counts, comparisons, scalar loops. */
#define SF_RARITY 1u
#define SF_INTER 2u
#define SF_ORDER 4u
#define SF_DROP 8u
#define SF_CENTROID 16u
#define SF_HAMMING 32u
#define SF_DEFAULT (SF_RARITY | SF_INTER | SF_ORDER | SF_DROP)
static unsigned g_scoring = SF_DEFAULT;

static uint32_t PopCount32(uint32_t x)
{
    uint32_t c = 0;
    while (x != 0)
    {
        c += x & 1u;
        x >>= 1;
    }
    return c;
}

static uint32_t SignMask(const float *v, float mean)
{
    uint32_t m = 0;
    uint32_t d;
    for (d = 0; d < EMBEDDING_DIM; d++)
    {
        if (v[d] > mean)
            m |= (1u << d);
    }
    return m;
}

/* QKV attention over transferred truth.
   Q = distinct query symbols (case variants probed) with query
   positions. K = sentence symbols with positions. V = the
   literal sentence.
   compatibility (default) =
     rarity: occurrence-count importance 1/(1+freq) per hit
       (transferred truth; frequent scaffolding self-attenuates,
       no word lists) +
     top-m overlap: distributional agreement between query and
       sentence centroids (word + positional vectors), counted
       by index comparison only (no matmul: no dot products,
       no softmax, no gradients) +
     order: hit tokens in the same relative order as in the
       query (positional syntax).
   CENTROID branch: hits + cosine(centroids) instead.
   HAMMING branch: hits + (32 - popcount(qmask^kmas)) on
   mean-centered sign masks instead.
   Gate: zero hits -> 0 (fail-closed). Fully traceable. */
static float QKVScore(const TL_SENT *s, const SYMBOL_ID *qids,
                      const float *qnov, const uint32_t *qpos,
                      uint32_t nq, const uint32_t *qsig,
                      const GRAPH *graph,
                      const EMBEDDING_TABLE *emb)
{
    float qsum[EMBEDDING_DIM];
    float ssum[EMBEDDING_DIM];
    float total = 0.0f;
    uint32_t d;
    uint32_t i;
    uint32_t j;
    uint32_t conc = 0;
    uint32_t pairs = 0;
    uint32_t inter = 0;
    uint32_t k;
    uint32_t hits = 0;
    for (d = 0; d < EMBEDDING_DIM; d++)
    {
        qsum[d] = 0.0f;
        ssum[d] = 0.0f;
    }
    for (i = 0; i < nq; i++)
    {
        int hit = 0;
        for (j = 0; j < s->ntok; j++)
        {
            if (s->ids[j] == qids[i])
            {
                hit = 1;
                break;
            }
        }
        if (hit)
        {
            hits++;
            /* Phase 2: use precomputed novelty from inverted index */
            if (g_scoring & SF_RARITY)
            {
                float nv = qnov[i];
                if (g_invindex.ready)
                {
                    uint32_t ei;
                    for (ei = 0; ei < g_invindex.nent; ei++)
                    {
                        if (g_invindex.entries[ei].sym == qids[i])
                        {
                            nv = g_invindex.entries[ei].novelty;
                            break;
                        }
                    }
                }
                total += nv;
            }
        }
    }
    if (hits == 0)
        return 0.0f;
    /* recall first: sentences addressing more of the question
       outrank partial matches (scale-separated; below stays
       under 1000: inter<=8, order<=1, rarity<nq<=64) */
    total += (float)hits * 1000.0f;
    /* span density: query hits packed close (short min-span)
       outrank dispersed ones. Bounded proximity bonus, no
       libm, never overrides integer gaps. */
    {
        uint32_t mn = 0;
        uint32_t mx = 0;
        int any = 0;
        for (j = 0; j < s->ntok; j++)
        {
            uint32_t k;
            for (k = 0; k < nq; k++)
            {
                if (s->ids[j] == qids[k])
                {
                    if (!any)
                    {
                        mn = j;
                        mx = j;
                        any = 1;
                    }
                    else
                    {
                        if (j < mn)
                            mn = j;
                        if (j > mx)
                            mx = j;
                    }
                    break;
                }
            }
        }
        if (any)
            total += 1.0f / (1.0f + (float)(mx - mn));
    }
    /* Phase 3: symbolic positional encoding — relative distance
       and order coherence between matching tokens. Closer tokens
       and same-order pairs get bonus; reversed pairs get penalty. */
    {
        uint32_t pi_arr[64];
        int hit_arr[64];
        uint32_t nmatch = 0;
        uint32_t coherence = 0;
        float prox_sum = 0.0f;
        for (i = 0; i < nq && nmatch < 64; i++)
        {
            hit_arr[nmatch] = 0;
            for (j = 0; j < s->ntok; j++)
            {
                if (s->ids[j] == qids[i])
                {
                    pi_arr[nmatch] = j;
                    hit_arr[nmatch] = 1;
                    nmatch++;
                    break;
                }
            }
        }
        for (i = 0; i < nmatch; i++)
        {
            if (!hit_arr[i])
                continue;
            /* proximity: closer to query center = higher weight */
            prox_sum += 1.0f / (1.0f + (float)(pi_arr[i]));
        }
        total += prox_sum;
        /* order coherence: pairs in same relative order as query */
        for (i = 0; i < nmatch; i++)
        {
            if (!hit_arr[i])
                continue;
            for (k = i + 1; k < nmatch; k++)
            {
                if (!hit_arr[k])
                    continue;
                if ((qpos[i] < qpos[k] && pi_arr[i] < pi_arr[k]) ||
                    (qpos[i] > qpos[k] && pi_arr[i] > pi_arr[k]))
                    coherence++;
            }
        }
        if (nmatch > 1)
            total += (float)coherence / (float)(nmatch - 1);
    }
    /* centroids with positional vectors (CENTROID and HAMMING
       branches only; INTER reads the precomputed sentence
       signature plus the hoisted query signature) */
    if (g_scoring & (SF_CENTROID | SF_HAMMING))
    {
    for (i = 0; i < nq; i++)
    {
        const float *w = EmbeddingGetVector(emb, qids[i]);
        for (d = 0; d < EMBEDDING_DIM; d++)
        {
            if (w != NULL)
                qsum[d] += w[d];
            qsum[d] += g_posvec[qpos[i] % TL_POS_MAX][d];
        }
    }
    for (j = 0; j < s->ntok; j++)
    {
        const float *w = EmbeddingGetVector(emb, s->ids[j]);
        for (d = 0; d < EMBEDDING_DIM; d++)
        {
            if (w != NULL)
                ssum[d] += w[d];
            ssum[d] += g_posvec[j % TL_POS_MAX][d];
        }
    }
    }
    if (g_scoring & SF_CENTROID)
        return total + EmbeddingCosineSimilarity(qsum, ssum);
    if (g_scoring & SF_HAMMING)
    {
        float qm = 0.0f;
        float sm = 0.0f;
        for (d = 0; d < EMBEDDING_DIM; d++)
        {
            qm += qsum[d];
            sm += ssum[d];
        }
        qm /= (float)EMBEDDING_DIM;
        sm /= (float)EMBEDDING_DIM;
        return total + (float)(EMBEDDING_DIM -
                               PopCount32(SignMask(qsum, qm) ^
                                          SignMask(ssum, sm)));
    }
    if ((g_scoring & SF_INTER) && s->sig_ready)
    {
        for (i = 0; i < TL_TOPM; i++)
        {
            for (j = 0; j < TL_TOPM; j++)
            {
                if (qsig[i] == s->sig[j])
                {
                    inter++;
                    break;
                }
            }
        }
        total += (float)inter;
    }
    /* order bonus over distinct hit pairs */
    if (g_scoring & SF_ORDER)
    {
    for (i = 0; i < nq; i++)
    {
        uint32_t j;
        uint32_t k;
        uint32_t pi = 0;
        int hi = 0;
        for (j = 0; j < s->ntok; j++)
        {
            if (s->ids[j] == qids[i])
            {
                hi = 1;
                pi = j;
                break;
            }
        }
        if (!hi)
            continue;
        for (k = i + 1; k < nq; k++)
        {
            uint32_t m;
            for (m = 0; m < s->ntok; m++)
            {
                if (s->ids[m] == qids[k])
                {
                    pairs++;
                    if ((qpos[i] < qpos[k] && pi < m) ||
                        (qpos[i] > qpos[k] && pi > m))
                        conc++;
                    break;
                }
            }
        }
    }
    if (pairs > 0)
        total += (float)conc / (float)pairs;
    }
    /* lexical density: grammatical sentences outrank
       number/punctuation-heavy index lines. Bounded [0,1]. */
    total += s->density;
    /* Phase 6: cross-attention — boost score when sentence tokens
       have character-level overlap with query words (soft alignment).
       Limited to first 32 sentence tokens, skip on large corpora. */
    if (graph != NULL && graph->symbols != NULL && s->ntok <= 32)
    {
        float xa_sum = 0.0f;
        uint32_t xa_count = 0;
        uint32_t xa_limit = (s->ntok < 32) ? s->ntok : 32;
        for (i = 0; i < nq; i++)
        {
            const char *qw_str = NULL;
            /* look up query word string from graph */
            if (graph != NULL)
            {
                const SYMBOL *qs = SymbolGet(graph->symbols, qids[i]);
                if (qs != NULL && qs->name != NULL)
                    qw_str = qs->name;
            }
            if (qw_str == NULL)
                continue;
            for (j = 0; j < xa_limit; j++)
            {
                const char *sw_str = NULL;
                if (graph != NULL)
                {
                    const SYMBOL *ss = SymbolGet(graph->symbols, s->ids[j]);
                    if (ss != NULL && ss->name != NULL)
                        sw_str = ss->name;
                }
                if (sw_str == NULL)
                    continue;
                {
                    float xa = CrossAttentionScore(qw_str, sw_str);
                    if (xa > 0.2f)
                    {
                        xa_sum += xa;
                        xa_count++;
                    }
                }
            }
        }
        if (xa_count > 0)
            total += xa_sum * 10.0f;
    }
    if (g_temperature > 0.0f && g_temperature != 1.0f)
        total /= g_temperature;
    return total;
}

/* Phase 6: cross-attention — character n-gram overlap between
   query token and corpus symbol. Soft alignment without dictionary. */
static float CrossAttentionScore(const char *query, const char *symbol)
{
    uint32_t qlen, slen;
    uint32_t qgrams[256];
    uint32_t sgrams[256];
    uint32_t nqg = 0, nsg = 0;
    uint32_t i, j;
    uint32_t overlap = 0;
    float score;
    if (query == NULL || symbol == NULL)
        return 0.0f;
    qlen = (uint32_t)strlen(query);
    slen = (uint32_t)strlen(symbol);
    if (qlen < 2 || slen < 2)
        return 0.0f;
    /* build bigrams for query (lowercased) */
    for (i = 0; i + 1 < qlen && nqg < 256; i++)
    {
        unsigned char c1 = (unsigned char)query[i];
        unsigned char c2 = (unsigned char)query[i + 1];
        if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
        if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
        if ((c1 >= 'a' && c1 <= 'z') && (c2 >= 'a' && c2 <= 'z'))
            qgrams[nqg++] = (uint32_t)c1 * 256 + (uint32_t)c2;
    }
    /* build bigrams for symbol */
    for (i = 0; i + 1 < slen && nsg < 256; i++)
    {
        unsigned char c1 = (unsigned char)symbol[i];
        unsigned char c2 = (unsigned char)symbol[i + 1];
        if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
        if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
        if ((c1 >= 'a' && c1 <= 'z') && (c2 >= 'a' && c2 <= 'z'))
            sgrams[nsg++] = (uint32_t)c1 * 256 + (uint32_t)c2;
    }
    if (nqg == 0 || nsg == 0)
        return 0.0f;
    /* count overlapping bigrams */
    for (i = 0; i < nqg; i++)
    {
        for (j = 0; j < nsg; j++)
        {
            if (qgrams[i] == sgrams[j])
            {
                overlap++;
                break;
            }
        }
    }
    /* Dice coefficient on bigrams */
    score = (2.0f * (float)overlap) / ((float)nqg + (float)nsg);
    return score;
}

/* ASCII case variants of a query token (byte-exact symbols keep
   original case; the query probes all four spellings). */
static uint32_t CaseVariants(const char *w, char out[4][64])
{
    size_t L;
    size_t i;
    if (w == NULL || w[0] == '\0')
        return 0;
    L = strlen(w);
    if (L >= 64)
        return 0;
    strcpy(out[0], w);
    strcpy(out[1], w);
    strcpy(out[2], w);
    strcpy(out[3], w);
    if (out[1][0] >= 'a' && out[1][0] <= 'z')
        out[1][0] = (char)(out[1][0] - 32);
    for (i = 0; i < L; i++)
    {
        if (out[2][i] >= 'a' && out[2][i] <= 'z')
            out[2][i] = (char)(out[2][i] - 32);
        if (out[3][i] >= 'A' && out[3][i] <= 'Z')
            out[3][i] = (char)(out[3][i] + 32);
    }
    return 4;
}

SYMBOL_ID TextLexFindSymbol(const GRAPH *graph, const char *w)
{
    char vars[4][64];
    uint32_t nv;
    uint32_t v;
    if (graph == NULL || w == NULL)
        return SYMBOL_INVALID;
    nv = CaseVariants(w, vars);
    for (v = 0; v < nv; v++)
    {
        SYMBOL_ID id = SymbolFind(graph->symbols, vars[v]);
        if (id != SYMBOL_INVALID)
            return id;
    }
    return SYMBOL_INVALID;
}

uint32_t TextLexRetrieve(const TEXTLEX *tl, const GRAPH *graph,
                         const EMBEDDING_TABLE *emb,
                         const char **words, uint32_t nwords,
                         uint32_t *out_idx, float *out_score,
                         uint32_t max)
{
    SYMBOL_ID qids[64];
    float qnov[64];
    uint32_t qpos[64];
    uint32_t nq = 0;
    uint32_t i;
    uint32_t nret = 0;
    uint32_t qidx = 0;
    if (tl == NULL || graph == NULL || emb == NULL || words == NULL ||
        out_idx == NULL || out_score == NULL || max == 0)
        return 0;
    (void)emb;
    for (i = 0; i < nwords && nq < 64; i++)
    {
        char vars[4][64];
        uint32_t nv;
        uint32_t v;
        if (words[i] == NULL)
            continue;
        nv = CaseVariants(words[i], vars);
        for (v = 0; v < nv && nq < 64; v++)
        {
            SYMBOL_ID id;
            const SYMBOL *s;
            uint32_t k;
            int seen = 0;
            id = SymbolFind(graph->symbols, vars[v]);
            if (id == SYMBOL_INVALID)
                continue;
            for (k = 0; k < nq; k++)
            {
                if (qids[k] == id)
                {
                    seen = 1;
                    break;
                }
            }
            if (seen)
                continue;
            qids[nq] = id;
            qnov[nq] = KVCacheLookup(id, graph);
            qpos[nq] = qidx;
            nq++;
        }
        qidx++;
    }
    if (nq == 0)
        return 0;
    /* word-level DF drop (serving default on): drop the strictly
       most document-frequent query WORD when more than one
       distinct word orients (case variants stand together).
       Scaffolding orients nothing; the topic keeps the compass.
       Structural, no lists; ties and single words keep all. */
    if ((g_scoring & SF_DROP) && nq > 0)
    {
        uint32_t df[64];
        uint32_t qw[64];
        uint32_t i;
        uint32_t nw = 0;
        uint32_t w;
        uint32_t wdf[64];
        uint32_t mx = 0;
        uint32_t nmx = 0;
        uint32_t dw = 0;
        for (i = 0; i < nq; i++)
        {
            uint32_t s;
            uint32_t k;
            int seen = 0;
            df[i] = 0;
            for (s = 0; s < tl->nsent; s++)
            {
                uint32_t j;
                for (j = 0; j < tl->sents[s].ntok; j++)
                {
                    if (tl->sents[s].ids[j] == qids[i])
                    {
                        df[i]++;
                        break;
                    }
                }
            }
            for (k = 0; k < nw; k++)
            {
                if (qw[k] == qpos[i])
                {
                    seen = 1;
                    break;
                }
            }
            if (!seen && nw < 64)
            {
                qw[nw] = qpos[i];
                wdf[nw] = df[i];
                nw++;
            }
            else if (seen)
            {
                for (k = 0; k < nw; k++)
                {
                    if (qw[k] == qpos[i] && df[i] < wdf[k])
                        wdf[k] = df[i];
                }
            }
        }
        (void)qidx;
        for (w = 0; w < nw; w++)
        {
            if (wdf[w] > mx)
            {
                mx = wdf[w];
                nmx = 1;
                dw = w;
            }
            else if (wdf[w] == mx)
                nmx++;
        }
        if (nmx == 1 && nw > 1)
        {
            uint32_t o = 0;
            for (i = 0; i < nq; i++)
            {
                int drop = 0;
                for (w = 0; w < nw; w++)
                {
                    if (qw[w] == qpos[i] && w == dw)
                    {
                        drop = 1;
                        break;
                    }
                }
                if (!drop)
                {
                    qids[o] = qids[i];
                    qnov[o] = qnov[i];
                    qpos[o] = qpos[i];
                    o++;
                }
            }
            nq = o;
        }
    }
    if (nq == 0)
        return 0;
    {
        /* query signature, once per query (not per sentence) */
        float qsum[EMBEDDING_DIM];
        uint32_t qsig[TL_TOPM];
        uint32_t d;
        for (d = 0; d < EMBEDDING_DIM; d++)
            qsum[d] = 0.0f;
        for (i = 0; i < nq; i++)
        {
            const float *w = EmbeddingGetVector(emb, qids[i]);
            for (d = 0; d < EMBEDDING_DIM; d++)
            {
                if (w != NULL)
                    qsum[d] += w[d];
                qsum[d] += g_posvec[qpos[i] % TL_POS_MAX][d];
            }
        }
        LayerNorm(qsum, EMBEDDING_DIM);
        TopDims(qsum, qsig, TL_TOPM);
        /* Phase 2: QKV fast-path — use inverted index to find
           candidate sentences (those containing >=1 query symbol)
           instead of scanning all sentences. O(matches) vs O(nsent).
           Skip for large corpora (>10k sentences) to avoid build cost. */
        if (tl->nsent < 10000 && g_invindex.ready)
        {
            /* build candidate set from inverted index */
            char *seen = (char *)calloc(tl->nsent, 1);
            uint32_t *cands = NULL;
            uint32_t ncands = 0;
            uint32_t ci;
            for (i = 0; i < nq && g_invindex.ready; i++)
            {
                uint32_t ei;
                for (ei = 0; ei < g_invindex.nent; ei++)
                {
                    if (g_invindex.entries[ei].sym == qids[i])
                    {
                        TL_INVENTRY *e = &g_invindex.entries[ei];
                        for (ci = 0; ci < e->nsents; ci++)
                        {
                            uint32_t si = e->sents[ci];
                            if (!seen[si])
                            {
                                seen[si] = 1;
                                ncands++;
                            }
                        }
                        break;
                    }
                }
            }
            /* convert seen bitmap to cands array */
            if (ncands > 0)
            {
                uint32_t ci2 = 0;
                cands = (uint32_t *)malloc(ncands * sizeof(uint32_t));
                for (i = 0; i < tl->nsent; i++)
                {
                    if (seen[i])
                        cands[ci2++] = i;
                }
            }
            free(seen);
            /* Phase 5: sparse attention — window around each match
               + global anchors (every sqrt(nsent) sentences).
               Reduces scored set from all matches to local+global.
               Skip on large corpora (>20k sentences). */
            if (ncands > 0 && tl->nsent > 100 && tl->nsent < 20000)
            {
                uint32_t window = 32;
                uint32_t stride = 1;
                uint32_t si2;
                char *win_seen = (char *)calloc(tl->nsent, 1);
                {
                    uint32_t s2 = tl->nsent;
                    stride = 1;
                    while (stride * stride < s2 && stride < 256)
                        stride++;
                }
                for (si2 = 0; si2 < tl->nsent; si2 += stride)
                    win_seen[si2] = 1;
                for (ci = 0; ci < ncands; ci++)
                {
                    uint32_t center = cands[ci];
                    uint32_t lo = (center > window) ? center - window : 0;
                    uint32_t hi = center + window;
                    if (hi >= tl->nsent) hi = tl->nsent - 1;
                    for (si2 = lo; si2 <= hi; si2++)
                        win_seen[si2] = 1;
                }
                free(cands);
                ncands = 0;
                for (si2 = 0; si2 < tl->nsent; si2++)
                    if (win_seen[si2]) ncands++;
                cands = (uint32_t *)malloc(ncands * sizeof(uint32_t));
                {
                    uint32_t ci2 = 0;
                    for (si2 = 0; si2 < tl->nsent; si2++)
                        if (win_seen[si2]) cands[ci2++] = si2;
                }
                free(win_seen);
            }
            /* score candidates */
            for (ci = 0; ci < ncands; ci++)
            {
                float sc;
                uint32_t j;
                i = cands[ci];
                sc = QKVScore(&tl->sents[i], qids, qnov, qpos, nq,
                              qsig, graph, emb);
                if (sc <= 0.0f)
                    continue;
                j = nret;
                if (j < max)
                {
                    out_idx[j] = i;
                    out_score[j] = sc;
                    nret++;
                }
                else
                {
                    uint32_t m = 0;
                    for (j = 0; j < max; j++)
                    {
                        if (out_score[j] < out_score[m])
                            m = j;
                    }
                    if (sc <= out_score[m])
                        continue;
                    out_idx[m] = i;
                    out_score[m] = sc;
                    j = m;
                }
                while (j > 0 && out_score[j] > out_score[j - 1])
                {
                    uint32_t ti = out_idx[j];
                    float ts = out_score[j];
                    out_idx[j] = out_idx[j - 1];
                    out_score[j] = out_score[j - 1];
                    out_idx[j - 1] = ti;
                    out_score[j - 1] = ts;
                    j--;
                }
            }
            if (cands != NULL) free(cands);
            /* fallback: if inverted index found nothing, score all */
            if (ncands == 0)
            {
                for (i = 0; i < tl->nsent; i++)
                {
                    float sc;
                    uint32_t j;
                    sc = QKVScore(&tl->sents[i], qids, qnov, qpos, nq,
                                  qsig, graph, emb);
                    if (sc <= 0.0f)
                        continue;
                    j = nret;
                    if (j < max)
                    {
                        out_idx[j] = i;
                        out_score[j] = sc;
                        nret++;
                    }
                    else
                    {
                        uint32_t m = 0;
                        for (j = 0; j < max; j++)
                        {
                            if (out_score[j] < out_score[m])
                                m = j;
                        }
                        if (sc <= out_score[m])
                            continue;
                        out_idx[m] = i;
                        out_score[m] = sc;
                        j = m;
                    }
                    while (j > 0 && out_score[j] > out_score[j - 1])
                    {
                        uint32_t ti = out_idx[j];
                        float ts = out_score[j];
                        out_idx[j] = out_idx[j - 1];
                        out_score[j] = out_score[j - 1];
                        out_idx[j - 1] = ti;
                        out_score[j - 1] = ts;
                        j--;
                    }
                }
            }
        }
        else
        {
            /* large corpus: score all sentences (no inverted index) */
            for (i = 0; i < tl->nsent; i++)
            {
                float sc;
                uint32_t j;
                sc = QKVScore(&tl->sents[i], qids, qnov, qpos, nq,
                              qsig, graph, emb);
                if (sc <= 0.0f)
                    continue;
                j = nret;
                if (j < max)
                {
                    out_idx[j] = i;
                    out_score[j] = sc;
                    nret++;
                }
                else
                {
                    uint32_t m = 0;
                    for (j = 0; j < max; j++)
                    {
                        if (out_score[j] < out_score[m])
                            m = j;
                    }
                    if (sc <= out_score[m])
                        continue;
                    out_idx[m] = i;
                    out_score[m] = sc;
                    j = m;
                }
                while (j > 0 && out_score[j] > out_score[j - 1])
                {
                    uint32_t ti = out_idx[j];
                    float ts = out_score[j];
                    out_idx[j] = out_idx[j - 1];
                    out_score[j] = out_score[j - 1];
                    out_idx[j - 1] = ti;
                    out_score[j - 1] = ts;
                    j--;
                }
            }
        }
    }
    return nret;
}

/* experiment harness: same retrieval under scoring flags
   (saves/restores serving default). Test-only callers. */
uint32_t TextLexRetrieveV(const TEXTLEX *tl, const GRAPH *graph,
                          const EMBEDDING_TABLE *emb,
                          const char **words, uint32_t nwords,
                          uint32_t *out_idx, float *out_score,
                          uint32_t max, unsigned flags)
{
    unsigned saved = g_scoring;
    uint32_t r;
    g_scoring = flags;
    r = TextLexRetrieve(tl, graph, emb, words, nwords, out_idx,
                        out_score, max);
    g_scoring = saved;
    return r;
}

/* signature finalization: per-sentence top-m over word +
   positional vectors. Recomputed every ingest (fresh, order-
   deterministic; replay rewrites identical values). */
void TextLexSetTemperature(float t)
{
    g_temperature = (t > 0.0f) ? t : 1.0f;
}

/* Phase 1: layer normalization + temperature (transformer concepts).
   LayerNorm: normalize embedding centroid before signature extraction.
   Temperature: scale QKVScore output — T<1 sharpens, T>1 softens. */

static void LayerNorm(float *vec, uint32_t dim)
{
    float mean = 0.0f;
    float var = 0.0f;
    uint32_t d;
    for (d = 0; d < dim; d++)
        mean += vec[d];
    mean /= (float)dim;
    for (d = 0; d < dim; d++)
    {
        float diff = vec[d] - mean;
        var += diff * diff;
    }
    var /= (float)dim;
    {
        float inv = (var > 1e-6f) ? 1.0f / sqrtf(var) : 1.0f;
        for (d = 0; d < dim; d++)
            vec[d] = (vec[d] - mean) * inv;
    }
}

static void FinalizeSigs(TEXTLEX *tl, GRAPH *graph,
                         EMBEDDING_TABLE *emb)
{
    uint32_t i;
    if (tl == NULL)
        return;
    for (i = 0; i < tl->nsent; i++)
    {
        TL_SENT *s = &tl->sents[i];
        float cent[EMBEDDING_DIM];
        uint32_t j;
        uint32_t d;
        for (d = 0; d < EMBEDDING_DIM; d++)
            cent[d] = 0.0f;
        for (j = 0; j < s->ntok; j++)
        {
            const float *w = NULL;
            if (graph != NULL && emb != NULL)
                w = EmbeddingGetVector(emb, s->ids[j]);
            for (d = 0; d < EMBEDDING_DIM; d++)
            {
                if (w != NULL)
                    cent[d] += w[d];
                cent[d] += g_posvec[j % TL_POS_MAX][d];
            }
        }
        LayerNorm(cent, EMBEDDING_DIM);
        TopDims(cent, s->sig, TL_TOPM);
        s->sig_ready = 1;
    }
    /* Build inverted index: symbol → sentences containing it + novelty.
       Skip for large corpora (>10k sentences) to avoid build cost. */
    if (tl->nsent < 10000)
    {
        uint32_t si;
        if (g_invindex.entries != NULL)
        {
            for (si = 0; si < g_invindex.nent; si++)
                free(g_invindex.entries[si].sents);
            free(g_invindex.entries);
        }
        g_invindex.nent = 0;
        g_invindex.cap = 256;
        g_invindex.entries = (TL_INVENTRY *)calloc(g_invindex.cap,
                                                   sizeof(TL_INVENTRY));
        g_invindex.ready = 1;
        for (si = 0; si < tl->nsent; si++)
        {
            TL_SENT *s = &tl->sents[si];
            uint32_t ti;
            for (ti = 0; ti < s->ntok; ti++)
            {
                SYMBOL_ID id = s->ids[ti];
                uint32_t ei;
                int found = 0;
                for (ei = 0; ei < g_invindex.nent; ei++)
                {
                    if (g_invindex.entries[ei].sym == id)
                    {
                        TL_INVENTRY *e = &g_invindex.entries[ei];
                        if (e->nsents >= e->cap)
                        {
                            e->cap = e->cap ? e->cap * 2 : 16;
                            e->sents = (uint32_t *)realloc(e->sents,
                                                           e->cap * sizeof(uint32_t));
                        }
                        e->sents[e->nsents++] = si;
                        found = 1;
                        break;
                    }
                }
                if (!found)
                {
                    TL_INVENTRY *e;
                    const SYMBOL *sym;
                    if (g_invindex.nent >= g_invindex.cap)
                    {
                        g_invindex.cap *= 2;
                        g_invindex.entries = (TL_INVENTRY *)realloc(
                            g_invindex.entries,
                            g_invindex.cap * sizeof(TL_INVENTRY));
                    }
                    e = &g_invindex.entries[g_invindex.nent];
                    e->sym = id;
                    e->sents = (uint32_t *)malloc(16 * sizeof(uint32_t));
                    e->sents[0] = si;
                    e->nsents = 1;
                    e->cap = 16;
                    sym = SymbolGet(graph ? graph->symbols : NULL, id);
                    e->novelty = (sym == NULL) ? 1.0f
                                              : 1.0f / (1.0f + (float)sym->frequency);
                    g_invindex.nent++;
                }
            }
        }
    }
}

static int IsWordByte(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c >= 0x80;
}

static int IsSpaceByte(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static int IsCtrlByte(unsigned char c)
{
    return c < 0x20 && !IsSpaceByte(c);
}

static void StatsDrop(TEXTLEX_STATS *st, const unsigned char *p,
                      size_t n)
{
    uint32_t i;
    size_t k;
    char tmp[TL_DROP_LEN];
    st->toolong_dropped++;
    if (st->dropped_distinct >= TL_DROP_SAMPLE)
        return;
    k = n < (size_t)(TL_DROP_LEN - 1) ? n : (size_t)(TL_DROP_LEN - 1);
    memcpy(tmp, p, k);
    tmp[k] = '\0';
    for (i = 0; i < st->dropped_distinct; i++)
        if (strcmp(st->dropped_sample[i], tmp) == 0)
            return;
    strcpy(st->dropped_sample[st->dropped_distinct], tmp);
    st->dropped_distinct++;
}

static int TokBufPush(TL_TOKBUF *b, uint64_t off, uint32_t len,
                      uint32_t line)
{
    TL_RAWTOK *nt;
    if (b->ntok >= b->cap)
    {
        uint32_t ncap = b->cap == 0 ? 256 : b->cap * 2;
        nt = (TL_RAWTOK *)realloc(b->toks, ncap * sizeof(TL_RAWTOK));
        if (nt == NULL)
            return 0;
        b->toks = nt;
        b->cap = ncap;
    }
    b->toks[b->ntok].off = off;
    b->toks[b->ntok].len = len;
    b->toks[b->ntok].line = line;
    b->ntok++;
    return 1;
}

/* Lex one paragraph span [a,b) of the image. Words byte-exact
   (no folding); interior '/- kept between word bytes; every other
   printable byte is a single-char token (punctuation is signal). */
static void LexSpan(const unsigned char *img, uint64_t a, uint64_t b,
                    const uint64_t *linestarts, uint32_t nlines,
                    uint32_t *cursor, TL_TOKBUF *out,
                    TEXTLEX_STATS *st)
{
    uint64_t p = a;
    (void)nlines;
    while (p < b)
    {
        unsigned char c = img[p];
        if (IsSpaceByte(c))
        {
            p++;
            continue;
        }
        if (IsCtrlByte(c))
        {
            st->ctrl_skipped++;
            p++;
            continue;
        }
        while (*cursor + 1 < nlines && linestarts[*cursor + 1] <= p)
            (*cursor)++;
        if (IsWordByte(c))
        {
            uint64_t s = p;
            while (p < b)
            {
                unsigned char d = img[p];
                if (IsWordByte(d))
                {
                    p++;
                    continue;
                }
                if ((d == '\'' || d == '-') && p + 1 < b &&
                    IsWordByte(img[p + 1]) && p > s)
                {
                    p += 2;
                    continue;
                }
                break;
            }
            if (p - s >= (uint64_t)TL_WORD_MAX)
            {
                StatsDrop(st, img + s, (size_t)(p - s));
                continue;
            }
            TokBufPush(out, s, (uint32_t)(p - s), *cursor + 1);
        }
        else
        {
            /* single printable punctuation token */
            TokBufPush(out, p, 1, *cursor + 1);
            p++;
        }
    }
}

static int IsTermTok(const unsigned char *img, uint64_t off,
                     uint32_t len)
{
    return len == 1 && (img[off] == '.' || img[off] == '?' ||
                        img[off] == '!');
}

/* Peek after a terminator, skipping spaces and closers/openers.
   Boundary on upper/digit/high-byte/EOL. Structural, no lists. */
static int IsBoundary(const unsigned char *img, uint64_t end,
                      uint64_t b)
{
    uint64_t p = end;
    unsigned char c;
    for (;;)
    {
        if (p >= b)
            return 1;
        c = img[p];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' ||
            c == '"' || c == '\'' || c == ')' || c == ']' ||
            c == '}' || c == '(' || c == '[' || c == '{')
        {
            p++;
            continue;
        }
        /* U+201D ” and U+2019 ’ closers */
        if (c == 0xE2 && p + 2 < b && img[p + 1] == 0x80 &&
            (img[p + 2] == 0x9D || img[p + 2] == 0x99))
        {
            p += 3;
            continue;
        }
        break;
    }
    if (p >= b)
        return 1;
    c = img[p];
    return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           c >= 0x80;
}

/* deterministic zero init (vectors start empty; counts below
   are compiled facts, never fitted) */
static float *EnsureZero(EMBEDDING_TABLE *emb, SYMBOL_ID id)
{
    const float *v;
    float tmp[EMBEDDING_DIM];
    uint32_t d;
    if (emb == NULL || id == SYMBOL_INVALID)
        return NULL;
    v = EmbeddingGetVector(emb, id);
    if (v != NULL)
        return (float *)v;
    for (d = 0; d < EMBEDDING_DIM; d++)
        tmp[d] = 0.0f;
    if (!EmbeddingSetVector(emb, id, tmp))
        return NULL;
    return (float *)EmbeddingGetVector(emb, id);
}

/* co-count bump: symbol a co-occurred with b (exact, commutative
   per order; deterministic replay). */
static void CoCount(EMBEDDING_TABLE *emb, SYMBOL_ID a, SYMBOL_ID b,
                    TEXTLEX_STATS *st)
{
    float *va;
    uint32_t h;
    va = EnsureZero(emb, a);
    if (va == NULL)
        return;
    h = (uint32_t)(((uint64_t)b * 2654435761u) >> 16) % EMBEDDING_DIM;
    va[h] += 1.0f;
    st->pair_updates++;
}

/* co-occurrence hygiene (structural classes only; storage
   untouched): a token counts iff it holds ASCII-alnum content
   and is neither a pure-digit run nor a single alpha char
   (reference marks, page numbers, initials stay as symbols
   with frequencies, but never shape distributional vectors). */
static int IsCountable(const unsigned char *p, uint32_t n)
{
    uint32_t i;
    int alnum = 0;
    int nondigit = 0;
    if (p == NULL || n == 0)
        return 0;
    for (i = 0; i < n; i++)
    {
        unsigned char c = p[i];
        int a = ((c >= 'A' && c <= 'Z') ||
                 (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                 c >= 0x80);
        if (a)
            alnum = 1;
        if (!((c >= '0' && c <= '9')))
            nondigit = 1;
    }
    if (!alnum || !nondigit)
        return 0;
    if (n == 1)
    {
        unsigned char c = p[0];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
            return 0;
    }
    return 1;
}

static int SentDup(const TEXTLEX *tl, uint64_t first_off,
                   uint32_t ntok, const SYMBOL_ID *ids)
{
    uint32_t i;
    for (i = 0; i < tl->nsent; i++)
    {
        const TL_SENT *s = &tl->sents[i];
        if (s->ntok == ntok && s->offs[0] == first_off &&
            memcmp(s->ids, ids, ntok * sizeof(SYMBOL_ID)) == 0)
            return 1;
    }
    return 0;
}

static int SentStore(TEXTLEX *tl, const TL_RAWTOK *toks, uint32_t ntok,
                     const SYMBOL_ID *ids, const unsigned char *img)
{
    TL_SENT *s;
    uint32_t i;
    if (tl->nsent >= tl->cap)
    {
        uint32_t ncap = tl->cap == 0 ? 1024 : tl->cap * 2;
        TL_SENT *ns = (TL_SENT *)realloc(tl->sents,
                                         ncap * sizeof(TL_SENT));
        if (ns == NULL)
            return 0;
        tl->sents = ns;
        tl->cap = ncap;
    }
    s = &tl->sents[tl->nsent];
    memset(s, 0, sizeof(*s));
    s->offs = (uint64_t *)malloc(ntok * sizeof(uint64_t));
    s->lens = (uint32_t *)malloc(ntok * sizeof(uint32_t));
    s->ids = (SYMBOL_ID *)malloc(ntok * sizeof(SYMBOL_ID));
    s->lines = (uint32_t *)malloc(ntok * sizeof(uint32_t));
    if (s->offs == NULL || s->lens == NULL || s->ids == NULL ||
        s->lines == NULL)
    {
        SentFree(s);
        return 0;
    }
    for (i = 0; i < ntok; i++)
    {
        s->offs[i] = toks[i].off;
        s->lens[i] = toks[i].len;
        s->ids[i] = ids[i];
        s->lines[i] = toks[i].line;
    }
    s->ntok = ntok;
    s->cap = ntok;
    /* lexical density: tokens carrying 3+ alpha bytes over total.
       Grammatical sentences score near 1; index/TOC lines full
       of numbers and punctuation score low. Structural only. */
    {
        uint32_t k;
        uint32_t lex = 0;
        for (k = 0; k < ntok; k++)
        {
            const unsigned char *tp = img + toks[k].off;
            uint32_t m;
            uint32_t na = 0;
            for (m = 0; m < toks[k].len; m++)
            {
                unsigned char c = tp[m];
                if ((c >= 'A' && c <= 'Z') ||
                    (c >= 'a' && c <= 'z') || c >= 0x80)
                    na++;
            }
            if (na >= 3)
                lex++;
        }
        s->density = ntok > 0 ? (float)lex / (float)ntok : 0.0f;
    }
    tl->nsent++;
    return 1;
}

/* Ingest token slice [a,n) as one sentence: intern, dedup,
   store, count. Truth transferred, never trained: no vector
   updates here, only deterministic init + frequency facts. */
static void IngestSentence(GRAPH *graph, EMBEDDING_TABLE *emb,
                           TEXTLEX *tl, const unsigned char *img,
                           const TL_RAWTOK *toks, uint32_t n,
                           uint32_t *covered, uint32_t maxline,
                           TEXTLEX_STATS *st)
{
    SYMBOL_ID *ids;
    uint32_t i;
    uint32_t syms0;
    char word[TL_WORD_MAX];
    if (n == 0)
        return;
    ids = (SYMBOL_ID *)malloc(n * sizeof(SYMBOL_ID));
    if (ids == NULL)
        return;
    syms0 = SymbolCount(graph->symbols);
    for (i = 0; i < n; i++)
    {
        uint32_t L = toks[i].len < (uint32_t)(TL_WORD_MAX - 1)
                         ? toks[i].len
                         : (uint32_t)(TL_WORD_MAX - 1);
        /* tokens longer than the cap never reach here (lexer
           drops them); truncation is a belt-and-braces guard */
        memcpy(word, img + toks[i].off, L);
        word[L] = '\0';
        ids[i] = GraphAddSymbol(graph, word);
        if (toks[i].line >= 1 && toks[i].line <= maxline)
            covered[toks[i].line - 1] = 1;
    }
    if (SentDup(tl, toks[0].off, n, ids))
    {
        st->nsent_dup++;
        free(ids);
        return;
    }
    if (!SentStore(tl, toks, n, ids, img))
    {
        free(ids);
        return;
    }
    st->nsent_new++;
    st->syms_new += SymbolCount(graph->symbols) - syms0;
    /* frequency truth: init 1 covers a fresh symbol's first
       occurrence; every other occurrence increments exactly once.
       Distributional transfer (exact counts, never fitted):
       sentence-wide co-occurrence plus word@position, every
       ordered pair both present. Commutative per order. */
    for (i = 0; i < n; i++)
    {
        uint32_t j;
        int first_here = 1;
        if (ids[i] == SYMBOL_INVALID)
            continue;
        EnsureZero(emb, ids[i]);
        if (ids[i] > syms0)
        {
            for (j = 0; j < i; j++)
            {
                if (ids[j] == ids[i])
                {
                    first_here = 0;
                    break;
                }
            }
            if (first_here)
            {
                st->ntok_kept++;
                continue;
            }
        }
        SymbolIncrementFrequency(graph->symbols, ids[i]);
        st->ntok_kept++;
    }
    for (i = 0; i < n; i++)
    {
        uint32_t j;
        uint32_t h;
        int countable;
        if (ids[i] == SYMBOL_INVALID)
            continue;
        countable = IsCountable(img + toks[i].off, toks[i].len);
        if (!countable)
            continue;
        for (j = 0; j < n; j++)
        {
            if (i == j || ids[j] == SYMBOL_INVALID)
                continue;
            if (!IsCountable(img + toks[j].off, toks[j].len))
                continue;
            CoCount(emb, ids[i], ids[j], st);
        }
        h = (uint32_t)(((uint64_t)POSBUCKET(i) * 2654435761u) >> 16) %
            EMBEDDING_DIM;
        {
            float *va = EnsureZero(emb, ids[i]);
            if (va != NULL)
            {
                va[h] += 1.0f;
                st->pair_updates++;
            }
        }
        g_posvec[i % TL_POS_MAX][(uint32_t)(((uint64_t)ids[i] *
                                            2654435761u) >>
                                           16) %
                                 EMBEDDING_DIM] += 1.0f;
        st->pair_updates++;
    }
    free(ids);
}

TEXTLEX_STATS TextLexIngest(GRAPH *graph, TEXTLEX *tl,
                            const char *path,
                            uint32_t first_line, uint32_t last_line)
{
    TEXTLEX_STATS st;
    FILE *f;
    long fsize;
    unsigned char *img;
    size_t got;
    uint64_t *linestarts;
    uint32_t nlines;
    uint32_t caplines;
    uint64_t p;
    uint32_t i;
    uint32_t *covered;
    uint32_t maxline;
    TL_TOKBUF para;
    uint32_t cursor;
    uint64_t sent_start;
    uint32_t k;
    EMBEDDING_TABLE *emb;
    memset(&st, 0, sizeof(st));
    if (graph == NULL || tl == NULL || path == NULL)
        return st;
    emb = graph->embeddings;
    f = fopen(path, "rb");
    if (f == NULL)
        return st;
    fseek(f, 0, SEEK_END);
    fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize <= 0)
    {
        fclose(f);
        return st;
    }
    img = (unsigned char *)malloc((size_t)fsize);
    if (img == NULL)
    {
        fclose(f);
        return st;
    }
    got = fread(img, 1, (size_t)fsize, f);
    fclose(f);
    if (got != (size_t)fsize)
    {
        free(img);
        return st;
    }
    st.bytes_read = (uint64_t)got;
    /* keep one image per store for literal display (session) */
    if (tl->image == NULL)
    {
        tl->image = (unsigned char *)malloc((size_t)fsize);
        if (tl->image != NULL)
        {
            memcpy(tl->image, img, (size_t)fsize);
            tl->imagelen = (size_t)fsize;
        }
    }
    /* line table */
    caplines = 4096;
    linestarts =
        (uint64_t *)malloc(caplines * sizeof(uint64_t));
    if (linestarts == NULL)
    {
        free(img);
        return st;
    }
    nlines = 1;
    linestarts[0] = 0;
    for (p = 0; p < (uint64_t)fsize; p++)
    {
        if (img[p] == '\n')
        {
            if (nlines >= caplines)
            {
                uint64_t *nl;
                caplines *= 2;
                nl = (uint64_t *)realloc(
                    linestarts, caplines * sizeof(uint64_t));
                if (nl == NULL)
                {
                    free(linestarts);
                    free(img);
                    return st;
                }
                linestarts = nl;
            }
            linestarts[nlines++] = p + 1;
        }
    }
    if (last_line == 0 || last_line > nlines)
        maxline = nlines;
    else
        maxline = last_line;
    if (first_line < 1)
        first_line = 1;
    covered = (uint32_t *)calloc(maxline, sizeof(uint32_t));
    if (covered == NULL)
    {
        free(linestarts);
        free(img);
        return st;
    }
    memset(&para, 0, sizeof(para));
    cursor = 0;
    sent_start = 0;
    /* walk lines in range, paragraphs = non-blank runs */
    for (i = first_line; i <= maxline; i++)
    {
        uint64_t a = linestarts[i - 1];
        uint64_t b = (i < nlines) ? linestarts[i] : (uint64_t)fsize;
        uint64_t q = a;
        int blank = 1;
        st.lines_seen++;
        while (q < b)
        {
            if (img[q] != ' ' && img[q] != '\t' && img[q] != '\r' &&
                img[q] != '\n')
            {
                blank = 0;
                break;
            }
            q++;
        }
        if (blank)
        {
            /* paragraph boundary: split pending tokens */
            if (para.ntok > 0)
            {
                /* fall through to sentence flush below */
            }
            else
                continue;
        }
        else
        {
            uint32_t before = para.ntok;
            st.lines_content++;
            LexSpan(img, a, b, linestarts, nlines, &cursor, &para,
                    &st);
            if (para.ntok == before)
                continue;
        }
        /* boundary or end: split paragraph buffer into sentences.
           Peek runs to the end of the buffered paragraph so
           wrapped lines never cut a sentence. */
        if (blank || i == maxline)
        {
            uint64_t pend;
            k = 0;
            sent_start = 0;
            if (para.ntok == 0)
                continue;
            pend = para.toks[para.ntok - 1].off +
                   para.toks[para.ntok - 1].len;
            while (k < para.ntok)
            {
                uint64_t tend;
                /* find next terminator from k */
                uint32_t j = k;
                int found = 0;
                while (j < para.ntok)
                {
                    if (IsTermTok(img, para.toks[j].off,
                                  para.toks[j].len))
                    {
                        tend = para.toks[j].off +
                               para.toks[j].len;
                        if (IsBoundary(img, tend, pend))
                        {
                            found = 1;
                            break;
                        }
                    }
                    j++;
                }
                if (found)
                {
                    IngestSentence(graph, emb, tl, img,
                                   para.toks + sent_start,
                                   (j - sent_start) + 1, covered,
                                   maxline, &st);
                    sent_start = j + 1;
                    k = j + 1;
                }
                else
                    break;
            }
            if (sent_start < para.ntok)
            {
                IngestSentence(graph, emb, tl, img,
                               para.toks + sent_start,
                               para.ntok - sent_start, covered,
                               maxline, &st);
            }
            para.ntok = 0;
        }
    }
    /* coverage: content lines spanned */
    for (i = first_line; i <= maxline; i++)
    {
        uint64_t a = linestarts[i - 1];
        uint64_t b = (i < nlines) ? linestarts[i] : (uint64_t)fsize;
        uint64_t q = a;
        int blank = 1;
        while (q < b)
        {
            if (img[q] != ' ' && img[q] != '\t' && img[q] != '\r' &&
                img[q] != '\n')
            {
                blank = 0;
                break;
            }
            q++;
        }
        if (!blank && covered[i - 1])
            st.lines_covered++;
    }
    st.lines_content = 0;
    for (i = first_line; i <= maxline; i++)
    {
        uint64_t a = linestarts[i - 1];
        uint64_t b = (i < nlines) ? linestarts[i] : (uint64_t)fsize;
        uint64_t q = a;
        while (q < b)
        {
            if (img[q] != ' ' && img[q] != '\t' && img[q] != '\r' &&
                img[q] != '\n')
            {
                st.lines_content++;
                break;
            }
            q++;
        }
    }
    free(covered);
    free(para.toks);
    free(linestarts);
    free(img);
    FinalizeSigs(tl, graph, emb);
    return st;
}

static int IsAllUpperStr(const char *s)
{
    if (s == NULL || s[0] == '\0')
        return 0;
    for (size_t i = 0; s[i] != '\0'; i++)
    {
        if (s[i] >= 'a' && s[i] <= 'z')
            return 0;
    }
    return 1;
}

/* Concept concentration discovery: key topics discovered from the embedding topology.
   Peaked count vectors = focused topic; flat count vectors = glue / scaffolding.
   Reads the relation vectors directly; deterministic and structural. */
uint32_t TextLexTopConcepts(const GRAPH *graph, const EMBEDDING_TABLE *emb,
                            TL_CONCEPT *out, uint32_t max_out)
{
    if (graph == NULL || graph->symbols == NULL || emb == NULL || out == NULL || max_out == 0)
        return 0;

    uint32_t nsyms = SymbolCount(graph->symbols);
    uint32_t ncs = 0, ccap = 0;
    TL_CONCEPT *cs = NULL;

    for (uint32_t i = 1; i <= nsyms; i++)
    {
        const SYMBOL *ss = SymbolGet(graph->symbols, i);
        if (ss == NULL || ss->frequency < 5 || ss->name == NULL)
            continue;

        /* Lexical filter: genuine words only (letter start, length >= 3).
           Reference marks, digits and initials stay in storage, out of concept ranking. */
        unsigned char c0 = (unsigned char)ss->name[0];
        if (!((c0 >= 'a' && c0 <= 'z') || (c0 >= 'A' && c0 <= 'Z')))
            continue;
        if (strlen(ss->name) < 3)
            continue;
        /* Ignore all-uppercase section markers / roman numerals (PART, CHAPTER, XII) */
        if (IsAllUpperStr(ss->name))
            continue;

        const float *v = EmbeddingGetVector(emb, i);
        if (v == NULL)
            continue;

        float tot = 0.0f, mx = 0.0f;
        for (uint32_t d = 0; d < EMBEDDING_DIM; d++)
        {
            tot += v[d];
            if (v[d] > mx)
                mx = v[d];
        }
        if (tot <= 0.0f)
            continue;

        float conc = mx / tot;
        /* Skip fixed formatting/template artifacts with extreme single-bucket dominance */
        if (conc >= 0.95f)
            continue;

        if (ncs >= ccap)
        {
            uint32_t nc = ccap == 0 ? 1024 : ccap * 2;
            TL_CONCEPT *nn = (TL_CONCEPT *)realloc(cs, nc * sizeof(TL_CONCEPT));
            if (nn == NULL)
                break;
            cs = nn;
            ccap = nc;
        }

        cs[ncs].id = i;
        cs[ncs].name = ss->name;
        cs[ncs].conc = conc;
        cs[ncs].freq = ss->frequency;
        ncs++;
    }

    if (ncs == 0)
    {
        free(cs);
        return 0;
    }

    /* Rank top max_out by concentration (selection sort) */
    uint32_t limit = ncs < max_out ? ncs : max_out;
    for (uint32_t k = 0; k < limit; k++)
    {
        uint32_t m = k;
        for (uint32_t j = k + 1; j < ncs; j++)
        {
            if (cs[j].conc > cs[m].conc ||
                (cs[j].conc == cs[m].conc && cs[j].freq > cs[m].freq))
                m = j;
        }
        if (m != k)
        {
            TL_CONCEPT t = cs[k];
            cs[k] = cs[m];
            cs[m] = t;
        }
        out[k] = cs[k];
    }

    free(cs);
    return limit;
}

/* Direct symbol lookup: find first sentence containing target_id.
   O(n) integer scan — no embeddings, no TF-IDF, no case variants.
   Returns sentence index or UINT32_MAX if not found. */
uint32_t TextLexFindSentenceBySymbol(const TEXTLEX *tl, SYMBOL_ID target_id)
{
    uint32_t s, t;
    if (tl == NULL || target_id == SYMBOL_INVALID)
        return UINT32_MAX;
    for (s = 0; s < tl->nsent; s++)
    {
        TL_SENT *st = &tl->sents[s];
        for (t = 0; t < st->ntok; t++)
        {
            if (st->ids[t] == target_id)
                return s;
        }
    }
    return UINT32_MAX;
}
