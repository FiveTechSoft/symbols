/* meta_graph.c — interpretation layer. See meta_graph.h. */
#include <string.h>

#include "meta_graph.h"

uint64_t MGKeyWords(const char **words, uint32_t n)
{
    uint64_t h = 1469598103934665603ULL;
    uint32_t order[8];
    uint32_t i;
    uint32_t m;
    if (words == NULL)
        return h;
    for (i = 0; i < n && i < 8; i++)
        order[i] = i;
    /* insertion sort of indices by word bytes (deterministic) */
    for (i = 1; i < n && i < 8; i++)
    {
        uint32_t j = i;
        while (j > 0 && strcmp(words[order[j - 1]], words[order[j]]) > 0)
        {
            uint32_t t = order[j];
            order[j] = order[j - 1];
            order[j - 1] = t;
            j--;
        }
    }
    m = n < 8 ? n : 8;
    for (i = 0; i < m; i++)
    {
        const char *w = words[order[i]];
        size_t k;
        if (w == NULL)
            continue;
        for (k = 0; w[k] != '\0'; k++)
        {
            h ^= (uint64_t)(unsigned char)w[k];
            h *= 1099511628211ULL;
        }
        h ^= 0xFFULL;
        h *= 1099511628211ULL;
    }
    return h;
}

void MGInit(METAGRAPH *mg)
{
    uint32_t i;
    if (mg == NULL)
        return;
    for (i = 0; i < MG_ENTRIES; i++)
    {
        mg->entries[i].qkey = 0;
        mg->entries[i].file = 0;
        mg->entries[i].idx = 0;
        mg->entries[i].adj = 0;
    }
    mg->nent = 0;
    mg->epos = 0;
    for (i = 0; i < MG_EDGES; i++)
    {
        mg->edges[i].ffile = 0;
        mg->edges[i].fidx = 0;
        mg->edges[i].tfile = 0;
        mg->edges[i].tidx = 0;
        mg->edges[i].w = 0;
    }
    mg->nedge = 0;
    mg->gpos = 0;
}

static MG_ENTRY *FindEntry(METAGRAPH *mg, uint64_t qkey,
                           uint32_t file, uint32_t idx)
{
    uint32_t i;
    uint32_t n = mg->nent < MG_ENTRIES ? mg->nent : MG_ENTRIES;
    for (i = 0; i < n; i++)
    {
        if (mg->entries[i].qkey == qkey &&
            mg->entries[i].file == file && mg->entries[i].idx == idx)
            return &mg->entries[i];
    }
    return NULL;
}

static MG_ENTRY *AllocEntry(METAGRAPH *mg, uint64_t qkey,
                            uint32_t file, uint32_t idx)
{
    MG_ENTRY *e;
    if (mg->nent < MG_ENTRIES)
    {
        e = &mg->entries[mg->nent++];
    }
    else
    {
        e = &mg->entries[mg->epos];
        mg->epos = (mg->epos + 1) % MG_ENTRIES;
    }
    e->qkey = qkey;
    e->file = file;
    e->idx = idx;
    e->adj = 0;
    return e;
}

void MGObserve(METAGRAPH *mg, uint64_t qkey, uint32_t file,
                 uint32_t idx)
{
    if (mg == NULL)
        return;
    if (FindEntry(mg, qkey, file, idx) == NULL)
        AllocEntry(mg, qkey, file, idx);
}

void MGEngage(METAGRAPH *mg, uint64_t qkey, uint32_t file,
                uint32_t idx)
{
    MG_ENTRY *e;
    if (mg == NULL)
        return;
    e = FindEntry(mg, qkey, file, idx);
    if (e == NULL)
        e = AllocEntry(mg, qkey, file, idx);
    if (e->adj < MG_BOOST_CAP)
        e->adj += 25;
}

void MGAttract(METAGRAPH *mg, uint32_t ffile, uint32_t fidx,
                 uint32_t tfile, uint32_t tidx)
{
    MG_EDGE *e;
    uint32_t i;
    uint32_t n;
    if (mg == NULL)
        return;
    if (ffile == tfile && fidx == tidx)
        return;
    n = mg->nedge < MG_EDGES ? mg->nedge : MG_EDGES;
    for (i = 0; i < n; i++)
    {
        if (mg->edges[i].ffile == ffile &&
            mg->edges[i].fidx == fidx &&
            mg->edges[i].tfile == tfile &&
            mg->edges[i].tidx == tidx)
        {
            if (mg->edges[i].w < MG_BOOST_CAP)
                mg->edges[i].w += 25;
            return;
        }
    }
    if (mg->nedge < MG_EDGES)
    {
        e = &mg->edges[mg->nedge++];
    }
    else
    {
        e = &mg->edges[mg->gpos];
        mg->gpos = (mg->gpos + 1) % MG_EDGES;
    }
    e->ffile = ffile;
    e->fidx = fidx;
    e->tfile = tfile;
    e->tidx = tidx;
    e->w = 25;
}

int MGBoost(const METAGRAPH *mg, uint64_t qkey, uint32_t file,
              uint32_t idx, const uint32_t *shown, uint32_t nshown)
{
    const MG_ENTRY *e;
    uint32_t i;
    int tot = 0;
    if (mg == NULL)
        return 0;
    e = FindEntry((METAGRAPH *)mg, qkey, file, idx);
    if (e != NULL)
        tot += e->adj;
    if (shown != NULL)
    {
        uint32_t n =
            mg->nedge < MG_EDGES ? mg->nedge : MG_EDGES;
        for (i = 0; i < nshown; i++)
        {
            uint32_t sf = shown[i] >> 24;
            uint32_t sx = shown[i] & 0xFFFFFFu;
            uint32_t k;
            for (k = 0; k < n; k++)
            {
                if (mg->edges[k].ffile == sf &&
                    mg->edges[k].fidx == sx &&
                    mg->edges[k].tfile == file &&
                    mg->edges[k].tidx == idx)
                    tot += mg->edges[k].w;
            }
        }
    }
    if (tot > MG_BOOST_CAP)
        tot = MG_BOOST_CAP;
    if (tot < -MG_BOOST_CAP)
        tot = -MG_BOOST_CAP;
    return tot;
}
