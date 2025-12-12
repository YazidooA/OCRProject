#ifndef ROTATION_H
#define ROTATION_H

#include <SDL2/SDL.h>

// Rotate a surface around its center by `angle` degrees.
// Returns a NEW surface (caller must SDL_FreeSurface), or NULL on error.
SDL_Surface *rotate(SDL_Surface *surface, double angle);

// Estimate the deskew angle (in degrees) of a document-like image.
// Positive angle means you should call rotate(surface, angle) to deskew.
double auto_deskew_correction(SDL_Surface *surface);

#endif
