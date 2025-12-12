// rotation.c
#include "rotation.h"

#include <stdio.h>      // fprintf
#include <math.h>       // cos, sin, hypot, lrint
#include <stdlib.h>     // malloc, calloc, free

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define DEG2RAD(a) ((a) * (M_PI / 180.0))

/* ---------------------------------------------------------------------------
 * rotate
 *  Simple nearest-neighbor rotation around the image center.
 *  - surface: input image (not modified)
 *  - angle  : rotation angle in degrees (positive = counter-clockwise)
 *  Returns a new SDL_Surface with the same size as input.
 * -------------------------------------------------------------------------- */
SDL_Surface *rotate(SDL_Surface *surface, double angle) {
    if (!surface) return NULL;

    double rad = DEG2RAD(angle);           // convert angle to radians
    int w = surface->w, h = surface->h;

    // Create target surface with same format and size
    SDL_Surface *rotated = SDL_CreateRGBSurface(
        0, w, h,
        surface->format->BitsPerPixel,
        surface->format->Rmask,
        surface->format->Gmask,
        surface->format->Bmask,
        surface->format->Amask
    );
    if (!rotated) {
        fprintf(stderr, "rotate: SDL_CreateRGBSurface: %s\n", SDL_GetError());
        return NULL;
    }

    int cx = w / 2;                         // rotation center x
    int cy = h / 2;                         // rotation center y

    Uint32 *src = (Uint32*)surface->pixels;
    Uint32 *dst = (Uint32*)rotated->pixels;
    int src_pitch = surface->pitch / 4;     // pixels per row in src
    int dst_pitch = rotated->pitch / 4;     // pixels per row in dst

    double c = cos(rad), s = sin(rad);      // precompute trig

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int xr = x - cx;                // coords relative to center
            int yr = y - cy;

            // Backward mapping: dest -> source
            int xs = (int)( c * xr + s * yr + cx );
            int ys = (int)(-s * xr + c * yr + cy );

            // If source coords inside image, copy pixel, else put black
            if (xs >= 0 && xs < w && ys >= 0 && ys < h)
                dst[y * dst_pitch + x] = src[ys * src_pitch + xs];
            else
                dst[y * dst_pitch + x] = 0; // ARGB = fully transparent black
        }
    }

    return rotated;
}

/* ---------------------------------------------------------------------------
 * auto_deskew_correction
 *  Estimate global skew angle of a document-like image using a Hough-based
 *  line energy measure (coarse + fine search).
 *
 *  Steps:
 *    1) Convert to 32bpp ARGB and downscale if too wide (speed).
 *    2) Coarse Hough: x ∈ [-90°,90°], step 1° -> find best orientation.
 *    3) Fine Hough around best x in [x-1.5°, x+1.5°], step 0.1°.
 *    4) Fold angle around closest multiple of 90° to get small correction.
 *
 *  Returns the correction angle in degrees: rotate(surface, angle) ≈ deskew.
 * -------------------------------------------------------------------------- */
