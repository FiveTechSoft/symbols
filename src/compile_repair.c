/* compile_repair.c: single-edit repair candidates from the first compiler
   error; see compile_repair.h. Pure text: the caller compiles each
   candidate and keeps only a unique one that builds. */
#include "compile_repair.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int idc(int c) { return isalnum(c) || c == '_'; }

/* Damerau (optimal string alignment) distance, small strings only */
static int osa(const char *a, const char *b, int cap)
{
    size_t la = strlen(a), lb = strlen(b);
    static int d[80][80];
    if (la >= 79 || lb >= 79 || (la > lb ? la - lb : lb - la) > (size_t)cap)
        return cap + 1;
    for (size_t i = 0; i <= la; i++) d[i][0] = (int)i;
    for (size_t j = 0; j <= lb; j++) d[0][j] = (int)j;
    for (size_t i = 1; i <= la; i++)
        for (size_t j = 1; j <= lb; j++) {
            int v = d[i - 1][j - 1] + (a[i - 1] != b[j - 1]);
            if (d[i - 1][j] + 1 < v) v = d[i - 1][j] + 1;
            if (d[i][j - 1] + 1 < v) v = d[i][j - 1] + 1;
            if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1] && d[i - 2][j - 2] + 1 < v)
                v = d[i - 2][j - 2] + 1;
            d[i][j] = v;
        }
    return d[la][lb];
}

static int is_c_or_h(const char *rel)
{
    size_t n = strlen(rel);
    return n > 2 && rel[n - 2] == '.' && (rel[n - 1] == 'c' || rel[n - 1] == 'h');
}

static const char *base_of(const char *rel)
{
    const char *s = strrchr(rel, '/');
    return s ? s + 1 : rel;
}

static int count_token(const char *s, const char *t)
{
    size_t n = strlen(t);
    int c = 0;
    for (const char *p = s; (p = strstr(p, t)) != NULL; p += n)
        if ((p == s || !idc((unsigned char)p[-1])) && !idc((unsigned char)p[n]))
            c++;
    return c;
}

static char *replace_token_all(const char *s, const char *from, const char *to)
{
    size_t fl = strlen(from), tl = strlen(to), n = strlen(s);
    int c = count_token(s, from);
    char *o = (char *)malloc(n + (size_t)c * (tl + 1) + 1), *w = o;
    if (!o) return NULL;
    for (const char *p = s; *p; ) {
        if (!strncmp(p, from, fl) && (p == s || !idc((unsigned char)p[-1])) && !idc((unsigned char)p[fl])) {
            memcpy(w, to, tl); w += tl; p += fl;
        } else
            *w++ = *p++;
    }
    *w = '\0';
    return o;
}

static char *splice(const char *s, size_t at, size_t cut, const char *ins)
{
    size_t n = strlen(s), il = strlen(ins);
    char *o = (char *)malloc(n - cut + il + 1);
    if (!o) return NULL;
    memcpy(o, s, at);
    memcpy(o + at, ins, il);
    memcpy(o + at + il, s + at + cut, n - at - cut + 1);
    return o;
}

static size_t line_offset(const char *s, int line)
{
    size_t off = 0;
    for (int l = 1; l < line && s[off]; l++) {
        const char *e = strchr(s + off, '\n');
        if (!e) return strlen(s);
        off = (size_t)(e - s) + 1;
    }
    return off;
}

/* subject between the first pair of quotes (ASCII ' or UTF-8 curly) after p */
static int quoted(const char *p, char *out, size_t size)
{
    const char *a = NULL, *e;
    size_t skip = 0;
    for (const char *q = p; *q; q++) {
        if (*q == '\'') { a = q + 1; skip = 1; break; }
        if (!strncmp(q, "\xe2\x80\x98", 3)) { a = q + 3; skip = 3; break; }
        if (*q == '`') { a = q + 1; skip = 1; break; }
    }
    if (!a) return 0;
    (void)skip;
    e = a;
    while (*e && *e != '\'' && strncmp(e, "\xe2\x80\x99", 3) != 0 && *e != '\n')
        e++;
    if ((size_t)(e - a) + 1 > size || e == a) return 0;
    memcpy(out, a, (size_t)(e - a));
    out[e - a] = '\0';
    return 1;
}

