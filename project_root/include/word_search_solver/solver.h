#ifndef SOLVER_H
#define SOLVER_H

#include "../common.h"

typedef struct {
    char** grid;
    int rows;
    int cols;
} WordGrid;

typedef struct {
    char** words;
    int count;
    int max_count;
} WordList;

typedef struct {
    Position start;
    Position end;
    bool found;
    int direction; // 0-7 pour les 8 directions
} SearchResult;

// Directions de recherche
typedef enum {
    DIR_RIGHT = 0,
    DIR_DOWN_RIGHT,
    DIR_DOWN,
    DIR_DOWN_LEFT,
    DIR_LEFT,
    DIR_UP_LEFT,
    DIR_UP,
    DIR_UP_RIGHT
} SearchDirection;

// Gestion des grilles
WordGrid* create_grid(int rows, int cols);
WordGrid* load_grid_from_file(const char* filename);
void free_grid(WordGrid* grid);
void print_grid(const WordGrid* grid);

// Gestion des listes de mots
WordList* create_word_list(int max_words);
void add_word_to_list(WordList* list, const char* word);
void free_word_list(WordList* list);

// Recherche de mots
SearchResult find_word_in_grid(const WordGrid* grid, const char* word);
SearchResult* find_all_words(const WordGrid* grid, const WordList* word_list, int* found_count);

// Utilitaires
bool is_valid_position(const WordGrid* grid, int x, int y);
char get_cell(const WordGrid* grid, int x, int y);

#endif // SOLVER_H