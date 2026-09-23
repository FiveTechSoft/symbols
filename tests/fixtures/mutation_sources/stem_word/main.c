/* Source: src/stem.c (StemStep, StemWord), verbatim.
   Oracle: the cases of tests/test_stem.c. */
#include <stdint.h>
#include <string.h>

static const char *SUFFIXES[] = {
    /* adverbios (los plurales en -CIONES se resuelven via ES + E:
       CANCIONES -> CANCIONE -> CANCION; una regla CIONES directa
       sobre-reduciria a CAN) */
    "MENTE",    /* RAPIDAMENTE -> RAPIDA */
    /* gerundios y participios */
    "IENDO",    /* COMIENDO -> COM */
    "ANDO",     /* HABLANDO -> HABL */
    "ENDO",     /* CORRIENDO -> CORR */
    "ADO",      /* HABLADO -> HABL */
    "IDO",      /* COMIDO -> COM */
    /* imperfecto / personas verbales largas */
    "ABAN",     /* HABLABAN -> HABL */
    "ABA",      /* HABLABA -> HABL */
    "IAN",      /* COMIAN -> COM */
    "AMOS",     /* HABLAMOS -> HABL */
    "EMOS",     /* COMEMOS -> COM */
    "IMOS",     /* VIVIMOS -> VIV */
    /* infinitivos -> raiz */
    "AR",       /* HABLAR -> HABL */
    "ER",       /* COMER -> COM */
    "IR",       /* VIVIR -> VIV */
    /* short verb persons and plurals */
    "AN",       /* HABLAN -> HABL */
    "EN",       /* COMEN -> COM */
    "AS",       /* HABLAS -> HABL */
    "ES",       /* ARBOLES -> ARBOL, CAPITALES -> CAPITAL */
    /* una letra: COMEN -> COME, GATOS -> GATO, COME -> COM.
       StemFindSymbol los prueba TODOS (BFS), asi que el orden
       aqui solo afecta a StemWord/StemStep (gana el mas largo). */
    "N",
    "S",
    "E",
    NULL
};

#define STEM_MIN_ROOT 3

int StemStep(const char *word, char *out, uint32_t out_size)
{
    if (word == NULL || out == NULL || out_size == 0)
        return 0;

    size_t len = strlen(word);
    if (len >= out_size)
        len = out_size - 1;

    /* Short words: already roots */
    if (len <= STEM_MIN_ROOT)
    {
        memcpy(out, word, len);
        out[len] = '\0';
        return 0;
    }

    for (int i = 0; SUFFIXES[i] != NULL; i++)
    {
        size_t slen = strlen(SUFFIXES[i]);
        if (len > slen && len - slen >= STEM_MIN_ROOT &&
            strcmp(word + len - slen, SUFFIXES[i]) == 0)
        {
            /* Guarda -CION: CANCION/AVION/CAMION son raices, no
               plurales verbales (evita CANCION -> CANCIO). */
            if (slen == 1 && SUFFIXES[i][0] == 'N' && len >= 4 &&
                strcmp(word + len - 3, "ION") == 0)
                continue;
            /* Guarda -IS: PARIS/CRISIS/TESIS no son plurales
               (evita PARIS -> PARI). El plural real en -S sigue
               vocal (GATO-S, CASA-S, BEBE-S). */
            if (slen == 1 && SUFFIXES[i][0] == 'S' &&
                strchr("AOE", word[len - 2]) == NULL)
                continue;
            size_t root = len - slen;
            memcpy(out, word, root);
            out[root] = '\0';
            return 1;
        }
    }

    memcpy(out, word, len);
    out[len] = '\0';
    return 0;
}

void StemWord(const char *word, char *out, uint32_t out_size)
{
    if (word == NULL || out == NULL || out_size == 0)
        return;

    char buf[128];
    strncpy(buf, word, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char next[128];
    /* Cascada acotada: 6 pasos bastan (MENTE + persona + numero) */
    for (int i = 0; i < 6; i++)
    {
        if (!StemStep(buf, next, sizeof(next)))
            break;
        strncpy(buf, next, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
    }

    strncpy(out, buf, out_size - 1);
    out[out_size - 1] = '\0';
}

int main(void)
{
    char out[128];
    int bad = 0;
    StemWord("GATOS", out, sizeof(out));
    bad += strcmp(out, "GATO") != 0;
    StemWord("GATO", out, sizeof(out));
    bad += strcmp(out, "GATO") != 0;
    StemWord("CAPITALES", out, sizeof(out));
    bad += strcmp(out, "CAPITAL") != 0;
    StemWord("ARBOLES", out, sizeof(out));
    bad += strcmp(out, "ARBOL") != 0;
    StemWord("COMEN", out, sizeof(out));
    bad += strcmp(out, "COM") != 0;
    StemWord("HABLAN", out, sizeof(out));
    bad += strcmp(out, "HABL") != 0;
    StemWord("HABLAR", out, sizeof(out));
    bad += strcmp(out, "HABL") != 0;
    StemWord("COMIENDO", out, sizeof(out));
    bad += strcmp(out, "COM") != 0;
    StemWord("RAPIDAMENTE", out, sizeof(out));
    bad += strcmp(out, "RAPIDA") != 0;
    StemWord("CANCIONES", out, sizeof(out));
    bad += strcmp(out, "CANCION") != 0;
    StemWord("PARIS", out, sizeof(out));
    bad += strcmp(out, "PARIS") != 0;
    StemWord("SOL", out, sizeof(out));
    bad += strcmp(out, "SOL") != 0;
    StemWord("MAR", out, sizeof(out));
    bad += strcmp(out, "MAR") != 0;
    StemWord("LUZ", out, sizeof(out));
    bad += strcmp(out, "LUZ") != 0;
    bad += StemStep("GATOS", out, sizeof(out)) != 1 || strcmp(out, "GATO") != 0;
    bad += StemStep("GATO", out, sizeof(out)) != 0;
    bad += StemStep("SOL", out, sizeof(out)) != 0;
    return bad == 0 ? 0 : 1;
}
