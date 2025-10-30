#include "rotation.h"
#define M_PI 3.14159265358979323846

SDL_Surface* rotate(SDL_Surface* surface, int angle) {
    if (!surface) return NULL;

    double rad = angle * M_PI / 180.0; // degrees to radians
    int w = surface->w;
    int h = surface->h;

    // nez surface, same size
    SDL_Surface *rotated = SDL_CreateRGBSurface(0, w, h, 
                                                surface->format->BitsPerPixel,
                                                surface->format->Rmask,
                                                surface->format->Gmask,
                                                surface->format->Bmask,
                                                surface->format->Amask);
    if (!rotated) {
        fprintf(stderr, "Erreur création surface : %s\n", SDL_GetError());
        return NULL;
    }

    /*Rotation center point*/
    int cx = w / 2;
    int cy = h / 2;

    Uint32 *src_pixels = (Uint32*) surface->pixels;
    Uint32 *dst_pixels = (Uint32*) rotated->pixels;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            // relatives coordonates to center
            int xr = x - cx;
            int yr = y - cy;

            // rotated coordinates
            int xs = (int)( cos(rad) * xr + sin(rad) * yr + cx );
            int ys = (int)(-sin(rad) * xr + cos(rad) * yr + cy );

            // if within bounds, copy pixel
            if (xs >= 0 && xs < w && ys >= 0 && ys < h) {
                dst_pixels[y * (rotated->pitch / 4) + x] =
                    src_pixels[ys * (surface->pitch / 4) + xs];
            } else {
                dst_pixels[y * (rotated->pitch / 4) + x] = 0; // black pixel
            }
        }
    }

    return rotated;
}




int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <image> <angle>\n", argv[0]);
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        fprintf(stderr, "IMG_Init Error: %s\n", IMG_GetError());
        SDL_Quit();
        return 1;
    }

    int angle = atoi(argv[2]);

    SDL_Surface *img = IMG_Load(argv[1]);
    if (!img) {
        fprintf(stderr, "Erreur chargement image: %s\n", IMG_GetError());
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Surface *rotated = rotate(img, angle);
    if (!rotated) {
        SDL_FreeSurface(img);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    // Sauvegarde du résultat
    if (SDL_SaveBMP(rotated, "rotated.bmp") != 0) {
        fprintf(stderr, "Erreur sauvegarde BMP: %s\n", SDL_GetError());
    } else {
        printf("Image tournée sauvegardée dans rotated.bmp\n");
    }

    SDL_FreeSurface(img);
    SDL_FreeSurface(rotated);
    IMG_Quit();
    SDL_Quit();
    return 0;
}
