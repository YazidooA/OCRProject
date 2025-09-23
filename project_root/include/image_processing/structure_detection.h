#ifndef STRUCTURE_DETECTION_H
#define STRUCTURE_DETECTION_H

#include "../common.h"

// Détection de structures
Rectangle detect_grid_area(const Image* img);
Rectangle detect_word_list_area(const Image* img);
Rectangle* detect_grid_cells(const Image* img, const Rectangle* grid_area, int* cell_count);

// Analyse de lignes et colonnes
int* detect_horizontal_lines(const Image* img, int* line_count);
int* detect_vertical_lines(const Image* img, int* line_count);

// Validation de structures
bool validate_grid_structure(const Rectangle* cells, int cell_count, int expected_rows, int expected_cols);
bool is_valid_cell(const Rectangle* cell, int min_width, int min_height);

#endif // STRUCTURE_DETECTION_H