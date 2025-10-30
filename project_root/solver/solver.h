#ifndef SOLVER_H
#define SOLVER_H

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

int intab(int i, int j, int nb_row, int nb_column);
void resolution(char **T, int nb_row, int nb_column, char *word, int out[4]);
void freeMatrix(char **matrix, int rows);
char** read_grid_from_file(const char* filename, int* rows, int* cols);

#endif
