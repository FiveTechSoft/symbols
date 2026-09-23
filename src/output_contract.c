/* output_contract: see include/output_contract.h. */
#include "output_contract.h"

#include <ctype.h>
#include <string.h>

typedef enum { UNIT_NONE, UNIT_CHARS, UNIT_WORDS, UNIT_LINES } UNIT;

/* Unit vocabulary (the only lexicon here), matched as word prefixes on
   lower-cased ASCII; accented forms are compared after stripping the
   UTF-8 bytes of the accent-bearing letter. */
static UNIT UnitOf(const char *w)
{
    if (strncmp(w, "char", 4) == 0 || strncmp(w, "caracter", 8) == 0)
        return UNIT_CHARS;
    if (strncmp(w, "word", 4) == 0 || strncmp(w, "palabra", 7) == 0)
        return UNIT_WORDS;
    if (strncmp(w, "line", 4) == 0 || strncmp(w, "lnea", 4) == 0)
        return UNIT_LINES;
    return UNIT_NONE;
}

/* Words that turn a following count into a lower bound. */
static int IsLowerBoundMarker(const char *prev2, const char *prev1)
{
    if (strcmp(prev1, ">=") == 0 || strcmp(prev1, ">") == 0 || strcmp(prev1, "\xE2\x89\xA5") == 0)
        return 1;
    if (strcmp(prev2, "at") == 0 && strcmp(prev1, "least") == 0)
        return 1;
    if (strcmp(prev2, "al") == 0 && strcmp(prev1, "menos") == 0)
        return 1;
    if (strcmp(prev1, "minimum") == 0 || strcmp(prev1, "mnimo") == 0)
        return 1;
    return 0;
}

/* Number words for "one line" / "una sola linea" style counts. */
static int CountOf(const char *w)
{
    int v = 0;
    const char *p = w;
    if (*p == '\0')
        return -1;
    while (isdigit((unsigned char)*p))
        v = v * 10 + (*p++ - '0');
    if (*p == '\0' && p != w)
        return v;
    if (strcmp(w, "one") == 0 || strcmp(w, "single") == 0 || strcmp(w, "una") == 0 ||
        strcmp(w, "un") == 0 || strcmp(w, "sola") == 0 || strcmp(w, "solo") == 0 ||
        strcmp(w, "nica") == 0)
        return 1;
    return -1;
}

/* Next token: lower-cased ASCII letters/digits, plus the comparator
   glyphs; non-ASCII letter bytes are dropped so "línea" -> "lnea". */
static const char *NextToken(const char *p, char *tok, size_t size)
{
    size_t o = 0;
    while (*p != '\0')
    {
        unsigned char c = (unsigned char)*p;
        if (isalnum(c) || c >= 0x80)
            break;
        if (c == '<' || c == '>')
        {
            tok[o++] = (char)c;
            p++;
            if (*p == '=')
                tok[o++] = *p++;
            tok[o] = '\0';
            return p;
        }
        p++;
    }
    if (*p == '\0')
        return NULL;
    if ((unsigned char)p[0] == 0xE2 && (unsigned char)p[1] == 0x89 &&
        ((unsigned char)p[2] == 0xA4 || (unsigned char)p[2] == 0xA5))
    {
        memcpy(tok, p, 3);
        tok[3] = '\0';
        return p + 3;
    }
    while (*p != '\0')
    {
        unsigned char c = (unsigned char)*p;
        if (isalnum(c))
        {
            if (o + 1 < size)
                tok[o++] = (char)tolower(c);
        }
        else if (c >= 0x80)
        {
            if ((unsigned char)p[0] == 0xE2 && o > 0)
                break; /* a glyph (dash, comparator) ends a word; a lone glyph
                          is consumed below so the scan always advances */
            /* letter byte of a multi-byte character: skip it */
        }
        else
            break;
        p++;
    }
    tok[o] = '\0';
    return p;
}

static void Tighten(int *slot, int v)
{
    if (v > 0 && (*slot == 0 || v < *slot))
        *slot = v;
}

