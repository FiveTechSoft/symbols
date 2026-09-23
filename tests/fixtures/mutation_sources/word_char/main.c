/* Source: src/chat.c (ChatIsWordChar), verbatim.
   Oracle: outputs of the original function on boundary inputs. */

static int ChatIsWordChar(unsigned char c)
{
    return (c >= '0' && c <= '9') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
           c == '\'' || c >= 0x80;
}

int main(void)
{
    int bad = 0;
    bad += (ChatIsWordChar('0')) != 1;
    bad += (ChatIsWordChar('9')) != 1;
    bad += (ChatIsWordChar('/')) != 0;
    bad += (ChatIsWordChar(':')) != 0;
    bad += (ChatIsWordChar('A')) != 1;
    bad += (ChatIsWordChar('Z')) != 1;
    bad += (ChatIsWordChar('@')) != 0;
    bad += (ChatIsWordChar('[')) != 0;
    bad += (ChatIsWordChar('a')) != 1;
    bad += (ChatIsWordChar('z')) != 1;
    bad += (ChatIsWordChar('`')) != 0;
    bad += (ChatIsWordChar('{')) != 0;
    bad += (ChatIsWordChar('\'')) != 1;
    bad += (ChatIsWordChar(0x7F)) != 0;
    bad += (ChatIsWordChar(0x80)) != 1;
    bad += (ChatIsWordChar(0xFF)) != 1;
    return bad == 0 ? 0 : 1;
}
