#include "draw_outline.h"

/* Draw a quad outline around a word.*/
void draw_outline(SDL_Renderer *renderer,
                            int x1, int y1, int x2, int y2,
                            int width, int stroke)
{
    // Calculate the perpendicular points to create a rectangle
    float angle = atan2(y2 - y1, x2 - x1);

    int x3 = x1 + width * cos(angle + M_PI / 2);
    int y3 = y1 + width * sin(angle + M_PI / 2);
    int x4 = x2 + width * cos(angle + M_PI / 2);
    int y4 = y2 + width * sin(angle + M_PI / 2);

    // Draw rectangle with specified stroke width
    for (int t = 0; t < stroke; t++) {
        SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
        SDL_RenderDrawLine(renderer, x2, y2, x4, y4);
        SDL_RenderDrawLine(renderer, x4, y4, x3, y3);
        SDL_RenderDrawLine(renderer, x3, y3, x1, y1);
    }
}



int main(int argc, char *argv[])
{
    if (argc != 3) {
        printf("Usage: %s <input.png> <output.png>\n", argv[0]);
        return 1;
    }

    // Initialisation SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "Erreur SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        fprintf(stderr, "Erreur IMG_Init: %s\n", IMG_GetError());
        SDL_Quit();
        return 1;
    }

    // Charger l'image
    SDL_Surface *image = IMG_Load(argv[1]);
    if (!image) {
        fprintf(stderr, "Erreur chargement image: %s\n", IMG_GetError());
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    // Créer un renderer logiciel lié à l'image
    SDL_Renderer *renderer = SDL_CreateSoftwareRenderer(image);
    if (!renderer) {
        fprintf(stderr, "Erreur création renderer: %s\n", SDL_GetError());
        SDL_FreeSurface(image);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    // RED
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);

    // Exemple : dessiner un rectangle incliné
    // Ligne base (50,50) -> (200,80), largeur 20, trait 2 px
    draw_outline(renderer, 50, 50, 200, 80, 20, 2);

    // save
    if (IMG_SavePNG(image, argv[2]) != 0) {
        fprintf(stderr, "Erreur sauvegarde image: %s\n", IMG_GetError());
    } else {
        printf("Image sauvegardée dans %s\n", argv[2]);
    }

    // clean
    SDL_DestroyRenderer(renderer);
    SDL_FreeSurface(image);
    IMG_Quit();
    SDL_Quit();

    return 0;
}