int OutputContractParse(const char *text, OUTPUT_CONTRACT *c)
{
    char t0[32] = "", t1[32] = "", t2[32] = "", t3[32] = "";
    const char *p = text;
    if (c == NULL)
        return 0;
    memset(c, 0, sizeof(*c));
    if (text == NULL)
        return 0;
    /* sliding window: t0 t1 [t2=count] [t3=unit], also count directly
       before unit with one modifier between ("one single line") */
    while ((p = NextToken(p, t3, sizeof(t3))) != NULL)
    {
        UNIT u = UnitOf(t3);
        if (u != UNIT_NONE)
        {
            const char *cnt = t2, *a = t1, *b = t0;
            int n = CountOf(t2);
            if (n < 0 && CountOf(t1) >= 0)
            {
                /* one modifier between count and unit */
                cnt = t1;
                n = CountOf(t1);
                a = t0;
                b = "";
            }
            /* number words ("one", "una") bound lines only; sizes in
               characters or words must be written as digits */
            if (n > 0 && !isdigit((unsigned char)cnt[0]) && u != UNIT_LINES)
                n = -1;
            if (n > 0 && !IsLowerBoundMarker(b, a))
            {
                if (u == UNIT_CHARS)
                    Tighten(&c->max_chars, n);
                else if (u == UNIT_WORDS)
                    Tighten(&c->max_words, n);
                else
                    Tighten(&c->max_lines, n);
            }
        }
        memcpy(t0, t1, sizeof(t0));
        memcpy(t1, t2, sizeof(t1));
        memcpy(t2, t3, sizeof(t2));
    }
    return c->max_chars > 0 || c->max_words > 0 || c->max_lines > 0;
}

static int CodePoints(const char *s, size_t len)
{
    int n = 0;
    for (size_t i = 0; i < len; i++)
        if (((unsigned char)s[i] & 0xC0) != 0x80)
            n++;
    return n;
}

static int Words(const char *s)
{
    int n = 0, in = 0;
    for (; *s; s++)
    {
        int sp = (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r');
        if (!sp && !in)
            n++;
        in = !sp;
    }
    return n;
}

int OutputContractCheck(const char *text, const OUTPUT_CONTRACT *c)
{
    int lines = 1;
    if (text == NULL || c == NULL || text[0] == '\0')
        return 0;
    for (const char *p = text; *p; p++)
        if (*p == '\n')
            lines++;
    if (c->max_lines > 0 && lines > c->max_lines)
        return 0;
    if (c->max_words > 0 && Words(text) > c->max_words)
        return 0;
    if (c->max_chars > 0 && CodePoints(text, strlen(text)) > c->max_chars)
        return 0;
    return 1;
}

int OutputContractCompose(const char *subject, const OUTPUT_CONTRACT *c,
                          char *out, size_t size)
{
    size_t o = 0;
    const char *p = subject;
    int prev_sp = 1;
    if (out == NULL || size == 0)
        return 0;
    out[0] = '\0';
    if (subject == NULL || c == NULL)
        return 0;
    /* first non-empty line, whitespace collapsed */
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    for (; *p && *p != '\n' && *p != '\r'; p++)
    {
        int sp = (*p == ' ' || *p == '\t');
        if (sp && prev_sp)
            continue;
        if (o + 1 < size)
            out[o++] = sp ? ' ' : *p;
        prev_sp = sp;
    }
    out[o] = '\0';
    /* trim trailing spaces, periods and colons; strip wrapping quotes */
    while (o > 0 && (out[o - 1] == ' ' || out[o - 1] == '.' || out[o - 1] == ':'))
        out[--o] = '\0';
    if (o >= 2 && (out[0] == '"' || out[0] == '\'' || out[0] == '`') && out[o - 1] == out[0])
    {
        memmove(out, out + 1, o - 2);
        o -= 2;
        out[o] = '\0';
    }
    if (o == 0)
        return 0;
    if (out[0] >= 'a' && out[0] <= 'z')
        out[0] = (char)(out[0] - 'a' + 'A');
    /* cut at word boundaries until every bound holds */
    int cut = 0;
    while (!OutputContractCheck(out, c))
    {
        cut = 1;
        char *sp = strrchr(out, ' ');
        if (sp == NULL)
        {
            /* single long word: cut at a code-point boundary */
            if (c->max_chars <= 0)
                break;
            size_t i = 0;
            int n = 0;
            while (out[i] && n < c->max_chars)
            {
                i++;
                while (((unsigned char)out[i] & 0xC0) == 0x80)
                    i++;
                n++;
            }
            out[i] = '\0';
            break;
        }
        *sp = '\0';
        o = strlen(out);
        while (o > 0 && (out[o - 1] == ',' || out[o - 1] == ';' || out[o - 1] == '-'))
            out[--o] = '\0';
    }
    /* after a cut, do not end on a dangling one- or two-letter word
       (articles, prepositions); a length heuristic, not a word list */
    if (cut)
    {
        char *sp;
        while ((sp = strrchr(out, ' ')) != NULL && CodePoints(sp + 1, strlen(sp + 1)) <= 2)
            *sp = '\0';
    }
    return out[0] != '\0' && OutputContractCheck(out, c);
}
