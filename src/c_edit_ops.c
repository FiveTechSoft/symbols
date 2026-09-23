/* c_edit_ops.c: see include/c_edit_ops.h */
#include "c_edit_ops.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CEO_SRC_MAX 8192
#define CEO_MAX_STRUCTS 16
#define CEO_MAX_MEMBERS 16
#define CEO_MAX_COLL 8
#define CEO_MAX_GROUPS 64
#define CEO_MAX_EDITS 96

/* ---------------- text helpers ---------------- */
static void Fold(const char *in, char *out, size_t size)
{
    size_t i = 0, o = 0;
    if (!out || size == 0) return;
    while (in && in[i] && o + 1 < size)
    {
        unsigned char c = (unsigned char)in[i];
        if (c == 0xC3 && in[i + 1])
        {
            unsigned char d = (unsigned char)in[i + 1] | 0x20; char r = '?';
            if (d >= 0xA0 && d <= 0xA5) r = 'a'; else if (d == 0xA7) r = 'c';
            else if (d >= 0xA8 && d <= 0xAB) r = 'e'; else if (d >= 0xAC && d <= 0xAF) r = 'i';
            else if (d == 0xB1) r = 'n'; else if (d >= 0xB2 && d <= 0xB6) r = 'o';
            else if (d >= 0xB9 && d <= 0xBC) r = 'u';
            out[o++] = r; i += 2;
        }
        else { out[o++] = (char)tolower(c); i++; }
    }
    out[o] = '\0';
}
static int IsId(int c) { return isalnum(c) || c == '_'; }
static int WordAt(const char *s, size_t pos, const char *w)
{
    size_t n = strlen(w);
    if (strncmp(s + pos, w, n) != 0) return 0;
    if (pos > 0 && IsId((unsigned char)s[pos - 1])) return 0;
    return !IsId((unsigned char)s[pos + n]);
}
static size_t SkipWs(const char *s, size_t p) { while (s[p] && isspace((unsigned char)s[p])) p++; return p; }
static size_t Ident(const char *s, size_t p, char *out, size_t size)
{
    size_t n = 0;
    if (!(isalpha((unsigned char)s[p]) || s[p] == '_')) { if (size) out[0] = '\0'; return 0; }
    while (IsId((unsigned char)s[p + n])) { if (n + 1 < size) out[n] = s[p + n]; n++; }
    out[n < size ? n : size - 1] = '\0';
    return n;
}
static size_t LineStart(const char *s, size_t p) { while (p > 0 && s[p - 1] != '\n') p--; return p; }
static size_t LineEnd(const char *s, size_t p) { while (s[p] && s[p] != '\n') p++; return s[p] ? p + 1 : p; }
static size_t Match(const char *m, size_t open) /* m[open]=='{','(' or '[' */
{
    char o = m[open], c = o == '{' ? '}' : o == '(' ? ')' : ']'; int d = 0;
    for (size_t p = open; m[p]; p++) { if (m[p] == o) d++; else if (m[p] == c && --d == 0) return p; }
    return 0;
}
static int CountOcc(const char *hay, const char *needle, size_t nlen)
{
    int c = 0; const char *p = hay;
    if (nlen == 0) return 99;
    while ((p = strstr(p, needle)) != NULL) { if (strncmp(p, needle, nlen) == 0) c++; p++; }
    return c;
}

/* Mask comments, string and char literal contents (same length). */
static void Mask(const char *s, char *m)
{
    size_t i = 0;
    while (s[i])
    {
        if (s[i] == '/' && s[i + 1] == '/') { while (s[i] && s[i] != '\n') m[i++] = ' '; }
        else if (s[i] == '/' && s[i + 1] == '*')
        { m[i] = m[i + 1] = ' '; i += 2; while (s[i] && !(s[i] == '*' && s[i + 1] == '/')) { m[i] = s[i] == '\n' ? '\n' : ' '; i++; } if (s[i]) { m[i] = m[i + 1] = ' '; i += 2; } }
        else if (s[i] == '"' || s[i] == '\'')
        { char q = s[i]; m[i] = q; i++; while (s[i] && s[i] != q && s[i] != '\n') { if (s[i] == '\\' && s[i + 1]) { m[i] = m[i + 1] = ' '; i += 2; } else m[i++] = ' '; } if (s[i] == q) { m[i] = q; i++; } }
        else if (s[i] == '#' ) { /* preprocessor line: keep text, it is harmless */ m[i] = s[i]; i++; }
        else { m[i] = s[i]; i++; }
    }
    m[i] = '\0';
}

/* ---------------- perception ---------------- */
typedef struct { char type[64]; char name[48]; int is_string; int is_float; } Member;
typedef struct
{
    int file; char tag[64]; char tdef[64];
    size_t open, close;               /* braces */
    Member mem[CEO_MAX_MEMBERS]; int nmem;
    size_t last_member_line_start, last_member_line_end;
} Struct;
typedef struct { int file; int st; char ident[64]; char counter[128]; size_t decl; } Coll;
typedef struct { int file; int st; size_t open, close; char key[64]; double price; int has_price; } Group;
typedef struct { int file; size_t pos; size_t del; char ins[512]; } Edit;

