#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

static void write_csv_header(FILE *f) {
    fprintf(f, "id");
    for (int i = 0; i < 784; ++i) 
        fprintf(f, ",p%d", i);
    fprintf(f, ",label\n");
}

/* Ouvre/Crée le CSV si besoin et retourne le prochain id (nb de lignes de données déjà présentes). */
static int ensure_csv_and_next_id(const char *csv_path) {
    FILE *f = fopen(csv_path, "r");
    if (!f) {
        FILE *g = fopen(csv_path, "w");
        if (!g) { 
            perror("fopen"); 
            exit(1); 
        }
        write_csv_header(g);
        fclose(g);
        return 0;
    }
    char line[4096];
    int lines = 0;
    int has_header = 0;
    if (fgets(line, sizeof(line), f)) {
        has_header = (strncmp(line, "id", 2) == 0);
        lines = has_header ? 0 : 1;
    }
    while (fgets(line, sizeof(line), f)) 
        lines++;
    fclose(f);
    return lines;
}

static inline int ascii_is_alpha(char c){return (c>='A'&&c<='Z')||(c>='a'&&c<='z');}
static inline char up(char c){return (c>='a'&&c<='z')?(char)(c-('a'-'A')):c;}
static const char* base(const char* p){const char* b=p+strlen(p);while(b>p&&b[-1]!='/'&&b[-1]!='\\')--b;return b;}

int parse_label0_25_from_filename(const char *path, int *out){
    const char *b = base(path);
    if (ascii_is_alpha(b[0])) { 
        *out = up(b[0]) - 'A'; 
        return 0; 
    }
    for (const char *p=b; *p && *p!='_'; ++p) 
        if (ascii_is_alpha(*p)){
            *out=up(*p)-'A'; 
            return 0;
        }
    return -1;
}

static void write_csv_row(FILE *f, int id, const uint8_t px[784], int label0_25) {
    fprintf(f, "%d", id);
    for (int i = 0; i < 784; ++i) 
        fprintf(f, ",%u", (unsigned)px[i]);
    fprintf(f, ",%d\n", label0_25);
}

/* Convertit une surface en 28x28 niveaux de gris (784) */
static int surface_to_gray28(SDL_Surface *src, uint8_t out784[784]) {
    if (!src) 
        return -1;
    SDL_Surface *rgba = SDL_ConvertSurfaceFormat(src, SDL_PIXELFORMAT_RGBA32, 0);
    if (!rgba) 
        return -1;

    int w = rgba->w, h = rgba->h;
    int side = (w < h ? w : h);
    SDL_Rect srcRect = { (w - side)/2, (h - side)/2, side, side };

    SDL_Surface *square = SDL_CreateRGBSurfaceWithFormat(0, side, side, 32, SDL_PIXELFORMAT_RGBA32);
    if (!square) { 
        SDL_FreeSurface(rgba); 
        return -1; 
    }
    if (SDL_BlitSurface(rgba, &srcRect, square, NULL) != 0) {
        SDL_FreeSurface(square); SDL_FreeSurface(rgba); return -1;
    }

    SDL_Surface *dst28 = SDL_CreateRGBSurfaceWithFormat(0, 28, 28, 32, SDL_PIXELFORMAT_RGBA32);
    if (!dst28) { 
        SDL_FreeSurface(square); 
        SDL_FreeSurface(rgba); 
        return -1; 
    }
    if (SDL_BlitScaled(square, NULL, dst28, NULL) != 0) {
        SDL_FreeSurface(dst28); 
        SDL_FreeSurface(square); 
        SDL_FreeSurface(rgba); 
        return -1;
    }

    for (int y = 0; y < 28; ++y) {
        uint8_t *row = (uint8_t*)dst28->pixels + y * dst28->pitch;
        for (int x = 0; x < 28; ++x) {
            uint32_t px = ((uint32_t*)row)[x];
            uint8_t r,g,b,a;
            SDL_GetRGBA(px, dst28->format, &r,&g,&b,&a);
            float grayf = 0.299f*r + 0.587f*g + 0.114f*b;
            int gray = (int)(grayf + 0.5f);
            if (gray < 0) {
                gray = 0;
            }
            if (gray > 255) {
                gray = 255;
            }
            out784[y*28 + x] = (uint8_t)gray;
        }
    }
    SDL_FreeSurface(dst28);
    SDL_FreeSurface(square);
    SDL_FreeSurface(rgba);
    return 0;
}

static int imagefile_to_gray28(const char *path, uint8_t out784[784]) {
    SDL_Surface *loaded = IMG_Load(path);
    if (!loaded) 
        return -1;
    int rc = surface_to_gray28(loaded, out784);
    SDL_FreeSurface(loaded);
    return rc;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr,
            "Usage: %s <out.csv> <img1> [img2 ...]\n"
            "Ex:    %s digital_letters.csv  data/A_001.png data/B_42.jpg\n"
            "       (le label est déduit de la première lettre du nom de fichier)\n",
            argv[0], argv[0]);
        return 1;
    }
    const char *outcsv = argv[1];

    if (SDL_Init(SDL_INIT_VIDEO) != 0) { 
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); 
        return 1; 
    }
    int flags = IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_WEBP | IMG_INIT_TIF;
    if ((IMG_Init(flags) & flags) == 0) { 
        fprintf(stderr, "IMG_Init: %s\n", IMG_GetError()); 
        SDL_Quit(); 
        return 1; 
    }

    int next_id = ensure_csv_and_next_id(outcsv);
    FILE *f = fopen(outcsv, "a");
    if (!f) { 
        perror("fopen"); 
        IMG_Quit(); 
        SDL_Quit(); 
        return 1; 
    }

    uint8_t vec[784];
    for (int i = 2; i < argc; ++i) {
        const char *img = argv[i];
        int label = -1;
        if (parse_label0_25_from_filename(img, &label) != 0) {
            fprintf(stderr, "Label introuvable dans '%s' (attendu 'A_123.png'). Skipped.\n", img);
            continue;
        }
        if (imagefile_to_gray28(img, vec) != 0) {
            fprintf(stderr, "Erreur conversion: %s\n", img);
            continue;
        }
        write_csv_row(f, next_id++, vec, label);
        printf("OK: %s -> %s (id=%d, label=%d)\n", img, outcsv, next_id-1, label);
    }

    fclose(f);
    IMG_Quit();
    SDL_Quit();
    return 0;
}
