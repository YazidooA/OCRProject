#ifndef OUTLINE_H
#define OUTLINE_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#define M_PI 3.14159265358979323846

// Draw a quad outline around a word, following its direction.
// (x1, y1) : center of the first letter
// (x2, y2) : center of the last letter
// width    : "height" of the tube around the word
// stroke   : thickness of the outline in pixels
void draw_outline(SDL_Renderer *renderer,
                  int x1, int y1, int x2, int y2,
                  int width, int stroke);

// Draw an axis-aligned rectangle outline (for grid, list, etc.).
// (x1, y1) and (x2, y2) are opposite corners.
// width is not used here, stroke is the outline thickness.
void rectangle(SDL_Renderer *renderer,
               int x1, int y1, int x2, int y2,
               int width, int stroke);

#endif
