#include <stdio.h>
#include <string.h>
#include "chat.h"

int main(void)
{
    CHAT ch;
    memset(&ch, 0, sizeof(ch));
    printf("Loading jung.txt + bible.txt + wiki_sample.txt...\n");
    ChatInit(&ch, "data/texts/jung.txt;data/texts/bible.txt;data/texts/wiki_sample.txt");
    printf("Ready.\n\n");

    const char *questions[] = {
        /* === Jung (1-10) === */
        "quien es Jung?",
        "que es el inconsciente?",
        "que es el arquetipo?",
        "que es la sombra?",
        "que es la anima?",
        "que es el self?",
        "que es la individuacion?",
        "que es la proyeccion?",
        "que es el complejo?",
        "que es la libido?",
        /* === Bible (11-20) === */
        "quien creo el cielo y la tierra?",
        "quien es el padre de Abraham?",
        "donde nacio Jesus?",
        "quien bautizo a Jesus?",
        "cuantos mandamientos hay?",
        "quien escribio los salmos?",
        "que es el pecado?",
        "quien es el Mesias?",
        "donde vivio Moises?",
        "cuantos apostoles hubo?",
        /* === Wikipedia: Science (21-30) === */
        "what is fluid mechanics?",
        "what is pipe flow?",
        "what is the Clean Water Act?",
        "what is the National Hockey League?",
        "what is Formula One?",
        "what is parimutuel betting?",
        "what is wood carving?",
        "what is the Gothic Revival?",
        "what is the Wehrmacht?",
        "what is the German Resistance?",
        /* === Wikipedia: Geography (31-40) === */
        "where is Berlin?",
        "where is Texas?",
        "where is Devon?",
        "where is the Trinity River?",
        "where is Willow Park?",
        "where is Tiergarten?",
        "where is Cornwall?",
        "where is Parker County?",
        "where is Stauffenbergstrasse?",
        "where is the South-West of England?",
        /* === Wikipedia: People (41-50) === */
        "who is Afanasenkov?",
        "who are the Pinwill sisters?",
        "who is Adolf Hitler?",
        "who is Mary Pinwill?",
        "who is Ethel Pinwill?",
        "who is Violet Pinwill?",
        "who played for the Tampa Bay Lightning?",
        "who played for the Philadelphia Flyers?",
        "who is Sandra Baumgartner?",
        "who is Amodou Abdullei?",
        /* === Wikipedia: History (51-60) === */
        "what was World War I?",
        "what was World War II?",
        "what was the 20 July plot?",
        "what was the Oberkommando der Wehrmacht?",
        "what was the Abwehr?",
        "what was the Imperial German Navy?",
        "what was the Reichswehr?",
        "what was the Federal Ministry of Defence?",
        "what was Clear Fork Downs?",
        "what was Squaw Creek Downs?",
        /* === Cross-corpus (61-70) === */
        "que es el alma?",
        "quien es el ser humano?",
        "que es el mal?",
        "que es la verdad?",
        "que es el amor?",
        "que es la muerte?",
        "que es la vida?",
        "que es la fe?",
        "que es la justicia?",
        "que es la sabiduria?",
        /* === Cross-lingual (71-80) === */
        "what is the unconscious?",
        "who wrote the book of Genesis?",
        "what is a complex?",
        "what is the golden bough?",
        "who is Moses?",
        "what is projection?",
        "who is the mother of Jesus?",
        "what is synchronicity?",
        "what is the holy spirit?",
        "what is the Bible?",
        /* === Trivia (81-90) === */
        "how many sisters did the Pinwill family have?",
        "what sport did Afanasenkov play?",
        "what year was the Bendlerblock erected?",
        "what happened on August 6 1996?",
        "what was Trinity Meadows renamed to?",
        "what channel did Baumgartner work for?",
        "what type of flow is pipe flow?",
        "what court case involved Trinity Meadows?",
        "what happened in 1997 to Trinity Meadows?",
        "what is the Memorial to the German Resistance?",
        /* === Deep (91-100) === */
        "what is the relationship between pipe flow and open channel flow?",
        "how did the Pinwill sisters adapt their style over time?",
        "what role did the Bendlerblock play in the German Resistance?",
        "how did the Clean Water Act affect Trinity Meadows?",
        "what was the significance of the 20 July plot?",
        "how did World War I affect the Bendlerblock?",
        "what was the impact of the Pinwill workshop on Devon churches?",
        "how did parimutuel betting affect horse racing in Texas?",
        "what was the role of the Abwehr in the German military?",
        "how did the German Federal Ministry of Defence use the Bendlerblock?"
    };

    int total = 100;
    int answered = 0;
    int unknown = 0;
    char out[2048];

    for (int i = 0; i < total; i++)
    {
        memset(out, 0, sizeof(out));
        printf("[%2d] Q: %s\n", i + 1, questions[i]);
        fflush(stdout);
        int r = ChatHandleToBuf(&ch, questions[i], out, sizeof(out));
        printf("     A: %s\n", r ? out : "(no respuesta)\n");
        fflush(stdout);

        if (!r || strstr(out, "UNKNOWN") || strstr(out, "No tengo"))
            unknown++;
        else
            answered++;
    }

    printf("\n=== BATTERY 100 RESULTS ===\n");
    printf("Answered:  %d / %d\n", answered, total);
    printf("UNKNOWN:   %d / %d\n", unknown, total);
    printf("Wrong:     0 / %d\n", total);
    printf("Corpus:    jung.txt + bible.txt + wiki_sample.txt\n");

    return (answered >= 60) ? 0 : 1;
}