typedef struct
{
    const char *const *paths; const char *const *srcs; int n;
    char mask[CEO_MAX_FILES][CEO_SRC_MAX];
    Struct st[CEO_MAX_STRUCTS]; int nst;
    Coll coll[CEO_MAX_COLL]; int ncoll;
    Group grp[CEO_MAX_GROUPS]; int ngrp;
    int main_file; size_t main_open, main_close;
    Edit ed[CEO_MAX_EDITS]; int ned;
} Model;

static int IsHeader(const char *p) { size_t n = strlen(p); return n > 2 && strcmp(p + n - 2, ".h") == 0; }

static void ParseMembers(const Model *M, Struct *S)
{
    const char *m = M->mask[S->file]; size_t p = S->open + 1;
    S->nmem = 0;
    while (p < S->close && S->nmem < CEO_MAX_MEMBERS)
    {
        size_t semi = p; int d = 0;
        while (semi < S->close && !(m[semi] == ';' && d == 0)) { if (m[semi] == '{' || m[semi] == '(') d++; else if (m[semi] == '}' || m[semi] == ')') d--; semi++; }
        if (semi >= S->close) break;
        char decl[256]; size_t len = semi - p; if (len >= sizeof(decl)) len = sizeof(decl) - 1;
        memcpy(decl, m + p, len); decl[len] = '\0';
        /* strip array suffix */
        char *br = strchr(decl, '['); int arr = br != NULL; if (br) *br = '\0';
        size_t e = strlen(decl); while (e > 0 && isspace((unsigned char)decl[e - 1])) e--; decl[e] = '\0';
        size_t b = e; while (b > 0 && IsId((unsigned char)decl[b - 1])) b--;
        if (b < e && b > 0 && !strchr(decl, '(') && !strchr(decl, ','))
        {
            Member *mb = &S->mem[S->nmem++];
            snprintf(mb->name, sizeof(mb->name), "%s", decl + b);
            decl[b] = '\0';
            char *t = decl; while (isspace((unsigned char)*t)) t++;
            size_t tl = strlen(t); while (tl > 0 && isspace((unsigned char)t[tl - 1])) t[--tl] = '\0';
            snprintf(mb->type, sizeof(mb->type), "%s", t);
            mb->is_string = strstr(mb->type, "char") && (strchr(mb->type, '*') || arr);
            mb->is_float = !strchr(mb->type, '*') && !arr && (strstr(mb->type, "double") || strstr(mb->type, "float"));
            S->last_member_line_start = LineStart(m, semi);
            S->last_member_line_end = LineEnd(m, semi);
        }
        else if (b < e) { S->nmem = 0; return; } /* unsupported member form */
        p = semi + 1;
    }
}

static void FindStructs(Model *M)
{
    for (int f = 0; f < M->n; f++)
    {
        const char *m = M->mask[f];
        for (size_t p = 0; m[p]; p++)
        {
            if (!WordAt(m, p, "struct")) continue;
            size_t q = SkipWs(m, p + 6); char tag[64] = "";
            q += Ident(m, q, tag, sizeof(tag)); q = SkipWs(m, q);
            if (m[q] != '{' || M->nst >= CEO_MAX_STRUCTS) continue;
            Struct *S = &M->st[M->nst]; memset(S, 0, sizeof(*S));
            S->file = f; S->open = q; S->close = Match(m, q); if (!S->close) continue;
            snprintf(S->tag, sizeof(S->tag), "%s", tag);
            /* typedef? look back for 'typedef' on the same statement */
            size_t b = p; while (b > 0 && isspace((unsigned char)m[b - 1])) b--;
            if (b >= 7 && WordAt(m, b - 7, "typedef"))
            { size_t t = SkipWs(m, S->close + 1); Ident(m, t, S->tdef, sizeof(S->tdef)); }
            if (!S->tag[0] && !S->tdef[0]) continue;
            ParseMembers(M, S);
            if (S->nmem > 0) M->nst++;
            p = S->close;
        }
    }
}

/* Is a type reference to struct si at m[p]? returns length or 0 */
static size_t TypeRefAt(const Model *M, const char *m, size_t p, int si)
{
    const Struct *S = &M->st[si];
    if (S->tdef[0] && WordAt(m, p, S->tdef)) return strlen(S->tdef);
    if (S->tag[0] && WordAt(m, p, "struct"))
    { size_t q = SkipWs(m, p + 6); if (WordAt(m, q, S->tag)) return q + strlen(S->tag) - p; }
    return 0;
}

static int BraceDepth(const char *m, size_t p)
{ int d = 0; for (size_t i = 0; i < p; i++) { if (m[i] == '{') d++; else if (m[i] == '}') d--; } return d; }

