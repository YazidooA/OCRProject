#ifndef LETTER_EXTRACTOR_H
#define LETTER_EXTRACTOR_H

#include <SDL2/SDL.h>
#include "../neural_network/digitalisation.h"

// Extract letters from a grid region [x1..x2] x [y1..y2] on the image.
// The result is an N x M matrix of 28x28 tiles (Uint8[784]) or NULL for empty cells.
// Returns 0 on success, negative value on error.
int extract_letters(SDL_Surface *src,
                    int x1, int y1, int x2, int y2,
                    Uint8 ****out_matrix,
                    int *out_N,
                    int *out_M);

#endif
