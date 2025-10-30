#ifndef STRUCTURE_DETECTION_H
#define STRUCTURE_DETECTION_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

// Structure for grayscale image representation
typedef struct {
    unsigned char* data;
    int width;
    int height;
    int channels;
} Image;

// Structure for rectangle representation
typedef struct {
    int x;
    int y;
    int width;
    int height;
} Rectangle;

// Structure for position representation
typedef struct {
    int x;
    int y;
} Position;

// Structure for detection results
typedef struct {
    Position start;
    Position end;
    int found;
    char word[50];
} SearchResult;

// Image loading functions
Image* load_image_sdl(const char* filename);
void save_image_sdl(Image* img, const char* filename);
void free_image(Image* img);

// Detection functions
Rectangle detect_grid_area(Image* img);
Rectangle detect_word_list_area(Image* img);
Rectangle* detect_grid_cells(Image* img, Rectangle grid_area, int* cell_count, int* rows, int* cols);

// Projection functions (used internally)
int* calculate_horizontal_projection(Image* img);
int* calculate_vertical_projection(Image* img);

// Utility functions (kept minimal - use external drawing functions when possible)
int is_inside_rectangle(int x, int y, Rectangle rect);

// NOTE: draw_line() removed - use draw_outline() from draw_outline.c instead
// NOTE: draw_rectangle() was never implemented

#endif // STRUCTURE_DETECTION_H