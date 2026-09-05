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
static int ResolvePredicatePass(const GRAPH *graph, const char *token,
                                int use_stem, char *out, size_t out_size);
static int ResolveEntity(const GRAPH *graph, const PARSED_SENTENCE *tokens,
                         uint32_t start, char *out, size_t out_size);

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

static int IsArticle(const char *word)
{
    return (strcmp(word, "EL") == 0 || strcmp(word, "LA") == 0 ||
            strcmp(word, "LOS") == 0 || strcmp(word, "LAS") == 0 ||
            strcmp(word, "UN") == 0 || strcmp(word, "UNA") == 0);
}

static int IsPreposition(const char *word)
{
    return (strcmp(word, "DE") == 0 || strcmp(word, "DEL") == 0 ||
            strcmp(word, "EN") == 0 || strcmp(word, "POR") == 0 ||
            strcmp(word, "PARA") == 0 || strcmp(word, "CON") == 0 ||
            strcmp(word, "A") == 0 || strcmp(word, "AL") == 0 ||
            strcmp(word, "SOBRE") == 0);
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

/* Span to symbol for INGEST: nearest-to-predicate wins. In statements
   arguments sit adjacent to their predicate (SVO adjacency), so the
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

    out[0] = '\0';
    for (uint32_t k = start; k < end; k++)
    {
        if (k > start) strcat(out, "_");
        strncat(out, tokens->tokens[k], out_size - strlen(out) - 1);
    }
    return (int)start;
}

/* Rarity of a token: its symbol frequency, 0 when unknown. Content
   bears rarity; glue repeats everywhere. */
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

    /* Clause roots: tokens describing used predicates (exact, then
       stemmed), in token order. Each root splits the input by position,
       so nested clauses each contribute their relation. */
    uint32_t roots[8];
    char names[8][64];
    uint32_t nroots = 0;
    for (int pass = 0; pass < 2 && nroots < 8; pass++)
    {
        for (uint32_t i = 0; i < tokens.count && nroots < 8; i++)
        {
            int known = 0;
            for (uint32_t k = 0; k < nroots; k++)
                if (roots[k] == i) { known = 1; break; }
            if (known)
                continue;
            if (ResolvePredicatePass(graph, tokens.tokens[i], pass,
                                     names[nroots], sizeof(names[0])))
                roots[nroots++] = i;
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

        char subj[128] = {0}, obj[128] = {0};
        if (SpanToSymbol(graph, &tokens, left_a, left_b, 1,
                         subj, sizeof(subj)) < 0)
            continue;
        if (SpanToSymbol(graph, &tokens, right_a, right_b, 0,
                         obj, sizeof(obj)) < 0)
            continue;

        SYMBOL_ID s = GraphAddSymbol(graph, subj);
        SYMBOL_ID p = GraphAddSymbol(graph, names[c]);
        SYMBOL_ID o = GraphAddSymbol(graph, obj);

        if (s == SYMBOL_INVALID || p == SYMBOL_INVALID || o == SYMBOL_INVALID)
            continue;

        int added = GraphAddRelation(graph, s, p, o);
        if (!added)
            continue;
        stored++;

        /* Update embeddings on every co-occurrence */
        if (graph->embeddings != NULL)
        {
            EMBEDDING_TABLE *emb = graph->embeddings;

            if (EmbeddingGetVector(emb, s) == NULL)
            {
                float v[EMBEDDING_DIM];
                EmbeddingRandomInit(v, (uint32_t)s * 2654435761u);
                EmbeddingSetVector(emb, s, v);
            }
            if (EmbeddingGetVector(emb, o) == NULL)
            {
                float v[EMBEDDING_DIM];
                EmbeddingRandomInit(v, (uint32_t)o * 2654435761u);
                EmbeddingSetVector(emb, o, v);
            }

            float *target  = (float *)EmbeddingGetVector(emb, s);
            float *context = (float *)EmbeddingGetVector(emb, o);
            if (target && context)
            {
                EmbeddingCooccur(target, context, 0.1f);
                EmbeddingCooccur(context, target, 0.1f);
                EmbeddingNormalize(target);
                EmbeddingNormalize(context);
            }
        }
    }

    return stored;
}

/* ============================================================
   Question Detection & Answering
   ============================================================ */

/* Dynamic predicate vocabulary: no word lists. A question token describes
   a graph predicate iff the wanted form (exact token or its stem) matches
   the predicate name or a stemmed compound part of it (HIJOS->HIJO via
   HIJO_DE, REYES->REY via REY_DE). Resolved against the live graph, so
   predicates learned at runtime become queryable immediately. Glue words
   (articles, prepositions, single letters) never match. */
static int PredNameMatches(const char *pname, const char *want)
{
    if (pname == NULL || want == NULL || want[0] == '\0')
        return 0;

    if (strcmp(pname, want) == 0)
        return 1;

    char parts[64];
    strncpy(parts, pname, sizeof(parts) - 1);
    parts[sizeof(parts) - 1] = '\0';
    char *saveptr = NULL;
    char *part = strtok_r(parts, "_", &saveptr);
    while (part != NULL)
    {
        /* Short parts (LA, DE, EN) are glue collisions, never meaning. */
        if (strlen(part) > 2 && !IsPreposition(part) && !IsArticle(part))
        {
            if (strcmp(part, want) == 0)
                return 1;
            char pstem[64];
            StemWord(part, pstem, sizeof(pstem));
            if (strcmp(pstem, want) == 0)
                return 1;
        }
        part = strtok_r(NULL, "_", &saveptr);
    }
    return 0;
}

/* First predicate (in graph order) described by the token. Pass 0 tries
   the exact token, pass 1 its stem. Returns 1 and the predicate name. */
static int ResolvePredicatePass(const GRAPH *graph, const char *token,
                                int use_stem, char *out, size_t out_size)
{
    if (graph == NULL || graph->relations == NULL || token == NULL ||
        out == NULL || out_size == 0)
        return 0;

    char stem[64];
    const char *want = token;
    if (use_stem)
    {
        StemWord(token, stem, sizeof(stem));
        want = stem;
    }

    uint32_t total = RelationCount(graph->relations);
    for (uint32_t i = 0; i < total; i++)
    {
        const RELATION *r = RelationGet(graph->relations, i);
        if (r == NULL || r->predicate == SYMBOL_INVALID)
            continue;
        const SYMBOL *p = SymbolGet(graph->symbols, r->predicate);
        if (p == NULL || p->name == NULL || p->name[0] == '\0')
            continue;
        if (PredNameMatches(p->name, want))
        {
            strncpy(out, p->name, out_size - 1);
            out[out_size - 1] = '\0';
            return 1;
        }
    }
    return 0;
}

/* Embedding description: the (token, predicate) pair with the highest
   cosine similarity across all used predicates. No threshold: the ranking
   IS the answer, and it sharpens as the model learns (rich model -> rich
   embeddings). Consulted only when symbolic resolution fails, so it can
   only turn past misses into hits, never override a symbolic hit. Tokens
   without a vector are skipped. */
static int ResolvePredicateEmbed(const GRAPH *graph,
                                 const PARSED_SENTENCE *tokens,
                                 char *out, size_t out_size, int *out_pos)
{
    if (graph == NULL || graph->embeddings == NULL || tokens == NULL ||
        out == NULL || out_size == 0)
        return 0;

    SYMBOL_ID pids[1024];
    uint32_t np = 0;
    uint32_t total = RelationCount(graph->relations);
    for (uint32_t i = 0; i < total && np < 1024; i++)
    {
        const RELATION *r = RelationGet(graph->relations, i);
        if (r == NULL || r->predicate == SYMBOL_INVALID)
            continue;
        int known = 0;
        for (uint32_t k = 0; k < np; k++)
            if (pids[k] == r->predicate) { known = 1; break; }
        if (!known)
            pids[np++] = r->predicate;
    }
    if (np == 0)
        return 0;

    float best_score = -2.0f;
    SYMBOL_ID best_pid = SYMBOL_INVALID;
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
            const float *pv = EmbeddingGetVector(graph->embeddings, pids[k]);
            if (pv == NULL)
                continue;
            float s = EmbeddingCosineSimilarity(tv, pv);
            if (s > best_score)
            {
                best_score = s;
                best_pid = pids[k];
                best_pos = (int)t;
            }
        }
    }

    if (best_pid == SYMBOL_INVALID)
        return 0;
    const SYMBOL *p = SymbolGet(graph->symbols, best_pid);
    if (p == NULL || p->name == NULL || p->name[0] == '\0')
        return 0;
    strncpy(out, p->name, out_size - 1);
    out[out_size - 1] = '\0';
    if (out_pos != NULL)
        *out_pos = best_pos;
    return 1;
}

