#include "pipeline_interface.h"
#include "image_cleaner.h"
#include "../setup_image/setup_image.h"
#include "../rotation/rotation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

SDL_Surface* preprocess_surface(SDL_Surface* original) {
    if (!original) return NULL;
    
    printf("\n=== Prétraitement de l'image ===\n");
    
    // Copier la surface pour ne pas modifier l'original
    SDL_Surface* preprocessed = SDL_ConvertSurface(original, original->format, 0);
    if (!preprocessed) {
        fprintf(stderr, "Erreur: copie surface échouée\n");
        return NULL;
    }
    
    // 1. Conversion en niveaux de gris
    printf("[1/3] Conversion en niveaux de gris...\n");
    convert_to_grayscale(preprocessed);
    
    // 2. Seuillage d'Otsu
    printf("[2/3] Seuillage d'Otsu...\n");
    apply_otsu_thresholding(preprocessed);
    
    // 3. Suppression du bruit
    printf("[3/3] Suppression du bruit...\n");
    apply_noise_removal(preprocessed, 2);
    
    printf("✓ Prétraitement terminé\n\n");
    
    return preprocessed;
}

/**
 * PIPELINE PHASE 1 COMPLÈTE (avec prétraitement)
 * 
 * @param original_surface Surface originale d'Ibrahim
 * @param csv_path Chemin du CSV pour l'IA
 * @param preprocessed_surface Pointeur pour stocker la surface prétraitée (optionnel)
 * @return ExtractionResult à conserver pour phase 2
 */
ExtractionResult* pipeline_phase1_with_preprocessing(
    SDL_Surface* original_surface,
    const char* csv_path,
    SDL_Surface** preprocessed_surface_out
) {
    if (!original_surface || !csv_path) return NULL;
    
    printf("\n╔════════════════════════════════════════╗\n");
    printf("║  PHASE 1 : Prétraitement et CSV       ║\n");
    printf("╚════════════════════════════════════════╝\n");
    
    // Prétraitement
    SDL_Surface* preprocessed = preprocess_surface(original_surface);
    if (!preprocessed) return NULL;
    
    // Extraction et génération CSV
    ExtractionResult* result = surface_to_csv_for_ia(preprocessed, csv_path);
    
    // Retourner la surface prétraitée si demandé (pour debug/save)
    if (preprocessed_surface_out) {
        *preprocessed_surface_out = preprocessed;
    } else {
        SDL_FreeSurface(preprocessed);
    }
    
    return result;
}

/**
 * FONCTION PRINCIPALE POUR IBRAHIM
 * Cette fonction fait tout le pipeline en 2 appels
 */
typedef struct {
    ExtractionResult* extraction;  // À conserver entre phase 1 et 2
    SDL_Surface* preprocessed;     // Surface prétraitée (optionnel)
    char csv_path[256];             // Chemin du CSV généré
    char grid_path[256];            // Chemin du fichier grille
} PipelineContext;

/**
 * PHASE 1 : Ibrahim donne la surface, on génère le CSV
 */
PipelineContext* ibrahim_pipeline_phase1(
    SDL_Surface* input_surface,
    const char* output_dir
) {
    if (!input_surface || !output_dir) return NULL;
    
    PipelineContext* ctx = (PipelineContext*)malloc(sizeof(PipelineContext));
    if (!ctx) return NULL;
    
    // Générer les chemins
    snprintf(ctx->csv_path, sizeof(ctx->csv_path), "%s/grid_for_ia.csv", output_dir);
    snprintf(ctx->grid_path, sizeof(ctx->grid_path), "%s/grid_for_solver.txt", output_dir);
    
    // Lancer phase 1 avec prétraitement
    ctx->extraction = pipeline_phase1_with_preprocessing(
        input_surface,
        ctx->csv_path,
        &ctx->preprocessed  // Conserver la surface prétraitée
    );
    
    if (!ctx->extraction) {
        free(ctx);
        return NULL;
    }
    
    printf("\n>>> CSV prêt pour l'IA : %s\n", ctx->csv_path);
    printf(">>> Attendu : %d lettres (%dx%d grille)\n", 
           ctx->extraction->count, 
           ctx->extraction->rows, 
           ctx->extraction->cols);
    
    return ctx;
}

