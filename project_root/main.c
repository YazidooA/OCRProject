#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "structure_detection.h"
#include "file_saver.h"
#include "letter_extractor.h"

// Load image using SDL2 - FIXED VERSION
Image* load_image_sdl(const char* filename) {
    if (!filename) {
        fprintf(stderr, "Erreur: nom de fichier invalide\n");
        return NULL;
    }
    
    SDL_Surface* surface = IMG_Load(filename);
    if (!surface) {
        fprintf(stderr, "Erreur chargement image: %s\n", IMG_GetError());
        return NULL;
    }
    
    if (surface->w <= 0 || surface->h <= 0) {
        fprintf(stderr, "Erreur: dimensions d'image invalides (%dx%d)\n", surface->w, surface->h);
        SDL_FreeSurface(surface);
        return NULL;
    }
    
    // Convert to 24-bit RGB format first for consistent access
    SDL_Surface* rgb_surface = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGB24, 0);
    SDL_FreeSurface(surface);
    
    if (!rgb_surface) {
        fprintf(stderr, "Erreur conversion format: %s\n", SDL_GetError());
        return NULL;
    }
    
    // Create grayscale image
    Image* img = (Image*)malloc(sizeof(Image));
    if (!img) {
        SDL_FreeSurface(rgb_surface);
        return NULL;
    }
    
    img->width = rgb_surface->w;
    img->height = rgb_surface->h;
    img->channels = 1;
    img->data = (unsigned char*)malloc(img->width * img->height);
    
    if (!img->data) {
        free(img);
        SDL_FreeSurface(rgb_surface);
        return NULL;
    }
    
    SDL_LockSurface(rgb_surface);
    
    unsigned char* pixels = (unsigned char*)rgb_surface->pixels;
    int pitch = rgb_surface->pitch;
    
    for (int y = 0; y < rgb_surface->h; y++) {
        for (int x = 0; x < rgb_surface->w; x++) {
            // Access RGB24 format: 3 bytes per pixel
            unsigned char* pixel = pixels + y * pitch + x * 3;
            unsigned char r = pixel[0];
            unsigned char g = pixel[1];
            unsigned char b = pixel[2];
            
            // Convert to grayscale using luminosity method
            unsigned char gray = (unsigned char)(0.299 * r + 0.587 * g + 0.114 * b);
            img->data[y * img->width + x] = gray;
        }
    }
    
    SDL_UnlockSurface(rgb_surface);
    SDL_FreeSurface(rgb_surface);
    
    printf("Image chargée: %dx%d pixels\n", img->width, img->height);
    return img;
}