/* Does the token name a used predicate exactly (no stemming)? Used for
   the entity rule: the entity is never a predicate name ("... David es"
   must resolve to DAVID, not ES). */
static int TokenNamesPredicate(const GRAPH *graph, const char *token)
{
    if (graph == NULL || graph->relations == NULL || token == NULL)
        return 0;

    uint32_t total = RelationCount(graph->relations);
    for (uint32_t i = 0; i < total; i++)
    {
        const RELATION *r = RelationGet(graph->relations, i);
        if (r == NULL || r->predicate == SYMBOL_INVALID)
            continue;
        const SYMBOL *p = SymbolGet(graph->symbols, r->predicate);
        if (p != NULL && p->name != NULL && strcmp(p->name, token) == 0)
            return 1;
    }
    return 0;
}

/* Entity = longest trailing token run (up to 4) naming an existing
   symbol. Trailing tokens that name used predicates are skipped first
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
        TokenNamesPredicate(graph, tokens->tokens[end - 1]))
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

    /* Fact shape: descriptor token pointing to a graph predicate plus an
       entity. The entity usually trails the descriptor ("la CAPITAL de
       FRANCIA es") but may precede it ("Paris es"): both directions are
       tried, trailing first. No separator word, no copula check, no
       opener check: the shape validates itself because both ends
       resolve against the live map. Descriptor search runs exact, then
       stemmed, then by embedding description. */
    {
        /* Per token, exact then stemmed: position outranks inflection.
           The descriptor precedes the copula structurally, so an early
           stem hit (CAPITALES->CAPITAL) wins over a later exact one. */
        for (uint32_t i = 0; i < tokens.count; i++)
        {
            for (int pass = 0; pass < 2; pass++)
            {
                char pred[64] = {0};
                if (!ResolvePredicatePass(graph, tokens.tokens[i], pass,
                                          pred, sizeof(pred)))
                    continue;
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
                    continue;
                strcpy(q.subject, subj);
                strcpy(q.predicate, pred);
                q.valid = 1;
                q.is_question = 1;
                return q;
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
            char pred[64] = {0};
            int epos = -1;
            if (ResolvePredicateEmbed(graph, &desc, pred, sizeof(pred),
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
                    strcpy(q.predicate, pred);
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
        /* Find predicate */
        SYMBOL_ID pred_id = StemFindSymbol(graph->symbols, q->predicate);

        if (pred_id != SYMBOL_INVALID)
        {
            RELATION *results[8];
            uint32_t n = GraphQuerySubjectPredicate(
                graph, subj_id, pred_id, results, 8);

            if (n > 0)
            {
                /* Order answers by semantic-area coherence: candidate
                   triples rank by similarity to the subject's area
                   (mean of its composed relation vectors). Relations
                   establish meanings, so the most coherent triple
                   answers first. Order only; membership is exact. */
                if (graph->embeddings != NULL && n > 1)
                {
                    float area[EMBEDDING_DIM] = {0};
                    int area_n = 0;
                    RELATION *area_rels[32];
                    uint32_t na = GraphQuerySubject(graph, subj_id,
                                                    area_rels, 32);
                    for (uint32_t a = 0; a < na; a++)
                    {
                        float comp[EMBEDDING_DIM];
                        if (EmbeddingComposeRelation(
                                graph->embeddings,
                                area_rels[a]->subject,
                                area_rels[a]->predicate,
                                area_rels[a]->object, comp))
                        {
                            for (int d = 0; d < EMBEDDING_DIM; d++)
                                area[d] += comp[d];
                            area_n++;
                        }
                    }
                    if (area_n > 0)
                    {
                        for (int d = 0; d < EMBEDDING_DIM; d++)
                            area[d] /= (float)area_n;
                        for (uint32_t i = 1; i < n; i++)
                        {
                            RELATION *rk = results[i];
                            float ck[EMBEDDING_DIM];
                            if (!EmbeddingComposeRelation(
                                    graph->embeddings, subj_id, pred_id,
                                    rk->object, ck))
                                continue;
                            float sk = EmbeddingCosineSimilarity(ck, area);
                            uint32_t j = i;
                            while (j > 0)
                            {
                                float cj[EMBEDDING_DIM];
                                if (!EmbeddingComposeRelation(
                                        graph->embeddings, subj_id, pred_id,
                                        results[j - 1]->object, cj))
                                    break;
                                float sj = EmbeddingCosineSimilarity(cj, area);
                                if (sj >= sk)
                                    break;
                                results[j] = results[j - 1];
                                j--;
                            }
                            results[j] = rk;
                        }
                    }
                }

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
