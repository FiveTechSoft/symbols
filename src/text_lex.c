/* text_lex.c — corpus-to-graph converter. See text_lex.h doctrine. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "text_lex.h"
#include "symbol.h"
#include "embedding.h"

#define TL_POS_MAX 4096

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

void TextLexClear(TEXTLEX *tl)
{
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
            if (g_scoring & SF_RARITY)
                total += qnov[i];
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
    return total;
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
            s = SymbolGet(graph->symbols, id);
            qnov[nq] = (s == NULL)
                           ? 1.0f
                           : 1.0f / (1.0f + (float)s->frequency);
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
        TopDims(qsum, qsig, TL_TOPM);
        for (i = 0; i < tl->nsent; i++)
        {
            float sc;
            uint32_t j;
            sc = QKVScore(&tl->sents[i], qids, qnov, qpos, nq,
                          qsig, graph, emb);
        if (sc <= 0.0f)
            continue;
        /* insertion rank, max-independent: top[0] is always the
           global max (replace the minimum, bubble from there) */
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
        TopDims(cent, s->sig, TL_TOPM);
        s->sig_ready = 1;
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
                     const SYMBOL_ID *ids)
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
    if (!SentStore(tl, toks, n, ids))
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