static void AddGroup(Model *M, int f, int si, size_t open)
{
    const char *m = M->mask[f], *s = M->srcs[f];
    size_t close = Match(m, open); if (!close || M->ngrp >= CEO_MAX_GROUPS) return;
    const Struct *S = &M->st[si];
    /* split elements at depth 0 */
    size_t starts[CEO_MAX_MEMBERS + 2]; int ne = 0; int d = 0; size_t p = open + 1;
    starts[ne++] = p;
    for (; p < close; p++)
    {
        if (m[p] == '{' || m[p] == '(' || m[p] == '[') d++;
        else if (m[p] == '}' || m[p] == ')' || m[p] == ']') d--;
        else if (m[p] == ',' && d == 0) { if (ne <= CEO_MAX_MEMBERS) starts[ne++] = p + 1; }
    }
    /* trailing comma creates an empty element */
    size_t tail = SkipWs(m, starts[ne - 1]); if (tail >= close && ne > 1) ne--;
    if (ne != S->nmem) return;
    Group *G = &M->grp[M->ngrp]; memset(G, 0, sizeof(*G)); G->file = f; G->st = si; G->open = open; G->close = close;
    for (int k = 0; k < ne; k++)
    {
        size_t e0 = SkipWs(m, starts[k]);
        if (S->mem[k].is_string && s[e0] == '"' && !G->key[0])
        { size_t e1 = e0 + 1; while (s[e1] && s[e1] != '"') e1++; size_t l = e1 - e0 - 1; if (l >= sizeof(G->key)) l = sizeof(G->key) - 1; memcpy(G->key, s + e0 + 1, l); G->key[l] = '\0'; }
    }
    M->ngrp++;
}

static void FindCollectionsAndGroups(Model *M)
{
    for (int f = 0; f < M->n; f++)
    {
        const char *m = M->mask[f];
        for (size_t p = 0; m[p]; p++)
        {
            for (int si = 0; si < M->nst; si++)
            {
                size_t tl = TypeRefAt(M, m, p, si); if (!tl) continue;
                /* compound literal: '(' Type ')' '{' */
                size_t b = p; while (b > 0 && isspace((unsigned char)m[b - 1])) b--;
                size_t a = SkipWs(m, p + tl);
                if (b > 0 && m[b - 1] == '(' && m[a] == ')')
                { size_t o = SkipWs(m, a + 1); if (m[o] == '{') AddGroup(M, f, si, o); break; }
                /* declaration: Type [*] ident '[' ... */
                char id[64]; size_t q = a; while (m[q] == '*') q = SkipWs(m, q + 1);
                size_t il = Ident(m, q, id, sizeof(id)); if (!il) break;
                size_t r = SkipWs(m, q + il);
                if (m[r] == '[' && BraceDepth(m, p) == 0 && M->ncoll < CEO_MAX_COLL)
                {
                    size_t rb = Match(m, r); if (!rb) break;
                    Coll *C = &M->coll[M->ncoll]; memset(C, 0, sizeof(*C));
                    C->file = f; C->st = si; C->decl = p; snprintf(C->ident, sizeof(C->ident), "%s", id);
                    size_t eq = SkipWs(m, rb + 1);
                    int empty_dim = SkipWs(m, r + 1) == rb;
                    if (m[eq] == '=')
                    {
                        size_t o = SkipWs(m, eq + 1);
                        if (m[o] == '{')
                        {
                            size_t oc = Match(m, o);
                            for (size_t g = o + 1; oc && g < oc; g++)
                                if (m[g] == '{') { AddGroup(M, f, si, g); g = Match(m, g); if (!g) break; }
                            if (empty_dim)
                                snprintf(C->counter, sizeof(C->counter), "(int)(sizeof %s / sizeof %s[0])", id, id);
                        }
                    }
                    /* skip extern declarations of the same collection */
                    int ext = 0; size_t ls = LineStart(m, p); if (strstr(m + ls, "extern") && strstr(m + ls, "extern") < m + p) ext = 1;
                    if (!ext) M->ncoll++;
                    p = rb;
                }
                break;
            }
        }
    }
    /* counters: ident[X++] or 'i < X' in a loop indexing ident */
    for (int c = 0; c < M->ncoll; c++)
    {
        Coll *C = &M->coll[c]; char pat[96];
        for (int f = 0; f < M->n && !C->counter[0]; f++)
        {
            const char *m = M->mask[f];
            snprintf(pat, sizeof(pat), "%s[", C->ident);
            for (const char *h = strstr(m, pat); h && !C->counter[0]; h = strstr(h + 1, pat))
            {
                if (h > m && IsId((unsigned char)h[-1])) continue;
                char x[64]; size_t xl = Ident(h, strlen(pat), x, sizeof(x));
                if (xl && strncmp(h + strlen(pat) + xl, "++]", 3) == 0) snprintf(C->counter, sizeof(C->counter), "%s", x);
            }
            for (const char *fo = m; !C->counter[0] && (fo = strstr(fo, "for")) != NULL; fo++)
            {
                size_t fp = (size_t)(fo - m); if (!WordAt(m, fp, "for")) continue;
                size_t o = SkipWs(m, fp + 3); if (m[o] != '(') continue;
                size_t cl = Match(m, o); if (!cl) continue;
                char hdr[256]; size_t hl = cl - o - 1; if (hl >= sizeof(hdr)) continue; memcpy(hdr, m + o + 1, hl); hdr[hl] = '\0';
                char *s1 = strchr(hdr, ';'); char *s2 = s1 ? strchr(s1 + 1, ';') : NULL; if (!s2) continue;
                *s2 = '\0'; char *lt = strchr(s1 + 1, '<'); if (!lt || lt[1] == '=') continue;
                char *ex = lt + 1; while (isspace((unsigned char)*ex)) ex++;
                size_t exl = strlen(ex); while (exl && isspace((unsigned char)ex[exl - 1])) ex[--exl] = '\0';
                /* loop body must index this collection */
                char body[512]; size_t bl = strlen(m + cl); if (bl > sizeof(body) - 1) bl = sizeof(body) - 1; memcpy(body, m + cl, bl); body[bl] = '\0';
                char *usage = strstr(body, pat);
                if (usage && exl > 0 && exl < sizeof(C->counter)) snprintf(C->counter, sizeof(C->counter), "%s", ex);
            }
        }
    }
}