static int push(CR_CAND *out, int n, int max, int file, char *text, int tier, const char *rule, const char *detail)
{
    if (!text) return n;
    for (int i = 0; i < n; i++)
        if (out[i].file == file && out[i].text && !strcmp(out[i].text, text)) { free(text); return n; }
    if (n >= max) { free(text); return n; }
    memset(&out[n], 0, sizeof(out[n]));
    out[n].file = file;
    out[n].text = text;
    out[n].tier = tier;
    snprintf(out[n].rule, sizeof(out[n].rule), "%s", rule);
    snprintf(out[n].detail, sizeof(out[n].detail), "%s", detail);
    return n + 1;
}

/* identifiers of the workspace C sources and headers (code only) */
static int collect_idents(const TASK_OPS_WORKSPACE *ws, char (*ids)[64], int max)
{
    int n = 0;
    for (int f = 0; f < ws->count; f++) {
        const char *s = ws->files[f].data;
        if (!is_c_or_h(ws->files[f].rel)) continue;
        for (const char *p = s; *p; ) {
            if (p[0] == '/' && p[1] == '*') { const char *e = strstr(p + 2, "*/"); p = e ? e + 2 : p + strlen(p); continue; }
            if (p[0] == '/' && p[1] == '/') { while (*p && *p != '\n') p++; continue; }
            if (*p == '"' || *p == '\'') { char q = *p++; while (*p && *p != q && *p != '\n') { if (*p == '\\' && p[1]) p++; p++; } if (*p) p++; continue; }
            if (isalpha((unsigned char)*p) || *p == '_') {
                const char *b = p;
                while (idc((unsigned char)*p)) p++;
                size_t l = (size_t)(p - b);
                if (l >= 3 && l < 64) {
                    int dup = 0;
                    for (int i = 0; i < n && !dup; i++)
                        dup = strlen(ids[i]) == l && !strncmp(ids[i], b, l);
                    if (!dup && n < max) { memcpy(ids[n], b, l); ids[n][l] = '\0'; n++; }
                }
                continue;
            }
            p++;
        }
    }
    return n;
}

static int includes(const char *s, const char *hdr)
{
    char q[300];
    snprintf(q, sizeof(q), "\"%s\"", hdr);
    return strstr(s, q) != NULL;
}

static size_t after_includes(const char *s)
{
    size_t at = 0, off = 0;
    while (s[off]) {
        const char *e = strchr(s + off, '\n');
        size_t next = e ? (size_t)(e - s) + 1 : strlen(s);
        const char *l = s + off;
        while (*l == ' ' || *l == '\t') l++;
        if (!strncmp(l, "#include", 8)) at = next;
        off = next;
    }
    return at;
}

/* definition header "... F(...)" followed by '{' : [b, e) of the header */
static int find_def(const char *s, const char *f, size_t *b, size_t *e)
{
    size_t fl = strlen(f);
    int found = 0;
    for (const char *p = s; (p = strstr(p, f)) != NULL; p += fl) {
        if ((p > s && idc((unsigned char)p[-1])) || idc((unsigned char)p[fl])) continue;
        const char *q = p + fl;
        while (*q == ' ') q++;
        if (*q != '(') continue;
        int depth = 0;
        for (; *q; q++) { if (*q == '(') depth++; else if (*q == ')' && --depth == 0) break; }
        if (!*q) continue;
        const char *r = q + 1;
        while (*r == ' ' || *r == '\t' || *r == '\n' || *r == '\r') r++;
        if (*r != '{') continue;
        const char *ls = p;
        while (ls > s && ls[-1] != '\n') ls--;
        if (*ls == ' ' || *ls == '\t') continue;   /* file scope only */
        *b = (size_t)(ls - s);
        *e = (size_t)(q + 1 - s);
        found++;
    }
    return found;
}

