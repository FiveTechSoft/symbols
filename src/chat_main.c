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
    corpus[0] = '\0';
    PERSONA_ID init_persona = PERSONA_NEUTRAL;

    for (int i = 1; i < argc; i++)
    {
        if ((strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--persona") == 0) && i + 1 < argc)
        {
            init_persona = PersonaFindByName(argv[++i]);
            continue;
        }
        if (corpus[0] != '\0')
            strncat(corpus, ";", sizeof(corpus) - strlen(corpus) - 1);
        strncat(corpus, argv[i], sizeof(corpus) - strlen(corpus) - 1);
    }
    if (corpus[0] == '\0')
    {
        static const char *cand_paths[] = {
            "data/c_lang/c_corpus.txt",
            "data/texts/c_corpus.txt",
            "data/texts/corpus.txt",
            "data/corpus.txt",
            "data/texts/bible.txt",
            "data/texts/jung.txt"
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
    if (init_persona != PERSONA_NEUTRAL)
    {
        ChatSetPersona(&chat.ch, init_persona);
        if (init_persona == PERSONA_PIRATE_QUANTUM)
            printf("Ahoy! El contramaestre cuantico del siglo XVIII esta al timon.\n");
        else
            printf("Modo persona activo: %s\n", PersonaGetName(init_persona));
    }
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
    ChatDestroy(&chat.ch);
    return 0;
}