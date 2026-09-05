#include <stdlib.h>
#include <string.h>
#include "sudoku.h"

/* Unidades (27x9) y pares (81x20) precalculados una vez. */
static int UNITS[27][9];
static int PEERS[81][20];
static int TABLES_READY = 0;

static void BuildTables(void)
{
    if (TABLES_READY)
        return;
    int u = 0;
    for (int r = 0; r < 9; r++, u++)
        for (int c = 0; c < 9; c++)
            UNITS[u][c] = r * 9 + c;
    for (int c = 0; c < 9; c++, u++)
        for (int r = 0; r < 9; r++)
            UNITS[u][r] = r * 9 + c;
    for (int br = 0; br < 3; br++)
        for (int bc = 0; bc < 3; bc++, u++)
        {
            int k = 0;
            for (int r = 0; r < 3; r++)
                for (int c = 0; c < 3; c++)
                    UNITS[u][k++] = (br * 3 + r) * 9 + (bc * 3 + c);
        }
    for (int i = 0; i < 81; i++)
    {
        int seen[81] = {0};
        int n = 0;
        seen[i] = 1;
        for (int ui = 0; ui < 27; ui++)
        {
            int has = 0;
            for (int k = 0; k < 9; k++)
                if (UNITS[ui][k] == i) { has = 1; break; }
            if (!has)
                continue;
            for (int k = 0; k < 9; k++)
            {
                int cell = UNITS[ui][k];
                if (!seen[cell]) { seen[cell] = 1; PEERS[i][n++] = cell; }
            }
        }
    }
    TABLES_READY = 1;
}

static int PopCount(uint16_t m)
{
    int n = 0;
    while (m) { n += m & 1u; m >>= 1; }
    return n;
}

static int SingleDigit(uint16_t m)
{
    for (int d = 0; d < 9; d++)
        if (m == (uint16_t)(1u << d))
            return d;
    return -1;
}

int SudokuParse(const char *str81, SUDOKU *out)
{
    if (!str81 || !out || strlen(str81) < 81)
        return 0;
    BuildTables();
    for (int i = 0; i < 81; i++)
    {
        char ch = str81[i];
        if (ch >= '1' && ch <= '9')
            out->cand[i] = (uint16_t)(1u << (ch - '1'));
        else if (ch == '0' || ch == '.')
            out->cand[i] = SUDOKU_ALL;
        else
            return 0;
    }
    return 1;
}

static void TraceFix(SUDOKU_TRACE *t, int cell, int digit, SUDOKU_TECH tech)
{
    if (!t || t->n >= SUDOKU_MAX_STEPS)
        return;
    t->steps[t->n].cell = (uint8_t)cell;
    t->steps[t->n].digit = (uint8_t)digit;
    t->steps[t->n].tech = tech;
    t->n++;
}

/* Un paso de propagacion. Devuelve: -1 contradiccion, 0 sin cambios, 1 cambios. */
static int PropagateStep(SUDOKU *b, SUDOKU_TRACE *trace)
{
    int changed = 0;

    /* Singles desnudos: el fijo elimina su digito de los pares */
    for (int i = 0; i < 81; i++)
    {
        int d = SingleDigit(b->cand[i]);
        if (d < 0)
        {
            if (b->cand[i] == 0)
                return -1;
            continue;
        }
        uint16_t bit = (uint16_t)(1u << d);
        for (int k = 0; k < 20; k++)
        {
            int p = PEERS[i][k];
            if (b->cand[p] & bit)
            {
                b->cand[p] &= (uint16_t)~bit;
                if (b->cand[p] == 0)
                    return -1;
                changed = 1;
                int fixed = SingleDigit(b->cand[p]);
                if (fixed >= 0)
                    TraceFix(trace, p, fixed, SSTEP_NAKED);
            }
        }
    }

    /* Singles escondidos: digito que solo cabe en una celda de la unidad */
    for (int ui = 0; ui < 27; ui++)
    {
        for (int d = 0; d < 9; d++)
        {
            uint16_t bit = (uint16_t)(1u << d);
            int count = 0, where = -1;
            for (int k = 0; k < 9; k++)
            {
                int cell = UNITS[ui][k];
                if (b->cand[cell] & bit) { count++; where = cell; }
            }
            if (count == 0)
                return -1;
            if (count == 1 && SingleDigit(b->cand[where]) != d)
            {
                b->cand[where] = bit;
                changed = 1;
                TraceFix(trace, where, d, SSTEP_HIDDEN);
            }
        }
    }

    return changed;
}

int SudokuPropagate(SUDOKU *b)
{
    if (!b)
        return 0;
    BuildTables();
    for (;;)
    {
        int r = PropagateStep(b, NULL);
        if (r < 0)
            return 0;
        if (r == 0)
            return 1;
    }
}

static int IsSolved(const SUDOKU *b)
{
    for (int i = 0; i < 81; i++)
        if (SingleDigit(b->cand[i]) < 0)
            return 0;
    return 1;
}