int CompileRepairCandidates(const TASK_OPS_WORKSPACE *ws, const char *diag, CR_CAND *out, int max)
{
    char file[512] = "", msg[512] = "", subj[128] = "";
    int line = 0, col = 0, fi = -1, n = 0, link = 0;
    if (!ws || !diag) return 0;
    for (const char *p = diag; p && *p; ) {
        const char *e = strchr(p, '\n');
        size_t len = e ? (size_t)(e - p) : strlen(p);
        char l[1024];
        snprintf(l, sizeof(l), "%.*s", (int)(len < 1023 ? len : 1023), p);
        char *er = strstr(l, ": error: ");
        char *fe = strstr(l, ": fatal error: ");
        char *ur = strstr(l, "undefined reference to ");
        if (er || fe) {
            char *m = er ? er + 9 : fe + 15;
            *(er ? er : fe) = '\0';
            if (sscanf(l, "%511[^:]:%d:%d", file, &line, &col) >= 2) {
                snprintf(msg, sizeof(msg), "%s%s", fe ? "fatal: " : "", m);
                break;
            }
        } else if (ur) {
            if (quoted(ur, subj, sizeof(subj))) { link = 1; snprintf(msg, sizeof(msg), "undefined reference"); break; }
        }
        p = e ? e + 1 : NULL;
    }
    if (!msg[0]) return 0;
    if (!link) {
        for (int i = 0; i < ws->count; i++)
            if (!strcmp(ws->files[i].rel, file)) fi = i;
        if (fi < 0) return 0;
        if (!strstr(msg, "No such file")) quoted(msg, subj, sizeof(subj));
    }
    const char *src = fi >= 0 ? ws->files[fi].data : NULL;
    char d[160];

    /* header_near */
    if (strstr(msg, "No such file")) {
        char name[256];
        if (sscanf(msg, "fatal: %255[^:]:", name) == 1) {
            for (int i = 0; i < ws->count; i++) {
                const char *b = base_of(ws->files[i].rel);
                if (strcmp(b, name) && osa(b, name, 2) <= 2) {
                    char from[300], to[300];
                    snprintf(from, sizeof(from), "\"%s\"", name);
                    snprintf(to, sizeof(to), "\"%s\"", b);
                    const char *at = strstr(src, from);
                    if (at) {
                        snprintf(d, sizeof(d), "#include \"%s\" -> \"%s\"", name, b);
                        n = push(out, n, max, fi, splice(src, (size_t)(at - src), strlen(from), to), 1, "header_near", d);
                    }
                }
            }
        }
        return n;
    }
    /* semicolon */
    if (!strncmp(msg, "expected ", 9) && (strstr(msg, "';'") || strstr(msg, "\xe2\x80\x98;\xe2\x80\x99")) && strstr(msg, " before ")) {
        size_t at = line_offset(src, line) + (size_t)(col > 0 ? col - 1 : 0);
        if (at > strlen(src)) at = strlen(src);
        size_t k = at;
        while (k > 0 && isspace((unsigned char)src[k - 1])) k--;
        if (k > 0 && !strchr(";{}", src[k - 1])) {
            snprintf(d, sizeof(d), "';' added at line %d", line);
            n = push(out, n, max, fi, splice(src, k, 0, ";"), 1, "semicolon", d);
        }
    }
    /* close_brace */
    if (strstr(msg, "at end of input")) {
        size_t stack[256];
        int sp = 0;
        for (const char *p = src; *p; p++) {
            if (p[0] == '/' && p[1] == '*') { const char *e2 = strstr(p + 2, "*/"); if (!e2) break; p = e2 + 1; continue; }
            if (p[0] == '/' && p[1] == '/') { while (p[1] && p[1] != '\n') p++; continue; }
            if (*p == '"' || *p == '\'') { char q = *p++; while (*p && *p != q && *p != '\n') { if (*p == '\\' && p[1]) p++; p++; } if (!*p) break; continue; }
            if (*p == '{' && sp < 256) stack[sp++] = (size_t)(p - src);
            else if (*p == '}' && sp > 0) sp--;
        }
        if (sp > 0) {
            size_t ob = stack[sp - 1], ls = ob;
            while (ls > 0 && src[ls - 1] != '\n') ls--;
            int ind = 0;
            while (src[ls + ind] == ' ' || src[ls + ind] == '\t') ind++;
            const char *nl = strchr(src + ob, '\n');
            size_t at = strlen(src);
            for (const char *q = nl ? nl + 1 : NULL; q && *q; ) {
                const char *qe = strchr(q, '\n');
                int qi = 0;
                while (q[qi] == ' ' || q[qi] == '\t') qi++;
                if (q[qi] && q[qi] != '\n' && qi <= ind) { at = (size_t)(q - src); break; }
                q = qe ? qe + 1 : NULL;
            }
            char ins[80];
            int need_nl = at == strlen(src) && at > 0 && src[at - 1] != '\n';
            snprintf(ins, sizeof(ins), "%s%.*s}\n", need_nl ? "\n" : "", ind < 60 ? ind : 60, src + ls);
            snprintf(d, sizeof(d), "closed the block opened at line %d", (int)(1 + (int)0));
            {
                int ln = 1;
                for (size_t k = 0; k < ob; k++) ln += src[k] == '\n';
                snprintf(d, sizeof(d), "closed the block opened at line %d", ln);
            }
            n = push(out, n, max, fi, splice(src, at, 0, ins), 2, "close_brace", d);
        }
    }
    if (!subj[0])
        return n;

    int undecl = strstr(msg, "undeclared") != NULL;
    int member = strstr(msg, "has no member") != NULL;
    int implicit = strstr(msg, "implicit declaration") != NULL;
    int typen = strstr(msg, "unknown type name") != NULL;
    int incomplete = strstr(msg, "storage size") || strstr(msg, "incomplete type") || strstr(msg, "undefined type");
    int proto = strstr(msg, "conflicting types") || strstr(msg, "too few arguments") || strstr(msg, "too many arguments");

    /* ident_near */
    if (undecl || member || implicit || link) {
        static char ids[2048][64];
        int ni = collect_idents(ws, ids, 2048);
        int cap = strlen(subj) <= 4 ? 1 : 2;
        for (int i = 0; i < ni; i++) {
            if (!strcmp(ids[i], subj) || osa(ids[i], subj, cap) > cap) continue;
            snprintf(d, sizeof(d), "%.60s -> %.60s", subj, ids[i]);
            if (link) {
                if (n < max) {
                    memset(&out[n], 0, sizeof(out[n]));
                    out[n].file = -1;
                    snprintf(out[n].from, sizeof(out[n].from), "%s", subj);
                    snprintf(out[n].to, sizeof(out[n].to), "%s", ids[i]);
                    out[n].tier = 1;
                    snprintf(out[n].rule, sizeof(out[n].rule), "ident_near");
                    snprintf(out[n].detail, sizeof(out[n].detail), "%s", d);
                    n++;
                }
            } else
                n = push(out, n, max, fi, replace_token_all(src, subj, ids[i]), 1, "ident_near", d);
        }
    }
    /* include_def */
    if (undecl || typen || incomplete || implicit) {
        char want[128];
        snprintf(want, sizeof(want), "%s", subj);
        const char *st = strstr(msg, "struct ");
        if (st) sscanf(st + 7, "%127[A-Za-z0-9_]", want);
        else if (incomplete) {   /* storage size of 'v': the struct named on that line */
            size_t lo = line_offset(src, line);
            const char *le = strchr(src + lo, '\n');
            char ln[512];
            snprintf(ln, sizeof(ln), "%.*s", (int)(le ? (size_t)(le - (src + lo)) : strlen(src + lo)), src + lo);
            const char *s2 = strstr(ln, "struct ");
            if (s2) sscanf(s2 + 7, "%127[A-Za-z0-9_]", want);
        }
        for (int i = 0; i < ws->count; i++) {
            const char *rel = ws->files[i].rel;
            size_t rl = strlen(rel);
            if (i == fi || rl < 3 || strcmp(rel + rl - 2, ".h") || includes(src, base_of(rel)))
                continue;
            if (count_token(ws->files[i].data, want) == 0)
                continue;
            char ins[300];
            size_t at = after_includes(src);
            snprintf(ins, sizeof(ins), "#include \"%s\"\n", base_of(rel));
            snprintf(d, sizeof(d), "%.60s defined in %.80s: included", want, base_of(rel));
            n = push(out, n, max, fi, splice(src, at, 0, ins), 1, "include_def", d);
        }
    }
    /* proto_add: an implicit call to a function with exactly one definition
       in the workspace gets that definition's header as a prototype - in the
       one local header both files include (tier 1), else in the calling file
       after its includes (tier 2) */
    if (implicit) {
        int defs = 0, df = -1;
        size_t db = 0, de = 0;
        for (int i = 0; i < ws->count; i++) {
            size_t b, e;
            int k = find_def(ws->files[i].data, subj, &b, &e);
            if (k) { defs += k; df = i; db = b; de = e; }
        }
        const char *ds = df >= 0 ? ws->files[df].data : NULL;
        if (defs == 1 && de - db < 400 && !(df != fi && !strncmp(ds + db, "static", 6) && !idc((unsigned char)ds[db + 6]))) {
            char proto[420];
            snprintf(proto, sizeof(proto), "%.*s;\n", (int)(de - db), ds + db);
            for (char *c = proto; c[1]; c++) if (*c == '\n' || *c == '\r' || *c == '\t') *c = ' ';
            if (df != fi) {
                for (int h = 0; h < ws->count; h++) {
                    const char *rel = ws->files[h].rel, *hs = ws->files[h].data;
                    size_t rl = strlen(rel);
                    if (rl < 3 || strcmp(rel + rl - 2, ".h") || !includes(src, base_of(rel)) || !includes(ds, base_of(rel)))
                        continue;
                    size_t at = strlen(hs);
                    const char *endif = NULL;
                    for (const char *q = strstr(hs, "#endif"); q; q = strstr(q + 1, "#endif")) endif = q;
                    if (endif && strstr(hs, "#ifndef")) at = (size_t)(endif - hs);
                    char ins[440];
                    snprintf(ins, sizeof(ins), "%s%s", at == strlen(hs) && at && hs[at - 1] != '\n' ? "\n" : "", proto);
                    snprintf(d, sizeof(d), "%.60s declared in %.80s", subj, base_of(rel));
                    n = push(out, n, max, h, splice(hs, at, 0, ins), 1, "proto_add", d);
                }
            }
            snprintf(d, sizeof(d), "%.60s declared in %.80s", subj, base_of(ws->files[fi].rel));
            n = push(out, n, max, fi, splice(src, after_includes(src), 0, proto), 2, "proto_add", d);
        }
    }
    /* loop_decl */
    if (undecl) {
        char pat[3][100];
        snprintf(pat[0], 100, "for (%s =", subj);
        snprintf(pat[1], 100, "for (%s=", subj);
        snprintf(pat[2], 100, "for(%s =", subj);
        char *cur = NULL;
        for (int k = 0; k < 3; k++) {
            const char *base = cur ? cur : src;
            if (!strstr(base, pat[k])) continue;
            char rep[120];
            snprintf(rep, sizeof(rep), "for (int %s%s", subj, pat[k] + strlen(pat[k]) - (pat[k][strlen(pat[k]) - 2] == ' ' ? 2 : 1));
            /* replace every occurrence of this pattern */
            char *o = strdup(base);
            for (char *at; o && (at = strstr(o, pat[k])) != NULL; ) {
                char *nw = splice(o, (size_t)(at - o), strlen(pat[k]), rep);
                free(o);
                o = nw;
            }
            free(cur);
            cur = o;
        }
        if (cur) {
            snprintf(d, sizeof(d), "%.60s declared in its for loop", subj);
            n = push(out, n, max, fi, cur, 2, "loop_decl", d);
        }
    }
    /* proto_sync */
    if (proto) {
        int defs = 0, df = -1;
        size_t db = 0, de = 0;
        for (int i = 0; i < ws->count; i++) {
            size_t b, e;
            int k = find_def(ws->files[i].data, subj, &b, &e);
            if (k) { defs += k; df = i; db = b; de = e; }
        }
        if (defs == 1) {
            char hdr[512];
            const char *ds = ws->files[df].data;
            size_t hl = de - db < 500 ? de - db : 500;
            snprintf(hdr, sizeof(hdr), "%.*s;", (int)hl, ds + db);
            for (char *c = hdr; *c; c++) if (*c == '\n' || *c == '\r') *c = ' ';
            const char *hs = hdr;
            if (!strncmp(hs, "static ", 7) ) hs += 0;
            for (int i = 0; i < ws->count; i++) {
                const char *s = ws->files[i].data;
                size_t fl = strlen(subj);
                for (const char *p = s; (p = strstr(p, subj)) != NULL; p += fl) {
                    if ((p > s && idc((unsigned char)p[-1])) || idc((unsigned char)p[fl])) continue;
                    const char *ls = p;
                    while (ls > s && ls[-1] != '\n') ls--;
                    if (*ls == ' ' || *ls == '\t' || *ls == '#') continue;
                    const char *q = p + fl;
                    while (*q == ' ') q++;
                    if (*q != '(') continue;
                    int depth = 0;
                    for (; *q; q++) { if (*q == '(') depth++; else if (*q == ')' && --depth == 0) break; }
                    if (!*q) continue;
                    const char *r = q + 1;
                    while (*r == ' ' || *r == '\t') r++;
                    if (*r != ';') continue;
                    size_t pb = (size_t)(ls - s), pe = (size_t)(r + 1 - s);
                    if (pe - pb == strlen(hs) && !strncmp(s + pb, hs, pe - pb)) continue;
                    snprintf(d, sizeof(d), "prototype of %.60s matches its definition", subj);
                    n = push(out, n, max, i, splice(s, pb, pe - pb, hs), 1, "proto_sync", d);
                }
            }
        }
    }
    return n;
}

void CompileRepairFree(CR_CAND *c, int n)
{
    for (int i = 0; i < n; i++) { free(c[i].text); c[i].text = NULL; }
}

int CompileRepairEditDistance(const char *a, const char *b, int cap)
{
    return osa(a, b, cap);
}
