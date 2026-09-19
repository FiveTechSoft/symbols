/* text_lex: corpus-to-graph converter (lexical-syntactic analyzer).
   English code comments (project rule).
   Doctrine: the corpus enters COMPLETE. No stemming, no case folding,
   no stopwords, no SVO extraction, no triplets-as-knowledge.
   Every printable token becomes a symbol, byte-exact (source file
   is read-only; sentences keep byte offsets for literal display).
   Relations are MACHINERY, never claims: token order lives in the
   sentence store; co-occurrence pairs (adjacent + word@position,
   same Hebbian function and rate as ingest.c) train the embedding
   table, so embeddings become properties of symbols, relations and
   positions alike. Genuine garbage only: control bytes, blank
   lines, over-long runs. Everything dropped is counted AND
   sampled for audit. Deterministic: same bytes in -> same graph.
   Idempotent replay: second pass adds zero symbols/sentences and
   yields bit-identical vectors. */
#ifndef TEXT_LEX_H
#define TEXT_LEX_H

#include <stdint.h>
#include "graph.h"
#include "embedding.h"

#define TL_WORD_MAX 128
#define TL_DROP_SAMPLE 16
#define TL_DROP_LEN 32

typedef struct
{
    uint64_t bytes_read;
    uint32_t lines_seen;
    uint32_t lines_content;   /* non-blank lines */
    uint32_t lines_covered;   /* content lines spanned by >=1 sentence */
    uint32_t nsent_new;       /* sentences stored this call */
    uint32_t nsent_dup;       /* sentences already stored */
    uint32_t ntok_kept;       /* word/punct occurrences stored */
    uint32_t syms_new;        /* new symbols this call */
    uint32_t pair_updates;    /* Hebbian pair updates run */
    uint32_t toolong_dropped; /* alnum runs >= TL_WORD_MAX */
    uint32_t ctrl_skipped;    /* control bytes ignored */
    uint32_t dropped_distinct;
    char dropped_sample[TL_DROP_SAMPLE][TL_DROP_LEN];
} TEXTLEX_STATS;

typedef struct
{
    uint64_t *offs;   /* token byte offsets into the file image */
    uint32_t *lens;   /* token byte lengths */
    SYMBOL_ID *ids;   /* interned symbol per token */
    uint32_t *lines;  /* file line number per token (1-based) */
    uint32_t ntok;
    uint32_t cap;
    uint32_t sig[8];  /* top-m signature (finalized post-ingest) */
    int sig_ready;
    float density;    /* lexical ratio: tokens with 3+ alpha bytes */
} TL_SENT;

typedef struct
{
    TL_SENT *sents;
    uint32_t nsent;
    uint32_t cap;
    /* source image kept for literal display (session-scoped;
       one file per store; freed with the store) */
    unsigned char *image;
    size_t imagelen;
} TEXTLEX;

TEXTLEX *TextLexCreate(void);
void TextLexFree(TEXTLEX *tl);
/* release contents, keep usable (for unload-by-rebuild) */
void TextLexClear(TEXTLEX *tl);
/* reset positional accumulators (unload rebuilds from zero) */
void TextLexPosReset(void);
/* Whole file (first_line=1, last_line=0 means all). Deterministic. */
TEXTLEX_STATS TextLexIngest(GRAPH *graph, TEXTLEX *tl,
                            const char *path,
                            uint32_t first_line, uint32_t last_line);
uint32_t TextLexSentCount(const TEXTLEX *tl);
const TL_SENT *TextLexSentence(const TEXTLEX *tl, uint32_t idx);
/* first symbol id matching w under ASCII case variants
   (exact, First-upper, UPPER, lower); INVALID when absent */
SYMBOL_ID TextLexFindSymbol(const GRAPH *graph, const char *w);
/* Literal display: join token byte slices with single spaces
   (words byte-exact; wrap whitespace normalized, source intact). */
uint32_t TextLexSentenceText(const TEXTLEX *tl, uint32_t idx,
                             const unsigned char *img, size_t imglen,
                             char *out, size_t size);
/* Attention retrieval: rank sentences holding query symbols by
   novelty-weighted overlap plus query/sentence centroid cosine.
   Returns ranked count (<= max). */
uint32_t TextLexRetrieve(const TEXTLEX *tl, const GRAPH *graph,
                         const EMBEDDING_TABLE *emb,
                         const char **words, uint32_t nwords,
                         uint32_t *out_idx, float *out_score,
                         uint32_t max);
/* scoring flags for the experiment harness (see text_lex.c) */
#define SF_RARITY 1u
#define SF_INTER 2u
#define SF_ORDER 4u
#define SF_DROP 8u
#define SF_CENTROID 16u
#define SF_HAMMING 32u
uint32_t TextLexRetrieveV(const TEXTLEX *tl, const GRAPH *graph,
                          const EMBEDDING_TABLE *emb,
                          const char **words, uint32_t nwords,
                          uint32_t *out_idx, float *out_score,
                          uint32_t max, unsigned flags);

/* Concept concentration entry: key topics discovered from the embedding topology */
typedef struct
{
    SYMBOL_ID   id;
    const char *name;
    float       conc;
    uint64_t    freq;
} TL_CONCEPT;

/* Discover top thematic concepts from graph and embedding table by concentration
   (max bucket share of the count vector; peaked = focused topic, flat = glue).
   Returns count stored in out (up to max_out). */
uint32_t TextLexTopConcepts(const GRAPH *graph, const EMBEDDING_TABLE *emb,
                            TL_CONCEPT *out, uint32_t max_out);

/* Direct symbol lookup: find first sentence containing target_id.
   Returns sentence index or UINT32_MAX if not found. */
uint32_t TextLexFindSentenceBySymbol(const TEXTLEX *tl, SYMBOL_ID target_id);

/* Phase 1: temperature control for QKVScore scaling.
   T < 1 sharpens (top-1 dominant), T > 1 softens (uniform). */
void TextLexSetTemperature(float t);

#endif /* TEXT_LEX_H */