/**
 * PHASE 2 : L'IA retourne les lettres, on résout et dessine
 */
SDL_Surface* ibrahim_pipeline_phase2(
    PipelineContext* ctx,
    const char* recognized_letters,
    const char** words_to_find,
    int word_count
) {
    if (!ctx || !recognized_letters || !words_to_find) return NULL;
    
    printf("\n╔════════════════════════════════════════╗\n");
    printf("║  PHASE 2 : Résolution et Annotation   ║\n");
    printf("╚════════════════════════════════════════╝\n");
    
    // Vérifier que les lettres ont la bonne longueur
    if ((int)strlen(recognized_letters) != ctx->extraction->count) {
        fprintf(stderr, "Erreur: longueur lettres incorrecte (%zu != %d)\n",
                strlen(recognized_letters), ctx->extraction->count);
        return NULL;
    }
    
    // Utiliser la surface prétraitée (meilleure qualité de dessin)
    SDL_Surface* result = pipeline_phase2_solve_and_draw(
        ctx->preprocessed,
        ctx->extraction,
        recognized_letters,
        words_to_find,
        word_count,
        ctx->grid_path
    );
    
    return result;
}

/**
 * Libérer le contexte du pipeline
 */
void free_pipeline_context(PipelineContext* ctx) {
    if (ctx) {
        if (ctx->extraction) free_extraction_result(ctx->extraction);
        if (ctx->preprocessed) SDL_FreeSurface(ctx->preprocessed);
        free(ctx);
    }
}

/* ========================================
   EXEMPLE D'UTILISATION POUR IBRAHIM
   ======================================== */

/**
 * Fonction à intégrer dans le event_handler d'Ibrahim (touche 'k')
 */
void ibrahim_process_grid(
    struct image_data* data,
    SDL_Surface* surface,
    SDL_Renderer* renderer,
    SDL_Texture* texture
) {
    printf("\n╔════════════════════════════════════════╗\n");
    printf("║  TRAITEMENT DE LA GRILLE (TOUCHE K)   ║\n");
    printf("╚════════════════════════════════════════╝\n");
    
    // === PHASE 1 : Génération du CSV ===
    PipelineContext* ctx = ibrahim_pipeline_phase1(surface, ".");
    if (!ctx) {
        fprintf(stderr, "Erreur phase 1\n");
        return;
    }
    
    // Sauvegarder la surface prétraitée (optionnel)
    save_surface(data, ctx->preprocessed, "preprocessed");
    
    // === ICI : APPEL À VOTRE IA ===
    printf("\n⏸  PAUSE : Appeler votre IA maintenant\n");
    printf("   Fichier : %s\n", ctx->csv_path);
    printf("   Retour attendu : string de %d caractères\n\n", ctx->extraction->count);
    
    // Simulation de l'IA (REMPLACER PAR VOTRE VRAIE IA)
    char* recognized_letters = (char*)malloc((ctx->extraction->count + 1) * sizeof(char));
    
    // TODO: REMPLACER PAR APPEL À VOTRE IA
    // recognized_letters = your_ia_function(ctx->csv_path);
    
    // Pour l'exemple, on simule une grille
    for (int i = 0; i < ctx->extraction->count; i++) {
        recognized_letters[i] = 'A' + (i % 26);
    }
    recognized_letters[ctx->extraction->count] = '\0';
    
    printf("✓ IA terminée : %s\n", recognized_letters);
    
    // === PHASE 2 : Résolution et annotation ===
    const char* words[] = {"HELLO", "WORLD", "CODE", "GRID", "SEARCH"};
    int word_count = 5;
    
    SDL_Surface* annotated = ibrahim_pipeline_phase2(
        ctx,
        recognized_letters,
        words,
        word_count
    );
    
    if (annotated) {
        // Sauvegarder le résultat
        save_surface(data, annotated, "solved");
        
        // Afficher dans l'interface
        actualize_rendering(renderer, texture, annotated);
        
        printf("\n✓ Traitement terminé !\n");
        printf("  Surface annotée affichée\n");
        printf("  Fichiers sauvegardés :\n");
        printf("    - %s_preprocessed.%s\n", data->name, data->filetype);
        printf("    - %s_solved.%s\n", data->name, data->filetype);
        printf("    - grid_for_solver.txt\n");
        
        SDL_FreeSurface(annotated);
    }
    
    // Cleanup
    free(recognized_letters);
    free_pipeline_context(ctx);
}


