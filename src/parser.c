#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "parser.h"
#include "compat.h"
#include "stem.h"
#include "embedding.h"
#include "learning.h"

/* Forward: descriptor and entity resolution live with the query
   path below; the ingest tree above needs them. */
static int ResolveRelationPass(const GRAPH *graph, const char *token,
                                char *out, size_t out_size,
                                int *out_trusted);
static int ResolveEntity(const GRAPH *graph, const PARSED_SENTENCE *tokens,
                         uint32_t start, char *out, size_t out_size);
static uint64_t TokenRarity(const GRAPH *graph, const char *token);
static uint64_t EffFreq(const GRAPH *graph, const char *token);

/* ============================================================
   Utility
   ============================================================ */

static void ToUpperCopy(const char *src, char *dst, size_t dst_size)
{
    size_t i;
    for (i = 0; src[i] && i < dst_size - 1; i++)
        dst[i] = (char)toupper((unsigned char)src[i]);
    dst[i] = '\0';
}

/* ============================================================
   Tokenizer
   ============================================================ */

int ParserTokenize(const char *input, PARSED_SENTENCE *out)
{
    if (input == NULL || out == NULL)
        return 0;

    memset(out, 0, sizeof(*out));

    char buffer[1024];
    strncpy(buffer, input, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    NormalizeDiacritics(buffer);

    char *saveptr = NULL;
    char *token = strtok_r(buffer, " \t\n,;:!?.()\"'¿¡", &saveptr);

    while (token != NULL && out->count < PARSER_MAX_TOKENS)
    {
        if (strlen(token) > 0)
        {
            ToUpperCopy(token, out->tokens[out->count], 64);
            out->count++;
        }
        token = strtok_r(NULL, " \t\n,;:!?.()\"'¿¡", &saveptr);
    }

    return (int)out->count;
}

/* Span to symbol for INGEST: nearest-to-relation wins. In statements
   arguments sit adjacent to their relation (SVO adjacency), so the
   leading run wins in right spans and the trailing run in left spans.
   (Query entities behave the opposite way: see ResolveEntity.)
   Unknown spans join whole: symbols are generated. */
static int SpanToSymbol(const GRAPH *graph, const PARSED_SENTENCE *tokens,
                        uint32_t start, uint32_t end, int trailing,
                        char *out, size_t out_size)
{
    if (graph == NULL || tokens == NULL || out == NULL || out_size == 0)
        return -1;
    if (start >= end || end > tokens->count)
        return -1;

    uint32_t span = end - start;
    char cand[128];
    for (uint32_t len = (span < 4 ? span : 4); len >= 1; len--)
    {
        if (trailing)
        {
            uint32_t s = end - len + 1;
            while (s > start)
            {
                s--;
                cand[0] = '\0';
                for (uint32_t k = 0; k < len; k++)
                {
                    if (k > 0) strcat(cand, "_");
                    strcat(cand, tokens->tokens[s + k]);
                }
                if (SymbolFind(graph->symbols, cand) != SYMBOL_INVALID)
                {
                    strncpy(out, cand, out_size - 1);
                    out[out_size - 1] = '\0';
                    return (int)s;
                }
            }
        }
        else
        {
            for (uint32_t s = start; s + len <= end; s++)
            {
                cand[0] = '\0';
                for (uint32_t k = 0; k < len; k++)
                {
                    if (k > 0) strcat(cand, "_");
                    strcat(cand, tokens->tokens[s + k]);
                }
                if (SymbolFind(graph->symbols, cand) != SYMBOL_INVALID)
                {
                    strncpy(out, cand, out_size - 1);
                    out[out_size - 1] = '\0';
                    return (int)s;
                }
            }
        }
    }

    /* Purity tier: longest run using only the span's rarest tier.
       Known glue (DE, EL) carries huge frequency; novel content sits
       at zero. This keeps MARIA out of DE_MARIA without naming either. */
    {
        uint64_t floor = UINT64_MAX;
        for (uint32_t k = start; k < end; k++)
        {
            uint64_t r = TokenRarity(graph, tokens->tokens[k]);
            if (r < floor)
                floor = r;
        }
        for (uint32_t len = (span < 4 ? span : 4); len >= 1; len--)
        {
            for (uint32_t s = start; s + len <= end; s++)
            {
                int pure = 1;
                for (uint32_t k = 0; k < len; k++)
                {
                    if (TokenRarity(graph, tokens->tokens[s + k]) != floor)
                    {
                        pure = 0;
                        break;
                    }
                }
                if (!pure)
                    continue;
                cand[0] = '\0';
                for (uint32_t k = 0; k < len; k++)
                {
                    if (k > 0) strcat(cand, "_");
                    strcat(cand, tokens->tokens[s + k]);
                }
                strncpy(out, cand, out_size - 1);
                out[out_size - 1] = '\0';
                return (int)s;
            }
            if (len == 1)
                break;
        }
    }

    out[0] = '\0';
    for (uint32_t k = start; k < end; k++)
    {
        if (k > start) strcat(out, "_");
        strncat(out, tokens->tokens[k], out_size - strlen(out) - 1);
    }
    return (int)start;
}

/* Clause boundaries: local frequency maxima with content on both
   sides. Glue betrays itself by repetition (Zipf), never by name: a
   token far more frequent than both neighbors splits the input, and
   belongs to neither clause. On virgin maps nothing splits (graceful
   degradation to one clause). Bounds-checked interior only, so edge
   articles can never shatter an entity. */
static uint32_t FindSplits(const GRAPH *graph, const PARSED_SENTENCE *tokens,
                           uint32_t *outs, uint32_t maxouts)
{
    uint32_t n = 0;
    if (graph == NULL || tokens == NULL || outs == NULL || maxouts == 0)
        return 0;
    if (tokens->count < 5)
        return 0;

    for (uint32_t i = 2; i + 2 < tokens->count && n < maxouts; i++)
    {
        uint64_t f = EffFreq(graph, tokens->tokens[i]);
        if (f == 0)
            continue;
        uint64_t fl = EffFreq(graph, tokens->tokens[i - 1]);
        uint64_t fr = EffFreq(graph, tokens->tokens[i + 1]);
        if (f > fl && f > fr)
            outs[n++] = i;
    }
    return n;
}

/* Session token census: raw occurrence counts per token hash. Glue
   never stands alone as a symbol, so graph frequencies miss it; the
   census sees every token as read, letting splitters find coordinators
   (Y) once they recur. Saturating, order-deterministic per input.
   Effective frequency = max(graph, census). */
#define CENSUS_BUCKETS 65536
static uint32_t tok_census[CENSUS_BUCKETS];

static uint32_t CensusHash(const char *s)
{
    uint32_t h = 5381u;
    while (s && *s)
    {
        h = h * 33u + (unsigned char)*s;
        s++;
    }
    return h % CENSUS_BUCKETS;
}

static void CensusAdd(const char *token)
{
    if (token == NULL)
        return;
    uint32_t b = CensusHash(token);
    if (tok_census[b] < 60000u)
        tok_census[b]++;
}

static uint64_t EffFreq(const GRAPH *graph, const char *token)
{
    uint64_t gf = TokenRarity(graph, token);
    uint64_t cf = (uint64_t)tok_census[CensusHash(token)];
    return (gf > cf) ? gf : cf;
}
static uint64_t TokenRarity(const GRAPH *graph, const char *token)
{
    if (graph == NULL || token == NULL)
        return 0;
    SYMBOL_ID id = SymbolFind(graph->symbols, token);
    if (id == SYMBOL_INVALID)
        return 0;
    const SYMBOL *s = SymbolGet(graph->symbols, id);
    return (s != NULL) ? s->frequency : 0;
}

int ParserIngestSentence(GRAPH *graph, const char *sentence)
{
    if (graph == NULL || sentence == NULL)
        return 0;

    PARSED_SENTENCE tokens;
    int n_tokens = ParserTokenize(sentence, &tokens);

    if (n_tokens < 2)
        return 0;

    /* Every token read feeds the session census, so recurrence itself
       becomes visible (coordinators never stand alone as symbols). */
    for (uint32_t ci = 0; ci < tokens.count; ci++)
        CensusAdd(tokens.tokens[ci]);

    /* Segments first: split at local frequency maxima (glue betrayed
       by repetition), then parse each segment recursively on its own.
       Short inputs never split; virgin maps never split. */
    {
        uint32_t splits[8];
        uint32_t nsplit = FindSplits(graph, &tokens, splits, 8);
        if (nsplit > 0)
        {
            int stored = 0;
            char buf[4096];
            uint32_t seg_a = 0;
            for (uint32_t sg = 0; ; sg++)
            {
                uint32_t seg_b = (sg < nsplit) ? splits[sg] : tokens.count;
                if (seg_b > seg_a + 1)
                {
                    buf[0] = '\0';
                    for (uint32_t k = seg_a; k < seg_b; k++)
                    {
                        if (k > seg_a) strcat(buf, " ");
                        strncat(buf, tokens.tokens[k],
                                sizeof(buf) - strlen(buf) - 1);
                    }
                    stored += ParserIngestSentence(graph, buf);
                }
                if (sg >= nsplit)
                    break;
                seg_a = splits[sg] + 1;
            }
            return stored;
        }
    }

    /* Clause roots: tokens describing used relations (exact, then
       stemmed), in token order. Each root splits the input by position,
       so nested clauses each contribute their relation. Untrusted
       (sourceless) roots are dropped when any trusted root exists:
       bulk-grown noise must not partition clean input. */
    uint32_t roots[8];
    char names[8][64];
    int trusted[8];
    uint32_t nroots = 0;
    for (uint32_t i = 0; i < tokens.count && nroots < 8; i++)
    {
        int known = 0;
        for (uint32_t k = 0; k < nroots; k++)
            if (roots[k] == i) { known = 1; break; }
        if (known)
            continue;
        int tr = 0;
        if (ResolveRelationPass(graph, tokens.tokens[i],
                                 names[nroots], sizeof(names[0]), &tr))
        {
            roots[nroots] = i;
            trusted[nroots] = tr;
            nroots++;
        }
    }
    {
        int any_trusted = 0;
        for (uint32_t k = 0; k < nroots; k++)
            if (trusted[k]) { any_trusted = 1; break; }
        if (any_trusted)
        {
            uint32_t w = 0;
            for (uint32_t k = 0; k < nroots; k++)
            {
                if (!trusted[k])
                    continue;
                if (w != k)
                {
                    roots[w] = roots[k];
                    strcpy(names[w], names[k]);
                    trusted[w] = trusted[k];
                }
                w++;
            }
            nroots = w;
        }
    }

    /* Bootstrap on a virgin map: the rarest token nearest the middle
       roots the single clause. Documented heuristic for first contact
       only; the live vocabulary takes over from the second sentence. */
    if (nroots == 0)
    {
        uint32_t mid = tokens.count / 2;
        uint32_t best = mid;
        uint64_t best_r = TokenRarity(graph, tokens.tokens[mid]);
        uint32_t best_d = 0;
        for (uint32_t i = 0; i < tokens.count; i++)
        {
            uint64_t r = TokenRarity(graph, tokens.tokens[i]);
            uint32_t d = (i > mid) ? i - mid : mid - i;
            if (r < best_r || (r == best_r && d < best_d))
            {
                best_r = r;
                best_d = d;
                best = i;
            }
        }
        strncpy(names[0], tokens.tokens[best], sizeof(names[0]) - 1);
        names[0][sizeof(names[0]) - 1] = '\0';
        roots[0] = best;
        nroots = 1;
    }

    /* One relation per clause root: entities left and right. */
    int stored = 0;
    for (uint32_t c = 0; c < nroots; c++)
    {
        uint32_t left_a = (c == 0) ? 0 : roots[c - 1] + 1;
        uint32_t left_b = roots[c];
        uint32_t right_a = roots[c] + 1;
        uint32_t right_b = (c + 1 < nroots) ? roots[c + 1] : tokens.count;
        if (left_a >= left_b || right_a >= right_b)
            continue;

        char obj[128] = {0};
        if (SpanToSymbol(graph, &tokens, right_a, right_b, 0,
                         obj, sizeof(obj)) < 0)
            continue;

        /* Left-span regions: coordinated subjects each contribute one
           triple sharing predicate and object. Cuts at interior
           effective-frequency maxima (the same Zipf glue signal as
           clause splits, one level down). */
        uint32_t cuts[8];
        uint32_t ncut = 0;
        for (uint32_t k = left_a + 1; k + 1 < left_b && ncut < 8; k++)
        {
            uint64_t f = EffFreq(graph, tokens.tokens[k]);
            if (f == 0)
                continue;
            if (f > EffFreq(graph, tokens.tokens[k - 1]) &&
                f > EffFreq(graph, tokens.tokens[k + 1]))
                cuts[ncut++] = k;
        }

        uint32_t r0 = left_a;
        for (uint32_t r = 0; ; r++)
        {
            uint32_t r1 = (r < ncut) ? cuts[r] : left_b;
            if (r1 > r0)
            {
                char subj[128] = {0};
                if (SpanToSymbol(graph, &tokens, r0, r1, 1,
                                 subj, sizeof(subj)) >= 0)
                {
                    SYMBOL_ID s = GraphAddSymbol(graph, subj);
                    SYMBOL_ID p = GraphAddSymbol(graph, names[c]);
                    SYMBOL_ID o = GraphAddSymbol(graph, obj);

                    if (s != SYMBOL_INVALID && p != SYMBOL_INVALID &&
                        o != SYMBOL_INVALID &&
                        GraphAddRelation(graph, s, p, o))
                    {
                        stored++;

                        /* Update embeddings on every co-occurrence */
                        if (graph->embeddings != NULL)
                        {
                            EMBEDDING_TABLE *emb = graph->embeddings;

                            if (EmbeddingGetVector(emb, s) == NULL)
                            {
                                float v[EMBEDDING_DIM];
                                EmbeddingRandomInit(
                                    v, (uint32_t)s * 2654435761u);
                                EmbeddingSetVector(emb, s, v);
                            }
                            if (EmbeddingGetVector(emb, o) == NULL)
                            {
                                float v[EMBEDDING_DIM];
                                EmbeddingRandomInit(
                                    v, (uint32_t)o * 2654435761u);
                                EmbeddingSetVector(emb, o, v);
                            }

                            float *target =
                                (float *)EmbeddingGetVector(emb, s);
                            float *context =
                                (float *)EmbeddingGetVector(emb, o);
                            if (target && context)
                            {
                                EmbeddingCooccur(target, context, 0.1f);
                                EmbeddingCooccur(context, target, 0.1f);
                                EmbeddingNormalize(target);
                                EmbeddingNormalize(context);
                            }
                        }
                    }
                }
            }
            if (r >= ncut)
                break;
            r0 = cuts[r] + 1;
        }
    }

    return stored;
}

/* Tracked ingest: preprocess through the discourse context, ingest,
   then push the stored entities back (subjects first). New triples
   append in storage order, so the [base, total) range is exactly what
   this call stored. Later sentences resolve pronouns against them. */
int ParserIngestSentenceCtx(GRAPH *graph, CONTEXT *ctx, const char *sentence)
{
    if (graph == NULL || sentence == NULL)
        return 0;

    char resolved[2048];
    const char *text = sentence;
    if (ctx != NULL)
    {
        ContextPreprocessSentence(ctx, graph, sentence,
                                  resolved, sizeof(resolved));
        text = resolved;
    }

    uint32_t base = RelationCount(graph->relations);
    int stored = ParserIngestSentence(graph, text);
    if (ctx != NULL && stored > 0)
    {
        uint32_t total = RelationCount(graph->relations);
        for (uint32_t i = base; i < total; i++)
        {
            const RELATION *r = RelationGet(graph->relations, i);
            if (r == NULL)
                continue;
            const SYMBOL *s = SymbolGet(graph->symbols, r->subject);
            const SYMBOL *o = SymbolGet(graph->symbols, r->object);
            if (s != NULL && s->name != NULL)
                ContextPushEntity(ctx, r->subject, s->name, 1);
            if (o != NULL && o->name != NULL)
                ContextPushEntity(ctx, r->object, o->name, 0);
        }
    }
    return stored;
}

/* ============================================================
   Question Detection & Answering
   ============================================================ */

/* Relation vocabulary cache: distinct used relations with precomputed
   match keys (full name plus stemmed compound parts). Rebuilt whenever
   the relation count changes, so growth invalidates it. The cache is
   what makes whole-text ingest affordable: per-token scans drop from
   thousands of relations with repeated stemming to ~1k flat entries.
   Single-threaded use. */
#define REL_CACHE_MAX 1024
#define REL_CACHE_KEYS 9

typedef struct
{
    SYMBOL_ID rid;
    char      name[64];
    char      keys[REL_CACHE_KEYS][64];
    uint32_t  nkeys;
    int       trusted;
} REL_CACHE_ENTRY;

static REL_CACHE_ENTRY rel_cache[REL_CACHE_MAX];
static uint32_t rel_cache_n = 0;
static uint32_t rel_cache_gen = 0xFFFFFFFFu;

static void RelCacheAddKey(REL_CACHE_ENTRY *e, const char *key)
{
    if (e == NULL || key == NULL || key[0] == '\0')
        return;
    for (uint32_t k = 0; k < e->nkeys; k++)
        if (strcmp(e->keys[k], key) == 0)
            return;
    if (e->nkeys >= REL_CACHE_KEYS)
        return;
    strncpy(e->keys[e->nkeys], key, sizeof(e->keys[0]) - 1);
    e->keys[e->nkeys][sizeof(e->keys[0]) - 1] = '\0';
    e->nkeys++;
}

static uint32_t RelCacheFill(const GRAPH *graph)
{
    if (graph == NULL || graph->relations == NULL)
        return 0;

    uint32_t rc = RelationCount(graph->relations);
    if (rc == rel_cache_gen)
        return rel_cache_n;

    rel_cache_n = 0;
    for (uint32_t i = 0; i < rc && rel_cache_n < REL_CACHE_MAX; i++)
    {
        const RELATION *r = RelationGet(graph->relations, i);
        if (r == NULL || r->relation == SYMBOL_INVALID)
            continue;

        int known = -1;
        for (uint32_t k = 0; k < rel_cache_n; k++)
            if (rel_cache[k].rid == r->relation) { known = (int)k; break; }

        /* Trust: a relation is trusted iff some relation carries
           provenance. Sourceless triples (grown on the fly, bulk
           noise) never outrank curated words. Presence alone decides. */
        int curated = (r->source != SYMBOL_INVALID);
        if (curated && known >= 0)
            rel_cache[(uint32_t)known].trusted = 1;
        if (known >= 0)
            continue;

        const SYMBOL *p = SymbolGet(graph->symbols, r->relation);
        if (p == NULL || p->name == NULL || p->name[0] == '\0')
            continue;

        REL_CACHE_ENTRY *e = &rel_cache[rel_cache_n];
        e->rid = r->relation;
        e->nkeys = 0;
        e->trusted = curated;
        strncpy(e->name, p->name, sizeof(e->name) - 1);
        e->name[sizeof(e->name) - 1] = '\0';
        RelCacheAddKey(e, p->name);

        /* Glue check without word lists: a part far more frequent
           than the rarest sibling is repetition, not meaning
           (DE inside HIJO_DE). Global minimum first, so order never
           matters. Zipf gap, documented factor. */
        uint64_t minfreq = UINT64_MAX;
        {
            char probe[64];
            strncpy(probe, p->name, sizeof(probe) - 1);
            probe[sizeof(probe) - 1] = '\0';
            char *psave = NULL;
            char *ppart = strtok_r(probe, "_", &psave);
            while (ppart != NULL)
            {
                if (strlen(ppart) > 2)
                {
                    uint64_t fr = TokenRarity(graph, ppart);
                    if (fr < minfreq)
                        minfreq = fr;
                }
                ppart = strtok_r(NULL, "_", &psave);
            }
        }
        char parts[64];
        strncpy(parts, p->name, sizeof(parts) - 1);
        parts[sizeof(parts) - 1] = '\0';
        char *saveptr = NULL;
        char *part = strtok_r(parts, "_", &saveptr);
        while (part != NULL)
        {
            if (strlen(part) <= 2)
            {
                part = strtok_r(NULL, "_", &saveptr);
                continue;
            }
            uint64_t fr = TokenRarity(graph, part);
            if (minfreq != UINT64_MAX && fr > minfreq * 8)
            {
                part = strtok_r(NULL, "_", &saveptr);
                continue;
            }
            RelCacheAddKey(e, part);
            char pstem[64];
            StemWord(part, pstem, sizeof(pstem));
            RelCacheAddKey(e, pstem);
            part = strtok_r(NULL, "_", &saveptr);
        }
        rel_cache_n++;
    }
    rel_cache_gen = rc;
    return rel_cache_n;
}

/* Longest common prefix length: morphological closeness, no lists. */
static size_t LcpLen(const char *a, const char *b)
{
    size_t n = 0;
    if (a == NULL || b == NULL)
        return 0;
    while (a[n] != '\0' && a[n] == b[n])
        n++;
    return n;
}

/* Best relation described by the token: every cache key matching the
   exact token or its stem is a candidate; the winner maximizes shared
   affix with the token, then predicate frequency (established use),
   then graph order. "COMEN" picks COME over COMAR (affix 4 over 3);
   "HIJOS" picks HIJO_DE; exact hits win naturally by full length.
  Stem collisions resolve by description quality, never by position. */
static int ResolveRelationPass(const GRAPH *graph, const char *token,
                                char *out, size_t out_size,
                                int *out_trusted)
{
    if (graph == NULL || token == NULL || out == NULL || out_size == 0)
        return 0;

    char stem[64];
    StemWord(token, stem, sizeof(stem));

    uint32_t n = RelCacheFill(graph);
    int found = 0;
    size_t best_affix = 0;
    uint64_t best_freq = 0;
    const char *best_name = NULL;
    int best_trusted = 0;

    for (uint32_t k = 0; k < n; k++)
    {
        for (uint32_t j = 0; j < rel_cache[k].nkeys; j++)
        {
            if (strcmp(rel_cache[k].keys[j], token) != 0 &&
                strcmp(rel_cache[k].keys[j], stem) != 0)
                continue;
            size_t affix = LcpLen(token, rel_cache[k].name);
            uint64_t freq = TokenRarity(graph, rel_cache[k].name);
            if (!found || affix > best_affix ||
                (affix == best_affix && freq > best_freq))
            {
                found = 1;
                best_affix = affix;
                best_freq = freq;
                best_name = rel_cache[k].name;
                best_trusted = rel_cache[k].trusted;
            }
            break;
        }
    }

    if (!found || best_name == NULL)
        return 0;
    strncpy(out, best_name, out_size - 1);
    out[out_size - 1] = '\0';
    if (out_trusted != NULL)
        *out_trusted = best_trusted;
    return 1;
}

/* Embedding description: the (token, relation) pair with the highest
   cosine similarity across all used relations. No threshold: the ranking
   IS the answer, and it sharpens as the model learns (rich model -> rich
   embeddings). Consulted only when symbolic resolution fails, so it can
   only turn past misses into hits, never override a symbolic hit. Tokens
   without a vector are skipped. */
static int ResolveRelationEmbed(const GRAPH *graph,
                                 const PARSED_SENTENCE *tokens,
                                 char *out, size_t out_size, int *out_pos)
{
    if (graph == NULL || graph->embeddings == NULL || tokens == NULL ||
        out == NULL || out_size == 0)
        return 0;

    SYMBOL_ID rids[1024];
    uint32_t np = 0;
    uint32_t nc = RelCacheFill(graph);
    for (uint32_t k = 0; k < nc && np < 1024; k++)
        rids[np++] = rel_cache[k].rid;
    if (np == 0)
        return 0;

    float best_score = -2.0f;
    SYMBOL_ID best_rid = SYMBOL_INVALID;
    int best_pos = -1;
    for (uint32_t t = 0; t < tokens->count; t++)
    {
        SYMBOL_ID tid = SymbolFind(graph->symbols, tokens->tokens[t]);
        if (tid == SYMBOL_INVALID)
            continue;
        const float *tv = EmbeddingGetVector(graph->embeddings, tid);
        if (tv == NULL)
            continue;
        for (uint32_t k = 0; k < np; k++)
        {
            const float *pv = EmbeddingGetVector(graph->embeddings, rids[k]);
            if (pv == NULL)
                continue;
            float s = EmbeddingCosineSimilarity(tv, pv);
            if (s > best_score)
            {
                best_score = s;
                best_rid = rids[k];
                best_pos = (int)t;
            }
        }
    }

    if (best_rid == SYMBOL_INVALID)
        return 0;
    const SYMBOL *p = SymbolGet(graph->symbols, best_rid);
    if (p == NULL || p->name == NULL || p->name[0] == '\0')
        return 0;
    strncpy(out, p->name, out_size - 1);
    out[out_size - 1] = '\0';
    if (out_pos != NULL)
        *out_pos = best_pos;
    return 1;
}

/* Does the token name a used relation exactly (no stemming)? Used for
   the entity rule: the entity is never a relation name ("... David es"
   must resolve to DAVID, not ES). */
static int TokenNamesRelation(const GRAPH *graph, const char *token)
{
    if (graph == NULL || token == NULL)
        return 0;

    uint32_t n = RelCacheFill(graph);
    for (uint32_t k = 0; k < n; k++)
        if (strcmp(rel_cache[k].name, token) == 0)
            return 1;
    return 0;
}

/* Entity = longest trailing token run (up to 4) naming an existing
   symbol. Trailing tokens that name used relations are skipped first
   (dynamic copula-skip: "... David es" resolves to DAVID, never ES).
   Falls back to the last token so unknown entities still yield
   unanswerable questions instead of invalid ones. Returns the run start
   (or -1) and writes the _-joined name. */
static int ResolveEntity(const GRAPH *graph, const PARSED_SENTENCE *tokens,
                         uint32_t start, char *out, size_t out_size)
{
    if (graph == NULL || tokens == NULL || out == NULL || out_size == 0)
        return -1;
    if (start >= tokens->count)
        return -1;

    uint32_t end = tokens->count;
    if (end > start + 1 &&
        TokenNamesRelation(graph, tokens->tokens[end - 1]))
        end--;

    uint32_t span = (end > start) ? end - start : 0;
    if (span == 0)
    {
        strncpy(out, tokens->tokens[tokens->count - 1], out_size - 1);
        out[out_size - 1] = '\0';
        return (int)(tokens->count - 1);
    }

    uint32_t best_len = 0;
    uint32_t best_at = end - 1;
    char cand[128];
    for (uint32_t len = (span < 4 ? span : 4); len >= 1; len--)
    {
        for (uint32_t s = start; s + len <= end; s++)
        {
            cand[0] = '\0';
            for (uint32_t k = 0; k < len; k++)
            {
                if (k > 0) strcat(cand, "_");
                strcat(cand, tokens->tokens[s + k]);
            }
            if (SymbolFind(graph->symbols, cand) != SYMBOL_INVALID &&
                (len > best_len || (len == best_len && s + len == end)))
            {
                best_len = len;
                best_at = s;
            }
        }
        if (best_len > 0 && best_at + best_len == end)
            break;
    }

    if (best_len == 0)
    {
        strncpy(out, tokens->tokens[end - 1], out_size - 1);
        out[out_size - 1] = '\0';
        return (int)(end - 1);
    }

    out[0] = '\0';
    for (uint32_t k = 0; k < best_len; k++)
    {
        if (k > 0) strcat(out, "_");
        strncat(out, tokens->tokens[best_at + k], out_size - strlen(out) - 1);
    }
    return (int)best_at;
}

QUESTION ParserDetectQuestion(const GRAPH *graph, const char *input)
{
    QUESTION q;
    memset(&q, 0, sizeof(q));

    if (graph == NULL || input == NULL)
        return q;

    /* Tokenize */
    PARSED_SENTENCE tokens;
    ParserTokenize(input, &tokens);

    if (tokens.count < 2)
        return q;

    /* "?" is punctuation, not vocabulary. Anything else validates
       structurally below: if descriptor and entity both resolve
       against the live map, it is a question regardless of wording. */
    int has_question_mark = (strstr(input, "?") != NULL ||
                             strstr(input, "¿") != NULL);

    q.is_question = has_question_mark;

    /* Fact shape: descriptor token pointing to a graph relation plus an
       entity. The entity usually trails the descriptor ("la CAPITAL de
       FRANCIA es") but may precede it ("Paris es"): both directions are
       tried, trailing first. No separator word, no copula check, no
       opener check: the shape validates itself because both ends
       resolve against the live map. Descriptor search runs exact, then
       stemmed, then by embedding description. Among resolving tokens
       the winner is ranked, not positional: trusted vocabulary first,
       then rarest token, then earliest position. */
    {
        /* Ranked descriptor: every resolving token scores
           (untrusted, rarity, pass, position), lowest wins. Trust beats
           rarity (bulk-grown noise loses to curated words even standing
           earlier); rarity beats morphology (a novel stem hit like
           CAPITALES->CAPITAL outranks the copula); morphology beats
           position. No word lists, no thresholds. */
        /* Single ranking over all tokens: morphology lives inside
           resolution (affix ranking), so no pass loop is needed here. */
        int best_pos = -1;
        char best_rel[64] = {0};
        int best_untrusted = 1;
        uint64_t best_freq = UINT64_MAX;
        for (uint32_t i = 0; i < tokens.count; i++)
        {
            char rel[64] = {0};
            int trusted = 0;
            if (!ResolveRelationPass(graph, tokens.tokens[i],
                                      rel, sizeof(rel), &trusted))
                continue;
            int untrusted = trusted ? 0 : 1;
            uint64_t freq = TokenRarity(graph, tokens.tokens[i]);
            if (best_pos < 0 || untrusted < best_untrusted ||
                (untrusted == best_untrusted && freq < best_freq))
            {
                best_pos = (int)i;
                strcpy(best_rel, rel);
                best_untrusted = untrusted;
                best_freq = freq;
            }
        }
        if (best_pos >= 0)
        {
            uint32_t i = (uint32_t)best_pos;
            {
                char subj[128] = {0};
                int spos = ResolveEntity(graph, &tokens, i + 1,
                                         subj, sizeof(subj));
                if (spos < 0)
                {
                    PARSED_SENTENCE pre;
                    memset(&pre, 0, sizeof(pre));
                    for (uint32_t k = 0; k < i &&
                                        pre.count < PARSER_MAX_TOKENS; k++)
                    {
                        strcpy(pre.tokens[pre.count], tokens.tokens[k]);
                        pre.count++;
                    }
                    spos = ResolveEntity(graph, &pre, 0,
                                         subj, sizeof(subj));
                }
                if (spos < 0 || strlen(subj) == 0)
                {
                    /* Winner has no entity: fall through to the
                       embedding pass rather than trying worse winners. */
                }
                else
                {
                    strcpy(q.subject, subj);
                    strcpy(q.relation, best_rel);
                    q.valid = 1;
                    q.is_question = 1;
                    return q;
                }
            }
        }
        {
            PARSED_SENTENCE desc;
            memset(&desc, 0, sizeof(desc));
            for (uint32_t i = 0; i < tokens.count &&
                                desc.count < PARSER_MAX_TOKENS; i++)
            {
                strcpy(desc.tokens[desc.count], tokens.tokens[i]);
                desc.count++;
            }
            char rel[64] = {0};
            int epos = -1;
            if (ResolveRelationEmbed(graph, &desc, rel, sizeof(rel),
                                      &epos) && epos >= 0)
            {
                char subj[128] = {0};
                int spos = ResolveEntity(graph, &tokens, (uint32_t)epos + 1,
                                         subj, sizeof(subj));
                if (spos < 0)
                {
                    PARSED_SENTENCE pre;
                    memset(&pre, 0, sizeof(pre));
                    for (uint32_t k = 0; k < (uint32_t)epos &&
                                        pre.count < PARSER_MAX_TOKENS; k++)
                    {
                        strcpy(pre.tokens[pre.count], tokens.tokens[k]);
                        pre.count++;
                    }
                    spos = ResolveEntity(graph, &pre, 0,
                                         subj, sizeof(subj));
                }
                if (spos >= 0 && strlen(subj) > 0)
                {
                    strcpy(q.subject, subj);
                    strcpy(q.relation, rel);
                    q.valid = 1;
                    q.is_question = 1;
                    return q;
                }
            }
        }
    }

    return q;
}

static uint32_t AppendName(const GRAPH *graph, SYMBOL_ID id,
                           char *out, uint32_t pos, uint32_t max_len)
{
    const SYMBOL *sym = SymbolGet(graph->symbols, id);
    if (!sym || !sym->name)
        return pos;
    uint32_t name_len = (uint32_t)strlen(sym->name);
    if (pos + name_len < max_len)
    {
        memcpy(out + pos, sym->name, name_len);
        pos += name_len;
    }
    return pos;
}

/* Order relations by semantic-area coherence around center: each
   candidate scores by similarity between its composed vector and the
   mean of center's composed subject-relations (the area). Insertion
   sort, in place. Order only; membership stays exact. No-op without
   embeddings or with fewer than 2 items. */
void ParserRankByArea(const GRAPH *graph, SYMBOL_ID center,
                      RELATION **rels, uint32_t n)
{
    if (graph == NULL || graph->embeddings == NULL || rels == NULL || n < 2)
        return;
    if (center == SYMBOL_INVALID)
        return;

    float area[EMBEDDING_DIM] = {0};
    int area_n = 0;
    RELATION *area_rels[32];
    uint32_t na = GraphQuerySubject(graph, center, area_rels, 32);
    for (uint32_t a = 0; a < na; a++)
    {
        float comp[EMBEDDING_DIM];
        if (EmbeddingComposeRelation(graph->embeddings,
                                     area_rels[a]->subject,
                                     area_rels[a]->relation,
                                     area_rels[a]->object, comp))
        {
            for (int d = 0; d < EMBEDDING_DIM; d++)
                area[d] += comp[d];
            area_n++;
        }
    }
    if (area_n == 0)
        return;
    for (int d = 0; d < EMBEDDING_DIM; d++)
        area[d] /= (float)area_n;

    for (uint32_t i = 1; i < n; i++)
    {
        RELATION *rk = rels[i];
        float ck[EMBEDDING_DIM];
        if (!EmbeddingComposeRelation(graph->embeddings, rk->subject,
                                      rk->relation, rk->object, ck))
            continue;
        float sk = EmbeddingCosineSimilarity(ck, area);
        uint32_t j = i;
        while (j > 0)
        {
            float cj[EMBEDDING_DIM];
            if (!EmbeddingComposeRelation(graph->embeddings,
                                          rels[j - 1]->subject,
                                          rels[j - 1]->relation,
                                          rels[j - 1]->object, cj))
                break;
            if (EmbeddingCosineSimilarity(cj, area) >= sk)
                break;
            rels[j] = rels[j - 1];
            j--;
        }
        rels[j] = rk;
    }
}

int ParserAnswerQuestion(
    const GRAPH *graph,
    const QUESTION *q,
    char *out_answer,
    uint32_t max_len)
{
    if (graph == NULL || q == NULL || !q->valid || out_answer == NULL)
        return 0;

    out_answer[0] = '\0';

    /* Find subject symbol (exact first, then morphological fallback) */
    SYMBOL_ID subj_id = StemFindSymbol(graph->symbols, q->subject);

    /* Try direct match first */
    if (subj_id != SYMBOL_INVALID)
    {
        /* Find relation */
        SYMBOL_ID rel_id = StemFindSymbol(graph->symbols, q->relation);

        if (rel_id != SYMBOL_INVALID)
        {
            RELATION *results[8];
            uint32_t n = GraphQuerySubjectRelation(
                graph, subj_id, rel_id, results, 8);

            if (n > 0)
            {
                /* Order answers by semantic-area coherence. */
                ParserRankByArea(graph, subj_id, results, n);

                /* Build answer from objects */
                uint32_t pos = 0;
                for (uint32_t i = 0; i < n && i < 5; i++)
                {
                    const SYMBOL *obj = SymbolGet(graph->symbols, results[i]->object);
                    if (obj)
                    {
                        if (i > 0 && pos + 2 < max_len)
                        {
                            out_answer[pos++] = ',';
                            out_answer[pos++] = ' ';
                        }
                        uint32_t name_len = (uint32_t)strlen(obj->name);
                        if (pos + name_len < max_len)
                        {
                            memcpy(out_answer + pos, obj->name, name_len);
                            pos += name_len;
                        }
                    }
                }
                out_answer[pos] = '\0';
                return 1;
            }
        }
    }

    /* No exact triple stored: honest unknown. Guessing from nearby
       relations fabricates answers. */
    return 0;
}
