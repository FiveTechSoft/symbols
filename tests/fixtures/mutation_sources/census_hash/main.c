/* Source: src/parser.c (CensusHash), verbatim.
   Oracle: bucket values of the original function. */
#include <stdint.h>

#define CENSUS_BUCKETS 65536

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

int main(void)
{
    int bad = 0;
    bad += CensusHash("") != 5381;
    bad += CensusHash("A") != 46566;
    bad += CensusHash("COMEN") != 3223;
    bad += CensusHash("HIJOS") != 12642;
    bad += CensusHash("capital") != 51107;
    bad += CensusHash(0) != 5381;
    return bad == 0 ? 0 : 1;
}
