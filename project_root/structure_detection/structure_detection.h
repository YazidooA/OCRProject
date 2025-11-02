#ifndef STRUCTURE_DETECTION_H
#define STRUCTURE_DETECTION_H

#include <SDL2/SDL.h>

int detect_grid_and_list(SDL_Surface *src, SDL_Rect *grid, SDL_Rect *list);
static int find_dense_band(const Uint32 *P, int pitch, int y1, int y2,
                           int x0, int x1, SDL_Rect *out);

#endif
