#ifndef SUDOKU_H
#define SUDOKU_H

#include <stdint.h>

/* ============================================================
   Sudoku 9x9: propagacion de restricciones + backtracking (MRV).

   Diseno: cada celda guarda sus candidatos como bitmask de 9 bits.
   La propagacion aplica singles desnudos y escondidos hasta punto
   fijo; si no basta, la busqueda elige la celda con menos
   candidatos (MRV) y ramifica. Sudoku es determinista: aqui no
   hay aprendizaje, solo resolucion exacta y verificable.
   ============================================================ */

#define SUDOKU_N 81
#define SUDOKU_ALL 0x1FFu /* los 9 bits a 1 */

typedef struct
{
    uint16_t cand[SUDOKU_N]; /* bit d (0..8) = digito d+1 posible */
} SUDOKU;

/* Lee 81 caracteres ('1'..'9', '0'/'.' para vacia). 1 ok, 0 formato mal. */
int SudokuParse(const char *str81, SUDOKU *out);

/* Propaga restricciones hasta punto fijo. 1 consistente, 0 contradiccion. */
int SudokuPropagate(SUDOKU *b);

/* Resuelve (primera solucion) in-place. 1 resuelto, 0 sin solucion.
   out_nodes cuenta asignaciones de busqueda (0 = solo propagacion). */
int SudokuSolve(SUDOKU *b, uint64_t *out_nodes);

/* Cuenta soluciones hasta 'limit' (para unicidad). No modifica *b. */
int SudokuCount(const SUDOKU *b, int limit, uint64_t *out_nodes);

/* Vuelca el tablero a 81 chars ('0' si no resuelta). */
void SudokuToString(const SUDOKU *b, char out81[82]);

/* Celdas fijas (pista dada). */
int SudokuGivens(const SUDOKU *b);

/* Dificultad estimada a partir de nodos de busqueda y pistas. */
const char *SudokuDifficulty(uint64_t nodes, int givens);

/* ============================================================
   Traza explicable: cada fijacion con su tecnica. El CLI la
   ingiere al grafo como triples (SUDOKU_PASO_k --TECNICA--> ...),
   asi el motor recuerda COMO resolvio, no solo el resultado.
   ============================================================ */

typedef enum
{
    SSTEP_GIVEN = 0,  /* pista original */
    SSTEP_NAKED,      /* single desnudo: unico candidato de la celda */
    SSTEP_HIDDEN,     /* single escondido: unico hueco del digito en la unidad */
    SSTEP_GUESS       /* ramificacion de busqueda (camino exitoso) */
} SUDOKU_TECH;

typedef struct
{
    uint8_t     cell;  /* 0..80 */
    uint8_t     digit; /* 0..8 (= digito-1) */
    SUDOKU_TECH tech;
} SUDOKU_STEP;

#define SUDOKU_MAX_STEPS 256

typedef struct
{
    SUDOKU_STEP steps[SUDOKU_MAX_STEPS];
    int         n;
} SUDOKU_TRACE;

/* Resuelve registrando la traza del camino exitoso (ramas muertas
   descartadas). 1 resuelto, 0 sin solucion. trace puede ser NULL. */
int SudokuSolveSteps(SUDOKU *b, uint64_t *out_nodes, SUDOKU_TRACE *trace);

/* Nombre de tecnica para triples y mensajes. */
const char *SudokuTechName(SUDOKU_TECH tech);

/* ============================================================
   Calibracion por historial (el unico aprendizaje real aqui).
   Clasifica los nodos de un puzzle nuevo contra la distribucion
   de nodos de casos ya resueltos (percentiles p33/p66): los
   umbrales salen de TU experiencia, no de constantes a mano.
   Con menos de 3 casos no hay distribucion: devuelve la base.
   ============================================================ */
const char *SudokuDifficultyCalibrated(uint64_t nodes,
                                       const uint64_t *hist_nodes,
                                       int n_hist);

#endif
