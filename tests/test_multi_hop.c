/* test_multi_hop: reasoning depth measurement.
   bible.txt: 0/1/2/3-hop father chain from text at runtime.
   taxonomy TSV: 2-hop over a different TRANSITIVE family (isa).
   Scores CORRECT / WRONG / UNKNOWN. WRONG must stay 0. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "chat.h"

static int is_unknown(const char *answer)
{
    return strstr(answer, "No tengo") != NULL ||
           strstr(answer, "No entendi") != NULL ||
           strstr(answer, "UNKNOWN") != NULL;
}

static int check(const char *q, const char *answer,
                 const char *expected, const char *label,
                 int *wrong)
{
    int pass = (strstr(answer, expected) != NULL);
    int unk = is_unknown(answer);
    const char *tag = pass ? "PASS" : (unk ? "UNKNOWN" : "WRONG");
    printf("  [%s] %s\n", tag, label);
    if (!pass)
    {
        printf("    Q: %s\n    A: %s\n    Expected: %s\n",
               q, answer, expected);
        if (!unk && wrong != NULL)
            (*wrong)++;
    }
    return pass;
}

static int check_unk(const char *q, const char *answer,
                     const char *label, int *wrong)
{
    int unk = is_unknown(answer);
    printf("  [%s] %s\n", unk ? "PASS" : "WRONG", label);
    if (!unk)
    {
        printf("    Q: %s\n    A: %s\n", q, answer);
        if (wrong != NULL)
            (*wrong)++;
    }
    return unk;
}

int main(void)
{
    CHAT ch;
    char out[2048];
    int passed = 0, total = 0, wrong = 0;

    memset(&ch, 0, sizeof(ch));
    printf("Loading bible.txt (reasoning deduced from text at runtime)...\n");
    ChatInit(&ch, "data/texts/bible.txt");
    printf("Ready. KB pairs: %u\n\n", ch.kb.num_pairs);

    printf("\n--- 0-HOP: Direct entity lookup ---\n");
    memset(out, 0, sizeof(out));
    ChatHandleToBuf(&ch, "que es el pecado?", out, sizeof(out));
    total++;
    passed += check("que es el pecado?", out, "sin",
                    "0-hop: pecado via dict→sin", &wrong);

    printf("\n--- 1-HOP: Father-of (named fact, not verse echo) ---\n");
    memset(out, 0, sizeof(out));
    ChatHandleToBuf(&ch, "quien es el padre de David?", out, sizeof(out));
    total++;
    passed += check("quien es el padre de David?", out,
                    "El padre de David es Jesse",
                    "1-hop: father of David = Jesse (named)", &wrong);

    memset(out, 0, sizeof(out));
    ChatHandleToBuf(&ch, "quien es el padre de Solomon?", out, sizeof(out));
    total++;
    passed += check("quien es el padre de Solomon?", out,
                    "El padre de Solomon es David",
                    "1-hop: father of Solomon = David (named)", &wrong);

    memset(out, 0, sizeof(out));
    ChatHandleToBuf(&ch, "quien es el padre de Isaac?", out, sizeof(out));
    total++;
    passed += check("quien es el padre de Isaac?", out,
                    "El padre de Isaac es Abraham",
                    "1-hop: father of Isaac = Abraham (named)", &wrong);

    memset(out, 0, sizeof(out));
    ChatHandleToBuf(&ch, "quien es el padre de Abraham?", out, sizeof(out));
    total++;
    if (strstr(out, "Your") != NULL || strstr(out, "your") != NULL)
    {
        printf("  [WRONG] 1-hop: father of Abraham must not be a pronoun\n");
        printf("    A: %s\n", out);
        wrong++;
    }
    else if (strstr(out, "Terah") != NULL || strstr(out, "Tare") != NULL)
    {
        passed += check("quien es el padre de Abraham?", out, "Tera",
                        "1-hop: father of Abraham = Terah (named)", &wrong);
    }
    else
    {
        passed += check_unk("quien es el padre de Abraham?", out,
                            "1-hop: father of Abraham UNKNOWN (no unique named parent)",
                            &wrong);
    }

    /* begat Pattern C, named */
    memset(out, 0, sizeof(out));
    ChatHandleToBuf(&ch, "quien es el padre de Obed?", out, sizeof(out));
    total++;
    passed += check("quien es el padre de Obed?", out,
                    "El padre de Obed es Boaz",
                    "1-hop: father of Obed = Boaz (begat)", &wrong);

    /* promotion: verified text pair landed in the KB */
    {
        char pars[8][CHAT_TOKEN_MAX];
        uint32_t np = ChatParentsList(&ch, "david", pars, 8);
        int has_jesse = 0;
        for (uint32_t i = 0; i < np; i++)
            if (strcmp(pars[i], "jesse") == 0)
                has_jesse = 1;
        printf("  [%s] promote: father(jesse,david) in KB (pairs=%u)\n",
               has_jesse ? "PASS" : "FAIL", ch.kb.num_pairs);
        total++;
        if (has_jesse)
            passed++;
        else
            wrong++;
    }

    /* second ask uses the learned pair; still a named fact */
    memset(out, 0, sizeof(out));
    ChatHandleToBuf(&ch, "quien es el padre de David?", out, sizeof(out));
    total++;
    passed += check("quien es el padre de David?", out,
                    "El padre de David es Jesse",
                    "1-hop replay from KB: still named Jesse", &wrong);

    printf("\n--- 2-HOP: Grandfather (father->father chain) ---\n");
    memset(out, 0, sizeof(out));
    ChatHandleToBuf(&ch, "quien es el abuelo de David?", out, sizeof(out));
    total++;
    passed += check("quien es el abuelo de David?", out,
                    "El abuelo de David es Obed",
                    "2-hop: grandfather of David = Obed", &wrong);

    memset(out, 0, sizeof(out));
    ChatHandleToBuf(&ch, "quien es el abuelo de Solomon?", out, sizeof(out));
    total++;
    passed += check("quien es el abuelo de Solomon?", out,
                    "El abuelo de Solomon es Jesse",
                    "2-hop: grandfather of Solomon = Jesse", &wrong);

    printf("\n--- 3-HOP: padre del abuelo (father->father->father) ---\n");
    memset(out, 0, sizeof(out));
    ChatHandleToBuf(&ch, "quien es el padre del abuelo de David?",
                    out, sizeof(out));
    total++;
    passed += check("quien es el padre del abuelo de David?", out,
                    "El padre del abuelo de David es Boaz",
                    "3-hop: great-grandfather of David = Boaz", &wrong);

    memset(out, 0, sizeof(out));
    ChatHandleToBuf(&ch, "who is the father of the grandfather of David?",
                    out, sizeof(out));
    total++;
    passed += check("who is the father of the grandfather of David?", out,
                    "Boaz",
                    "3-hop EN: father of grandfather of David = Boaz",
                    &wrong);

    printf("\n--- NEGATIVE: Chain break → UNKNOWN ---\n");
    memset(out, 0, sizeof(out));
    ChatHandleToBuf(&ch, "quien es el abuelo de Adam?", out, sizeof(out));
    total++;
    passed += check_unk("quien es el abuelo de Adam?", out,
                        "2-hop negative: grandfather of Adam = UNKNOWN",
                        &wrong);

    memset(out, 0, sizeof(out));
    ChatHandleToBuf(&ch, "quien es el padre del abuelo de Adam?",
                    out, sizeof(out));
    total++;
    passed += check_unk("quien es el padre del abuelo de Adam?", out,
                        "3-hop negative: great-grandfather of Adam = UNKNOWN",
                        &wrong);

    printf("\n--- CROSS-DOMAIN: Text entity + relation ---\n");
    memset(out, 0, sizeof(out));
    ChatHandleToBuf(&ch, "quien es el padre de David?", out, sizeof(out));
    total++;
    passed += check("quien es el padre de David?", out, "Jesse",
                    "cross-domain: David from text, father = Jesse",
                    &wrong);

    printf("\n=== MULTI-HOP (bible) ===\n");
    printf("Passed:   %d / %d\n", passed, total);
    printf("WRONG:    %d\n", wrong);

    /* taxonomy 2-hop: a different TRANSITIVE family (isa / HIJO_DE) */
    printf("\n--- TAXONOMY 2-HOP (isa family, not father) ---\n");
    {
        const char *tsv = "test_multi_hop_tax.tsv";
        FILE *f = fopen(tsv, "w");
        if (f == NULL)
        {
            printf("  FAIL cannot write taxonomy scratch\n");
            return 1;
        }
        fputs("robin\tHIJO_DE\tbird\n", f);
        fputs("bird\tHIJO_DE\tanimal\n", f);
        fclose(f);
        CHAT tax;
        memset(&tax, 0, sizeof(tax));
        ChatInit(&tax, tsv);
        remove(tsv);

        memset(out, 0, sizeof(out));
        ChatHandleToBuf(&tax, "quien es el abuelo de robin?", out,
                        sizeof(out));
        total++;
        passed += check("quien es el abuelo de robin?", out,
                        "El abuelo de Robin es Animal",
                        "taxonomy 2-hop: robin→bird→animal", &wrong);

        memset(out, 0, sizeof(out));
        ChatHandleToBuf(&tax, "es robin hijo de animal?", out,
                        sizeof(out));
        total++;
        passed += check("es robin hijo de animal?", out,
                        "Si, Robin es hijo de Animal",
                        "taxonomy 2-hop bool: robin isa animal", &wrong);

        memset(out, 0, sizeof(out));
        ChatHandleToBuf(&tax, "quien es el padre del abuelo de robin?",
                        out, sizeof(out));
        total++;
        passed += check_unk("quien es el padre del abuelo de robin?", out,
                            "taxonomy 3-hop negative: no 3rd isa link",
                            &wrong);
    }

    printf("\n=== MULTI-HOP RESULTS ===\n");
    printf("Passed:   %d / %d\n", passed, total);
    printf("WRONG:    %d\n", wrong);
    printf("KB pairs (bible): %u\n", ch.kb.num_pairs);

    /* named 1-hop + Abraham fail-closed + promote + 3-hop + taxonomy 2-hop. WRONG = 0. */
    return (passed >= 17 && wrong == 0) ? 0 : 1;
}
