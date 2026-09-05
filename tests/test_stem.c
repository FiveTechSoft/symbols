#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symbol.h"
#include "stem.h"

static void Assert(int cond, const char *msg)
{
    if (!cond)
    {
        printf("FAIL: %s\n", msg);
        exit(EXIT_FAILURE);
    }
}

static void CheckStem(const char *word, const char *expected)
{
    char out[128];
    StemWord(word, out, sizeof(out));
    if (strcmp(out, expected) != 0)
    {
        printf("FAIL: StemWord(%s) = %s, esperaba %s\n", word, out, expected);
        exit(EXIT_FAILURE);
    }
    printf("  %-14s -> %s\n", word, out);
}

int main(void)
{
    printf("========================================\n");
    printf("     SYMBOLIC LLM - STEMMER ES          \n");
    printf("========================================\n\n");

    /* 1. Reduccion directa */
    printf("-- StemWord --\n");
    CheckStem("GATOS", "GATO");
    CheckStem("GATO", "GATO");
    CheckStem("CAPITALES", "CAPITAL");
    CheckStem("ARBOLES", "ARBOL");
    CheckStem("COMEN", "COM");
    CheckStem("HABLAN", "HABL");
    CheckStem("HABLAR", "HABL");
    CheckStem("COMIENDO", "COM");
    CheckStem("RAPIDAMENTE", "RAPIDA");
    CheckStem("CANCIONES", "CANCION");
    /* Short words and names: untouched */
    CheckStem("PARIS", "PARIS");
    CheckStem("SOL", "SOL");
    CheckStem("MAR", "MAR");
    CheckStem("LUZ", "LUZ");

    /* 2. Un paso */
    printf("-- StemStep --\n");
    {
        char out[128];
        Assert(StemStep("GATOS", out, sizeof(out)) == 1 &&
               strcmp(out, "GATO") == 0, "GATOS -S-> GATO");
        Assert(StemStep("GATO", out, sizeof(out)) == 0,
               "GATO ya es raiz");
        Assert(StemStep("SOL", out, sizeof(out)) == 0,
               "SOL no se toca (minimo 3)");
    }

    /* 3. Lookup fallback: queries only, never creates symbols */
    printf("-- StemFindSymbol --\n");
    SYMBOL_TABLE *t = SymbolTableCreate(64);
    SYMBOL_ID s_gato = SymbolAdd(t, "GATO");
    SYMBOL_ID s_come = SymbolAdd(t, "COME");
    SYMBOL_ID s_cap  = SymbolAdd(t, "CAPITAL");
    SYMBOL_ID s_arbol = SymbolAdd(t, "ARBOL");
    uint32_t before = SymbolCount(t);

    Assert(StemFindSymbol(t, "GATO") == s_gato, "exacta GATO");
    Assert(StemFindSymbol(t, "GATOS") == s_gato, "GATOS -> GATO");
    Assert(StemFindSymbol(t, "COMEN") == s_come, "COMEN -> COME (intermedia)");
    Assert(StemFindSymbol(t, "CAPITALES") == s_cap, "CAPITALES -> CAPITAL");
    Assert(StemFindSymbol(t, "ARBOLES") == s_arbol, "ARBOLES -> ARBOL");
    Assert(StemFindSymbol(t, "XYZQ") == SYMBOL_INVALID, "desconocida sigue INVALID");
    Assert(SymbolCount(t) == before, "el fallback no debe crear simbolos");

    SymbolTableDestroy(t);

    printf("\n========================================\n");
    printf("Stemmer verificado OK.\n");
    printf("========================================\n");

    return EXIT_SUCCESS;
}
