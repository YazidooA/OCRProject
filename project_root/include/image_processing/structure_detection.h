// structure_detection.h
#ifndef STRUCTURE_DETECTION_H
#define STRUCTURE_DETECTION_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Structure pour représenter une image
typedef struct {
    unsigned char* data;
    int width;
    int height;
    int channels;
} Image;

// Structure pour représenter un rectangle
typedef struct {
    int x;
    int y;
    int width;
    int height;
} Rectangle;

// Structure pour représenter une position
typedef struct {
    int x;
    int y;
} Position;

// Structure pour les résultats de détection
typedef struct {
    Position start;
    Position end;
    int found;
    char word[50];
} SearchResult;

// Fonctions de détection
Rectangle detect_grid_area(Image* img);
Rectangle detect_word_list_area(Image* img);
Rectangle* detect_grid_cells(Image* img, Rectangle grid_area, int* cell_count);
void save_solved_grid(Image* img, SearchResult* results, int result_count, const char* output_path);

// Fonctions utilitaires
int count_transitions(unsigned char* line, int length, int threshold);
int is_inside_rectangle(int x, int y, Rectangle rect);
void draw_rectangle(Image* img, Rectangle rect, unsigned char color);
void draw_line(Image* img, Position start, Position end, unsigned char color, int thickness);

#endif