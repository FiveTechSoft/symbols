/* Source: src/server_proto.c (DecodeU), verbatim.
   Oracle: outputs of the original function on boundary inputs. */

static int DecodeU(unsigned v, char *out)
{
    if (v < 0x80)
    {
        out[0] = (char)v;
        return 1;
    }
    if (v < 0x800)
    {
        out[0] = (char)(0xC0 | (v >> 6));
        out[1] = (char)(0x80 | (v & 0x3F));
        return 2;
    }
    out[0] = (char)(0xE0 | (v >> 12));
    out[1] = (char)(0x80 | ((v >> 6) & 0x3F));
    out[2] = (char)(0x80 | (v & 0x3F));
    return 3;
}

static unsigned enc(unsigned v) { char o[4] = {0}; int n = DecodeU(v, o); return (unsigned)n << 24 | (unsigned char)o[0] << 16 | (unsigned char)o[1] << 8 | (unsigned char)o[2]; }

int main(void)
{
    int bad = 0;
    bad += (enc(0x41)) != 21037056;
    bad += (enc(0x7F)) != 25100288;
    bad += (enc(0x80)) != 46301184;
    bad += (enc(0xE9)) != 46377216;
    bad += (enc(0x7FF)) != 48217856;
    bad += (enc(0x800)) != 65052800;
    bad += (enc(0x20AC)) != 65176236;
    bad += (enc(0xFFFF)) != 66043839;
    bad += (enc(0)) != 16777216;
    return bad == 0 ? 0 : 1;
}