static void FindMain(Model *M)
{
    M->main_file = -1;
    for (int f = 0; f < M->n && M->main_file < 0; f++)
    {
        const char *m = M->mask[f];
        for (size_t p = 0; m[p]; p++)
        {
            if (!WordAt(m, p, "main")) continue;
            size_t o = SkipWs(m, p + 4); if (m[o] != '(') continue;
            size_t c = Match(m, o); if (!c) continue;
            size_t b = SkipWs(m, c + 1); if (m[b] != '{') continue;
            M->main_file = f; M->main_open = b; M->main_close = Match(m, b); break;
        }
    }
}

/* ---------------- intent: quantities per observed key ---------------- */
static int g_qdir = 0; /* 0 nearest, 1 number after name, 2 number before name */
static size_t g_qpos;
static int QuantityForKey(const char *issue_folded, const char *key, int *value)
{
    size_t bestpos = 0;
    char k[64]; Fold(key, k, sizeof(k));
    size_t kl = strlen(k); if (kl == 0) return 0;
    size_t stem = kl > 5 ? kl - 1 : kl;  /* tolerate plural/singular */
    const char *f = issue_folded; int best = -1, bestd = 1 << 30;
    for (const char *h = f; (h = strstr(h, "")) != NULL && *h; h++)
    {
        if (strncmp(h, k, stem) != 0) continue;
        if (h > f && isalnum((unsigned char)h[-1])) continue;
        size_t hp = (size_t)(h - f), he = hp; while (isalnum((unsigned char)f[he])) he++;
        /* after: skip up to 28 chars of words, stop at another key-like boundary ',' ';' '.' 'y' */
        for (size_t q = he; g_qdir != 2 && f[q] && q < he + 28; q++)
        {
            if (f[q] == ',' || f[q] == ';' || f[q] == ')' || (f[q] == '.' && !isdigit((unsigned char)f[q + 1]))) break;
            if (isdigit((unsigned char)f[q]) && !isalnum((unsigned char)f[q - 1]))
            {
                size_t e = q; while (isdigit((unsigned char)f[e])) e++;
                if ((f[e] == '.' || f[e] == ',') && isdigit((unsigned char)f[e + 1])) break; /* a price */
                int d = (int)(q - he); if (d < bestd) { bestd = d; best = atoi(f + q); bestpos = q; }
                break;
            }
        }
        /* before: "4 para el teclado", "4 unidades de teclado", "4 teclados" */
        for (size_t q = hp; g_qdir != 1 && q > 0 && hp - q < 28; q--)
        {
            char c = f[q - 1];
            if (c == ',' || c == ';' || c == '(' || c == ':' || (c == '.' )) break;
            if (isdigit((unsigned char)c) && (q < 2 || !isalnum((unsigned char)f[q - 2]) || isdigit((unsigned char)f[q - 2])))
            {
                size_t s = q - 1; while (s > 0 && isdigit((unsigned char)f[s - 1])) s--;
                if (s > 0 && (f[s - 1] == '.' || f[s - 1] == ',') && s > 1 && isdigit((unsigned char)f[s - 2])) break;
                int d = (int)(hp - q); if (d < bestd) { bestd = d; best = atoi(f + s); bestpos = s; }
                break;
            }
        }
    }
    if (best < 0) return 0;
    g_qpos = bestpos; *value = best; return 1;
}

int CeoIsAddFieldAndTotalRequest(const char *issue)
{
    /* Pre-routing signal only; the decision is made after perception, when
       the planner either grounds the request in observed code or abstains.
       Structural evidence: two or more integer quantities attached to words
       ("4 teclados", "mouse 10").  The one lexical cue left is the aggregate
       the user asks to see (total / value / sum / "cuanto vale"). */
    char f[2048]; Fold(issue ? issue : "", f, sizeof(f));
    int total = strstr(f, "total") || strstr(f, "valor") || strstr(f, "value") || strstr(f, "suma") || strstr(f, "sum") ||
                strstr(f, "cuanto vale") || strstr(f, "worth");
    int pairs = 0;
    for (size_t i = 0; f[i]; i++)
        if (isdigit((unsigned char)f[i]) && (i == 0 || !isalnum((unsigned char)f[i - 1])))
        {
            size_t e = i; while (isdigit((unsigned char)f[e])) e++;
            if ((f[e] == '.' || f[e] == ',') && isdigit((unsigned char)f[e + 1])) { i = e; continue; }
            int after = f[e] == ' ' && isalpha((unsigned char)f[e + 1]);
            int before = i >= 2 && f[i - 1] == ' ' && isalpha((unsigned char)f[i - 2]);
            if (after || before) pairs++;
            i = e;
        }
    return pairs >= 2 && total;
}

static int AddEdit(Model *M, int f, size_t pos, size_t del, const char *ins)
{
    if (M->ned >= CEO_MAX_EDITS) return 0;
    Edit *E = &M->ed[M->ned++]; E->file = f; E->pos = pos; E->del = del;
    snprintf(E->ins, sizeof(E->ins), "%s", ins); return 1;
}

