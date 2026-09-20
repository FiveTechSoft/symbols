#include <stdio.h>
#include <string.h>
#include "chat.h"

static int contains_ignore_case(const char *haystack, const char *needle)
{
    if (!haystack || !needle || !*needle)
        return 0;
    size_t nlen = strlen(needle);
    size_t hlen = strlen(haystack);
    if (nlen > hlen)
        return 0;
    for (size_t i = 0; i <= hlen - nlen; i++)
    {
        size_t j;
        for (j = 0; j < nlen; j++)
        {
            char h = haystack[i + j];
            char n = needle[j];
            if (h >= 'A' && h <= 'Z') h += 32;
            if (n >= 'A' && n <= 'Z') n += 32;
            if (h != n) break;
        }
        if (j == nlen)
            return 1;
    }
    return 0;
}

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

    /* Expected ground-truth keywords/entities for validation */
    const char *expected_keywords[][4] = {
        /* 1: quien es Jung? */ {"Jung", "psiquiatra", "psicologo", NULL},
        /* 2: que es el inconsciente? */ {"inconsciente", "mente", "psique", NULL},
        /* 3: que es el arquetipo? */ {"arquetipo", "imagen", "primordial", NULL},
        /* 4: que es la sombra? */ {"sombra", "inconsciente", "aspecto", NULL},
        /* 5: que es la anima? */ {"anima", "femenin", "arquetipo", NULL},
        /* 6: que es el self? */ {"self", "si-mismo", "personalidad", NULL},
        /* 7: que es la individuacion? */ {"individuacion", "proceso", "integracion", NULL},
        /* 8: que es la proyeccion? */ {"proyeccion", "objeto", "inconsciente", NULL},
        /* 9: que es el complejo? */ {"complejo", "afecto", "constelacion", NULL},
        /* 10: que es la libido? */ {"libido", "energia", "deseo", NULL},
        /* 11: quien creo el cielo y la tierra? */ {"Dios", "God", "Senor", NULL},
        /* 12: quien es el padre de Abraham? */ {"Tare", "Terah", "Taré", NULL},
        /* 13: donde nacio Jesus? */ {"Belen", "Bethlehem", "Judea", NULL},
        /* 14: quien bautizo a Jesus? */ {"Juan", "John", "Bautista", NULL},
        /* 15: cuantos mandamientos hay? */ {"10", "diez", "mandamientos", NULL},
        /* 16: quien escribio los salmos? */ {"David", "salmista", "Asaf", NULL},
        /* 17: que es el pecado? */ {"pecado", "transgresion", "iniquidad", NULL},
        /* 18: quien es el Mesias? */ {"Cristo", "Jesus", "Ungido", NULL},
        /* 19: donde vivio Moises? */ {"Egipto", "Egypt", "desierto", NULL},
        /* 20: cuantos apostoles hubo? */ {"12", "doce", "apostoles", NULL},
        /* 21: what is fluid mechanics? */ {"fluid", "mechanics", "flow", NULL},
        /* 22: what is pipe flow? */ {"conduit", "pipe", "flow", NULL},
        /* 23: what is the Clean Water Act? */ {"water", "pollution", "act", NULL},
        /* 24: what is the National Hockey League? */ {"hockey", "league", "nhl", NULL},
        /* 25: what is Formula One? */ {"racing", "f1", "grand prix", NULL},
        /* 26: what is parimutuel betting? */ {"betting", "gambling", "horse", NULL},
        /* 27: what is wood carving? */ {"wood", "carving", "sculpture", NULL},
        /* 28: what is the Gothic Revival? */ {"gothic", "revival", "architectur", NULL},
        /* 29: what is the Wehrmacht? */ {"wehrmacht", "military", "german", NULL},
        /* 30: what is the German Resistance? */ {"resistance", "nazi", "hitler", NULL},
        /* 31: where is Berlin? */ {"germany", "german", "tiergarten", NULL},
        /* 32: where is Texas? */ {"texas", "states", "america", NULL},
        /* 33: where is Devon? */ {"devon", "england", "uk", NULL},
        /* 34: where is the Trinity River? */ {"trinity", "texas", "river", NULL},
        /* 35: where is Willow Park? */ {"park", "texas", "parker", NULL},
        /* 36: where is Tiergarten? */ {"tiergarten", "berlin", "germany", NULL},
        /* 37: where is Cornwall? */ {"cornwall", "england", "south-west", NULL},
        /* 38: where is Parker County? */ {"parker", "texas", "county", NULL},
        /* 39: where is Stauffenbergstrasse? */ {"stauffenberg", "berlin", "bendlerblock", NULL},
        /* 40: where is the South-West of England? */ {"england", "devon", "cornwall", NULL},
        /* 41: who is Afanasenkov? */ {"afanasenkov", "hockey", "player", NULL},
        /* 42: who are the Pinwill sisters? */ {"pinwill", "wood", "carving", NULL},
        /* 43: who is Adolf Hitler? */ {"hitler", "nazi", "dictator", NULL},
        /* 44: who is Mary Pinwill? */ {"pinwill", "mary", "sister", NULL},
        /* 45: who is Ethel Pinwill? */ {"pinwill", "ethel", "sister", NULL},
        /* 46: who is Violet Pinwill? */ {"pinwill", "violet", "sister", NULL},
        /* 47: who played for the Tampa Bay Lightning? */ {"afanasenkov", "hockey", "player", NULL},
        /* 48: who played for the Philadelphia Flyers? */ {"afanasenkov", "hockey", "player", NULL},
        /* 49: who is Sandra Baumgartner? */ {"baumgartner", "sky", "journalist", NULL},
        /* 50: who is Amodou Abdullei? */ {"abdullei", "football", "player", NULL},
        /* 51: what was World War I? */ {"war", "1914", "world", NULL},
        /* 52: what was World War II? */ {"war", "1939", "world", NULL},
        /* 53: what was the 20 July plot? */ {"assassination", "hitler", "plot", NULL},
        /* 54: what was the Oberkommando der Wehrmacht? */ {"wehrmacht", "okw", "command", NULL},
        /* 55: what was the Abwehr? */ {"abwehr", "intelligence", "military", NULL},
        /* 56: what was the Imperial German Navy? */ {"navy", "kaiserliche", "marine", NULL},
        /* 57: what was the Reichswehr? */ {"reichswehr", "weimar", "military", NULL},
        /* 58: what was the Federal Ministry of Defence? */ {"ministry", "defence", "defense", NULL},
        /* 59: what was Clear Fork Downs? */ {"track", "racing", "horse", NULL},
        /* 60: what was Squaw Creek Downs? */ {"track", "racing", "horse", NULL},
        /* 61: que es el alma? */ {"alma", "espiritu", "psique", NULL},
        /* 62: quien es el ser humano? */ {"hombre", "humano", "persona", NULL},
        /* 63: que es el mal? */ {"mal", "maldad", "pecado", NULL},
        /* 64: que es la verdad? */ {"verdad", "realidad", "palabra", NULL},
        /* 65: que es el amor? */ {"amor", "caridad", "afecto", NULL},
        /* 66: que es la muerte? */ {"muerte", "morir", "fin", NULL},
        /* 67: que es la vida? */ {"vida", "vivir", "existencia", NULL},
        /* 68: que es la fe? */ {"fe", "creencia", "confianza", NULL},
        /* 69: que es la justicia? */ {"justicia", "rectitud", "juicio", NULL},
        /* 70: que es la sabiduria? */ {"sabiduria", "ciencia", "conocimiento", NULL},
        /* 71: what is the unconscious? */ {"unconscious", "psyche", "mind", NULL},
        /* 72: who wrote the book of Genesis? */ {"moses", "moises", "god", NULL},
        /* 73: what is a complex? */ {"complex", "feeling", "idea", NULL},
        /* 74: what is the golden bough? */ {"frazer", "bough", "myth", NULL},
        /* 75: who is Moses? */ {"moses", "moises", "prophet", NULL},
        /* 76: what is projection? */ {"projection", "defense", "unconscious", NULL},
        /* 77: who is the mother of Jesus? */ {"mary", "maria", "mother", NULL},
        /* 78: what is synchronicity? */ {"synchronicity", "coincidence", "meaningful", NULL},
        /* 79: what is the holy spirit? */ {"spirit", "holy", "ghost", NULL},
        /* 80: what is the Bible? */ {"bible", "scriptures", "testament", NULL},
        /* 81: how many sisters did the Pinwill family have? */ {"sisters", "mary", "pinwill", NULL},
        /* 82: what sport did Afanasenkov play? */ {"hockey", "ice", "nhl", NULL},
        /* 83: what year was the Bendlerblock erected? */ {"1914", "1911", "erected", NULL},
        /* 84: what happened on August 6 1996? */ {"trinity", "closed", "meadows", NULL},
        /* 85: what was Trinity Meadows renamed to? */ {"trinity", "meadows", "track", NULL},
        /* 86: what channel did Baumgartner work for? */ {"sky", "sport", "channel", NULL},
        /* 87: what type of flow is pipe flow? */ {"conduit", "closed", "pipe", NULL},
        /* 88: what court case involved Trinity Meadows? */ {"bankruptcy", "court", "case", NULL},
        /* 89: what happened in 1997 to Trinity Meadows? */ {"1997", "sold", "auction", NULL},
        /* 90: what is the Memorial to the German Resistance? */ {"memorial", "resistance", "bendlerblock", NULL},
        /* 91: what is the relationship between pipe flow and open channel flow? */ {"closed", "conduit", "pressure", NULL},
        /* 92: how did the Pinwill sisters adapt their style over time? */ {"carving", "wood", "style", NULL},
        /* 93: what role did the Bendlerblock play in the German Resistance? */ {"stauffenberg", "plot", "resistance", NULL},
        /* 94: how did the Clean Water Act affect Trinity Meadows? */ {"water", "discharge", "violation", NULL},
        /* 95: what was the significance of the 20 July plot? */ {"hitler", "assassinate", "coup", NULL},
        /* 96: how did World War I affect the Bendlerblock? */ {"naval", "war", "staff", NULL},
        /* 97: what was the impact of the Pinwill workshop on Devon churches? */ {"churches", "devon", "pinwill", NULL},
        /* 98: how did parimutuel betting affect horse racing in Texas? */ {"betting", "racing", "horse", NULL},
        /* 99: what was the role of the Abwehr in the German military? */ {"intelligence", "military", "espionage", NULL},
        /* 100: how did the German Federal Ministry of Defence use the Bendlerblock? */ {"defence", "defense", "office", NULL}
    };

    int total = 100;
    int correct = 0;
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

        /* Ground-truth classification */
        if (!r || strstr(out, "UNKNOWN") || strstr(out, "No tengo"))
        {
            unknown++;
        }
        else
        {
            int matched = 0;
            for (int k = 0; k < 3 && expected_keywords[i][k] != NULL; k++)
            {
                if (contains_ignore_case(out, expected_keywords[i][k]))
                {
                    matched = 1;
                    break;
                }
            }
            if (matched)
                correct++;
            else
                wrong++;
        }
    }

    int answered = correct + wrong;
    printf("\n=== BATTERY 100 RESULTS (Ground-Truth Validated) ===\n");
    printf("Total:     %d\n", total);
    printf("Correct:   %d / %d (%.1f%%)\n", correct, total, (correct * 100.0) / total);
    printf("Wrong:     %d / %d (%.1f%%)\n", wrong, total, (wrong * 100.0) / total);
    printf("UNKNOWN:   %d / %d (%.1f%%)\n", unknown, total, (unknown * 100.0) / total);
    printf("Answered:  %d / %d (%.1f%%)\n", answered, total, (answered * 100.0) / total);
    printf("Corpus:    jung.txt + bible.txt + wiki_sample.txt\n");

    return (correct > 0) ? 0 : 1;
}