static int Search(SUDOKU *b, uint64_t *nodes, SUDOKU_TRACE *trace)
{
    if (!b)
        return 0;
    int entry_mark = trace ? trace->n : 0;
    for (;;)
    {
        int r = PropagateStep(b, trace);
        if (r < 0)
        {
            if (trace)
                trace->n = entry_mark;
            return 0;
        }
        if (r == 0)
            break;
    }
    if (IsSolved(b))
        return 1;

    /* MRV: celda no fija con menos candidatos */
    int pick = -1, best = 10;
    for (int i = 0; i < 81; i++)
    {
        int n = PopCount(b->cand[i]);
        if (n > 1 && n < best) { best = n; pick = i; }
    }
    if (pick < 0)
        return 0;

    int mark = trace ? trace->n : 0;
    SUDOKU save = *b;
    uint16_t opts = save.cand[pick];
    for (int d = 0; d < 9; d++)
    {
        if (!(opts & (1u << d)))
            continue;
        if (nodes)
            (*nodes)++;
        *b = save;
        if (trace)
            trace->n = mark;
        b->cand[pick] = (uint16_t)(1u << d);
        TraceFix(trace, pick, d, SSTEP_GUESS);
        if (Search(b, nodes, trace))
            return 1;
    }
    *b = save;
    if (trace)
        trace->n = entry_mark; /* descarta ramas muertas: solo camino exitoso */
    return 0;
}

int SudokuSolve(SUDOKU *b, uint64_t *out_nodes)
{
    if (!b)
        return 0;
    BuildTables();
    if (out_nodes)
        *out_nodes = 0;
    SUDOKU work = *b;
    if (!Search(&work, out_nodes, NULL))
        return 0;
    *b = work;
    return 1;
}

int SudokuSolveSteps(SUDOKU *b, uint64_t *out_nodes, SUDOKU_TRACE *trace)
{
    if (!b)
        return 0;
    BuildTables();
    if (out_nodes)
        *out_nodes = 0;
    if (trace)
        trace->n = 0;
    SUDOKU work = *b;
    if (trace)
    {
        for (int i = 0; i < 81; i++)
        {
            int d = SingleDigit(work.cand[i]);
            if (d >= 0)
                TraceFix(trace, i, d, SSTEP_GIVEN);
        }
    }
    if (!Search(&work, out_nodes, trace))
        return 0;
    *b = work;
    return 1;
}

static int CmpU64(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *)a;
    uint64_t y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

const char *SudokuDifficultyCalibrated(uint64_t nodes,
                                       const uint64_t *hist_nodes,
                                       int n_hist)
{
    if (!hist_nodes || n_hist < 3)
        return SudokuDifficulty(nodes, 30); /* sin historial: base */

    /* Copia acotada y ordenada (256 bastan: mas alla el percentil
       apenas se mueve y esto corre en el REPL interactivo) */
    int n = (n_hist > 256) ? 256 : n_hist;
    uint64_t s[256];
    for (int i = 0; i < n; i++)
        s[i] = hist_nodes[i];
    qsort(s, (size_t)n, sizeof(uint64_t), CmpU64);

    uint64_t p33 = s[n / 3];
    uint64_t p66 = s[(2 * n) / 3];
    if (nodes <= p33)
        return "FACIL (segun tu historial)";
    if (nodes <= p66)
        return "MEDIA (segun tu historial)";
    return "DIFICIL (segun tu historial)";
}

const char *SudokuTechName(SUDOKU_TECH tech)
{
    switch (tech)
    {
        case SSTEP_GIVEN:  return "PISTA";
        case SSTEP_NAKED:  return "SINGLE_DESNUDO";
        case SSTEP_HIDDEN: return "SINGLE_ESCONDIDO";
        case SSTEP_GUESS:  return "RAMA_BUSQUEDA";
        default:           return "?";
    }
}

/* Enumeracion acotada: cuenta soluciones hasta 'cap'. */
static int SearchCount(SUDOKU *b, int cap, uint64_t *nodes)
{
    if (cap <= 0)
        return 0;
    if (!SudokuPropagate(b))
        return 0;
    if (IsSolved(b))
        return 1;

    int pick = -1, best = 10;
    for (int i = 0; i < 81; i++)
    {
        int n = PopCount(b->cand[i]);
        if (n > 1 && n < best) { best = n; pick = i; }
    }
    if (pick < 0)
        return 0;

    SUDOKU save = *b;
    uint16_t opts = save.cand[pick];
    int total = 0;
    for (int d = 0; d < 9 && total < cap; d++)
    {
        if (!(opts & (1u << d)))
            continue;
        if (nodes)
            (*nodes)++;
        *b = save;
        b->cand[pick] = (uint16_t)(1u << d);
        total += SearchCount(b, cap - total, nodes);
    }
    *b = save;
    return total;
}

int SudokuCount(const SUDOKU *b, int limit, uint64_t *out_nodes)
{
    if (!b || limit <= 0)
        return 0;
    BuildTables();
    if (out_nodes)
        *out_nodes = 0;
    SUDOKU work = *b;
    return SearchCount(&work, limit, out_nodes);
}

void SudokuToString(const SUDOKU *b, char out81[82])
{
    for (int i = 0; i < 81; i++)
    {
        int d = b ? SingleDigit(b->cand[i]) : -1;
        out81[i] = (d >= 0) ? (char)('1' + d) : '0';
    }
    out81[81] = '\0';
}

int SudokuGivens(const SUDOKU *b)
{
    if (!b)
        return 0;
    /* Aproximacion: celdas con un solo candidato. El llamador debe
       medirlo sobre el tablero recien parseado (antes de propagar). */
    int n = 0;
    for (int i = 0; i < 81; i++)
        if (SingleDigit(b->cand[i]) >= 0)
            n++;
    return n;
}

const char *SudokuDifficulty(uint64_t nodes, int givens)
{
    if (nodes == 0)
        return "FACIL (solo propagacion)";
    if (nodes < 100 && givens >= 30)
        return "MEDIA (poca busqueda)";
    if (nodes < 5000)
        return "DIFICIL (busqueda)";
    return "EXTREMA (busqueda profunda)";
}
