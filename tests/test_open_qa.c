/* Open-domain QA: entity-grounded retrieval, no false-friend Lev.
   WRONG must stay 0. UNKNOWN is allowed when the corpus has no name. */
#include <stdio.h>
#include <string.h>
#include "chat.h"

static int is_unknown(const char *a)
{
    return strstr(a, "No tengo") != NULL ||
           strstr(a, "No entendi") != NULL ||
           strstr(a, "UNKNOWN") != NULL;
}

static int has_ci(const char *hay, const char *need)
{
    size_t nlen, hlen, i, j;
    if (!hay || !need || !need[0])
        return 0;
    nlen = strlen(need);
    hlen = strlen(hay);
    if (nlen > hlen)
        return 0;
    for (i = 0; i + nlen <= hlen; i++)
    {
        for (j = 0; j < nlen; j++)
        {
            char h = hay[i + j];
            char n = need[j];
            if (h >= 'A' && h <= 'Z') h = (char)(h + 32);
            if (n >= 'A' && n <= 'Z') n = (char)(n + 32);
            if (h != n)
                break;
        }
        if (j == nlen)
            return 1;
    }
    return 0;
}

static int check_any(const char *q, const char *a,
                     const char *const *need, const char *label,
                     int *wrong)
{
    int i, ok = 0;
    if (is_unknown(a))
    {
        printf("  [UNKNOWN] %s\n    A: %.160s\n", label, a);
        return 0;
    }
    for (i = 0; need[i] != NULL; i++)
    {
        if (has_ci(a, need[i]))
        {
            ok = 1;
            break;
        }
    }
    printf("  [%s] %s\n", ok ? "PASS" : "WRONG", label);
    if (!ok)
    {
        printf("    Q: %s\n    A: %.200s\n", q, a);
        if (wrong)
            (*wrong)++;
    }
    return ok;
}

static int check_not(const char *a, const char *banned, const char *label,
                     int *wrong)
{
    int bad = has_ci(a, banned);
    printf("  [%s] %s\n", bad ? "WRONG" : "PASS", label);
    if (bad && wrong)
        (*wrong)++;
    return !bad;
}

int main(void)
{
    CHAT ch;
    char out[2048];
    int passed = 0, total = 0, wrong = 0;
    const char *need[8];

    memset(&ch, 0, sizeof(ch));
    printf("Loading bible.txt...\n");
    ChatInit(&ch, "data/texts/bible.txt");

    memset(out, 0, sizeof(out));
    ChatHandleToBuf(&ch, "donde nacio Jesus?", out, sizeof(out));
    total++;
    need[0] = "Bethlehem"; need[1] = "Bethlehem"; need[2] = NULL;
    passed += check_any("donde nacio Jesus?", out, need,
                        "WHERE: Jesus born → Bethlehem", &wrong);

    memset(out, 0, sizeof(out));
    ChatHandleToBuf(&ch, "quien creo el cielo y la tierra?", out, sizeof(out));
    total++;
    need[0] = "God"; need[1] = "LORD"; need[2] = "created"; need[3] = NULL;
    passed += check_any("quien creo el cielo y la tierra?", out, need,
                        "WHO: created heaven → God/created", &wrong);
    total++;
    passed += check_not(out, "crew", "WHO: created is not cock crew", &wrong);

    memset(out, 0, sizeof(out));
    ChatHandleToBuf(&ch, "cuantos mandamientos hay?", out, sizeof(out));
    total++;
    need[0] = "ten"; need[1] = "commandments"; need[2] = "10"; need[3] = NULL;
    passed += check_any("cuantos mandamientos hay?", out, need,
                        "COUNT: commandments → ten", &wrong);
    total++;
    passed += check_not(out, "withered", "COUNT: not hay/grass verse", &wrong);

    memset(out, 0, sizeof(out));
    ChatHandleToBuf(&ch, "quien mato a Goliat?", out, sizeof(out));
    total++;
    need[0] = "Goliath"; need[1] = NULL;
    passed += check_any("quien mato a Goliat?", out, need,
                        "WHO: Goliath named in answer", &wrong);
    total++;
    passed += check_not(out, "owl", "WHO: Goliath is not owl/mate", &wrong);

    memset(out, 0, sizeof(out));
    ChatHandleToBuf(&ch, "quien es el padre de David?", out, sizeof(out));
    total++;
    need[0] = "Jesse"; need[1] = NULL;
    passed += check_any("quien es el padre de David?", out, need,
                        "1-hop father still Jesse", &wrong);

    memset(out, 0, sizeof(out));
    ChatHandleToBuf(&ch, "quien bautizo a Jesus?", out, sizeof(out));
    total++;
    need[0] = "John"; need[1] = "Baptist"; need[2] = NULL;
    passed += check_any("quien bautizo a Jesus?", out, need,
                        "WHO: baptized Jesus → John", &wrong);

    memset(out, 0, sizeof(out));
    ChatHandleToBuf(&ch, "que es el mal?", out, sizeof(out));
    total++;
    passed += check_not(out, "Malachi", "WHAT: mal is not Malachi", &wrong);

    memset(out, 0, sizeof(out));
    ChatHandleToBuf(&ch, "que es la fe?", out, sizeof(out));
    total++;
    passed += check_not(out, "Fear God", "WHAT: fe is not Fear God", &wrong);

    memset(out, 0, sizeof(out));
    ChatHandleToBuf(&ch, "que es el yo?", out, sizeof(out));
    total++;
    passed += check_not(out, "YORK", "WHAT: yo is not YORK", &wrong);

    ChatDestroy(&ch);

    memset(&ch, 0, sizeof(ch));
    printf("Loading jung.txt...\n");
    ChatInit(&ch, "data/texts/jung.txt");
    memset(out, 0, sizeof(out));
    ChatHandleToBuf(&ch, "que es el inconsciente?", out, sizeof(out));
    total++;
    need[0] = "unconscious"; need[1] = "inconsciente"; need[2] = NULL;
    passed += check_any("que es el inconsciente?", out, need,
                        "WHAT: inconsciente → unconscious", &wrong);
    total++;
    passed += check_not(out, "197", "WHAT: unconscious is not a page index",
                        &wrong);

    memset(out, 0, sizeof(out));
    ChatHandleToBuf(&ch, "que es el ello?", out, sizeof(out));
    total++;
    passed += check_not(out, "Ibid", "WHAT: ello/id is not Ibid", &wrong);
    ChatDestroy(&ch);

    printf("\n=== OPEN QA ===\nPassed: %d / %d\nWRONG: %d\n",
           passed, total, wrong);
    return (wrong == 0 && passed >= 8) ? 0 : 1;
}
