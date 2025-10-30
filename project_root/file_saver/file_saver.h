#ifndef FILE_SAVER_H
#define FILE_SAVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "letter_extractor.h"  // Pour LetterGrid, Letter
#include "structure_detection.h"  // Pour Image, Position, SearchResult (and draw_line)
#include "../neural_network/neural_network.h"  // Pour Network



// Basic text file operations
void write_text_file(const char* filename, char** lines, int line_count);
void free_text_lines(char** lines, int line_count);

// Results conversion
char** results_to_text_lines(SearchResult* results, int result_count, int* line_count);

// Grid matrix operations
void save_grid_matrix(const char* filename, char** matrix, int rows, int cols);
void free_matrix(char** matrix, int rows);

// Main functions for saving results
void save_solved_grid(Image* img, SearchResult* results, int result_count, 
                     const char* output_path);

// FONCTION PRINCIPALE: Reconnaissance avec réseau de neurones
void save_recognized_grid_with_nn(LetterGrid* grid, Network* net, 
                                  const char* output_path);

#endif