#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "parser.h"
#include "compat.h"
#include "stem.h"
#include "embedding.h"
#include "learning.h"
#include "i18n.h"

/* Forward: descriptor and entity resolution live with the query
   path below; the ingest tree above needs them. */
static int ResolveRelationPass(const GRAPH *graph, const char *token,
                                char *out, size_t out_size,
                                int *out_trusted);
static int ResolveEntity(const GRAPH *graph, const PARSED_SENTENCE *tokens,
                         uint32_t start, char *out, size_t out_size);
static uint64_t TokenRarity(const GRAPH *graph, const char *token);
static uint64_t EffFreq(const GRAPH *graph, const char *token);
static int TokenNamesRelation(const GRAPH *graph, const char *token);
static int IsGlueToken(const char *token);
static void SurfaceRecord(const PARSED_SENTENCE *tokens,
                          uint32_t subj_a, uint32_t subj_b,
                          uint32_t pred_pos,
                          uint32_t obj_a, uint32_t obj_b,
                          const char *pred);

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

    /* Copula slots: strip leading/trailing tokens that name a used
       relation ("Roma es _", "_ es capital"). Positional, never
       lexical: the copula sits at the span edge by definition. Bare
       NO is edge glue too (closed-class, like delimiters): negation
       marks the clause, never an entity ("Roma no _" resolves ROMA).
       Closed articles likewise (EL/LA/...: syntax, same license as
       the query-side run skip and the lint STOP_SUBJ list). */
    while (end > start + 1 &&
           (TokenNamesRelation(graph, tokens->tokens[start]) ||
            IsGlueToken(tokens->tokens[start])))
        start++;
    while (end > start + 1 &&
           (TokenNamesRelation(graph, tokens->tokens[end - 1]) ||
            IsGlueToken(tokens->tokens[end - 1])))
        end--;

    /* No frequency-ratio edge strip here: raw standalone frequency
       cannot tell glue from popular content (ITALIA 44 vs DE 4 would
       eat the content). Relation-naming edges are already gone via
       the copula strip above; the tiers below resolve the rest. */

    uint32_t span = end - start;
    char cand[128];
    /* Tier 0: longest known compound (length 2+). Single known tokens
       wait for the purity tier below, so known glue (DE, EL) can never
       swallow novel content sitting next to it. Candidates naming a
       used relation are skipped (a relation name is not an entity:
       "MIEMBRO_DE" inside a span must not resolve the object). */
    for (uint32_t len = (span < 4 ? span : 4); len >= 2; len--)
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
                if (TokenNamesRelation(graph, cand))
                    continue;
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
                if (TokenNamesRelation(graph, cand))
                    continue;
                if (SymbolFind(graph->symbols, cand) != SYMBOL_INVALID)
                {
                    strncpy(out, cand, out_size - 1);
                    out[out_size - 1] = '\0';
                    return (int)s;
                }
            }
        }
    }

    /* Novelty tier: longest run of never-seen tokens, anchor-ordered.
       Novel content (MARIA, PAN) outranks known glue (DE, CON) without
       naming either: what the map never saw cannot be glue. */
    for (uint32_t len = (span < 4 ? span : 4); len >= 1; len--)
    {
        if (trailing)
        {
            uint32_t s = end - len + 1;
            while (s > start)
            {
                s--;
                int novel = 1;
                for (uint32_t k = 0; k < len; k++)
                {
                    if (SymbolFind(graph->symbols,
                                   tokens->tokens[s + k]) != SYMBOL_INVALID)
                    {
                        novel = 0;
                        break;
                    }
                }
                if (!novel)
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
        }
        else
        {
            for (uint32_t s = start; s + len <= end; s++)
            {
                int novel = 1;
                for (uint32_t k = 0; k < len; k++)
                {
                    if (SymbolFind(graph->symbols,
                                   tokens->tokens[s + k]) != SYMBOL_INVALID)
                    {
                        novel = 0;
                        break;
                    }
                }
                if (!novel)
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
        }
        if (len == 1)
            break;
    }

    /* Known singles, trailing-first always (same as the query side):
       nothing novel here, take the last thing that exists. Unifies
       ingest and query entity rules. */
    {
        uint32_t s = end;
        while (s > start)
        {
            s--;
            cand[0] = '\0';
            strcat(cand, tokens->tokens[s]);
            if (SymbolFind(graph->symbols, cand) != SYMBOL_INVALID)
            {
                strncpy(out, cand, out_size - 1);
                out[out_size - 1] = '\0';
                return (int)s;
            }
        }
    }

    return -1;
}

/* Clause boundaries: local frequency maxima with content on both
   sides. Glue betrays itself by repetition (Zipf), never by name: a
   token far more frequent than both neighbors splits the input, and
   belongs to neither clause. On virgin maps nothing splits (graceful
   degradation to one clause). Bounds-checked interior only, so edge
   articles can never shatter an entity. A maximum splits ONLY between
   two relation roots: a frequent copula inside one clause ("Noruega
   no es miembro ...") must never strand the subject in another
   segment. Roots must be trusted (sourced vocabulary): bulk noise
   partitions nothing. */
static int TrustedRootInRange(const GRAPH *graph,
                              const PARSED_SENTENCE *tokens,
                              uint32_t a, uint32_t b)
{
    char rel[64];
    int trusted = 0;
    for (uint32_t i = a; i < b && i < tokens->count; i++)
    {
        rel[0] = '\0';
        trusted = 0;
        if (ResolveRelationPass(graph, tokens->tokens[i],
                                rel, sizeof(rel), &trusted) &&
            trusted)
            return 1;
    }
    return 0;
}

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
        if (f > fl && f > fr &&
            TrustedRootInRange(graph, tokens, 0, i) &&
            TrustedRootInRange(graph, tokens, i + 1, tokens->count))
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

/* Does this line read as a question, punctuation or not? A '?'
   settles it. Without punctuation only interrogative-only
   closed-class tokens can prove it: the pronouns QUIEN/CUAL/CUALES
   and second-person copulas ERES/SOIS (first-person SOY stays OUT:
   "yo soy medico" is a fact worth storing, and its copula already
   resolves through the lemma table). QUE/DONDE/CUANDO/COMO and their
   English cousins share syntax with declarative relatives, so they
   require the mark. Questions query; they never store: ingesting
   them would fabricate triples out of question words
   (QUIEN--SOY-->YO). */
static int SentenceIsInterrogative(const char *sentence,
                                   const PARSED_SENTENCE *tokens)
{
    if (strchr(sentence, '?') != NULL)
        return 1;

    static const char *const QUESTION_ONLY[] = {
        "QUIEN", "CUAL", "CUALES", "ERES", "SOIS",
        "WHO", "WHOM", "WHOSE", NULL
    };

    for (uint32_t i = 0; i < tokens->count; i++)
    {
        for (uint32_t m = 0; QUESTION_ONLY[m] != NULL; m++)
        {
            if (strcmp(tokens->tokens[i], QUESTION_ONLY[m]) == 0)
                return 1;
        }
    }
    return 0;
}

int ParserIsQuestion(const char *sentence)
{
    if (sentence == NULL)
        return 0;
    PARSED_SENTENCE tokens;
    ParserTokenize(sentence, &tokens);
    return SentenceIsInterrogative(sentence, &tokens);
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

    if (SentenceIsInterrogative(sentence, &tokens))
        return 0;

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

    /* Adjacent roots compete for the same slots ("es capital"):
       keep the LONGEST name. Symbol frequency is corrupt here (it
       counts relation-slot occurrences, so popular predicates look
       "common" and the copula would eat content roots); name length
       approximates specificity structurally: compounds outrank bare
       copulas, ties fall back to rarity. No names involved. */
    {
        uint32_t w = 0;
        for (uint32_t k = 0; k < nroots; k++)
        {
            if (w > 0 && roots[k] == roots[w - 1] + 1)
            {
                size_t l_old = strlen(names[w - 1]);
                size_t l_new = strlen(names[k]);
                if (l_new > l_old)
                {
                    roots[w - 1] = roots[k];
                    strcpy(names[w - 1], names[k]);
                    trusted[w - 1] = trusted[k];
                    continue;
                }
                uint64_t f_old = TokenRarity(graph, names[w - 1]);
                uint64_t f_new = TokenRarity(graph, names[k]);
                if (f_new < f_old)
                {
                    roots[w - 1] = roots[k];
                    strcpy(names[w - 1], names[k]);
                    trusted[w - 1] = trusted[k];
                }
            }
            else
            {
                if (w != k)
                {
                    roots[w] = roots[k];
                    strcpy(names[w], names[k]);
                    trusted[w] = trusted[k];
                }
                w++;
            }
        }
        nroots = w;
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

                    /* Negation marks the clause, never the entity: a
                       bare NO anywhere between the region start and
                       the relation root stores a NEGATIVE triple
                       ("Roma no es ...", "Noruega no es miembro ...").
                       Per-region scan, so coordinated positives
                       ("Madrid sí") beside a negated region stay
                       positive. ALLOW_BOTH keeps history auditable;
                       weights duel it out. Negatives skip embedding
                       co-occurrence (non-co-occurrence must not train
                       association) but keep the surface mold
                       (positions, not truth). */
                    int neg = 0;
                    for (uint32_t nk = r0; nk < roots[c]; nk++)
                        if (strcmp(tokens.tokens[nk], "NO") == 0)
                        {
                            neg = 1;
                            break;
                        }
                    int added = 0;
                    if (s != SYMBOL_INVALID && p != SYMBOL_INVALID &&
                        o != SYMBOL_INVALID)
                        added = neg ?
                            GraphAddRelationPolar(graph, s, p, o,
                                                  POLARITY_NEGATIVE,
                                                  CONFLICT_ALLOW_BOTH) :
                            GraphAddRelation(graph, s, p, o);
                    if (added)
                    {
                        stored++;
                        SurfaceRecord(&tokens, r0, r1, roots[c],
                                      right_a, right_b, names[c]);

                        /* Update embeddings on every co-occurrence */
                        if (!neg && graph->embeddings != NULL)
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
   Surface patterns: learned molds, never rules
   ============================================================ */

/* A mold records how one stored triple was laid out in its input:
   which token slots held subject span (1), predicate (2), object
   span (3); the rest is glue kept literally. Rendering fills a mold
   of the same predicate with new symbols. Fluency grows with use;
   virgin maps echo bare triples. Session table (not persisted). */
#define SURFACE_MAX_PATTERNS 256
#define SURFACE_MAX_TOKENS 12

typedef struct
{
    char     pred[64];
    uint8_t  roles[SURFACE_MAX_TOKENS];
    char     glue[SURFACE_MAX_TOKENS][32];
    uint32_t ntok;
    uint32_t uses;
} SURFACE_PATTERN;

static SURFACE_PATTERN surface_pats[SURFACE_MAX_PATTERNS];
static uint32_t surface_n = 0;

static void SurfaceRecord(const PARSED_SENTENCE *tokens,
                          uint32_t subj_a, uint32_t subj_b,
                          uint32_t pred_pos,
                          uint32_t obj_a, uint32_t obj_b,
                          const char *pred)
{
    if (tokens == NULL || pred == NULL)
        return;
    uint32_t a = subj_a < obj_a ? subj_a : obj_a;
    uint32_t b = subj_b > obj_b ? subj_b : obj_b;
    if (b <= a || b - a > SURFACE_MAX_TOKENS)
        return;

    uint8_t roles[SURFACE_MAX_TOKENS] = {0};
    char glue[SURFACE_MAX_TOKENS][32];
    memset(glue, 0, sizeof(glue));
    for (uint32_t k = a; k < b; k++)
    {
        uint32_t j = k - a;
        if (k >= subj_a && k < subj_b)
            roles[j] = 1;
        else if (k == pred_pos)
            roles[j] = 2;
        else if (k >= obj_a && k < obj_b)
            roles[j] = 3;
        else
        {
            roles[j] = 0;
            strncpy(glue[j], tokens->tokens[k], sizeof(glue[j]) - 1);
        }
    }

    for (uint32_t i = 0; i < surface_n; i++)
    {
        SURFACE_PATTERN *p = &surface_pats[i];
        if (strcmp(p->pred, pred) != 0 || p->ntok != b - a)
            continue;
        int same = 1;
        for (uint32_t j = 0; j < p->ntok; j++)
        {
            if (p->roles[j] != roles[j] ||
                (roles[j] == 0 && strcmp(p->glue[j], glue[j]) != 0))
            {
                same = 0;
                break;
            }
        }
        if (same)
        {
            p->uses++;
            return;
        }
    }

    if (surface_n >= SURFACE_MAX_PATTERNS)
        return;
    SURFACE_PATTERN *p = &surface_pats[surface_n++];
    strncpy(p->pred, pred, sizeof(p->pred) - 1);
    p->ntok = b - a;
    p->uses = 1;
    for (uint32_t j = 0; j < p->ntok; j++)
    {
        p->roles[j] = roles[j];
        strcpy(p->glue[j], glue[j]);
    }
}

/* Render (subj, pred, obj) through the most-used mold of pred.
   Returns 1 on success, 0 when no mold exists (caller echoes). */
int SurfaceRender(const char *pred, const char *subj, const char *obj,
                  char *out, size_t out_size)
{
    if (pred == NULL || subj == NULL || obj == NULL ||
        out == NULL || out_size == 0)
        return 0;

    SURFACE_PATTERN *best = NULL;
    for (uint32_t i = 0; i < surface_n; i++)
    {
        if (strcmp(surface_pats[i].pred, pred) != 0)
            continue;
        if (best == NULL || surface_pats[i].uses > best->uses)
            best = &surface_pats[i];
    }
    if (best == NULL)
        return 0;

    out[0] = '\0';
    for (uint32_t j = 0; j < best->ntok; j++)
    {
        const char *w = best->glue[j];
        char tmp[128];
        if (best->roles[j] == 1)
        {
            strncpy(tmp, subj, sizeof(tmp) - 1);
            w = tmp;
        }
        else if (best->roles[j] == 2)
        {
            strncpy(tmp, pred, sizeof(tmp) - 1);
            w = tmp;
        }
        else if (best->roles[j] == 3)
        {
            strncpy(tmp, obj, sizeof(tmp) - 1);
            w = tmp;
        }
        tmp[sizeof(tmp) - 1] = '\0';
        if (j > 0)
            strncat(out, " ", out_size - strlen(out) - 1);
        strncat(out, w, out_size - strlen(out) - 1);
    }
    return (out[0] != '\0');
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

/* A compound part that itself names a used relation (IDIOMA inside
   IDIOMA_OFICIAL) is vocabulary, not repetition: frequency cannot
   demote it to glue. Scans triples; fill-time only (amortized). */
static int PartNamesUsedRelation(const GRAPH *graph, const char *part)
{
    if (graph == NULL || part == NULL)
        return 0;
    SYMBOL_ID id = SymbolFind(graph->symbols, part);
    if (id == SYMBOL_INVALID)
        return 0;
    uint32_t rc = RelationCount(graph->relations);
    for (uint32_t i = 0; i < rc; i++)
    {
        const RELATION *r = RelationGet(graph->relations, i);
        if (r != NULL && r->relation == id)
            return 1;
    }
    return 0;
}

static uint32_t RelCacheFill(const GRAPH *graph){
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
        /* Folded key: tokens are diacritic-normalized at read time
           while relation names keep accents (CIUDAD_MÁS_POBLADA).
           Without the folded form, accented predicates are
           unaskable. Same normalization both sides, no lists. */
        {
            char folded[64];
            strncpy(folded, p->name, sizeof(folded) - 1);
            folded[sizeof(folded) - 1] = '\0';
            NormalizeDiacritics(folded);
            if (strcmp(folded, p->name) != 0)
                RelCacheAddKey(e, folded);
        }

        /* Glue check without word lists: a part far more frequent
           than the rarest sibling is repetition, not meaning
           (DE inside HIJO_DE) — unless the part itself names a used
           relation (IDIOMA inside IDIOMA_OFICIAL): shared vocabulary
           keeps keying no matter its frequency. Global minimum first,
           so order never matters. Zipf gap, documented factor. */
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
            if (minfreq != UINT64_MAX && fr > minfreq * 8 &&
                !PartNamesUsedRelation(graph, part))
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

    /* NO is reserved (polarity marker): it never resolves, and no
       relation named NO answers (junk-vocabulary from the fragment
       era: a negation marker must not root clauses or qualify
       splits). The copula strip still sees it as edge glue. */
    if (strcmp(token, "NO") == 0)
        return 0;

    uint32_t n = RelCacheFill(graph);
    int found = 0;
    size_t best_affix = 0;
    int best_exact = 0;
    uint64_t best_freq = 0;
    const char *best_name = NULL;
    int best_trusted = 0;

    for (uint32_t k = 0; k < n; k++)
    {
        if (strcmp(rel_cache[k].name, "NO") == 0)
            continue;
        for (uint32_t j = 0; j < rel_cache[k].nkeys; j++)
        {
            if (strcmp(rel_cache[k].keys[j], token) != 0 &&
                strcmp(rel_cache[k].keys[j], stem) != 0)
                continue;
            size_t affix = LcpLen(token, rel_cache[k].name);
            /* Exact-name beats affix ties: IDIOMA names the bare
               relation even when IDIOMA_OFICIAL is more frequent.
               Frequency drift must never steal an exact hit. */
            int exact = (strcmp(token, rel_cache[k].name) == 0);
            uint64_t freq = TokenRarity(graph, rel_cache[k].name);
            if (!found || affix > best_affix ||
                (affix == best_affix && exact > best_exact) ||
                (affix == best_affix && exact == best_exact &&
                 freq > best_freq))
            {
                found = 1;
                best_affix = affix;
                best_exact = exact;
                best_freq = freq;
                best_name = rel_cache[k].name;
                best_trusted = rel_cache[k].trusted;
            }
            break;
        }
    }

    if (found && best_name != NULL)
    {
        strncpy(out, best_name, out_size - 1);
        out[out_size - 1] = '\0';
        if (out_trusted != NULL)
            *out_trusted = best_trusted;
        return 1;
    }

    /* Conjugated copulas never stem into one another by suffix rules
       (ERES does not reduce to ES). A closed, unambiguous set of
       personal forms points at the base copula, and only when that
       base is a relation this graph actually uses; SON and ERA stay
       out (homographs: the sound, the threshing floor). */
    {
        static const struct
        {
            const char *form;
            const char *base;
        } COPULA_LEMMA[] = {
            {"SOY", "ES"}, {"ERES", "ES"}, {"SOMOS", "ES"},
            {"SOIS", "ES"}, {"ERAS", "ES"}, {"ERAMOS", "ES"},
            {"ERAIS", "ES"}, {"ERAN", "ES"},
            {"ESTOY", "ESTAR"}, {"ESTAMOS", "ESTAR"},
            {"ESTAIS", "ESTAR"}, {"ESTAN", "ESTAR"},
            {"ESTABA", "ESTAR"},
            {NULL, NULL}
        };

        for (uint32_t c = 0; COPULA_LEMMA[c].form != NULL; c++)
        {
            if (strcmp(token, COPULA_LEMMA[c].form) != 0)
                continue;
            for (uint32_t k = 0; k < n; k++)
            {
                if (strcmp(rel_cache[k].name, COPULA_LEMMA[c].base) != 0)
                    continue;
                strncpy(out, rel_cache[k].name, out_size - 1);
                out[out_size - 1] = '\0';
                if (out_trusted != NULL)
                    *out_trusted = rel_cache[k].trusted;
                return 1;
            }
            return 0;
        }
    }

    return 0;
}

/* Closed-class edge glue: negation marker, articles, and the
   prepositions/conjunctions that never name entities (DE/EN/Y have
   zero entity lives in the map: every occurrence is fragment-era
   residue or bulk glue; measured breaking subject resolution on
   grown maps). Never an entity, never a descriptor; shared by
   ingest spans and query runs so both sides agree. Same license as
   delimiters and the lint STOP_SUBJ list: syntax, not vocabulary. */
static int IsGlueToken(const char *token)
{
    static const char *const GLUE[] = {
        "NO",
        "EL", "LA", "LOS", "LAS", "UN", "UNA", "UNOS", "UNAS", "LO",
        "AL", "DEL",
        "DE", "EN", "Y",
        NULL
    };
    if (token == NULL)
        return 0;
    for (int i = 0; GLUE[i] != NULL; i++)
        if (strcmp(token, GLUE[i]) == 0)
            return 1;
    return 0;
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
   Bare NO never joins an entity run (null-marker doctrine: negation
   marks, never names — otherwise the leftover NO symbol from the
   fragment era poisons resolution). Closed-class articles never join
   runs either (LA/EL/LOS/...: syntax, same license as delimiters and
   the lint STOP_SUBJ list; a trailing article would otherwise beat
   content by position). Falls back to the last token so unknown
   entities still yield unanswerable questions instead of invalid
   ones. Returns the run start (or -1) and writes the _-joined name. */
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
            int has_no = 0;
            for (uint32_t k = 0; k < len; k++)
            {
                /* Glue and copulas never join entity runs: a bare NO
                   or article, or a token naming a used relation, would
                   otherwise beat content by position (ES in
                   "ES OSLO LA" outranks OSLO). Same license as the
                   ingest edge strip. */
                const char *tok = tokens->tokens[s + k];
                if (IsGlueToken(tok) || TokenNamesRelation(graph, tok))
                {
                    has_no = 1;
                    break;
                }
                if (k > 0) strcat(cand, "_");
                strcat(cand, tokens->tokens[s + k]);
            }
            if (has_no)
                continue;
            /* A run naming a used relation is not an entity (mirror
               of the ingest Tier0 skip): "MIEMBRO_DE" inside a span
               must not resolve the subject. */
            if (TokenNamesRelation(graph, cand))
                continue;
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
        /* Last resort: walk back past relations and glue to the last
           content-or-novel token (a bare relation name like ES is
           never an entity). Unknowns still return, so unanswerable
           questions stay valid instead of invalid. */
        uint32_t w = end;
        while (w > start)
        {
            w--;
            if (!IsGlueToken(tokens->tokens[w]) &&
                !TokenNamesRelation(graph, tokens->tokens[w]))
                break;
        }
        if (IsGlueToken(tokens->tokens[w]) ||
            TokenNamesRelation(graph, tokens->tokens[w]))
            return -1;
        strncpy(out, tokens->tokens[w], out_size - 1);
        out[out_size - 1] = '\0';
        return (int)w;
    }

    out[0] = '\0';
    for (uint32_t k = 0; k < best_len; k++)
    {
        if (k > 0) strcat(out, "_");
        strncat(out, tokens->tokens[best_at + k], out_size - strlen(out) - 1);
    }
    return (int)best_at;
}

/* Question subject anchored on a clause root: the entity trail after
   the root first ("... CAPITAL DE FRANCIA"), then the span before it
   ("FRANCIA ES ..."). A bare interrogative leftover (QUIEN) with no
   content entity is a self-question whenever the graph has a self
   symbol: "¿quien eres?" asks about YO, never about the word QUIEN.
   Returns 0 when nothing resolves. */
static int ResolveQuestionSubject(const GRAPH *graph,
                                  const PARSED_SENTENCE *tokens,
                                  uint32_t root_pos,
                                  char *subj, size_t subj_size)
{
    int spos = ResolveEntity(graph, tokens, root_pos + 1, subj, subj_size);
    if (spos < 0)
    {
        PARSED_SENTENCE pre;
        memset(&pre, 0, sizeof(pre));
        for (uint32_t k = 0; k < root_pos &&
                            pre.count < PARSER_MAX_TOKENS; k++)
        {
            strcpy(pre.tokens[pre.count], tokens->tokens[k]);
            pre.count++;
        }
        spos = ResolveEntity(graph, &pre, 0, subj, subj_size);
    }
    if (spos < 0 || strlen(subj) == 0)
        return 0;

    if (strcmp(subj, "QUIEN") == 0 &&
        SymbolFind(graph->symbols, "YO") != SYMBOL_INVALID)
        strcpy(subj, "YO");

    return 1;
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

    /* Counting form: CUÁNTO/CUÁNTOS/CUÁNTA/CUÁNTAS (closed
       interrogatives, normalized already) turn the answer into the
       numeral. Subject and relation resolve exactly as usual; only
       the verbalization aggregates. */
    for (uint32_t ci = 0; ci < tokens.count; ci++)
    {
        const char *tok = tokens.tokens[ci];
        if (strncmp(tok, "CUANT", 5) == 0 &&
            (strcmp(tok + 5, "O") == 0 || strcmp(tok + 5, "OS") == 0 ||
             strcmp(tok + 5, "A") == 0 || strcmp(tok + 5, "AS") == 0))
        {
            q.is_count = 1;
            q.is_question = 1;
            break;
        }
    }

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
           (untrusted, rarity), lowest wins. Trust beats rarity
           (bulk-grown noise loses to curated words even standing
           earlier); rarity beats morphology (a novel stem hit like
           CAPITALES->CAPITAL outranks the copula). The evidence
           redirect below repairs the cases a bare ranking cannot
           (entity words naming curated relations, verbs that must
           lose to co-occurring nouns): a resolving descriptor that
           yields no triple with the asked entity is useless, so
           co-occurrence decides. No word lists, no thresholds. */
        /* Single ranking over all tokens: morphology lives inside
           resolution (affix ranking), so no pass loop is needed here. */
        int best_pos = -1;
        char best_rel[64] = {0};
        int best_untrusted = 1;
        uint64_t best_freq = UINT64_MAX;
        for (uint32_t i = 0; i < tokens.count; i++)
        {
            /* Bare NO is the polarity marker, never a descriptor:
               it flags negative verification instead of resolving
               (closed-class, like the CUANT forms above). */
            if (strcmp(tokens.tokens[i], "NO") == 0)
            {
                q.is_negative = 1;
                continue;
            }
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
            char subj[128] = {0};
            if (ResolveQuestionSubject(graph, &tokens, i,
                                       subj, sizeof(subj)))
            {
                /* Second entity for (S,R,O) verification: whichever
                   side the subject did not come from. Empty when the
                   question names a single entity (slot mode). */
                {
                    char trail[128] = {0};
                    if (ResolveEntity(graph, &tokens, i + 1,
                                      trail, sizeof(trail)) >= 0 &&
                        strcmp(trail, subj) != 0)
                        strcpy(q.object, trail);
                    else
                    {
                        PARSED_SENTENCE pre;
                        memset(&pre, 0, sizeof(pre));
                        for (uint32_t k = 0; k < i &&
                                            pre.count < PARSER_MAX_TOKENS;
                             k++)
                        {
                            strcpy(pre.tokens[pre.count],
                                   tokens.tokens[k]);
                            pre.count++;
                        }
                        char pre_ent[128] = {0};
                        if (ResolveEntity(graph, &pre, 0,
                                          pre_ent, sizeof(pre_ent)) >= 0 &&
                            strcmp(pre_ent, subj) != 0)
                            strcpy(q.object, pre_ent);
                    }
                }
                {
                    /* Subject-aware descriptor: the winning relation
                       may never touch this subject (bare IDIOMA wins
                       the ranking for Polonia, but only IDIOMA_OFICIAL
                       holds its fact). Among every resolving token,
                       prefer — in winner-rank order — the first
                       relation co-occurring with the subject.
                       Relations are vocabulary; co-occurrence with the
                       asked entity is evidence. No co-occurrence
                       anywhere keeps the winner (honest attempt). */
                    SYMBOL_ID subj_id = SymbolFind(graph->symbols, subj);
                    if (subj_id != SYMBOL_INVALID)
                    {
                        SYMBOL_ID best_rid =
                            SymbolFind(graph->symbols, best_rel);
                        RELATION *probe[1];
                        if (best_rid != SYMBOL_INVALID &&
                            GraphQuerySubjectRelation(graph, subj_id,
                                                      best_rid,
                                                      probe, 1) == 0)
                        {
                            /* Evidence-ordered redirect: every resolving
                               token contributes its relation plus the
                               key-sharing family (IDIOMA names IDIOMA
                               and IDIOMA_OFICIAL; "tiene" names TIENE
                               for a "Dios" question). Ranked by affix,
                               exact token-name match, established use;
                               the first touching the subject wins. A
                               resolving descriptor that yields no
                               triple is useless; evidence decides.
                               Runs ONLY when the winner co-occurs with
                               nothing, so every current pass is
                               untouched by construction. */
                            uint32_t c_idx[64];
                            size_t c_affix[64];
                            int c_exact[64];
                            uint64_t c_freq[64];
                            uint32_t ncand = 0;
                            uint32_t nrel = RelCacheFill(graph);
                            for (uint32_t t = 0;
                                 t < tokens.count && ncand < 64; t++)
                            {
                                if (strcmp(tokens.tokens[t], "NO") == 0)
                                    continue;
                                char trel[64] = {0};
                                int ttrusted = 0;
                                if (!ResolveRelationPass(graph,
                                                         tokens.tokens[t],
                                                         trel, sizeof(trel),
                                                         &ttrusted))
                                    continue;
                                char dstem[64] = {0};
                                StemWord(tokens.tokens[t], dstem,
                                         sizeof dstem);
                                for (uint32_t k = 0;
                                     k < nrel && ncand < 64; k++)
                                {
                                /* Reserved: NO never answers. */
                                if (strcmp(rel_cache[k].name, "NO") == 0)
                                    continue;
                                int names = 0;
                                for (uint32_t j = 0;
                                     j < rel_cache[k].nkeys; j++)
                                {
                                    if (strcmp(rel_cache[k].keys[j],
                                               tokens.tokens[t]) == 0 ||
                                        strcmp(rel_cache[k].keys[j],
                                               dstem) == 0)
                                    {
                                        names = 1;
                                        break;
                                    }
                                }
                                if (!names)
                                    continue;
                                if (strcmp(rel_cache[k].name,
                                           best_rel) == 0)
                                    continue;
                                {
                                    int dup = 0;
                                    for (uint32_t d = 0; d < ncand; d++)
                                        if (c_idx[d] == k)
                                        {
                                            dup = 1;
                                            break;
                                        }
                                    if (dup)
                                        continue;
                                }
                                c_idx[ncand] = k;
                                c_affix[ncand] =
                                    LcpLen(tokens.tokens[t],
                                           rel_cache[k].name);
                                c_exact[ncand] =
                                    (strcmp(tokens.tokens[t],
                                            rel_cache[k].name) == 0);
                                c_freq[ncand] =
                                    TokenRarity(graph,
                                                rel_cache[k].name);
                                ncand++;
                                }
                            }
                            /* Resolution order: affix, exact-name,
                               then predicate frequency. First
                               co-occurring wins. */
                            for (uint32_t a = 0; a < ncand; a++)
                            {
                                for (uint32_t b = a + 1; b < ncand; b++)
                                {
                                    if (c_affix[b] > c_affix[a] ||
                                        (c_affix[b] == c_affix[a] &&
                                         c_exact[b] > c_exact[a]) ||
                                        (c_affix[b] == c_affix[a] &&
                                         c_exact[b] == c_exact[a] &&
                                         c_freq[b] > c_freq[a]))
                                    {
                                        uint32_t t = c_idx[a];
                                        c_idx[a] = c_idx[b];
                                        c_idx[b] = t;
                                        size_t ta = c_affix[a];
                                        c_affix[a] = c_affix[b];
                                        c_affix[b] = ta;
                                        int te = c_exact[a];
                                        c_exact[a] = c_exact[b];
                                        c_exact[b] = te;
                                        uint64_t tf = c_freq[a];
                                        c_freq[a] = c_freq[b];
                                        c_freq[b] = tf;
                                    }
                                }
                            }
                            for (uint32_t c = 0; c < ncand; c++)
                            {
                                SYMBOL_ID crid = SymbolFind(graph->symbols,
                                    rel_cache[c_idx[c]].name);
                                if (crid != SYMBOL_INVALID &&
                                    GraphQuerySubjectRelation(graph, subj_id,
                                                              crid, probe,
                                                              1) > 0)
                                {
                                    strcpy(best_rel,
                                           rel_cache[c_idx[c]].name);
                                    break;
                                }
                            }
                        }
                    }
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
                if (ResolveQuestionSubject(graph, &tokens,
                                           (uint32_t)epos, subj,
                                           sizeof(subj)))
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
            /* Counting form: the numeral, not the list. Exact total
               of POSITIVE claims (denials are not counted).
               Zero is honest when nothing positive is held. */
            if (q->is_count)
            {
                uint32_t total = RelationCountBySubjectRelationPolar(
                    graph->relations, subj_id, rel_id,
                    POLARITY_POSITIVE);
                snprintf(out_answer, max_len, "%u", total);
                out_answer[max_len - 1] = '\0';
                return 1;
            }
            /* Negative verification (S,R,O): the resolved relation
               first, then fallback-ordered co-occurring relations
               (the descriptor "idioma" ambiguously covers IDIOMA and
               IDIOMA_OFICIAL: evidence decides, winner first).
               Exact direction first, then swapped: leading-NO forms
               ("¿No es Oslo la capital de Noruega?") put the holder
               last. The swap can only confirm against a stored swapped
               claim; documented risk, vanishingly rare in practice.
               Both polarities on one relation surface as a counted
               dispute (P4 evidence). */
            if (q->is_negative && q->object[0] != '\0')
            {
                SYMBOL_ID obj_id =
                    StemFindSymbol(graph->symbols, q->object);
                if (obj_id != SYMBOL_INVALID)
                {
                    SYMBOL_ID vrel[65];
                    uint32_t nvrel = 0;
                    vrel[nvrel++] = rel_id;
                    /* Relation family: every entry sharing a cache key
                       with the winner (IDIOMA names both IDIOMA and
                       IDIOMA_OFICIAL, either direction). Evidence
                       decides within the family, winner first. */
                    {
                        char fkeys[16][64];
                        uint32_t nfkeys = 0;
                        uint32_t nrel = RelCacheFill(graph);
                        for (uint32_t k = 0; k < nrel; k++)
                        {
                            if (strcmp(rel_cache[k].name,
                                       q->relation) != 0)
                                continue;
                            for (uint32_t j = 0;
                                 j < rel_cache[k].nkeys &&
                                 nfkeys < 16; j++)
                            {
                                strcpy(fkeys[nfkeys],
                                       rel_cache[k].keys[j]);
                                nfkeys++;
                            }
                        }
                        for (uint32_t k = 0;
                             k < nrel && nvrel < 65; k++)
                        {
                            if (strcmp(rel_cache[k].name, "NO") == 0 ||
                                strcmp(rel_cache[k].name,
                                       q->relation) == 0)
                                continue;
                            int shares = 0;
                            for (uint32_t j = 0;
                                 j < rel_cache[k].nkeys && !shares; j++)
                                for (uint32_t f = 0;
                                     f < nfkeys; f++)
                                    if (strcmp(rel_cache[k].keys[j],
                                               fkeys[f]) == 0)
                                    {
                                        shares = 1;
                                        break;
                                    }
                            if (!shares)
                                continue;
                            SYMBOL_ID crid = SymbolFind(graph->symbols,
                                rel_cache[k].name);
                            if (crid == SYMBOL_INVALID)
                                continue;
                            RELATION *probe[1];
                            if (GraphQuerySubjectRelation(graph, subj_id,
                                                          crid, probe,
                                                          1) > 0 ||
                                GraphQuerySubjectRelation(graph, obj_id,
                                                          crid, probe,
                                                          1) > 0)
                                vrel[nvrel++] = crid;
                        }
                    }
                    for (uint32_t v = 0; v < nvrel; v++)
                    {
                        RELATION *neg = RelationFindPolar(graph->relations,
                            subj_id, vrel[v], obj_id, POLARITY_NEGATIVE);
                        RELATION *pos = RelationFindPolar(graph->relations,
                            subj_id, vrel[v], obj_id, POLARITY_POSITIVE);
                        if (neg == NULL && pos == NULL)
                        {
                            neg = RelationFindPolar(graph->relations,
                                obj_id, vrel[v], subj_id,
                                POLARITY_NEGATIVE);
                            pos = RelationFindPolar(graph->relations,
                                obj_id, vrel[v], subj_id,
                                POLARITY_POSITIVE);
                        }
                        if (neg != NULL && pos == NULL)
                        {
                            snprintf(out_answer, max_len, "%s",
                                     LangString(I18N_NO, 0));
                            out_answer[max_len - 1] = '\0';
                            return 1;
                        }
                        if (pos != NULL && neg == NULL)
                        {
                            snprintf(out_answer, max_len, "%s",
                                     LangString(I18N_YES, 0));
                            out_answer[max_len - 1] = '\0';
                            return 1;
                        }
                        if (pos != NULL && neg != NULL)
                        {
                            snprintf(out_answer, max_len,
                                     "%s (%llu) / %s (%llu)",
                                     LangString(I18N_YES, 0),
                                     (unsigned long long)pos->count,
                                     LangString(I18N_NO, 0),
                                     (unsigned long long)neg->count);
                            out_answer[max_len - 1] = '\0';
                            return 1;
                        }
                    }
                }
                return 0;
            }
            RELATION *results[32];
            uint32_t n = GraphQuerySubjectRelation(
                graph, subj_id, rel_id, results, 32);

            /* Negatives never list as facts: a denied claim answering
               a slot question would be a lie. All-negative resolves
               to honest unknown. */
            RELATION *posit[32];
            uint32_t npos = 0;
            for (uint32_t f = 0; f < n && npos < 32; f++)
                if (results[f]->polarity != POLARITY_NEGATIVE)
                    posit[npos++] = results[f];

            if (npos > 0)
            {
                /* Order answers by semantic-area coherence. */
                ParserRankByArea(graph, subj_id, posit, npos);

                /* Build answer from ALL positive objects (recall over
                   truncation: the loop is buffer-guarded, so long
                   lists degrade to fits-in-buffer, never overflow).
                   A capped answer hides known facts (Leonardo holds
                   11 occupations, not 5). Objects carrying a negative
                   triple too surface the dispute with both counts. */
                uint32_t pos = 0;
                for (uint32_t i = 0; i < npos; i++)
                {
                    const SYMBOL *obj = SymbolGet(graph->symbols, posit[i]->object);
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
                        RELATION *contra = RelationFindPolar(
                            graph->relations, subj_id, rel_id,
                            posit[i]->object, POLARITY_NEGATIVE);
                        if (contra != NULL)
                        {
                            char mark[64];
                            snprintf(mark, sizeof(mark),
                                     " [DISPUTED +%llu/-%llu]",
                                     (unsigned long long)posit[i]->count,
                                     (unsigned long long)contra->count);
                            uint32_t mlen = (uint32_t)strlen(mark);
                            if (pos + mlen < max_len)
                            {
                                memcpy(out_answer + pos, mark, mlen);
                                pos += mlen;
                            }
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