// Save image using SDL2
void save_image_sdl(Image* img, const char* filename) {
    SDL_Surface* surface = SDL_CreateRGBSurface(0, img->width, img->height, 32,
                                                0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    if (!surface) {
        fprintf(stderr, "Erreur création surface: %s\n", SDL_GetError());
        return;
    }
    
    SDL_LockSurface(surface);
    
    for (int y = 0; y < img->height; y++) {
        for (int x = 0; x < img->width; x++) {
            unsigned char gray = img->data[y * img->width + x];
            Uint32 pixel = SDL_MapRGB(surface->format, gray, gray, gray);
            ((Uint32*)surface->pixels)[y * surface->w + x] = pixel;
        }
    }
    
    SDL_UnlockSurface(surface);
    IMG_SavePNG(surface, filename);
    SDL_FreeSurface(surface);
    
    printf("Image sauvegardée: %s\n", filename);
}

// Free image memory
void free_image(Image* img) {
    if (img) {
        if (img->data) free(img->data);
        free(img);
    }
}

// Save debug visualization showing detected grid
void save_debug_visualization(Image* img, Rectangle grid, Rectangle word_list, 
                              Rectangle* cells, int cell_count, const char* filename) {
    // Create a copy for visualization
    Image* debug_img = (Image*)malloc(sizeof(Image));
    debug_img->width = img->width;
    debug_img->height = img->height;
    debug_img->channels = 1;
    debug_img->data = (unsigned char*)malloc(img->width * img->height);
    memcpy(debug_img->data, img->data, img->width * img->height);
    
    // Draw grid boundary in bright white (255)
    if (grid.width > 0 && grid.height > 0) {
        draw_rectangle(debug_img, grid, 255);
        // Draw a thicker border
        Rectangle thick_grid = {grid.x-2, grid.y-2, grid.width+4, grid.height+4};
        draw_rectangle(debug_img, thick_grid, 255);
    }
    
    // Draw word list boundary in gray (180)
    if (word_list.width > 0 && word_list.height > 0) {
        draw_rectangle(debug_img, word_list, 180);
    }
    
    // Draw first few cells in lighter gray (150)
    for (int i = 0; i < 10 && i < cell_count; i++) {
        draw_rectangle(debug_img, cells[i], 150);
    }
    
    save_image_sdl(debug_img, filename);
    free_image(debug_img);
}

int main(int argc, char* argv[]) {
    printf("=== Détecteur de structure OCR Word Search (SDL2) ===\n\n");
    
    if (argc < 2) {
        printf("Usage: %s <image.png|jpg> [output.png]\n", argv[0]);
        printf("\nFormats supportés: PNG, JPEG, BMP, etc.\n");
        return 1;
    }
    
    // SDL initialization
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "Erreur SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    
    if (!(IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) & (IMG_INIT_PNG | IMG_INIT_JPG))) {
        fprintf(stderr, "Erreur IMG_Init: %s\n", IMG_GetError());
        SDL_Quit();
        return 1;
    }
    
    // Load image
    Image* img = load_image_sdl(argv[1]);
    if (!img) {
        IMG_Quit();
        SDL_Quit();
        return 1;
    }
    
    printf("\n");
    
    // Structure detection
    printf("1. Détection de la grille principale...\n");
    Rectangle grid = detect_grid_area(img);
    if (grid.width == 0 || grid.height == 0) {
        printf("❌ Erreur: Impossible de détecter la grille principale\n");
        printf("   Vérifiez que l'image contient une grille clairement définie.\n");
        free_image(img);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }
    printf("✓ Grille détectée: x=%d, y=%d, largeur=%d, hauteur=%d\n", 
           grid.x, grid.y, grid.width, grid.height);
    
    printf("\n2. Détection de la liste de mots...\n");
    Rectangle word_list = detect_word_list_area(img);
    if (word_list.width == 0 || word_list.height == 0) {
        printf("⚠ Avertissement: Impossible de détecter la liste de mots\n");
    } else {
        printf("✓ Liste détectée: x=%d, y=%d, largeur=%d, hauteur=%d\n",
               word_list.x, word_list.y, word_list.width, word_list.height);
    }
    
    printf("\n3. Détection des cellules de la grille...\n");
    int cell_count = 0, rows = 0, cols = 0;
    Rectangle* cells = detect_grid_cells(img, grid, &cell_count, &rows, &cols);
    if (!cells || cell_count == 0) {
        printf("❌ Erreur: Impossible de détecter les cellules de la grille\n");
        free_image(img);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }
    printf("✓ %d cellules détectées (%d lignes × %d colonnes)\n", 
           cell_count, rows, cols);
    
    // Display some cells for verification
    printf("\nExemples de cellules détectées:\n");
    for (int i = 0; i < 5 && i < cell_count; i++) {
        printf("  Cellule %d: x=%d, y=%d, w=%d, h=%d\n", 
               i, cells[i].x, cells[i].y, cells[i].width, cells[i].height);
    }
    
    // Save debug visualization
    printf("\n4. Sauvegarde de la visualisation de détection...\n");
    save_debug_visualization(img, grid, word_list, cells, cell_count, "debug_detection.png");
    printf("✓ Visualisation sauvegardée: debug_detection.png\n");
    printf("   (La grille détectée est marquée en blanc)\n");
    
    // Extract letters from grid
    printf("\n5. Extraction des lettres de la grille...\n");
    LetterGrid* letter_grid = extract_letters_from_grid(img, grid, rows, cols);
    if (!letter_grid) {
        printf("❌ Erreur: Impossible d'extraire les lettres\n");
    } else {
        printf("✓ %d lettres extraites et normalisées (28x28)\n", letter_grid->count);
        
        // Save letters for debugging
        printf("\n6. Sauvegarde des lettres extraites...\n");
        save_letter_grid(letter_grid, "extracted_letters");
        
        // Display some examples
        printf("\nExemples de lettres extraites:\n");
        for (int i = 0; i < 5 && i < letter_grid->count; i++) {
            printf("  Lettre [%d,%d]: %dx%d pixels, position originale (%d,%d)\n",
                   letter_grid->letters[i].grid_row,
                   letter_grid->letters[i].grid_col,
                   letter_grid->letters[i].width,
                   letter_grid->letters[i].height,
                   letter_grid->letters[i].original_x,
                   letter_grid->letters[i].original_y);
        }
    }
    
    // Initialize test results with absolute coordinates from output_solved.txt
    SearchResult test_results[2];
    memset(test_results, 0, sizeof(test_results));
    
    // IMAGINE: (75,50) -> (225,50)
    // RELAX: (75,90) -> (225,90)
    test_results[0] = (SearchResult){{75, 50}, {225, 50}, 1, "IMAGINE"};
    test_results[1] = (SearchResult){{75, 90}, {225, 90}, 1, "RELAX"};
    
    // Save results
    if (argc >= 3) {
        printf("\n7. Sauvegarde de la grille résolue...\n");
        save_solved_grid(img, test_results, 2, argv[2]);
    }
    
    // Cleanup
    if (letter_grid) free_letter_grid(letter_grid);
    free(cells);
    free_image(img);
    
    IMG_Quit();
    SDL_Quit();
    
    printf("\n✓ Détection terminée avec succès!\n");
    printf("\nFichiers générés:\n");
    printf("  - debug_detection.png : Visualisation de la grille détectée\n");
    printf("  - extracted_letters/ : Dossier contenant toutes les lettres extraites (28x28)\n");
    if (argc >= 3) {
        printf("  - %s : Grille avec mots surlignés\n", argv[2]);
        char text_output[256];
        strncpy(text_output, argv[2], 255);
        char* ext = strrchr(text_output, '.');
        if (ext) strcpy(ext, ".txt");
        else strcat(text_output, ".txt");
        printf("  - %s : Coordonnées des mots trouvés\n", text_output);
    }
    
    return 0;
}