#include "solver.h"

/*is (i,j) is within matrix*/
int intab(int i, int j, int nb_row, int nb_column) {
    return 0 <= i && i < nb_row && 0 <= j && j < nb_column;
}

/* seek word in matrix */
void resolution(char **T, int nb_row, int nb_column, char *word, int out[4]) {
    /*directions*/ 
    int dx[8] = { 0,  0,  1, -1,  1, -1,  1, -1};
    int dy[8] = { 1, -1,  0,  0,  1, -1, -1,  1};

    for (int i = 0; i < nb_row; i++) {
        for (int j = 0; j < nb_column; j++) {
            if (T[i][j] == word[0]) {  // first letter found !!
                for (int dir = 0; dir < 8; dir++) {
                    int k = 1;
                    while (word[k] != '\0' &&
                           intab(i + k * dx[dir], j + k * dy[dir], nb_row, nb_column) &&
                           T[i + k * dx[dir]][j + k * dy[dir]] == word[k]) {
                        k++;
                    }
                    if (word[k] == '\0') { // word found !!
                        out[0] = j;                   // x0 (column start)
                        out[1] = i;                   // y0 (line start)
                        out[2] = j + (k - 1) * dy[dir]; // x1 (column end)
                        out[3] = i + (k - 1) * dx[dir]; // y1 (line end)
                        return;
                    }
                }
            }
        }
    }
    out[0] = -1; // not found
}

/* its in the name :3 */
void freeMatrix(char **matrix, int rows) {
    for (int i = 0; i < rows; i++) {
        free(matrix[i]);
    }
    free(matrix);
}

/* Read grid from file */
char** read_grid_from_file(const char* filename, int* rows, int* cols) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        return NULL;
    }

    // read dimensions
    if (fscanf(file, "%d %d", rows, cols) != 2) {
        fprintf(stderr, "Invalid format\n");
        fclose(file);
        return NULL;
    }

    // Allocation
    char **matrix = malloc(*rows * sizeof(char*));
    for (int i = 0; i < *rows; i++) {
        matrix[i] = malloc(*cols * sizeof(char));
    }

    // read grid into matrix
    for (int i = 0; i < *rows; i++) {
        for (int j = 0; j < *cols; j++) {
            fscanf(file, " %c", &matrix[i][j]);
            matrix[i][j] = toupper(matrix[i][j]); // convert to uppercase
        }
    }

    fclose(file);
    return matrix;
}

int main(int argc, char *argv[]) {
    if (argc != 2 && argc != 3) {
        fprintf(stderr, "Usage: %s <file_grid> <word>\n", argv[0]);
        return 1;
    }

    int rows, cols;
    char **matrix = read_grid_from_file(argv[1], &rows, &cols);
    if (!matrix) return 1;

    char *word = argv[2];
    for (size_t i = 0; word[i]; i++) word[i] = toupper(word[i]);

    int out[4];
    resolution(matrix, rows, cols, word, out);

    if (out[0] == -1) {
        printf("Not Found\n");
    } else {
        printf("(%d,%d)(%d,%d)\n", out[0], out[1], out[2], out[3]);
    }

    freeMatrix(matrix, rows);
    return 0;
}
