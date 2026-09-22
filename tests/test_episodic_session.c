/* test_episodic_session.c: end-to-end session semantics of episodic memory.
   Covers learn -> query -> session restart -> query -> forget -> confirmed
   absence in the SAME running session (schema KB, text graph and reasoning
   graph), full clear scrubbing, and persistence-write failure propagation.
   Runs inside a scratch working directory so the real store is untouched. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "chat.h"

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define CHDIR(d) _chdir(d)
#define MKDIR(d) _mkdir(d)
#define RMDIR(d) _rmdir(d)
#else
#include <unistd.h>
#include <sys/stat.h>
#define CHDIR(d) chdir(d)
#define MKDIR(d) mkdir(d, 0755)
#define RMDIR(d) rmdir(d)
#endif

static int g_pass;
static int g_fail;

static void check(int cond, const char *name)
{
    if (cond)
    {
        printf("  [PASS] %s\n", name);
        g_pass++;
    }
    else
    {
        printf("  [FAIL] %s\n", name);
        g_fail++;
    }
}

static void ask(const CHAT *ch, const char *q, char *out, size_t size)
{
    memset(out, 0, size);
    ChatHandleToBuf((CHAT *)ch, q, out, size);
}

int main(void)
{
    char out[4096];

    printf("=== EPISODIC SESSION E2E (learn/restart/forget) ===\n");

    /* Isolated working directory: ChatInit resolves
       data/memory/episodic.tsv relative to the cwd. */
    MKDIR("test_episodic_session_scratch");
    MKDIR("test_episodic_session_scratch/data");
    MKDIR("test_episodic_session_scratch/data/memory");
    FILE *f = fopen("test_episodic_session_scratch/corpus.tsv", "w");
    if (f == NULL)
        return 1;
    fputs("paris\tIN\tfrance\n", f);
    fclose(f);
    if (CHDIR("test_episodic_session_scratch") != 0)
        return 1;

    /* --- Phase 1: learn and query in the same session --- */
    printf("\n--- Phase 1: learn + query ---\n");
    CHAT ch;
    memset(&ch, 0, sizeof(ch));
    ChatInit(&ch, "corpus.tsv");
    check(ChatLearnTriple(&ch, "Ada", "hermano_de", "Babbage", "user") == 1,
          "learn reports persisted success");
    check(ChatEpisodicCount(&ch) == 1, "fact counted in episodic memory");
    ask(&ch, "es ada hermano de babbage?", out, sizeof(out));
    check(strstr(out, "Si,") != NULL, "learned fact answers Si in-session");
    ChatDestroy(&ch);

    /* --- Phase 2: session restart over the same on-disk store --- */
    printf("\n--- Phase 2: restart + query ---\n");
    memset(&ch, 0, sizeof(ch));
    ChatInit(&ch, "corpus.tsv");
    check(ChatEpisodicCount(&ch) == 1, "fact survives session restart");
    ask(&ch, "es ada hermano de babbage?", out, sizeof(out));
    check(strstr(out, "Si,") != NULL, "restarted session still answers Si");

    /* --- Phase 3: selective forget and confirmed absence --- */
    printf("\n--- Phase 3: forget + confirmed absence ---\n");
    check(ChatForgetTriple(&ch, "Ada", "hermano_de", "Babbage") == 1,
          "forget reports success");
    check(ChatEpisodicCount(&ch) == 0, "forgotten fact leaves the store");
    ask(&ch, "es ada hermano de babbage?", out, sizeof(out));
    check(strstr(out, "Si,") == NULL, "forgotten fact no longer answers Si");
    check(strstr(out, "No tengo constancia") != NULL,
          "forgotten fact abstains honestly");
    check(ChatForgetTriple(&ch, "Ada", "hermano_de", "Babbage") == 0,
          "re-forget reports not-present");
    ChatDestroy(&ch);

    /* --- Phase 4: persistence across another restart --- */
    printf("\n--- Phase 4: forget survives restart ---\n");
    memset(&ch, 0, sizeof(ch));
    ChatInit(&ch, "corpus.tsv");
    check(ChatEpisodicCount(&ch) == 0, "forget persists across restart");
    ask(&ch, "es ada hermano de babbage?", out, sizeof(out));
    check(strstr(out, "Si,") == NULL, "restarted session keeps the absence");

    /* --- Phase 5: full clear scrubs the live session --- */
    printf("\n--- Phase 5: clear scrubs session ---\n");
    check(ChatLearnTriple(&ch, "Grace", "esposa_de", "Hopper", "user") == 1,
          "second fact learned");
    check(ChatLearnTriple(&ch, "Alan", "padre_de", "Turing", "user") == 1,
          "third fact learned");
    check(ChatEpisodicCount(&ch) == 2, "two facts counted");
    check(ChatEpisodicClear(&ch) == 1, "clear reports success");
    check(ChatEpisodicCount(&ch) == 0, "clear empties the store");
    ask(&ch, "es grace esposa de hopper?", out, sizeof(out));
    check(strstr(out, "Si,") == NULL, "cleared fact one stops answering");
    ask(&ch, "es alan padre de turing?", out, sizeof(out));
    check(strstr(out, "Si,") == NULL, "cleared fact two stops answering");
    ChatDestroy(&ch);

#ifndef _WIN32
    /* --- Phase 6: persistence-write failure (POSIX permissions) --- */
    printf("\n--- Phase 6: write failure propagation ---\n");
    if (geteuid() == 0)
    {
        /* chmod 0555 does not block root: skip rather than false-pass. */
        printf("  [SKIP] running as root; permission-based failure not enforceable\n");
    }
    else
    {
    memset(&ch, 0, sizeof(ch));
    ChatInit(&ch, "corpus.tsv");
    check(chmod("data/memory", 0555) == 0, "memory dir made unwritable");
    check(ChatLearnTriple(&ch, "Edsger", "hermano_de", "Dijkstra", "user") == 0,
          "learn reports the write failure");
    check(ChatEpisodicCount(&ch) == 0, "failed learn leaves no in-memory fact");
    ask(&ch, "es edsger hermano de dijkstra?", out, sizeof(out));
    check(strstr(out, "Si,") == NULL, "failed learn never answers Si");
    check(ChatEpisodicClear(&ch) == 0, "clear reports the write failure");
    check(chmod("data/memory", 0755) == 0, "memory dir writable again");
    check(ChatLearnTriple(&ch, "Edsger", "hermano_de", "Dijkstra", "user") == 1,
          "store recovers once writable");
    ChatDestroy(&ch);
    }
#endif

    /* Cleanup scratch state */
    if (CHDIR("..") != 0)
        return 1;
    remove("test_episodic_session_scratch/corpus.tsv");
    remove("test_episodic_session_scratch/data/memory/episodic.tsv");
    RMDIR("test_episodic_session_scratch/data/memory");
    RMDIR("test_episodic_session_scratch/data");
    RMDIR("test_episodic_session_scratch");

    printf("\n=======================================================\n");
    if (g_fail == 0)
    {
        printf("EPISODIC SESSION E2E SUMMARY: ALL %d CHECKS PASSED\n", g_pass);
        printf("=======================================================\n");
        return 0;
    }
    printf("EPISODIC SESSION E2E SUMMARY: %d FAILED (%d passed)\n", g_fail, g_pass);
    printf("=======================================================\n");
    return 1;
}
