#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "sudoku.h"

static void Assert(int cond, const char *msg)
{
    if (!cond)
    {
        printf("FAIL: %s\n", msg);
        exit(EXIT_FAILURE);
    }
}

/* Ejemplo canonico (Norvig): solucion exacta conocida */
static const char *EASY =
    "530070000600195000098000060800060003400803001700020006060000280000419005000080079";
static const char *EASY_SOL =
    "534678912672195348198342567859761423426853791713924856961537284287419635345286179";

/* AI Escargot (dificil): se verifica validez, no solucion exacta */
static const char *HARD =
    "1....7.9..3..2...8..96..5....53..9...1..8...26....4...3......1..4......7..7...3..";

static int BoardValid(const SUDOKU *b)
{
    char s[82];
    SudokuToString(b, s);
    for (int u = 0; u < 9; u++)
    {
        int seenR[10] = {0}, seenC[10] = {0}, seenB[10] = {0};
        for (int k = 0; k < 9; k++)
        {
            int r = s[u * 9 + k] - '0';
            int c = s[k * 9 + u] - '0';
            int br = (u / 3) * 3 + k / 3, bc = (u % 3) * 3 + k % 3;
            int v = s[br * 9 + bc] - '0';
            if (r < 1 || r > 9 || seenR[r]) return 0; seenR[r] = 1;
            if (c < 1 || c > 9 || seenC[c]) return 0; seenC[c] = 1;
            if (v < 1 || v > 9 || seenB[v]) return 0; seenB[v] = 1;
        }
    }
    return 1;
}

static int GivensKept(const char *givens, const SUDOKU *b)
{
    char s[82];
    SudokuToString(b, s);
    for (int i = 0; i < 81; i++)
        if (givens[i] >= '1' && givens[i] <= '9' && s[i] != givens[i])
            return 0;
    return 1;
}

