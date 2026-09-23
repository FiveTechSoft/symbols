/* Source: src/server_proto.c (HexVal), verbatim.
   Oracle: hex digit values and rejections. */

static int HexVal(char c, unsigned *v)
{
    if (c >= '0' && c <= '9')
        *v = (unsigned)(c - '0');
    else if (c >= 'a' && c <= 'f')
        *v = (unsigned)(c - 'a' + 10);
    else if (c >= 'A' && c <= 'F')
        *v = (unsigned)(c - 'A' + 10);
    else
        return 0;
    return 1;
}

int main(void)
{
    unsigned v = 99;
    int bad = 0;
    bad += !(HexVal('0', &v) == 1 && v == 0);
    bad += !(HexVal('9', &v) == 1 && v == 9);
    bad += !(HexVal('a', &v) == 1 && v == 10);
    bad += !(HexVal('f', &v) == 1 && v == 15);
    bad += !(HexVal('A', &v) == 1 && v == 10);
    bad += !(HexVal('F', &v) == 1 && v == 15);
    bad += HexVal('g', &v) != 0;
    bad += HexVal('G', &v) != 0;
    bad += HexVal('/', &v) != 0;
    bad += HexVal(':', &v) != 0;
    bad += HexVal('`', &v) != 0;
    bad += HexVal('@', &v) != 0;
    return bad == 0 ? 0 : 1;
}
