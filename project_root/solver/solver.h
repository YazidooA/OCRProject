// solver.h
#ifndef SOLVER_H
#define SOLVER_H

#include <stddef.h>

// Number of candidates kept per cell (top-k).
// Must match the value used by the neural-network code.
#define KTOP 3

// Top-k candidates for a single grid cell.
typedef struct {
    int   n;                   // 0..KTOP
    unsigned char cls[KTOP];   // 0..25 = A..Z
    float        weight[KTOP]; // normalized weights (sum ≈ 1)
} CellCand;

// Simple (class,weight) pair used in local decisions.
typedef struct {
    int   cls;    // 0..25 = A..Z
    float weight; // probability / score
} Candidate;

/* Return 1 if (i,j) is inside grid bounds, 0 otherwise. */
int intab(int i, int j, int nb_row, int nb_column);

/* Exact resolution (non probabilistic):
 *   Search `word` in grid T[nb_row][nb_column] in 8 directions.
 *   If found: out = {x0, y0, x1, y1} in (col,row).
 *   If not found: out[0] = -1.
 */
void resolution(char **T, int nb_row, int nb_column,
                const char *word, int out[4]);

/* Probabilistic resolution:
 *   - cells: top-k candidates per cell (size rows*cols).
 *   - grid_mat: top1 letters per cell (rows x cols).
 *
 * For each path of length |word| in 8 directions, we compute:
 *   matches = number of positions where top1 == target letter
 *   prefix  = length of longest correct prefix (from the start)
 *   acc     = sum_k log P(letter_k | cell_k)
 *
 * We select the path that maximizes lexicographically:
 *   (matches, prefix, acc).
 *
 * If a path is found:
 *   out = {x0, y0, x1, y1}, *best_score = acc.
 * If no path is valid:
 *   out[0] = -1, *best_score = very negative.
 */
void resolution_prob(const CellCand *cells, char **grid_mat,
                     int rows, int cols,
                     const char *word, int out[4], float *best_score);

/* Free a char** matrix allocated row-by-row (optional helper). */
void freeMatrix(char **matrix, int rows);

/* Debug helper: load a grid from text file with format:
 *   <rows> <cols>
 *   <rows * cols characters...>
 * Returns newly allocated matrix (or NULL on error).
 */
char **read_grid_from_file(const char *filename, int *rows, int *cols);

#endif
