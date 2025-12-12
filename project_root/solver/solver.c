// solver.c
#include "solver.h"

#include <ctype.h>   // toupper
#include <math.h>    // logf
#include <stdio.h>   // FILE, fopen, fscanf, perror, fprintf
#include <stdlib.h>  // malloc, free
#include <string.h>  // strlen

// Small floor value when a letter is missing in top-k.
#define MIN_EPS_WEIGHT 1e-6f

/* Check if (i,j) is inside the grid bounds. */
int intab(int i, int j, int nb_row, int nb_column) {
    return (i >= 0 && i < nb_row && j >= 0 && j < nb_column);
}

/* Exact (non probabilistic) resolver:
 *  Try all start cells and the 8 directions for `word`.
 */
void resolution(char **T, int nb_row, int nb_column,
                const char *word, int out[4]) {
    // 8 directions: up, down, left, right, 4 diagonals
    int dx[8] = { 0,  0,  1, -1,  1, -1,  1, -1};
    int dy[8] = { 1, -1,  0,  0,  1, -1, -1,  1};

    if (!T || !word || nb_row <= 0 || nb_column <= 0) {
        out[0] = -1;
        return;
    }

    for (int i = 0; i < nb_row; ++i) {
        for (int j = 0; j < nb_column; ++j) {
            if (T[i][j] != word[0])   // first letter must match
                continue;

            for (int dir = 0; dir < 8; ++dir) {
                int k = 1;            // index in word
                while (word[k] != '\0' &&
                       intab(i + k*dx[dir], j + k*dy[dir], nb_row, nb_column) &&
                       T[i + k*dx[dir]][j + k*dy[dir]] == word[k]) {
                    ++k;
                }

                if (word[k] == '\0') {
                    // Found: store (start_col, start_row, end_col, end_row)
                    out[0] = j;
                    out[1] = i;
                    out[2] = j + (k - 1) * dy[dir];
                    out[3] = i + (k - 1) * dx[dir];
                    return;
                }
            }
        }
    }
    // Not found
    out[0] = -1;
}

/* Free a char** matrix allocated row-by-row. */
void freeMatrix(char **matrix, int rows) {
    if (!matrix) return;
    for (int i = 0; i < rows; ++i)
        free(matrix[i]);
    free(matrix);
}

/* Read grid from text file:
 *  first line: "<rows> <cols>"
 *  then rows*cols characters.
 */
char **read_grid_from_file(const char *filename, int *rows, int *cols) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("read_grid_from_file: fopen");
        return NULL;
    }

    if (fscanf(file, "%d %d", rows, cols) != 2) {
        fprintf(stderr, "read_grid_from_file: invalid header\n");
        fclose(file);
        return NULL;
    }

    char **matrix = (char**)malloc((size_t)*rows * sizeof(char*));
    if (!matrix) {
        fclose(file);
        return NULL;
    }

    for (int i = 0; i < *rows; ++i) {
        matrix[i] = (char*)malloc((size_t)*cols * sizeof(char));
        if (!matrix[i]) {
            for (int k = 0; k < i; ++k) free(matrix[k]);
            free(matrix);
            fclose(file);
            return NULL;
        }
    }

    for (int i = 0; i < *rows; ++i) {
        for (int j = 0; j < *cols; ++j) {
            if (fscanf(file, " %c", &matrix[i][j]) != 1)
                matrix[i][j] = '?';
            matrix[i][j] = (char)toupper((unsigned char)matrix[i][j]);
        }
    }

    fclose(file);
    return matrix;
}

/* Internal helper: get weight of class `cls` in a given cell. */
static float cell_letter_weight(const CellCand *cell, int cls) {
    if (!cell) return MIN_EPS_WEIGHT;
    for (int i = 0; i < cell->n; ++i)
        if (cell->cls[i] == cls)
            return cell->weight[i];
    return MIN_EPS_WEIGHT;
}

/* Probabilistic resolver with match/prefix priority. */
void resolution_prob(const CellCand *cells, char **grid_mat,
                     int rows, int cols,
                     const char *word, int out[4], float *best_score) {
    if (best_score) *best_score = -1e30f;
    if (out) out[0] = -1;

    if (!cells || !grid_mat || !word || rows <= 0 || cols <= 0)
        return;

    const int dx[8] = { 0,  0,  1, -1,  1, -1,  1, -1 };
    const int dy[8] = { 1, -1,  0,  0,  1, -1, -1,  1 };

    int L = (int)strlen(word);
    if (L <= 0) return;

    int   best_matches = -1;
    int   best_prefix  = -1;
    float best_log     = -1e30f;
    int   bi = -1, bj = -1, bdir = -1;

    // Try every starting cell and the 8 directions.
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            for (int dir = 0; dir < 8; ++dir) {
                int   ii = i, jj = j;
                int   matches = 0, prefix = 0;
                int   prefix_ok = 1;
                float acc = 0.f;
                int   ok = 1;

                for (int k = 0; k < L; ++k) {
                    // Out-of-bounds check
                    if (ii < 0 || ii >= rows || jj < 0 || jj >= cols) {
                        ok = 0; break;
                    }

                    // Normalize target letter to uppercase A..Z
                    char target = word[k];
                    if (target >= 'a' && target <= 'z')
                        target = (char)(target - 'a' + 'A');
                    if (target < 'A' || target > 'Z') { ok = 0; break; }

                    // Grid top1 letter normalized
                    char topc = grid_mat[ii][jj];
                    if (topc >= 'a' && topc <= 'z')
                        topc = (char)(topc - 'a' + 'A');

                    // Count matches + longest prefix
                    if (topc == target) {
                        matches++;
                        if (prefix_ok) prefix++;
                    } else {
                        prefix_ok = 0;
                    }

                    int cls = target - 'A';
                    const CellCand *cell = &cells[ii*cols + jj];
                    float w = cell_letter_weight(cell, cls);
                    if (w < MIN_EPS_WEIGHT) w = MIN_EPS_WEIGHT;
                    acc += logf(w);

                    ii += dx[dir];
                    jj += dy[dir];
                }

                if (!ok) continue;

                // Lexicographic comparison on (matches, prefix, acc)
                if (matches > best_matches ||
                    (matches == best_matches && prefix > best_prefix) ||
                    (matches == best_matches && prefix == best_prefix && acc > best_log)) {
                    best_matches = matches;
                    best_prefix  = prefix;
                    best_log     = acc;
                    bi = i; bj = j; bdir = dir;
                }
            }
        }
    }

    if (bi < 0) {
        // No valid path found
        if (out) out[0] = -1;
        if (best_score) *best_score = -1e30f;
        return;
    }

    // Reconstruct endpoint
    int ei = bi + dx[bdir]*(L - 1);
    int ej = bj + dy[bdir]*(L - 1);

    if (out) {
        out[0] = bj;  // start col
        out[1] = bi;  // start row
        out[2] = ej;  // end col
        out[3] = ei;  // end row
    }
    if (best_score) *best_score = best_log;

    printf("resolution_prob: word=%s  matches=%d  prefix=%d  score=%.3f\n",
           word, best_matches, best_prefix, best_log);
}
