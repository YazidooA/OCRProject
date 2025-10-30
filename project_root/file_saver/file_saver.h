#ifndef FILE_SAVER_H
#define FILE_SAVER_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "structure_detection.h"

typedef char bstring[50];

// Text file operations
void give_text(bstring* text, char* namefile);
int myvim(char* file, char* text_array[]);

// Image saving with results
void save_solved_grid(Image* img, SearchResult* results, int result_count, const char* output_path);

#endif // FILE_SAVER_H