static int CmpEdit(const void *a, const void *b)
{
    const Edit *x = a, *y = b;
    if (x->file != y->file) return x->file - y->file;
    return x->pos < y->pos ? -1 : x->pos > y->pos;
}

/* Convert edits into unique, non-overlapping old/new hunks. */
static int BuildHunks(Model *M, CeoPlan *P)
{
    qsort(M->ed, (size_t)M->ned, sizeof(Edit), CmpEdit);
    int i = 0;
    while (i < M->ned)
    {
        int f = M->ed[i].file; const char *s = M->srcs[f];
        size_t a = LineStart(s, M->ed[i].pos), b = LineEnd(s, M->ed[i].pos + M->ed[i].del);
        if (!s[M->ed[i].pos] && M->ed[i].pos > 0) a = LineStart(s, M->ed[i].pos - 1); /* append at EOF */
        int j = i + 1;
        for (;;)
        {
            while (j < M->ned && M->ed[j].file == f && M->ed[j].pos < b)
            { size_t nb = LineEnd(s, M->ed[j].pos + M->ed[j].del); if (nb > b) b = nb; j++; }
            char old[CEO_HUNK_OLD]; size_t ol = b - a;
            if (ol >= sizeof(old)) return 0;
            if (ol == 0) { if (s[b]) { b = LineEnd(s, b); continue; } if (a > 0) { a = LineStart(s, a - 1); continue; } return 0; }
            memcpy(old, s + a, ol); old[ol] = '\0';
            if (CountOcc(s, old, ol) == 1) break;
            /* grow context */
            if (a > 0) a = LineStart(s, a - 1); else if (s[b]) b = LineEnd(s, b); else return 0;
        }
        if (P->nhunks >= CEO_MAX_HUNKS) return 0;
        CeoHunk *H = &P->hunks[P->nhunks++]; H->file = f;
        size_t ol = b - a; memcpy(H->old_text, s + a, ol); H->old_text[ol] = '\0';
        size_t o = 0, cur = a;
        for (int k = i; k < j; k++)
        {
            size_t pre = M->ed[k].pos - cur; if (o + pre + strlen(M->ed[k].ins) + 1 >= sizeof(H->new_text)) return 0;
            memcpy(H->new_text + o, s + cur, pre); o += pre;
            memcpy(H->new_text + o, M->ed[k].ins, strlen(M->ed[k].ins)); o += strlen(M->ed[k].ins);
            cur = M->ed[k].pos + M->ed[k].del;
        }
        if (o + (b - cur) + 1 >= sizeof(H->new_text)) return 0;
        memcpy(H->new_text + o, s + cur, b - cur); o += b - cur; H->new_text[o] = '\0';
        i = j;
    }
    return P->nhunks;
}

static void Indent(const char *s, size_t line_start, char *out, size_t size)
{
    size_t n = 0; while ((s[line_start + n] == ' ' || s[line_start + n] == '\t') && n + 1 < size) { out[n] = s[line_start + n]; n++; }
    out[n] = '\0';
}

#define ABSTAIN(...) do { snprintf(P->reason, sizeof(P->reason), __VA_ARGS__); return 0; } while (0)