double auto_deskew_correction(SDL_Surface *surface) {
    if (!surface) return 0.0;

    /* 1) Force 32 bpp + optional downscale (for speed) */
    SDL_Surface *s32 = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ARGB8888, 0);
    if (!s32) return 0.0;

    int W = s32->w, H = s32->h;
    int maxw = 1000;                        // max width for analysis
    double scale = (W > maxw) ? (double)maxw / (double)W : 1.0;
    int w = (int)lrint(W * scale);
    int h = (int)lrint(H * scale);

    SDL_Surface *small = SDL_CreateRGBSurfaceWithFormat(
        0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
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
    const Uint32 *pix   = (const Uint32*)small->pixels;
    int pitch           = small->pitch / 4; // pixels per row

    /* 2) Coarse Hough: theta ∈ [-90°, 90°], step 1° */
    int th_start = -90, th_end = 90, th_step = 1;
    int th_bins  = (th_end - th_start) / th_step + 1;

    int rmax  = (int)ceil(hypot((double)w, (double)h));
    int rbins = 2 * rmax + 1;               // rho bins in [-rmax, rmax]

    // Precompute cos/sin for all coarse angles
    double *ctab = (double*)malloc(sizeof(double) * th_bins);
    double *stab = (double*)malloc(sizeof(double) * th_bins);
    if (!ctab || !stab) {
        free(ctab); free(stab);
        SDL_UnlockSurface(small);
        SDL_FreeSurface(small);
        return 0.0;
    }
    for (int i = 0; i < th_bins; ++i) {
        double th = DEG2RAD(th_start + i*th_step);
        ctab[i] = cos(th);
        stab[i] = sin(th);
    }

    // (theta, rho) accumulator
    int *acc = (int*)calloc((size_t)th_bins * rbins, sizeof(int));
    if (!acc) {
        free(ctab); free(stab);
        SDL_UnlockSurface(small);
        SDL_FreeSurface(small);
        return 0.0;
    }

    // Edge selection: black pixel with at least one white neighbor
    for (int y = 1; y < h - 1; ++y) {
        const Uint32 *row = pix + y * pitch;
        for (int x = 1; x < w - 1; ++x) {
            Uint32 v = row[x];
            Uint8  a = (Uint8)(v >> 24);    // alpha
            Uint8  R = (Uint8)(v >> 16);    // red as luminance proxy

            if (a < 128 || R >= 128)        // not solid black-ish
                continue;

            Uint32 vL = row[x-1], vR = row[x+1];
            Uint32 vU = pix[(y-1)*pitch + x];
            Uint32 vD = pix[(y+1)*pitch + x];

            // If all neighbors also dark, it's a filled region, not an edge
            if (((Uint8)(vL >> 16) < 128) && ((Uint8)(vR >> 16) < 128) &&
                ((Uint8)(vU >> 16) < 128) && ((Uint8)(vD >> 16) < 128))
                continue;

            // Vote for all theta bins
            for (int ti = 0; ti < th_bins; ++ti) {
                double rho = x * ctab[ti] + y * stab[ti];
                int rbin = (int)lrint(rho) + rmax;
                if ((unsigned)rbin < (unsigned)rbins)
                    acc[ti*rbins + rbin] += 1;
            }
        }
    }

    // Energy per theta: sum of squares over all rho
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

    /* 3) Fine search around best theta (1D Hough per angle) */
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

        for (int y = 1; y < h - 1; ++y) {
            const Uint32 *row = pix + y * pitch;
            for (int x = 1; x < w - 1; ++x) {
                Uint32 v = row[x];
                Uint8  a = (Uint8)(v >> 24);
                Uint8  R = (Uint8)(v >> 16);
                if (a < 128 || R >= 128) continue;

                Uint32 vL = row[x-1], vR = row[x+1];
                Uint32 vU = pix[(y-1)*pitch + x];
                Uint32 vD = pix[(y+1)*pitch + x];
                if (((Uint8)(vL >> 16) < 128) && ((Uint8)(vR >> 16) < 128) &&
                    ((Uint8)(vU >> 16) < 128) && ((Uint8)(vD >> 16) < 128))
                    continue;

                int rbin = (int)lrint(x * c + y * s) + rmax;
                if ((unsigned)rbin < (unsigned)rbins) accR[rbin] += 1;
            }
        }

        long long e = 0;
        for (int r = 0; r < rbins; ++r) {
            int v = accR[r];
            e += (long long)v * (long long)v;
        }
        double E = (double)e;
        if (E > best_energy) {
            best_energy = E;
            best_theta_fine = thdeg;
        }

        free(accR);
    }

    SDL_UnlockSurface(small);
    SDL_FreeSurface(small);

    /* 4) Fold angle to closest horizontal / vertical direction */
    double nearest90 = 90.0 * round(best_theta_fine / 90.0); // { ...,-90,0,90,... }
    double skew = best_theta_fine - nearest90;               // in ~[-45,45]
    double corr = -skew;                                     // correction angle

    // Normalize to (-90,90]
    if (corr >  90.0) corr -= 180.0;
    if (corr <= -90.0) corr += 180.0;

    return corr;
}
