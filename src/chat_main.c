/* chat_main: REPL conversacional sobre el wrapper de clarificacion
   (Fase B), que a su vez usa el motor puro chat.
   Uso: chat_main [corpus.txt ...]
   Sin argumentos usa el primer fichero disponible de data/texts/ */
#include <stdio.h>
#include <string.h>
#include "chat_clarify.h"

int main(int argc, char **argv)
{
    char corpus[1024];
    if (argc > 1)
    {
        corpus[0] = '\0';
        for (int i = 1; i < argc; i++)
        {
            if (i > 1)
                strncat(corpus, ";", sizeof(corpus) - strlen(corpus) - 1);
            strncat(corpus, argv[i], sizeof(corpus) - strlen(corpus) - 1);
        }
    }
    else
    {
        static const char *cand_paths[] = {
            "data/texts/bible.txt",
            "data/texts/jung.txt",
            "data/texts/corpus.txt",
            "data/corpus.txt"
        };
        corpus[0] = '\0';
        for (size_t i = 0; i < sizeof(cand_paths) / sizeof(cand_paths[0]); i++)
        {
            FILE *f = fopen(cand_paths[i], "r");
            if (f != NULL)
            {
                fclose(f);
                strncpy(corpus, cand_paths[i], sizeof(corpus) - 1);
                corpus[sizeof(corpus) - 1] = '\0';
                break;
            }
        }
        if (corpus[0] == '\0')
        {
            fprintf(stderr, "no corpus found in data/texts/\n");
            return 1;
        }
    }
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