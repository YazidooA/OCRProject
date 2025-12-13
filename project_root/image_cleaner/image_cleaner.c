#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <err.h>

#define GRAY_LEVELS 256

void convert_to_grayscale(SDL_Surface* surface) {
    if (!surface) return;

    int width = surface->w;
    int height = surface->h;

    if (SDL_MUSTLOCK(surface)) SDL_LockSurface(surface);

    for (int y = 0; y < height; y++) {

        for (int x = 0; x < width; x++) {
            Uint32* pixels = (Uint32*)surface->pixels;
            Uint32 pixel = pixels[y * surface->pitch / 4 + x];

            Uint8 r, g, b;
            SDL_GetRGB(pixel, surface->format, &r, &g, &b);

            Uint8 gray = (Uint8)(0.299*r + 0.587*g + 0.114*b);

            Uint32 gray_pixel = SDL_MapRGB(surface->format, gray, gray, gray);
            pixels[y * surface->pitch / 4 + x] = gray_pixel;
        }


    }

    if (SDL_MUSTLOCK(surface)) SDL_UnlockSurface(surface);
}



void compute_histogram(SDL_Surface* surface, int histogram[GRAY_LEVELS]) {
    if (!surface) errx(1, "Surface is NULL");
    memset(histogram, 0, sizeof(int) * GRAY_LEVELS);

    int width = surface->w;
    int height = surface->h;

    if (SDL_MUSTLOCK(surface)) SDL_LockSurface(surface);
    Uint32* pixels = (Uint32*)surface->pixels;

    for (int y = 0; y < height; y++) {

        for (int x = 0; x < width; x++) {
            Uint32 pixel = pixels[y * surface->pitch / 4 + x];
            Uint8 r;
            Uint8 g;
            Uint8 b;
            SDL_GetRGB(pixel, surface->format, &r, &g, &b);
            histogram[r]++;
        }

    }

    if (SDL_MUSTLOCK(surface)) SDL_UnlockSurface(surface);
}

int compute_otsu_threshold(int histogram[GRAY_LEVELS], int total_pixels) {

    double sum_total = 0.0f;

    for (int i = 0; i < GRAY_LEVELS; i++) {
        sum_total += i * histogram[i];
    }

    double sum_background = 0.0f;
    int weight_background = 0;
    double max_variance = 0.0f;
    int threshold = 0;

    //ietre sur tout les thresholds stockes
    for (int y = 0; y < GRAY_LEVELS; y++) {

        //ajoute les pixels de ce niveau de gris au poid du fond
        weight_background += histogram[y];

        //si aucun pixel on continue
        if (weight_background == 0) continue;

        //calcule le poid du foreground actuel et si le poid est 0 on sort
        int weight_foreground = total_pixels - weight_background;
        if (weight_foreground == 0) break;

        // ajoute a la somme la masse de cette valeur de gris pour tout les pixels ayant cette valeure precise
        sum_background += y * histogram[y];

        //Calcule la moyenne des intensites pour le fond et le premier plan
        double mean_background = sum_background / (double)weight_background;
        double mean_foreground = (sum_total - sum_background) / (double)weight_foreground;

        //Calcule la variance pour ce seuil
        double between_class_variance = 
            (double)weight_background * (double)weight_foreground *
            (mean_background - mean_foreground) * (mean_background - mean_foreground);

        //Si la nouvelle variance est superieure, on override max variance
        if (between_class_variance > max_variance) {
            max_variance = between_class_variance;
            threshold = y;
        }

    }

    return threshold;
}


void apply_threshold(SDL_Surface* surface, int threshold) {
    int width = surface->w;
    int height = surface->h;

    if (SDL_MUSTLOCK(surface)) SDL_LockSurface(surface);
    Uint32* pixels = (Uint32*)surface->pixels;

    for (int y = 0; y < height; y++) {

        for (int x = 0; x < width; x++) {
            Uint32 pixel = pixels[y * surface->pitch / 4 + x];
            Uint8 r;
            Uint8 g;
            Uint8 b;
            SDL_GetRGB(pixel, surface->format, &r, &g, &b);

            Uint8 value =0;
            if(r >= threshold){
                value = 255;
            }else{
                value = 0;
            }

            pixels[y*surface->pitch / 4+x] = SDL_MapRGB(surface->format, value, value, value);
        }

    }

    if (SDL_MUSTLOCK(surface)) SDL_UnlockSurface(surface);
}

void apply_otsu_thresholding(SDL_Surface* surface) {
    if (!surface) errx(1, "Surface is NULL");

    int histogram[GRAY_LEVELS];
    compute_histogram(surface, histogram);

    int total_pixels = surface->w * surface->h;
    int threshold = compute_otsu_threshold(histogram, total_pixels);

    apply_threshold(surface, threshold);
}




void apply_noise_removal(SDL_Surface* surface, int threshold) {
    if (!surface) return;

    int width  = surface->w;
    int height = surface->h;
    int stride = surface->pitch / 4;

    if (SDL_MUSTLOCK(surface)) {
        SDL_LockSurface(surface);
    }

    size_t nPixels = (size_t)stride * height;
    Uint32 *copy = malloc(nPixels * sizeof(Uint32));

    if (!copy) errx(1, "Copy allocation failed in apply noise removal");

    memcpy(copy, surface->pixels, nPixels * sizeof(Uint32));

    //converti les couleurs en format utilisable sur la surface
    Uint32 black = SDL_MapRGB(surface->format, 0,0,0);
    Uint32 white = SDL_MapRGB(surface->format, 255,255,255);

    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            size_t idx = (size_t)y * stride + x;
            if (copy[idx] != black) continue; 

            int black_neighbors = 0;

            //check les voisins du pixel
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    size_t nidx = (size_t)(y + dy) * stride + (x + dx);
                    if (copy[nidx] == black) black_neighbors++;
                }
            }

            //si pas assez de voisons noirs le pixel noir devient un pixel blanc
            if (black_neighbors <= threshold) {
                ((Uint32*)surface->pixels)[idx] = white;
            }
        }
    }

    free(copy);
    if (SDL_MUSTLOCK(surface)) SDL_UnlockSurface(surface);
}