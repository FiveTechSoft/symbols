/* Source: src/server_proto.c (StripEnclitic), verbatim.
   Oracle: outputs of the original function on boundary inputs. */
#include <string.h>
#include <string.h>

static size_t StripEnclitic(const char *verb, char *out, size_t out_size)
{
    static const char *enclitics[] = {
        "mos", "dle", "dla", "dlo", "dles", "dlas", "dlos",
        "le", "la", "lo", "les", "las", "los",
        "me", "te", "nos", "os"
    };
    size_t vlen = strlen(verb);
    for (size_t e = 0; e < sizeof(enclitics) / sizeof(enclitics[0]); e++)
    {
        size_t elen = strlen(enclitics[e]);
        if (vlen > elen && strcmp(verb + vlen - elen, enclitics[e]) == 0)
        {
            size_t stem_len = vlen - elen;
            if (stem_len >= out_size) stem_len = out_size - 1;
            memcpy(out, verb, stem_len);
            out[stem_len] = '\0';
            return stem_len;
        }
    }
    size_t copy = vlen < out_size - 1 ? vlen : out_size - 1;
    memcpy(out, verb, copy);
    out[copy] = '\0';
    return copy;
}

static char sbuf[64];
static const char *strip(const char *v) { StripEnclitic(v, sbuf, sizeof(sbuf)); return sbuf; }
static const char *strip_small(const char *v) { StripEnclitic(v, sbuf, 5); return sbuf; }

int main(void)
{
    int bad = 0;
    bad += strcmp(strip("damelo"), "dame") != 0;
    bad += strcmp(strip("hazlo"), "haz") != 0;
    bad += strcmp(strip("lo"), "lo") != 0;
    bad += strcmp(strip("decidlos"), "deci") != 0;
    bad += strcmp(strip("vamos"), "va") != 0;
    bad += strcmp(strip("dime"), "di") != 0;
    bad += strcmp(strip("casa"), "casa") != 0;
    bad += strcmp(strip("os"), "os") != 0;
    bad += strcmp(strip("dos"), "d") != 0;
    bad += strcmp(strip_small("haciendolo"), "haci") != 0;
    return bad == 0 ? 0 : 1;
}
