/* Exercise the private subject-index rebuild with deterministic allocation faults.
   This target compiles relation.c directly so the fault hooks cannot affect other
   production code or depend on platform-specific allocator interposition. */
#include <stdio.h>
#include <stdlib.h>

static int fail_malloc_once;
static int fail_realloc_once;
static void *TestMalloc(size_t n)
{
    if (fail_malloc_once) { fail_malloc_once = 0; return NULL; }
    return malloc(n);
}
static void *TestRealloc(void *p, size_t n)
{
    if (fail_realloc_once) { fail_realloc_once = 0; return NULL; }
    return realloc(p, n);
}
#define malloc TestMalloc
#define realloc TestRealloc
#include "../src/relation.c"
#undef malloc
#undef realloc

static void Check(int ok, const char *why)
{
    if (!ok) { fprintf(stderr, "FAIL: %s\n", why); exit(1); }
}

int main(void)
{
    RELATION_TABLE *t = RelationTableCreate(16);
    RELATION *found[2];
    Check(t != NULL, "create table");
    Check(RelationAdd(t, 1, 2, 3), "add edge");
    Check(RelationFindBySubject(t, 1, found, 2) == 1,
          "initial subject lookup");

    /* A replacement must release old heads on every successful rebuild.
       LSan detects lost prior allocations, not just the final index. */
    for (int i = 0; i < 32; i++)
    {
        SubjectIndexRebuild(t);
        Check(t->subj_capacity != 0 &&
              RelationFindBySubject(t, 1, found, 2) == 1,
              "repeated rebuild and query");
    }

    fail_malloc_once = 1;
    SubjectIndexRebuild(t);
    Check(t->subj_heads == NULL && t->subj_capacity == 0,
          "head allocation failure invalidates index");
    Check(RelationFindBySubject(t, 1, found, 2) == 1,
          "head failure retains linear query fallback");
    SubjectIndexRebuild(t);
    Check(t->subj_capacity != 0, "recover from head failure");

    fail_realloc_once = 1;
    SubjectIndexRebuild(t);
    Check(t->subj_heads == NULL && t->subj_capacity == 0,
          "next allocation failure invalidates index");
    Check(RelationFindBySubject(t, 1, found, 2) == 1,
          "next failure retains linear query fallback");
    SubjectIndexRebuild(t);
    Check(t->subj_capacity != 0 &&
          RelationFindBySubject(t, 1, found, 2) == 1,
          "recover from next failure");
    RelationTableDestroy(t);
    puts("subject-index rebuild ownership and failure paths OK");
    return 0;
}
