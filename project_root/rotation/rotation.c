#include "rotation.h"
#define M_PI 3.14159265358979323846

SDL_Surface* rotate(SDL_Surface* surface, double angle) {
    if (!surface) return NULL;

    double rad = angle * M_PI / 180.0; // degrees to radians
    int w = surface->w;
    int h = surface->h;

    // new surface, same size
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


double auto_deskew_correction(SDL_Surface *surface) {
    if (!surface) 
        return 0.0;

    /* constante locales */
    #define DEG2RAD(a) ((a) * (M_PI/180.0))

    /* 1) Forcer 32 bpp + downscale pour la vitesse */
    SDL_Surface *s32 = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ARGB8888, 0);
    if (!s32) 
        return 0.0;

    int W = s32->w, H = s32->h;
    int maxw = 1000;
    double scale = (W > maxw) ? (double)maxw / (double)W : 1.0;
    int w = (int)lrint(W * scale);
    int h = (int)lrint(H * scale);

    SDL_Surface *small = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!small) { 
        SDL_FreeSurface(s32); 
        return 0.0; 
    }
    if (SDL_BlitScaled(s32, NULL, small, NULL) != 0) {
        SDL_FreeSurface(s32); 
        SDL_FreeSurface(small); 
        return 0.0;
    }
    SDL_FreeSurface(s32);

    if (SDL_LockSurface(small) != 0) { 
        SDL_FreeSurface(small); 
        return 0.0; 
    }
    const Uint32 *pix = (const Uint32*)small->pixels;
    const int pitch = small->pitch / 4;

    /* 2) Hough grossier : theta ∈ [-90,90], pas 1° */
    int th_start = -90, th_end = 90, th_step = 1;
    int th_bins = (th_end - th_start) / th_step + 1;

    int rmax = (int)ceil(hypot((double)w, (double)h));
    int rbins = 2 * rmax + 1;

    /* tables trig */
    double *ctab = (double*)malloc(sizeof(double) * th_bins);
    double *stab = (double*)malloc(sizeof(double) * th_bins);
    if (!ctab || !stab) { 
        free(ctab); 
        free(stab); 
        SDL_UnlockSurface(small); 
        SDL_FreeSurface(small); 
        return 0.0; 
    }
    for (int i = 0; i < th_bins; ++i) {
        double th = DEG2RAD(th_start + i*th_step);
        ctab[i] = cos(th);
        stab[i] = sin(th);
    }

    /* accumulateur (theta, rho) */
    int *acc = (int*)calloc((size_t)th_bins * rbins, sizeof(int));
    if (!acc) { 
        free(ctab); 
        free(stab); 
        SDL_UnlockSurface(small); 
        SDL_FreeSurface(small); 
        return 0.0; 
    }

    /* on ne prend que des bords grossiers: pixel noir avec au moins 1 voisin blanc */
    for (int y = 1; y < h-1; ++y) {
        const Uint32 *row = pix + y*pitch;
        for (int x = 1; x < w-1; ++x) {
            Uint32 v = row[x];
            Uint8 a = (Uint8)(v >> 24);
            Uint8 R = (Uint8)(v >> 16); /* noir si R<128 */
            if (a < 128 || R >= 128) {
                continue;
            }

            /* neighbors */
            Uint32 vL = row[x-1], vR = row[x+1];
            Uint32 vU = pix[(y-1)*pitch + x], vD = pix[(y+1)*pitch + x];
            if (((Uint8)(vL >> 16) < 128) && ((Uint8)(vR >> 16) < 128) &&
                ((Uint8)(vU >> 16) < 128) && ((Uint8)(vD >> 16) < 128)) {
                continue; // pas un bord net
            }

            for (int ti = 0; ti < th_bins; ++ti) {
                double rho = x*ctab[ti] + y*stab[ti];
                int rbin = (int)lrint(rho) + rmax;
                if ((unsigned)rbin < (unsigned)rbins) {
                    acc[ti*rbins + rbin] += 1;
                }
            }
        }
    }

    /* énergie par theta = somme des carrés des colonnes rho */
    double best_theta_coarse = 0.0, best_energy = -1.0;
    for (int ti = 0; ti < th_bins; ++ti) {
        long long e = 0;
        int *row = acc + ti*rbins;
        for (int r = 0; r < rbins; ++r) { 
            int v = row[r]; 
            e += (long long)v * (long long)v; 
        }
        double E = (double)e;
        if (E > best_energy) { 
            best_energy = E; 
            best_theta_coarse = (double)(th_start + ti*th_step); 
        }
    }

    free(acc);
    free(ctab);
    free(stab);

    /* Pour économiser la mémoire, on fait un Hough 1D (rho) par angle. */
    double fine_start = best_theta_coarse - 1.5;
    double fine_end   = best_theta_coarse + 1.5;
    double fine_step  = 0.1;

    double best_theta_fine = best_theta_coarse;
    best_energy = -1.0;

    int fine_bins = (int)floor((fine_end - fine_start) / fine_step + 0.5) + 1;
    for (int k = 0; k < fine_bins; ++k) {
        double thdeg = fine_start + k * fine_step;
        double c = cos(DEG2RAD(thdeg));
        double s = sin(DEG2RAD(thdeg));

        int *accR = (int*)calloc((size_t)rbins, sizeof(int));
        if (!accR) continue;

        for (int y = 1; y < h-1; ++y) {
            const Uint32 *row = pix + y*pitch;
            for (int x = 1; x < w-1; ++x) {
                Uint32 v = row[x];
                Uint8 a = (Uint8)(v >> 24);
                Uint8 R = (Uint8)(v >> 16);
                if (a < 128 || R >= 128) continue;

                Uint32 vL = row[x-1], vR = row[x+1];
                Uint32 vU = pix[(y-1)*pitch + x], vD = pix[(y+1)*pitch + x];
                if (((Uint8)(vL >> 16) < 128) && ((Uint8)(vR >> 16) < 128) &&
                    ((Uint8)(vU >> 16) < 128) && ((Uint8)(vD >> 16) < 128)) {
                    continue;
                }

                int rbin = (int)lrint(x*c + y*s) + rmax;
                if ((unsigned)rbin < (unsigned)rbins) accR[rbin] += 1;
            }
        }

        long long e = 0;
        for (int r = 0; r < rbins; ++r) { int v = accR[r]; e += (long long)v * (long long)v; }
        double E = (double)e;
        if (E > best_energy) { best_energy = E; best_theta_fine = thdeg; }

        free(accR);
    }

    SDL_UnlockSurface(small);
    SDL_FreeSurface(small);

    /* 4) Repli par rapport à l’horizontale/verticale -> petit angle */
    double nearest90 = 90.0 * round(best_theta_fine / 90.0);
    double skew = best_theta_fine - nearest90;            /* dans ~[-45,45] */
    double corr = -skew;                                  

    /* Normalisation finale */
    if (corr >  90.0) corr -= 180.0;
    if (corr <= -90.0) corr += 180.0;

    return corr;
}