int main(void)
{
    printf("========================================\n");
    printf("        SYMBOLIC LLM - SUDOKU           \n");
    printf("========================================\n\n");

    /* 1. Parse: formato */
    SUDOKU b;
    Assert(SudokuParse(EASY, &b) == 1, "parse facil");
    Assert(SudokuGivens(&b) == 30, "30 pistas en el facil");
    SUDOKU bad;
    Assert(SudokuParse("123", &bad) == 0, "corta rechazada");
    Assert(SudokuParse("53007000060019500009800006080006000340080300170002000606000028000041900500008007X",
                       &bad) == 0, "caracter malo rechazado");

    /* 2. Facil: solucion exacta */
    {
        SUDOKU t = b;
        uint64_t nodes = 0;
        Assert(SudokuSolve(&t, &nodes) == 1, "facil resuelto");
        char s[82];
        SudokuToString(&t, s);
        Assert(strcmp(s, EASY_SOL) == 0, "solucion exacta del facil");
        printf("  facil: nodos=%llu dificultad=%s\n",
               (unsigned long long)nodes, SudokuDifficulty(nodes, 30));
        Assert(SudokuCount(&b, 2, NULL) == 1, "facil tiene solucion unica");
    }

    /* 3. Dificil (Escargot): resuelve, valido, rapido */
    {
        SUDOKU t;
        Assert(SudokuParse(HARD, &t) == 1, "parse Escargot (81 chars)");
        int givens = SudokuGivens(&t);
        clock_t t0 = clock();
        uint64_t nodes = 0;
        Assert(SudokuSolve(&t, &nodes) == 1, "Escargot resuelto");
        double secs = (double)(clock() - t0) / CLOCKS_PER_SEC;
        Assert(BoardValid(&t), "Escargot valido (filas/cols/cajas)");
        Assert(GivensKept(HARD, &t), "Escargot respeta pistas");
        printf("  escargot: pistas=%d nodos=%llu %.2fs dificultad=%s\n",
               givens, (unsigned long long)nodes, secs,
               SudokuDifficulty(nodes, givens));
        Assert(secs < 5.0, "Escargot en menos de 5s");
    }

    /* 4. Sin solucion: duplicado en fila */
    {
        char dup[82];
        strcpy(dup, EASY);
        dup[1] = dup[0]; /* dos '5' en la primera fila */
        SUDOKU t;
        Assert(SudokuParse(dup, &t) == 1, "parse duplicado");
        Assert(SudokuSolve(&t, NULL) == 0, "duplicado sin solucion");
        Assert(SudokuCount(&t, 2, NULL) == 0, "conteo 0 del duplicado");
    }

    /* 5. Tablero vacio: al menos 2 soluciones (rapido con limite) */
    {
        char empty[82];
        memset(empty, '0', 81);
        empty[81] = '\0';
        SUDOKU t;
        Assert(SudokuParse(empty, &t) == 1, "parse vacio");
        Assert(SudokuCount(&t, 2, NULL) == 2, "vacio tiene >=2 soluciones");
    }

    /* 6. Traza: 81 fijaciones (30 pistas + 51 deducciones), cada
       celda exactamente una vez, digitos = solucion */
    {
        SUDOKU t;
        Assert(SudokuParse(EASY, &t) == 1, "parse para traza");
        SUDOKU_TRACE trace;
        uint64_t nodes = 0;
        Assert(SudokuSolveSteps(&t, &nodes, &trace) == 1, "traza resuelta");
        Assert(trace.n == 81, "traza cubre las 81 celdas");
        int seen[81] = {0};
        int n_given = 0, n_deduced = 0;
        char s[82];
        SudokuToString(&t, s);
        for (int k = 0; k < trace.n; k++)
        {
            int cell = trace.steps[k].cell;
            int dig = trace.steps[k].digit;
            Assert(cell >= 0 && cell < 81, "celda de traza valida");
            Assert(!seen[cell], "celda fijada una sola vez");
            seen[cell] = 1;
            Assert(s[cell] - '1' == dig, "digito de traza = solucion");
            if (trace.steps[k].tech == SSTEP_GIVEN)
                n_given++;
            else
                n_deduced++;
        }
        printf("  traza: %d pasos (%d pistas + %d deducciones)\n",
               trace.n, n_given, n_deduced);
        Assert(n_given == 30, "30 pistas en traza");
        Assert(n_deduced == 51, "51 deducciones en traza");
    }

    /* 7. Escargot exige busqueda real: hay ramas en la traza */
    {
        SUDOKU t;
        Assert(SudokuParse(HARD, &t) == 1, "parse Escargot traza");
        SUDOKU_TRACE trace;
        uint64_t nodes = 0;
        Assert(SudokuSolveSteps(&t, &nodes, &trace) == 1, "Escargot traza");
        int nguess = 0;
        for (int k = 0; k < trace.n; k++)
            if (trace.steps[k].tech == SSTEP_GUESS)
                nguess++;
        printf("  escargot: %d pasos, %d ramas de busqueda\n", trace.n, nguess);
        Assert(trace.n == 81, "traza Escargot cubre 81 celdas");
        Assert(nguess > 0, "Escargot necesita busqueda (ramas > 0)");
    }

    /* 8. Calibracion: percentiles del historial, base si < 3 casos */
    {
        Assert(strcmp(SudokuDifficultyCalibrated(0, NULL, 0),
                      SudokuDifficulty(0, 30)) == 0,
               "sin historial = base");
        uint64_t h2[] = {0, 15};
        Assert(strcmp(SudokuDifficultyCalibrated(999, h2, 2),
                      SudokuDifficulty(999, 30)) == 0,
               "2 casos = base");
        /* hist ordenado: 0,0,0,0,15,20 -> p33=s[2]=0, p66=s[4]=15 */
        uint64_t h6[] = {15, 0, 20, 0, 0, 0};
        Assert(strstr(SudokuDifficultyCalibrated(0, h6, 6), "FACIL") != NULL,
               "0 nodos = FACIL del historial");
        Assert(strstr(SudokuDifficultyCalibrated(15, h6, 6), "MEDIA") != NULL,
               "15 nodos = MEDIA del historial");
        Assert(strstr(SudokuDifficultyCalibrated(16, h6, 6), "DIFICIL") != NULL,
               "16 nodos = DIFICIL del historial");
        printf("  calibracion: FACIL/MEDIA/DIFICIL por percentiles OK\n");
    }

    printf("\n========================================\n");
    printf("Sudoku verificado OK.\n");
    printf("========================================\n");
    return EXIT_SUCCESS;
}
