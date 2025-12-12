#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <SDL2/SDL.h>

/* Lit la ligne 'row' (0 = première ligne de données), récupère 784 pixels. */
static int read_row_pixels(const char *csv, int row, uint8_t pix[784]) {
    FILE *f = fopen(csv, "r");
    if (!f) { perror("fopen"); return -1; }

    char line[20000];

    /* Sauter l'en-tête si présent (commence par "id") */
    long pos = ftell(f);
    if (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "id", 2) != 0) {
            /* Pas d'en-tête -> revenir au début de cette ligne */
            fseek(f, pos, SEEK_SET);
        }
    } else { fclose(f); return -1; }

    int cur = 0;
    while (fgets(line, sizeof(line), f)) {
        if (cur == row) {
            /* tokenise au ',' : id, p0..p783, label */
            char *tok = strtok(line, ",");    /* id */
            if (!tok) { fclose(f); return -1; }

            for (int i = 0; i < 784; ++i) {
                tok = strtok(NULL, ",");
                if (!tok) { fclose(f); return -1; }
                int v = atoi(tok);
                if (v < 0) {
                    v = 0;
                }
                if (v > 255) {
                    v = 255;
                }
                pix[i] = (uint8_t)v;
            }
            /* on ignore le label final */
            fclose(f);
            return 0;
        }
        cur++;
    }

    fclose(f);
    return -1; /* ligne non trouvée */
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <digital_letters.csv> <out.bmp> [row_index]\n", argv[0]);
        fprintf(stderr, "Ex:    %s digital_letters.csv preview.bmp 0\n", argv[0]);
        return 1;
    }
    const char *csv    = argv[1];
    const char *outbmp = argv[2];
    int row = (argc >= 4) ? atoi(argv[3]) : 0;

    uint8_t pix[784];
    if (read_row_pixels(csv, row, pix) != 0) {
        fprintf(stderr, "Echec lecture CSV (row=%d)\n", row);
        return 2;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 3;
    }

    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 28, 28, 32, SDL_PIXELFORMAT_RGBA32);
    if (!s) {
        fprintf(stderr, "CreateSurface: %s\n", SDL_GetError());
        SDL_Quit();
        return 3;
    }

    /* calcule un seuil auto = moyenne des 784 niveaux de gris */
    int sum = 0;
    for (int i = 0; i < 784; ++i) sum += pix[i];
    int T = sum / 784;  /* auto-threshold */

    /* puis même boucle que ci-dessus */
    for (int y = 0; y < 28; ++y) {
        uint32_t *rowp = (uint32_t *)((uint8_t*)s->pixels + y * s->pitch);
        for (int x = 0; x < 28; ++x) {
            uint8_t g  = pix[y * 28 + x];
            uint8_t bw = (g >= T) ? 255 : 0;
            rowp[x] = SDL_MapRGBA(s->format, bw, bw, bw, 255);
        }
    }


    if (SDL_SaveBMP(s, outbmp) != 0) {
        fprintf(stderr, "SDL_SaveBMP: %s\n", SDL_GetError());
        SDL_FreeSurface(s);
        SDL_Quit();
        return 4;
    }

    SDL_FreeSurface(s);
    SDL_Quit();
    printf("OK: %s (row %d)\n", outbmp, row);
    return 0;
}
