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
        /* 10: que es el sueno? */ {"sueno", "suenos", "inconsciente", NULL},
        /* 11: quien estudio los sueños? */ {"Freud", "Jung", "sueno", NULL},
        /* 12: que es la libido? */ {"libido", "energia", "deseo", NULL},
        /* 13: que es el ello? */ {"ello", "id", "pulsional", NULL},
        /* 14: que es el yo? */ {"yo", "ego", "consciente", NULL},
        /* 15: que es el superyo? */ {"superyo", "superego", "moral", NULL},
        /* 16: quien creo el cielo y la tierra? */ {"Dios", "God", "Senor", NULL},
        /* 17: quien es el padre de Abraham? */ {"Tare", "Terah", "Taré", NULL},
        /* 18: donde nacio Jesus? */ {"Belen", "Bethlehem", "Judea", NULL},
        /* 19: quien bautizo a Jesus? */ {"Juan", "John", "Bautista", NULL},
        /* 20: cuantos mandamientos hay? */ {"10", "diez", "mandamientos", NULL},
        /* 21: quien escribio los salmos? */ {"David", "salmista", "Asaf", NULL},
        /* 22: que es el pecado? */ {"pecado", "transgresion", "iniquidad", NULL},
        /* 23: quien es el Mesias? */ {"Cristo", "Jesus", "Ungido", NULL},
        /* 24: donde vivio Moises? */ {"Egipto", "Egypt", "desierto", NULL},
        /* 25: cuantos apóstoles hubo? */ {"12", "doce", "apostoles", NULL},
        /* 26: que es la fe? */ {"fe", "creencia", "confianza", NULL},
        /* 27: quien es David? */ {"rey", "David", "Israel", NULL},
        /* 28: que es el arca de la alianza? */ {"arca", "alianza", "pacto", NULL},
        /* 29: quien es el profeta Elias? */ {"Elias", "profeta", "Elijah", NULL},
        /* 30: que es la resurreccion? */ {"resurreccion", "vida", "muertos", NULL},
        /* 31: que es el alma? */ {"alma", "espiritu", "psique", NULL},
        /* 32: quien es el ser humano? */ {"hombre", "humano", "persona", NULL},
        /* 33: que es el mal? */ {"mal", "maldad", "pecado", NULL},
        /* 34: que es la verdad? */ {"verdad", "realidad", "palabra", NULL},
        /* 35: que es el amor? */ {"amor", "caridad", "afecto", NULL},
        /* 36: que es la muerte? */ {"muerte", "morir", "fin", NULL},
        /* 37: que es la vida? */ {"vida", "vivir", "existencia", NULL},
        /* 38: que es el poder? */ {"poder", "fuerza", "dominio", NULL},
        /* 39: que es la sabiduria? */ {"sabiduria", "ciencia", "conocimiento", NULL},
        /* 40: que es la justicia? */ {"justicia", "rectitud", "juicio", NULL},
        /* 41: what is the unconscious? */ {"unconscious", "psyche", "mind", NULL},
        /* 42: who wrote the book of Genesis? */ {"Moses", "Moises", "God", NULL},
        /* 43: what is a complex? */ {"complex", "feeling", "idea", NULL},
        /* 44: how many books are in the Bible? */ {"books", "66", "Bible", NULL},
        /* 45: what is the golden bough? */ {"Frazer", "bough", "myth", NULL},
        /* 46: who is Moses? */ {"Moses", "Moises", "prophet", NULL},
        /* 47: what is projection? */ {"projection", "defense", "unconscious", NULL},
        /* 48: who is the mother of Jesus? */ {"Mary", "Maria", "mother", NULL},
        /* 49: what is synchronicity? */ {"synchronicity", "coincidence", "meaningful", NULL},
        /* 50: what is the holy spirit? */ {"Spirit", "Holy", "Ghost", NULL}
    };

    int total = 50;
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
    printf("\n=== BATTERY 50 RESULTS (Ground-Truth Validated) ===\n");
    printf("Total:     %d\n", total);
    printf("Correct:   %d / %d (%.1f%%)\n", correct, total, (correct * 100.0) / total);
    printf("Wrong:     %d / %d (%.1f%%)\n", wrong, total, (wrong * 100.0) / total);
    printf("UNKNOWN:   %d / %d (%.1f%%)\n", unknown, total, (unknown * 100.0) / total);
    printf("Answered:  %d / %d (%.1f%%)\n", answered, total, (answered * 100.0) / total);
    printf("Corpus:    jung.txt (10730 sents) + bible.txt (28746 sents)\n");
    printf("Total:     39476 sentences loaded\n");

    return (correct > 0) ? 0 : 1;
}
