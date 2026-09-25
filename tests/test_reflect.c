/* test_reflect.c: reflection store - append, read back by key, only
   toolchain rows count, forget, and the rewrite leaves no temporary. */
#include "reflect.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

int main(void)
{
    const char *p = "test_reflect_store.tsv";
    char tmp[64];
    snprintf(tmp, sizeof(tmp), "%s.tmp", p);
    remove(p);
    REFLECTION r, out[8];
    memset(&r, 0, sizeof(r));
    snprintf(r.key, sizeof(r.key), "k1");
    r.attempt = 1;
    snprintf(r.op, sizeof(r.op), "c_contract");
    snprintf(r.detail, sizeof(r.detail), "boundary:\tline 1");   /* a tab never splits a row */
    snprintf(r.feedback, sizeof(r.feedback), "verify failed: intent=0\nscope");
    r.ts = 1790000000;
    ReflectCompose(&r);
    CHECK(strstr(r.text, "Tried c_contract") && strstr(r.text, "Next attempt: exclude this edit."));
    CHECK(ReflectAppend(p, &r));
    snprintf(r.key, sizeof(r.key), "k2");
    CHECK(ReflectAppend(p, &r));
    CHECK(ReflectLoad(p, "k1", out, 8) == 1 && out[0].attempt == 1 && !strcmp(out[0].op, "c_contract") &&
          !strcmp(out[0].detail, "boundary: line 1") && out[0].ts == 1790000000);
    CHECK(ReflectLoad(p, NULL, out, 8) == 2);
    FILE *f = fopen(p, "ab");   /* a row not from the toolchain is never read back */
    fputs("k1\t2\tx\ty\tz\tguess\t0\tt\n", f);
    fclose(f);
    CHECK(ReflectLoad(p, "k1", out, 8) == 1);
    CHECK(ReflectForget(p, "k1") == 1 && ReflectLoad(p, "k1", out, 8) == 0 && ReflectLoad(p, "k2", out, 8) == 1);
    f = fopen(tmp, "rb");
    CHECK(f == NULL);
    if (f) fclose(f);
    CHECK(ReflectLoad("no_such_reflect_store.tsv", "k1", out, 8) == 0);
    remove(p);
    printf("%s\n", fails ? "FAILED" : "OK");
    return fails != 0;
}
