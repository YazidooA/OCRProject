#ifndef ROTATION_H
#define ROTATION_H
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

SDL_Surface* rotate(SDL_Surface* Surface, double angle);
double auto_deskew_correction(SDL_Surface *surface);

#endif 