/* ========================================
   MODIFICATION DU EVENT_HANDLER
   ======================================== */

/**
 * NOUVEAU event_handler avec touche 'k' intégrée
 * Remplace la fonction dans setup_image.c
 */
int enhanced_event_handler(struct image_data *data, SDL_Renderer *renderer, SDL_Texture *texture,SDL_Surface **surface) {
    SDL_Event event;
    
    int prs_s = 0;
    int prs_ctrl = 0;
    
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                return 0;
                
            case SDL_KEYDOWN:
                switch(event.key.keysym.sym) {
                    case SDLK_c: // Reload Image
                        load_in_surface(data, surface);
                        actualize_rendering(renderer, texture, *surface);
                        break;
                        
                    case SDLK_r: // Rotate
                        {
                            double value = auto_deskew_correction(*surface);
                            *surface = rotate(*surface, value);
                            save_surface(data, *surface, "rotation");
                            actualize_rendering(renderer, texture, *surface);
                        }
                        break;
                        
                    case SDLK_g: // Grayscale
                        convert_to_grayscale(*surface);
                        save_surface(data, *surface, "grayscale");
                        actualize_rendering(renderer, texture, *surface);
                        break;
                        
                    case SDLK_h: // Otsu
                        apply_otsu_thresholding(*surface);
                        save_surface(data, *surface, "otsu_thresholding");
                        actualize_rendering(renderer, texture, *surface);
                        break;
                        
                    case SDLK_j: // Noise removal
                        apply_noise_removal(*surface, 2);
                        save_surface(data, *surface, "noise_removal");
                        actualize_rendering(renderer, texture, *surface);
                        break;
                        
                    case SDLK_k: // *** NOUVELLE FONCTIONNALITÉ ***
                        // Pipeline complet : prétraitement + IA + solver + dessin
                        ibrahim_process_grid(data, *surface, renderer, texture);
                        break;
                        
                    case SDLK_LCTRL:
                        prs_ctrl = 1;
                        break;
                        
                    case SDLK_s:
                        prs_s = 1;
                        break;
                }
                break;
        }
    }
    
    if (prs_s && prs_ctrl) save_sketch(data, renderer, "output");
    return 1;
}



int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <image.png>\n", argv[0]);
        return 1;
    }
    
    // Initialisation SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    
    if (!(IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) & (IMG_INIT_PNG | IMG_INIT_JPG))) {
        fprintf(stderr, "IMG_Init: %s\n", IMG_GetError());
        SDL_Quit();
        return 1;
    }
    
    // Charger l'image
    SDL_Surface* surface = IMG_Load(argv[1]);
    if (!surface) {
        fprintf(stderr, "IMG_Load: %s\n", IMG_GetError());
        IMG_Quit();
        SDL_Quit();
        return 1;
    }
    
    // Créer un contexte factice
    struct image_data data;
    fill_data(&data, argv[1]);
    
    // Simuler le traitement (sans renderer/texture)
    printf("Test du pipeline...\n");
    
    PipelineContext* ctx = ibrahim_pipeline_phase1(surface, ".");
    if (ctx) {
        // Simuler IA
        char* letters = (char*)malloc((ctx->extraction->count + 1) * sizeof(char));
        for (int i = 0; i < ctx->extraction->count; i++) {
            letters[i] = 'A' + (i % 26);
        }
        letters[ctx->extraction->count] = '\0';
        
        // Phase 2
        const char* words[] = {"TEST"};
        SDL_Surface* result = ibrahim_pipeline_phase2(ctx, letters, words, 1);
        
        if (result) {
            IMG_SavePNG(result, "test_result.png");
            printf("Résultat sauvegardé : test_result.png\n");
            SDL_FreeSurface(result);
        }
        
        free(letters);
        free_pipeline_context(ctx);
    }
    
    SDL_FreeSurface(surface);
    IMG_Quit();
    SDL_Quit();
    
    return 0;
}
