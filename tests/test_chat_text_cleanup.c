/* A CHAT owns its embedded TEXTLEX images and sentence arrays. Repeated
   create/destroy cycles must release them, including sparse text-file slots. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "chat.h"

static void Check(int yes, const char *message)
{
    if (!yes) { fprintf(stderr, "FAIL: %s\n", message); exit(1); }
}

int main(void)
{
    const char *path = "test_chat_text_cleanup_corpus.txt";
    FILE *f = fopen(path, "wb");
    Check(f != NULL, "create text fixture");
    fputs("The copper river crosses the quiet valley.\n"
          "Water passes through the old stone channel.\n", f);
    Check(fclose(f) == 0, "close text fixture");

    for (unsigned i = 0; i < 8; i++)
    {
        CHAT *ch = (CHAT *)malloc(sizeof(*ch));
        Check(ch != NULL, "allocate chat");
        ChatInit(ch, path);
        ch->episodic.auto_save = 0;
        Check(ch->ntfiles == 1 && ch->tlex[0].image != NULL &&
              ch->tlex[0].nsent > 0, "text ingestion owns image and sentences");
        /* Simulate a slot populated out of order. Cleanup cannot rely on
           ntfiles if a partial load or previous unload left a sparse slot. */
        if (i == 0)
        {
            ch->tlex[CHAT_TEXT_FILES_MAX - 1].image = malloc(3);
            Check(ch->tlex[CHAT_TEXT_FILES_MAX - 1].image != NULL,
                  "sparse slot allocation");
            ch->tlex[CHAT_TEXT_FILES_MAX - 1].imagelen = 3;
        }
        ChatDestroy(ch);
        Check(ch->tlex[0].image == NULL && ch->tlex[0].sents == NULL,
              "primary slot cleared");
        Check(ch->tlex[CHAT_TEXT_FILES_MAX - 1].image == NULL,
              "sparse slot cleared");
        ChatDestroy(ch); /* second call is safe after nulling owned pointers */
        free(ch);
    }
    remove(path);
    puts("embedded text cleanup on repeated sessions OK");
    return 0;
}
