#ifndef FILE_SAVER_H
#define FILE_SAVER_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "structure_detection.h"
#include "letter_extractor.h"

// Text file operations
void write_text_file(const char* filename, char** lines, int line_count);
char** results_to_text_lines(SearchResult* results, int result_count, int* line_count);
void free_text_lines(char** lines, int line_count);

// Grid matrix operations (for solver.c compatibility)
char** letter_grid_to_matrix(LetterGrid* grid, char* recognized_letters);
void save_grid_matrix(const char* filename, char** matrix, int rows, int cols);
void free_matrix(char** matrix, int rows);

// Main save functions for RDN output
void save_solved_grid(Image* img, SearchResult* results, int result_count, 
                     const char* output_path);
void save_recognized_grid_for_solver(LetterGrid* grid, char* recognized_letters, 
                                     const char* output_path);

// NEW: Complete RDN output structure
typedef struct {
    char image_path[512];    // Path to output image with highlighted words
    char text_path[512];     // Path to output text file with results
    int words_found;         // Number of words successfully found
    int words_total;         // Total number of words searched
    float success_rate;      // Percentage of words found
} RDNOutput;

// Save complete RDN output: highlighted image + detailed text results
RDNOutput save_complete_rdn_output(Image* img, SearchResult* results, 
                                    int result_count, const char* base_output_path);

#endif // FILE_SAVER_H  