/* Source: src/task_ops.c (CTOK, tok_distance), verbatim.
   Oracle: token edit distances. */
#include <stdio.h>
#include <string.h>

#define FRAG_MAX_TOK 24

typedef struct
{
    size_t start, end;
    char   text[64];
    int    name;          /* identifier/keyword */
} CTOK;

static int tok_distance(const CTOK *x, int nx, const CTOK *y, int ny)
{
    int d[FRAG_MAX_TOK + 1][FRAG_MAX_TOK + 1];
    for (int i = 0; i <= nx; i++) d[i][0] = i;
    for (int j = 0; j <= ny; j++) d[0][j] = j;
    for (int i = 1; i <= nx; i++)
        for (int j = 1; j <= ny; j++) {
            int sub = d[i - 1][j - 1] + (strcmp(x[i - 1].text, y[j - 1].text) != 0);
            int del = d[i - 1][j] + 1, ins = d[i][j - 1] + 1;
            d[i][j] = sub < del ? (sub < ins ? sub : ins) : (del < ins ? del : ins);
        }
    return d[nx][ny];
}

static int split(const char *s, CTOK *t)
{
    int n = 0;
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", s);
    for (char *w = strtok(buf, " "); w && n < FRAG_MAX_TOK; w = strtok(NULL, " "))
        snprintf(t[n++].text, sizeof(t[0].text), "%s", w);
    return n;
}

static int dist(const char *a, const char *b)
{
    CTOK x[FRAG_MAX_TOK], y[FRAG_MAX_TOK];
    int nx = split(a, x), ny = split(b, y);
    return tok_distance(x, nx, y, ny);
}

int main(void)
{
    int bad = 0;
    CTOK one[1], two[2];
    snprintf(one[0].text, 64, "a");
    snprintf(two[0].text, 64, "a");
    snprintf(two[1].text, 64, "b");
    bad += tok_distance(one, 1, two, 2) != 1;
    bad += tok_distance(two, 2, one, 1) != 1;
    bad += tok_distance(one, 0, two, 2) != 2;
    bad += dist("if ( a < b )", "if ( a < b )") != 0;
    bad += dist("if ( a < b )", "if ( a <= b )") != 1;
    bad += dist("x = y + 1 ;", "x = y - 2 ;") != 2;
    bad += dist("a b c", "c b a") != 2;
    bad += dist("return x ;", "x ;") != 1;
    bad += dist("f ( x )", "g ( x , y )") != 3;
    return bad == 0 ? 0 : 1;
}
