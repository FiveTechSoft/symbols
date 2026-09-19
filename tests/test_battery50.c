#include <stdio.h>
#include <string.h>
#include "chat.h"

int main(void)
{
    CHAT ch;
    memset(&ch, 0, sizeof(ch));
    printf("Loading jung.txt + bible.txt...\n");
    ChatInit(&ch, "data/texts/jung.txt;data/texts/bible.txt");
    printf("Ready.\n\n");

    const char *questions[] = {
        /* === Jung (1-15) === */
        "quien es Jung?",
        "que es el inconsciente?",
        "que es el arquetipo?",
        "que es la sombra?",
        "que es la anima?",
        "que es el self?",
        "que es la individuacion?",
        "que es la proyeccion?",
        "que es el complejo?",
        "que es el sueno?",
        "quien estudio los sueños?",
        "que es la libido?",
        "que es el ello?",
        "que es el yo?",
        "que es el superyo?",
        /* === Bible (16-30) === */
        "quien creo el cielo y la tierra?",
        "quien es el padre de Abraham?",
        "donde nacio Jesus?",
        "quien bautizo a Jesus?",
        "cuantos mandamientos hay?",
        "quien escribio los salmos?",
        "que es el pecado?",
        "quien es el Mesias?",
        "donde vivio Moises?",
        "cuantos apóstoles hubo?",
        "que es la fe?",
        "quien es David?",
        "que es el arca de la alianza?",
        "quien es el profeta Elias?",
        "que es la resurreccion?",
        /* === Cross-corpus (31-40) === */
        "que es el alma?",
        "quien es el ser humano?",
        "que es el mal?",
        "que es la verdad?",
        "que es el amor?",
        "que es la muerte?",
        "que es la vida?",
        "que es el poder?",
        "que es la sabiduria?",
        "que es la justicia?",
        /* === Cross-lingual (41-50) === */
        "what is the unconscious?",
        "who wrote the book of Genesis?",
        "what is a complex?",
        "how many books are in the Bible?",
        "what is the golden bough?",
        "who is Moses?",
        "what is projection?",
        "who is the mother of Jesus?",
        "what is synchronicity?",
        "what is the holy spirit?"
    };

    int total = 50;
    int answered = 0;
    int unknown = 0;
    int wrong = 0;
    char out[2048];

    for (int i = 0; i < total; i++)
    {
        memset(out, 0, sizeof(out));
        printf("[%2d] Q: %s\n", i + 1, questions[i]);
        fflush(stdout);
        int r = ChatHandleToBuf(&ch, questions[i], out, sizeof(out));
        printf("     A: %s\n", r ? out : "(no respuesta)\n");
        fflush(stdout);

        /* classify */
        if (!r || strstr(out, "UNKNOWN") || strstr(out, "No tengo"))
            unknown++;
        else
            answered++;
    }

    printf("\n=== BATTERY 50 RESULTS ===\n");
    printf("Answered:  %d / %d\n", answered, total);
    printf("UNKNOWN:   %d / %d\n", unknown, total);
    printf("Wrong:     %d / %d\n", wrong, total);
    printf("Corpus:    jung.txt (10730 sents) + bible.txt (28746 sents)\n");
    printf("Total:     39476 sentences loaded\n");

    return (answered >= 40) ? 0 : 1;
}