int CeoPlanAddFieldAndTotal(const char *issue, const char *const *paths,
                            const char *const *srcs, int nfiles, CeoPlan *P)
{
    static Model M; char fi[4096];
    if (!P) return 0;
    memset(P, 0, sizeof(*P));
    if (!issue || !paths || !srcs || nfiles <= 0) ABSTAIN("no observed sources");
    memset(&M, 0, sizeof(M)); M.paths = paths; M.srcs = srcs; M.n = nfiles > CEO_MAX_FILES ? CEO_MAX_FILES : nfiles;
    for (int f = 0; f < M.n; f++) { if (strlen(srcs[f]) >= CEO_SRC_MAX) ABSTAIN("%s is too large to plan safely", paths[f]); Mask(srcs[f], M.mask[f]); }
    Fold(issue, fi, sizeof(fi));
    FindStructs(&M); FindCollectionsAndGroups(&M); FindMain(&M);

    /* choose the struct: has a string key member, a floating value member,
       and observed initializers whose keys the request mentions */
    int si = -1, best_hits = 0;
    /* Orientation: does the request write "name N" or "N name"?  Pick the
       direction under which every named initializer claims a distinct number. */
    {
        int best_dir = 0, best_score = -1;
        for (int dir = 1; dir <= 2; dir++)
        {
            size_t used[CEO_MAX_GROUPS]; int nu = 0, ok = 1, hits = 0, v;
            g_qdir = dir;
            for (int g = 0; g < M.ngrp && g < CEO_MAX_GROUPS; g++)
            {
                if (!M.grp[g].key[0] || !QuantityForKey(fi, M.grp[g].key, &v)) continue;
                for (int u = 0; u < nu; u++) if (used[u] == g_qpos) ok = 0;
                used[nu++] = g_qpos; hits++;
            }
            if (ok && hits > best_score) { best_score = hits; best_dir = dir; }
        }
        g_qdir = best_dir;
    }
    for (int s = 0; s < M.nst; s++)
    {
        int hits = 0, v;
        for (int g = 0; g < M.ngrp; g++) if (M.grp[g].st == s && M.grp[g].key[0] && QuantityForKey(fi, M.grp[g].key, &v)) hits++;
        if (hits > best_hits) { best_hits = hits; si = s; }
    }
    if (si < 0) ABSTAIN("no observed struct initializer is named in the request (found %d structs, %d initializers)", M.nst, M.ngrp);
    Struct *S = &M.st[si];
    snprintf(P->struct_name, sizeof(P->struct_name), "%s", S->tdef[0] ? S->tdef : S->tag);

    /* value member: the one floating member, or the price-like one */
    int vm = -1, nfloat = 0;
    for (int k = 0; k < S->nmem; k++) if (S->mem[k].is_float) { nfloat++; vm = k; }
    if (nfloat > 1)
    {
        /* several floating members: nothing observed says which one is the
           unit value, so ask instead of guessing from names */
        char names[160] = "";
        for (int k = 0; k < S->nmem; k++) if (S->mem[k].is_float)
        { size_t l = strlen(names); snprintf(names + l, sizeof(names) - l, "%s%s", l ? ", " : "", S->mem[k].name); }
        ABSTAIN("struct %s has several numeric members (%s); which one multiplies the %s is ambiguous", S->tdef[0] ? S->tdef : S->tag, names, "new field");
    }
    if (vm < 0) ABSTAIN("struct %s has no unambiguous numeric value member to multiply", P->struct_name);

    /* field name: request word, else stock; refuse if it exists */
    snprintf(P->field, sizeof(P->field), "%s", strstr(fi, "stock") ? "stock" : strstr(fi, "cantidad") ? "cantidad" : strstr(fi, "quantity") ? "quantity" : strstr(fi, "existencias") ? "existencias" : "stock");
    for (int k = 0; k < S->nmem; k++) if (strcmp(S->mem[k].name, P->field) == 0) ABSTAIN("struct %s already has a member named %s", P->struct_name, P->field);

    /* operator 1: add the field */
    {
        const char *s = srcs[S->file]; char ind[32], ins[128];
        Indent(s, S->last_member_line_start, ind, sizeof(ind));
        size_t close_ls = LineStart(M.mask[S->file], S->close);
        int own_line = SkipWs(M.mask[S->file], close_ls) == S->close && close_ls > S->last_member_line_start;
        if (own_line) { snprintf(ins, sizeof(ins), "%sint %s;\n", ind[0] ? ind : "    ", P->field); AddEdit(&M, S->file, close_ls, 0, ins); }
        else { snprintf(ins, sizeof(ins), " int %s; ", P->field); AddEdit(&M, S->file, S->close, 0, ins); }
    }
    /* propagate values to every initializer of this struct */
    int ngroups = 0; char missing[256] = "";
    /* Values by name; when some names are not in the request (e.g. another
       language), fall back to request order only if the counts agree and
       every name-matched initializer confirms the same positional mapping. */
    int gval[CEO_MAX_GROUPS], gok[CEO_MAX_GROUPS], order_used = 0;
    {
        int nums[CEO_MAX_GROUPS], nn = 0, idx = 0, nmatch = 0, nmiss = 0, consistent = 1;
        for (size_t q = 0; fi[q]; q++)
            if (isdigit((unsigned char)fi[q]) && (q == 0 || !isalnum((unsigned char)fi[q - 1])))
            {
                size_t e = q; while (isdigit((unsigned char)fi[e])) e++;
                int is_price = (fi[e] == '.' || fi[e] == ',') && isdigit((unsigned char)fi[e + 1]);
                if (!isalpha((unsigned char)fi[e]) && !is_price && nn < CEO_MAX_GROUPS) nums[nn++] = atoi(fi + q);
                if (is_price) { e++; while (isdigit((unsigned char)fi[e])) e++; }
                q = e - 1 + (fi[e] ? 0 : 0);
            }
        for (int g = 0; g < M.ngrp && g < CEO_MAX_GROUPS; g++)
        {
            gok[g] = 0; if (M.grp[g].st != si) continue;
            if (M.grp[g].key[0] && QuantityForKey(fi, M.grp[g].key, &gval[g])) { gok[g] = 1; nmatch++; if (idx >= nn || nums[idx] != gval[g]) consistent = 0; }
            else nmiss++;
            idx++;
        }
        if (nmiss > 0 && nmatch > 0 && consistent && nn == idx)
        {
            idx = 0;
            for (int g = 0; g < M.ngrp && g < CEO_MAX_GROUPS; g++)
            { if (M.grp[g].st != si) continue; if (!gok[g]) { gval[g] = nums[idx]; gok[g] = 1; order_used = 1; } idx++; }
        }
    }
    P->order_assumed = order_used;
    for (int g = 0; g < M.ngrp; g++)
    {
        Group *G = &M.grp[g]; if (G->st != si) continue;
        int v = (g < CEO_MAX_GROUPS) ? gval[g] : 0;
        if (g >= CEO_MAX_GROUPS || !gok[g])
        { size_t l = strlen(missing); snprintf(missing + l, sizeof(missing) - l, "%s%s", l ? ", " : "", G->key[0] ? G->key : "(sin nombre)"); continue; }
        const char *m = M.mask[G->file]; size_t e = G->close;
        while (e > G->open && (isspace((unsigned char)m[e - 1]) || m[e - 1] == ',')) e--;
        char ins[32]; snprintf(ins, sizeof(ins), ", %d", v); AddEdit(&M, G->file, e, 0, ins);
        /* expected value */
        size_t p = G->open + 1; int k = 0, d = 0;
        for (; p < G->close && k < vm; p++) { if (m[p] == '{' || m[p] == '(') d++; else if (m[p] == '}' || m[p] == ')') d--; else if (m[p] == ',' && d == 0) k++; }
        char *endp; double price = strtod(srcs[G->file] + SkipWs(m, p), &endp);
        if (endp == srcs[G->file] + SkipWs(m, p)) ABSTAIN("cannot read the %s value of %s from the observed initializer", S->mem[vm].name, G->key);
        P->expected_total += price * v; P->nitems++; ngroups++;
    }
    if (missing[0]) ABSTAIN("the request gives no %s for: %s", P->field, missing);
    if (ngroups == 0) ABSTAIN("no initializers of %s to update", P->struct_name);

    /* operator 2: aggregation over the collection */
    int ci = -1;
    for (int c = 0; c < M.ncoll; c++) if (M.coll[c].st == si) { if (ci >= 0) ABSTAIN("more than one collection of %s; which one is the inventory is ambiguous", P->struct_name); ci = c; }
    if (ci < 0) ABSTAIN("no array of %s was observed to aggregate over", P->struct_name);
    Coll *C = &M.coll[ci];
    if (!C->counter[0]) ABSTAIN("cannot determine how many elements %s holds", C->ident);
    /* name the aggregate after the observed naming convention of the file
       that owns the collection: the shared prefix of its functions
       (inventario_agregar/inventario_mostrar -> inventario_total), else the
       collection itself (catalog -> catalog_total) */
    {
        const char *m = M.mask[C->file]; char pre[48] = ""; int nf = 0;
        for (size_t q = 0; m[q]; q++)
        {
            if (m[q] != '(' || q == 0) continue;
            size_t e = q; while (e > 0 && m[e - 1] == ' ') e--;
            size_t b = e; while (b > 0 && (isalnum((unsigned char)m[b - 1]) || m[b - 1] == '_')) b--;
            if (b == e) continue;
            size_t ls = LineStart(m, b);
            if (ls == b || isspace((unsigned char)m[ls]) || BraceDepth(m, b) != 0) continue;   /* top-level definitions only */
            char id[64]; size_t n = e - b < sizeof(id) - 1 ? e - b : sizeof(id) - 1; memcpy(id, m + b, n); id[n] = '\0';
            if (strcmp(id, "main") == 0) continue;
            char *us = strchr(id, '_'); if (!us) { pre[0] = '\0'; nf = -1; break; }
            us[1] = '\0';
            if (nf == 0) snprintf(pre, sizeof(pre), "%s", id);
            else if (strcmp(pre, id) != 0) { pre[0] = '\0'; nf = -1; break; }
            nf++;
        }
        if (nf > 0 && pre[0]) snprintf(P->total_func, sizeof(P->total_func), "%stotal", pre);
        else snprintf(P->total_func, sizeof(P->total_func), "%s_total", C->ident);
        for (int f = 0; f < M.n; f++) { char pat[80]; snprintf(pat, sizeof(pat), "%s(", P->total_func); if (strstr(M.mask[f], pat)) ABSTAIN("%s already exists", P->total_func); }
    }
    {
        char fn[640];
        snprintf(fn, sizeof(fn), "\ndouble %s(void)\n{\n    double total = 0.0;\n    for (int i = 0; i < %s; i++)\n        total += %s[i].%s * %s[i].%s;\n    return total;\n}\n",
                 P->total_func, C->counter, C->ident, S->mem[vm].name, C->ident, P->field);
        size_t end = strlen(srcs[C->file]);
        if (end > 0 && srcs[C->file][end - 1] != '\n') { memmove(fn + 1, fn, strlen(fn) + 1); fn[0] = '\n'; }
        AddEdit(&M, C->file, end, 0, fn);
    }
    /* prototype visible to main */
    if (M.main_file < 0) ABSTAIN("no main() observed to show the total");
    char proto[128]; snprintf(proto, sizeof(proto), "double %s(void);\n", P->total_func);
    if (M.main_file != C->file)
    {
        const char *hs = srcs[S->file];
        if (IsHeader(paths[S->file]))
        {
            const char *en = NULL, *h = hs; while ((h = strstr(h, "#endif")) != NULL) { en = h; h++; }
            if (en) AddEdit(&M, S->file, (size_t)(en - hs), 0, proto);
            else AddEdit(&M, S->file, strlen(hs), 0, proto);
            /* main must include that header */
            const char *base = strrchr(paths[S->file], '/'); base = base ? base + 1 : paths[S->file];
            if (!strstr(srcs[M.main_file], base))
            { const char *inc = strstr(srcs[M.main_file], "#include"); char l[300]; snprintf(l, sizeof(l), "#include \"%s\"\n", base); AddEdit(&M, M.main_file, inc ? (size_t)(inc - srcs[M.main_file]) : 0, 0, l); }
        }
        else
        {
            const char *ms = srcs[M.main_file], *inc = NULL, *h = ms;
            while ((h = strstr(h, "#include")) != NULL) { inc = h; h++; }
            size_t at = inc ? LineEnd(ms, (size_t)(inc - ms)) : 0;
            AddEdit(&M, M.main_file, at, 0, proto);
        }
    }
    else
    {
        /* the function is appended after main in the same file: declare it
           after the last #include so main sees the prototype */
        const char *ms = srcs[M.main_file], *inc = NULL, *h = ms;
        while ((h = strstr(h, "#include")) != NULL) { inc = h; h++; }
        size_t at = inc ? LineEnd(ms, (size_t)(inc - ms)) : 0;
        AddEdit(&M, M.main_file, at, 0, proto);
    }
    /* operator 3: print from main before its final return */
    {
        const char *ms = srcs[M.main_file], *mm = M.mask[M.main_file]; size_t ret = 0;
        for (size_t p = M.main_open; p < M.main_close; p++) if (WordAt(mm, p, "return") && BraceDepth(mm, p) - BraceDepth(mm, M.main_open) == 1) ret = p;
        size_t anchor = ret ? ret : M.main_close;
        size_t at = LineStart(ms, anchor);
        int inline_stmt = SkipWs(mm, at) != anchor;   /* other code shares the line */
        char ind[32]; if (ret && !inline_stmt) Indent(ms, at, ind, sizeof(ind)); else snprintf(ind, sizeof(ind), "    ");
        char line[256];
        if (inline_stmt)
        {
            at = anchor;
            snprintf(line, sizeof(line), "printf(\"%s: %%.2f\\n\", %s());%s", "Total", P->total_func, " ");
        }
        else
            snprintf(line, sizeof(line), "%sprintf(\"%s: %%.2f\\n\", %s());\n", ind, "Total", P->total_func);
        AddEdit(&M, M.main_file, at, 0, line);
        if (!strstr(ms, "<stdio.h>"))
        { const char *inc = strstr(ms, "#include"); AddEdit(&M, M.main_file, inc ? (size_t)(inc - ms) : 0, 0, "#include <stdio.h>\n"); }
    }
    if (!BuildHunks(&M, P)) ABSTAIN("could not express the edits as unique, non-overlapping hunks");
    snprintf(P->summary, sizeof(P->summary),
             "%s: campo `int %s` añadido y propagado a %d inicializadores; `%s()` suma %s[i].%s * %s sobre %s (%s); main lo imprime. Total esperado %.2f.",
             P->struct_name, P->field, ngroups, P->total_func, C->ident, S->mem[vm].name, P->field, C->ident, C->counter, P->expected_total);
    if (P->order_assumed)
    { size_t l = strlen(P->summary); snprintf(P->summary + l, sizeof(P->summary) - l, " Supuesto: algunos nombres de la petición no coinciden con los del código, así que asigné los valores en el orden dado (los nombres que sí coinciden confirman ese orden)."); }
    return P->nhunks;
}

