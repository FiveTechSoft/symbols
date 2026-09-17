/* chat_main: REPL conversacional sobre bible_chat.
   Uso: chat_main [corpus.tsv]
   Sin argumentos usa data/bible/bible_relations.tsv */
#include <stdio.h>
#include <string.h>
#include "bible_chat.h"

int main(int argc, char **argv)
{
    const char *corpus = (argc > 1)
                             ? argv[1]
                             : "data/bible/bible_relations.tsv";
    CHAT chat;
    ChatInit(&chat, corpus);
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
        ChatHandle(&chat, line);
    }
    return 0;
}