#include <string.h>
#include "stem.h"

/* Sufijos ordenados de mas largo a mas corto dentro de cada
   grupo: el primero que encaje gana (una capa por llamada).
   Minimo de raiz: 3 letras (evita destrozar MONOSILABOS como
   SOL, PAN, MAR o LUZ en el fallback). */
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

/* BFS over reductions: each level tries ALL applicable suffixes,
   not just the first. So COMEN reaches COME (via N) and COM (via EN),
   and HABLAN reaches HABL (via AN) and HABLA (via N). The longest
   form present in the graph wins. */
SYMBOL_ID StemFindSymbol(const SYMBOL_TABLE *table, const char *word)
{
    if (table == NULL || word == NULL || word[0] == '\0')
        return SYMBOL_INVALID;

    SYMBOL_ID id = SymbolFind(table, word);
    if (id != SYMBOL_INVALID)
        return id;

    /* Cola de fijos: nivel actual + siguiente (profundidad <= 4) */
    char cur[32][128];
    char nxt[32][128];
    uint32_t ncur = 1, nnxt = 0;

    strncpy(cur[0], word, sizeof(cur[0]) - 1);
    cur[0][sizeof(cur[0]) - 1] = '\0';

    for (int depth = 0; depth < 4 && ncur > 0; depth++)
    {
        nnxt = 0;
        for (uint32_t c = 0; c < ncur; c++)
        {
            size_t len = strlen(cur[c]);
            if (len <= STEM_MIN_ROOT)
                continue;

            for (int s = 0; SUFFIXES[s] != NULL; s++)
            {
                size_t slen = strlen(SUFFIXES[s]);
                if (len > slen && len - slen >= STEM_MIN_ROOT &&
                    strcmp(cur[c] + len - slen, SUFFIXES[s]) == 0)
                {
                    char cand[128];
                    memcpy(cand, cur[c], len - slen);
                    cand[len - slen] = '\0';

                    id = SymbolFind(table, cand);
                    if (id != SYMBOL_INVALID)
                        return id;

                    /* Enqueue if there is room (skips simple duplicates) */
                    if (nnxt < 32)
                    {
                        int dup = 0;
                        for (uint32_t k = 0; k < nnxt; k++)
                            if (strcmp(nxt[k], cand) == 0) { dup = 1; break; }
                        if (!dup)
                        {
                            strncpy(nxt[nnxt], cand, sizeof(nxt[nnxt]) - 1);
                            nxt[nnxt][sizeof(nxt[nnxt]) - 1] = '\0';
                            nnxt++;
                        }
                    }
                }
            }
        }

        ncur = nnxt;
        for (uint32_t c = 0; c < ncur; c++)
        {
            strncpy(cur[c], nxt[c], sizeof(cur[c]) - 1);
            cur[c][sizeof(cur[c]) - 1] = '\0';
        }
    }

    return SYMBOL_INVALID;
}
