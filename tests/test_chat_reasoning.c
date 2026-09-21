/* Chat L3: graph_reasoning on the hot path.
   Deduce implicit IN-edges; do not treat hypotheses as facts. */
#include <stdio.h>
#include <string.h>
#include "chat.h"

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

int main(void)
{
    const char *tsv = "test_chat_reasoning_in.tsv";
    FILE *f;
    CHAT ch;
    char out[2048];

    printf("=== CHAT GRAPH REASONING (L3 hot path) ===\n");
    f = fopen(tsv, "w");
    if (f == NULL)
        return 1;
    fputs("paris\tIN\tfrance\n", f);
    fputs("france\tIN\teurope\n", f);
    fputs("paris\tIN\teurope\n", f);
    fputs("madrid\tIN\tspain\n", f);
    fputs("spain\tIN\teurope\n", f);
    fputs("madrid\tIN\teurope\n", f);
    fputs("rome\tIN\titaly\n", f);
    fputs("italy\tIN\teurope\n", f);
    fputs("rome\tIN\teurope\n", f);
    fputs("toledo\tIN\tspain\n", f);
    fclose(f);

    memset(&ch, 0, sizeof(ch));
    ChatInit(&ch, tsv);
    ch.episodic.auto_save = 0;

    check(ch.rgraph != NULL, "reasoning graph mounted");
    check(ch.rbase.num_rules >= 1, "mined at least one Horn rule");

    memset(out, 0, sizeof(out));
    ChatHandleToBuf(&ch, "es paris in de europe?", out, sizeof(out));
    check(strstr(out, "Si,") != NULL && strstr(out, "Paris") != NULL,
          "observed paris in europe still answers Si");

    memset(out, 0, sizeof(out));
    ChatHandleToBuf(&ch, "es toledo in de europe?", out, sizeof(out));
    printf("  [INFO] toledo in europe -> %s", out);
    check(strstr(out, "Si,") != NULL && strstr(out, "Toledo") != NULL,
          "deduced toledo in europe (not in KB, inferred by in o in => in)");
    check(strstr(out, "No tengo") == NULL,
          "deduction is a fact, not an abstention");

    memset(out, 0, sizeof(out));
    ChatHandleToBuf(&ch, "es toledo in de mars?", out, sizeof(out));
    check(strstr(out, "No tengo") != NULL, "toledo in mars stays UNKNOWN");
    check(strstr(out, "Si,") == NULL, "UNKNOWN is not a yes");

    ChatDestroy(&ch);
    remove(tsv);

    printf("\n--- Commonsense seed (no bible) ---\n");
    memset(&ch, 0, sizeof(ch));
    ChatInit(&ch, NULL);
    ch.episodic.auto_save = 0;

    memset(out, 0, sizeof(out));
    ChatHandleToBuf(&ch, "donde esta la leche?", out, sizeof(out));
    printf("  [INFO] leche -> %s", out);
    check(strstr(out, "refrigerator") != NULL || strstr(out, "kitchen") != NULL,
          "WHERE milk uses commonsense, not bible");

    memset(out, 0, sizeof(out));
    ChatHandleToBuf(&ch, "que es un perro?", out, sizeof(out));
    printf("  [INFO] perro -> %s", out);
    check(strstr(out, "sentido comun") != NULL, "WHAT dog is commonsense");
    check(strstr(out, "IS_A") == NULL, "CS fact is realized, not dumped as IS_A");
    check(strstr(out, "canine") != NULL || strstr(out, "mammal") != NULL ||
          strstr(out, "animal") != NULL,
          "dog describes as canine/mammal/animal");

    memset(out, 0, sizeof(out));
    ChatHandleToBuf(&ch, "es un perro un animal?", out, sizeof(out));
    printf("  [INFO] perro animal -> %s", out);
    check(strstr(out, "Si,") != NULL, "dog is an animal via IS_A closure");
    check(strstr(out, "No tengo") == NULL, "dog-animal is not UNKNOWN");

    memset(out, 0, sizeof(out));
    ChatHandleToBuf(&ch, "es un perro un refrigerador?", out, sizeof(out));
    check(strstr(out, "No tengo") != NULL, "dog is not a refrigerator");

    memset(out, 0, sizeof(out));
    ChatHandleToBuf(&ch, "que es un pajaro?", out, sizeof(out));
    printf("  [INFO] pajaro -> %s", out);
    check(strstr(out, "fly") != NULL || strstr(out, "volar") != NULL ||
          strstr(out, "puede") != NULL || strstr(out, "can ") != NULL,
          "bird capability from commonsense");

    memset(out, 0, sizeof(out));
    ChatHandleToBuf(&ch, "para que sirve un cuchillo?", out, sizeof(out));
    printf("  [INFO] cuchillo -> %s", out);
    check(strstr(out, "cut") != NULL || strstr(out, "cort") != NULL,
          "AFFORDANCE knife from commonsense");

    ChatDestroy(&ch);

    printf("\n--- wiki_sample (general knowledge text) ---\n");
    memset(&ch, 0, sizeof(ch));
    ChatInit(&ch, "data/texts/wiki_sample.txt");
    ch.episodic.auto_save = 0;
    memset(out, 0, sizeof(out));
    ChatHandleToBuf(&ch, "what is pipe flow?", out, sizeof(out));
    printf("  [INFO] pipe flow -> %s", out);
    check(strstr(out, "pipe") != NULL || strstr(out, "flow") != NULL ||
          strstr(out, "conduit") != NULL,
          "wiki_sample answers pipe flow without bible");
    check(strstr(out, "Jesse") == NULL && strstr(out, "David") == NULL,
          "wiki answer is not a bible verse");
    ChatDestroy(&ch);

    printf("\n=== RESULTS %d passed, %d failed ===\n", g_pass, g_fail);
    return (g_fail == 0 && g_pass >= 14) ? 0 : 1;
}
