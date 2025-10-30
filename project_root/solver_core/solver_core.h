#ifndef SOLVER_CORE_H
#define SOLVER_CORE_H

#include "../common.h"
#include "solver.h"

// Algorithmes de recherche
bool search_direction(const WordGrid* grid, const char* word, int start_x, int start_y, int dx, int dy, SearchResult* result);
bool search_horizontal(const WordGrid* grid, const char* word, int row, SearchResult* result);
bool search_vertical(const WordGrid* grid, const char* word, int col, SearchResult* result);
bool search_diagonal(const WordGrid* grid, const char* word, SearchResult* result);

// Utilitaires de direction
void get_direction_vector(SearchDirection dir, int* dx, int* dy);
SearchDirection get_direction_from_vector(int dx, int dy);
const char* get_direction_name(SearchDirection dir);

// Validation
bool is_word_at_position(const WordGrid* grid, const char* word, int x, int y, int dx, int dy);
bool can_fit_word(const WordGrid* grid, const char* word, int x, int y, int dx, int dy);

#endif // SOLVER_CORE_H