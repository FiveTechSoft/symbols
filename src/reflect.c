/* reflect.c: see reflect.h. */
#include "reflect.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void clean(char *dst, size_t n, const char *src)
{
    size_t o = 0;
    for (; src && *src && o + 1 < n; src++)
        dst[o++] = (*src == '\t' || *src == '\n' || *src == '\r') ? ' ' : *src;
    dst[o] = '\0';
}

void ReflectCompose(REFLECTION *r)
{
    snprintf(r->text, sizeof(r->text), "Tried %s (%.200s); the toolchain reported: %.150s. Next attempt: exclude this edit.",
             r->op, r->detail, r->feedback);
}

static int parse_row(char *line, REFLECTION *r)
{
    char *f[8];
    int n = 0;
    f[n++] = line;
    for (char *p = line; *p && n < 8; p++)
        if (*p == '\t') { *p = '\0'; f[n++] = p + 1; }
    if (n < 8) return 0;
    char *nl = strpbrk(f[7], "\r\n");
    if (nl) *nl = '\0';
    if (strcmp(f[5], "toolchain")) return 0;   /* only toolchain feedback is ever read back */
    memset(r, 0, sizeof(*r));
    snprintf(r->key, sizeof(r->key), "%.31s", f[0]);
    r->attempt = atoi(f[1]);
    snprintf(r->op, sizeof(r->op), "%.31s", f[2]);
    snprintf(r->detail, sizeof(r->detail), "%.255s", f[3]);
    snprintf(r->feedback, sizeof(r->feedback), "%.159s", f[4]);
    r->ts = atoll(f[6]);
    snprintf(r->text, sizeof(r->text), "%.511s", f[7]);
    return 1;
}

int ReflectLoad(const char *path, const char *key, REFLECTION *out, int max)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    char line[1400];
    int n = 0;
    while (n < max && fgets(line, sizeof(line), f))
        if (parse_row(line, &out[n]) && (!key || !strcmp(out[n].key, key)))
            n++;
    fclose(f);
    return n;
}

static void row(FILE *f, const REFLECTION *r)
{
    char op[32], d[256], fb[160], t[512];
    clean(op, sizeof(op), r->op);
    clean(d, sizeof(d), r->detail);
    clean(fb, sizeof(fb), r->feedback);
    clean(t, sizeof(t), r->text);
    fprintf(f, "%s\t%d\t%s\t%s\t%s\ttoolchain\t%lld\t%s\n", r->key, r->attempt, op, d, fb, r->ts, t);
}

/* rewrite: every row not of drop_key, then add (may be NULL) */
static int rewrite(const char *path, const char *drop_key, const REFLECTION *add, int *dropped)
{
    static REFLECTION rows[REFLECT_MAX_ROWS];
    int n = ReflectLoad(path, NULL, rows, REFLECT_MAX_ROWS), keep0 = 0;
    char tmp[1100];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (!f) return 0;
    if (add && n == REFLECT_MAX_ROWS) keep0 = 1;   /* bounded: the oldest row gives way */
    *dropped = 0;
    for (int i = keep0; i < n; i++) {
        if (drop_key && !strcmp(rows[i].key, drop_key)) { (*dropped)++; continue; }
        row(f, &rows[i]);
    }
    if (add) row(f, add);
    int ok = fflush(f) == 0;
    ok &= fclose(f) == 0;
    if (!ok) { remove(tmp); return 0; }
#ifdef _WIN32
    remove(path);
#endif
    if (rename(tmp, path) != 0) { remove(tmp); return 0; }
    return 1;
}

int ReflectAppend(const char *path, const REFLECTION *r)
{
    int d;
    return rewrite(path, NULL, r, &d);
}

int ReflectForget(const char *path, const char *key)
{
    int d = 0;
    return rewrite(path, key, NULL, &d) ? d : -1;
}
