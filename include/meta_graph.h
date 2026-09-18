/* meta_graph: interpretation layer over transferred truth.
   English code comments (project rule).
   The objective layer (symbols, sentences, frequencies, count
   vectors) is NEVER written here: it stays bit-identical, and
   deleting this whole layer loses nothing but learned emphasis.
   This layer records USE, deterministically:
   - engagement: a shown sentence that earned continuation
     (follow-up on the same topic) gains bounded weight;
   - attraction: advancing from sentence A to sentence B writes
     a directed edge A->B (associative navigation learned from
     dialogue, not from text).
   Boosts never reach recall scale (1000): grounding always
   dominates interpretation. FIFO caps bound memory. Same
   session script replays bit-identically. */
#ifndef META_GRAPH_H
#define META_GRAPH_H

#include <stdint.h>

#define MG_ENTRIES 256
#define MG_EDGES 512
#define MG_BOOST_CAP 200

typedef struct
{
    uint64_t qkey;
    uint32_t file;
    uint32_t idx;
    int adj;
} MG_ENTRY;

typedef struct
{
    uint32_t ffile;
    uint32_t fidx;
    uint32_t tfile;
    uint32_t tidx;
    int w;
} MG_EDGE;

typedef struct
{
    MG_ENTRY entries[MG_ENTRIES];
    uint32_t nent;
    uint32_t epos;
    MG_EDGE edges[MG_EDGES];
    uint32_t nedge;
    uint32_t gpos;
} METAGRAPH;

/* FNV-1a over sorted words: deterministic topic key */
uint64_t MGKeyWords(const char **words, uint32_t n);
void MGInit(METAGRAPH *mg);
/* ensure (qkey,sent) exists */
void MGObserve(METAGRAPH *mg, uint64_t qkey, uint32_t file,
                 uint32_t idx);
/* engagement: shown sentence earned continuation (+25) */
void MGEngage(METAGRAPH *mg, uint64_t qkey, uint32_t file,
                uint32_t idx);
/* attraction edge on advance (from -> to, +25) */
void MGAttract(METAGRAPH *mg, uint32_t ffile, uint32_t fidx,
                 uint32_t tfile, uint32_t tidx);
/* total adjustment for (qkey,sent): entry + inbound edges
   from shown sentences. Capped at MG_BOOST_CAP. */
int MGBoost(const METAGRAPH *mg, uint64_t qkey, uint32_t file,
              uint32_t idx, const uint32_t *shown, uint32_t nshown);

#endif /* META_GRAPH_H */
