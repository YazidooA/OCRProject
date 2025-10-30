#ifndef OUTLINE_H
#define OUTLINE_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#define M_PI 3.14159265358979323846

// draw quad outline around word
void draw_outline(SDL_Renderer *renderer,
                            int x1, int y1, int x2, int y2,
                            int width, int stroke);

#endif
