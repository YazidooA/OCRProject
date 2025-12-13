#ifndef IMAGE_CLEANER_H
#define IMAGE_CLEANER_H

#include <SDL2/SDL.h>



void convert_to_grayscale(SDL_Surface* surface);

void apply_otsu_thresholding(SDL_Surface* surface);

void apply_noise_removal(SDL_Surface* surface, int threshold);

#endif
