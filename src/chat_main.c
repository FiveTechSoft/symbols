/* chat_main: REPL conversacional sobre el wrapper de clarificacion
   (Fase B), que a su vez usa el motor puro chat.
   Uso: chat_main [corpus.tsv]
   Sin argumentos usa data/bible/bible_relations.tsv */
#include <stdio.h>
#include <string.h>
#include "chat_clarify.h"

int main(int argc, char **argv)
{
    const char *corpus = (argc > 1)
                             ? argv[1]
                             : "data/bible/bible_relations.tsv";
    CLARIFY chat;
    ClarifyInit(&chat, corpus);
    printf("Listo. Escribe una pregunta (o 'salir').\n");
    char line[512];
    while (fgets(line, sizeof(line), stdin))
    {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        if (len == 0)
            continue;
        if (strcmp(line, "salir") == 0 || strcmp(line, "exit") == 0 ||
            strcmp(line, "quit") == 0)
            break;
        ClarifyHandle(&chat, line);
    }
    return 0;
}