int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s <image> [angle|auto]\n", argv[0]);
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

    SDL_Surface *img = IMG_Load(argv[1]);
    if (!img) {
        fprintf(stderr, "Erreur chargement image: %s\n", IMG_GetError());
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    // Choix de l'angle
    double angle_deg = 0.0;
    int use_auto = (argc == 2);           // pas d’angle => auto

    if (use_auto) {
        angle_deg = auto_deskew_correction(img);   // renvoie un double en degrés
        printf("Angle détecté: %.2f°\n", angle_deg);
    } else {
        // si on te passe "auto", on déclenche aussi l'auto
        if (strcmp(argv[2], "auto") == 0 || strcmp(argv[2], "AUTO") == 0) {
            angle_deg = auto_deskew_correction(img);
            printf("Angle détecté: %.2f°\n", angle_deg);
        } else {
            char *endptr = NULL;
            angle_deg = strtod(argv[2], &endptr);
            if (endptr == argv[2]) {
                fprintf(stderr, "Angle invalide: %s\n", argv[2]);
                SDL_FreeSurface(img);
                IMG_Quit(); SDL_Quit();
                return 1;
            }
            printf("Angle imposé: %.2f°\n", angle_deg);
        }
    }

    SDL_Surface *rotated = rotate(img, angle_deg);
    if (!rotated) {
        fprintf(stderr, "Echec rotation.\n");
        SDL_FreeSurface(img);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    if (SDL_SaveBMP(rotated, "rotated.bmp") != 0) {
        fprintf(stderr, "Erreur sauvegarde BMP: %s\n", SDL_GetError());
    } else {
        printf("Image tournée sauvegardée dans rotated.bmp\n");
    }

    SDL_FreeSurface(rotated);
    SDL_FreeSurface(img);
    IMG_Quit();
    SDL_Quit();
    return 0;
}