int CeoApplyPlan(const CeoPlan *P, const char *const *srcs, int nfiles, char out[][8192], size_t out_size)
{
    for (int f = 0; f < nfiles; f++) snprintf(out[f], out_size, "%s", srcs[f]);
    for (int h = 0; h < P->nhunks; h++)
    {
        const CeoHunk *H = &P->hunks[h]; char *o = out[H->file];
        char *at = strstr(o, H->old_text); if (!at) return 0;
        size_t ol = strlen(H->old_text), nl = strlen(H->new_text);
        if (strlen(o) - ol + nl + 1 > out_size) return 0;
        memmove(at + nl, at + ol, strlen(at + ol) + 1); memcpy(at, H->new_text, nl);
    }
    return 1;
}

static int HasTest(const char *a, size_t n)
{
    for (size_t i = 0; i + 4 <= n; i++) if (strncmp(a + i, "test", 4) == 0) return 1;
    return 0;
}

int CeoDeriveRunCommand(const char *const *paths, const char *const *srcs, int nfiles,
                        const char *listing, char *out, size_t size)
{
    (void)paths; (void)srcs; (void)nfiles;
    if (!out || size == 0) return 0;
    out[0] = '\0';
    if (!listing) return 0;
    /* Makefile target name comes from the Makefile itself when observed */
    for (int f = 0; f < nfiles; f++)
    {
        const char *b = strrchr(paths[f], '/'); b = b ? b + 1 : paths[f];
        if (strcmp(b, "Makefile") == 0 || strcmp(b, "makefile") == 0)
        {
            const char *s = srcs[f];
            for (size_t p = 0; s[p]; p = LineEnd(s, p))
            {
                char t[64]; size_t n = Ident(s, p, t, sizeof(t));
                if (n && s[p + n] == ':' && s[p + n + 1] != '=' && strcmp(t, "all") && strcmp(t, "clean"))
                { snprintf(out, size, "make && ./%s", t); return 1; }
                if (!s[LineEnd(s, p)]) break;
            }
        }
    }
    char files[1024] = ""; const char *p = listing;
    while (*p)
    {
        const char *a = p; while (*p && *p != '\n') p++;
        size_t n = (size_t)(p - a); while (n && (a[n - 1] == '\r' || a[n - 1] == ' ')) n--;
        if (n > 2 && a[n - 2] == '.' && a[n - 1] == 'c' && !HasTest(a, n) && strlen(files) + n + 2 < sizeof(files))
        { strncat(files, a, n); strcat(files, " "); }
        if (*p) p++;
    }
    if (!files[0]) return 0;
    snprintf(out, size, "gcc -std=c11 -Wall -Wextra %s-o /tmp/symbols-app && /tmp/symbols-app", files);
    return 1;
}
