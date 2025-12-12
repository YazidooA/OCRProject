#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "../neural_network/digitalisation.h"
#include <stdint.h>

/*
 * surface_to_28:
 *  - Take an SDL_Surface (already grayscale or at least R=G=B)
 *  - Sample it into a 28x28 grid (nearest neighbor, centered)
 *  - Write the result into out784 (28*28 bytes, row-major)
 *  - Return 0 on success, <0 on error
 */
int surface_to_28(SDL_Surface *src, uint8_t out784[784])
{
    // Basic safety check: null pointer
    if (!src)
        return -1;

    // Lock the surface so we can read pixel data safely
    if (SDL_LockSurface(src) != 0)
        return -2;

    // Width, height and pitch (in bytes) of the source surface
    const int W = src->w;
    const int H = src->h;
    const int pitch = src->pitch;  // number of bytes per scanline

    // Pixel format (used by SDL_GetRGBA)
    const SDL_PixelFormat *fmt = src->format;

    // Loop over the 28x28 output pixels
    for (int y = 0; y < 28; ++y) {
        // Map y in [0..27] to a float coordinate sy in the source image
        // We use centers: (y + 0.5) / 28.0, then scale to [0..H)
        double sy = ((y + 0.5) / 28.0) * H - 0.5;

        // Nearest neighbor: round to closest integer
        int iy = (int)(sy + 0.5);

        // Clamp iy inside [0, H-1]
        if (iy < 0)      iy = 0;
        if (iy >= H)     iy = H - 1;

        // Pointer to the beginning of row iy (in bytes)
        const uint8_t *row = (const uint8_t*)src->pixels + iy * pitch;

        for (int x = 0; x < 28; ++x) {
            // Same mapping for x in [0..27] -> sx in [0..W)
            double sx = ((x + 0.5) / 28.0) * W - 0.5;
            int ix = (int)(sx + 0.5);

            // Clamp ix inside [0, W-1]
            if (ix < 0)      ix = 0;
            if (ix >= W)     ix = W - 1;

            // Read the pixel at (ix, iy) as a 32-bit value
            Uint32 p = ((const Uint32*)row)[ix];

            // Extract RGBA components
            Uint8 r, g, b, a;
            SDL_GetRGBA(p, fmt, &r, &g, &b, &a);

            // The image is already grayscale (r = g = b).
            // If you want to invert, you could use: 255 - r.
            out784[y * 28 + x] = r;
        }
    }

    // Unlock the surface when we are done
    SDL_UnlockSurface(src);
    return 0;
}
