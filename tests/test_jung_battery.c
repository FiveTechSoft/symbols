#include <stdio.h>
#include <string.h>
#include "chat.h"

int main(void)
{
    CHAT ch;
    memset(&ch, 0, sizeof(ch));
    printf("Loading jung.txt...\n");
    ChatInit(&ch, "data/texts/jung.txt");
    printf("Ready.\n\n");

    const char *questions[] = {
        "quien es Jung?",
        "que es el inconsciente?",
        "que es el arquetipo?",
        "que es la sombra?",
        "que es la anima?",
        "que es el self?",
        "que es la individuacion?",
        "que es la proyeccion?",
        "que es el complejo?",
        "que es el sueño?",
        "quien estudio los sueños?",
        "que es la libido?",
        "que es el ello?",
        "que es el yo?",
        "que es el superyo?",
        "donde vivio Jung?",
        "cuantos arquetipos hay?",
        "que es la sincronicidad?",
        "que es la intuicion?",
        "que es el pensamiento?"
    };

    char out[2048];
    for (int i = 0; i < 20; i++)
    {
        memset(out, 0, sizeof(out));
        int r = ChatHandleToBuf(&ch, questions[i], out, sizeof(out));
        printf("[%2d] Q: %s\n", i + 1, questions[i]);
        printf("     A: %s\n", r ? out : "(no respuesta)\n");
    }
    return 0;
}
