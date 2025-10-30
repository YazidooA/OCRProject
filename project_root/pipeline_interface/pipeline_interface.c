#include "pipeline_interface.h"
#include "image_cleaner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ========== PREPROCESSING ==========

// Forward declaration for function in pipeline_implementation.c
SDL_Surface* pipeline_complete_with_nn(
    SDL_Surface* surface,
    const char* model_path,
    const char** words_to_find,
    int word_count,
    const char* output_dir
);

SDL_Surface* preprocess_surface(SDL_Surface* original) {
    if (!original) return NULL;
    
    printf("\n=== Prétraitement de l'image ===\n");
    
    SDL_Surface* preprocessed = SDL_ConvertSurface(original, original->format, 0);
    if (!preprocessed) {
        fprintf(stderr, "Erreur: copie surface échouée\n");
        return NULL;
    }
    
    printf("[1/3] Conversion en niveaux de gris...\n");
    convert_to_grayscale(preprocessed);
    
    printf("[2/3] Seuillage d'Otsu...\n");
    apply_otsu_thresholding(preprocessed);
    
    printf("[3/3] Suppression du bruit...\n");
    apply_noise_removal(preprocessed, 2);
    
    printf("✓ Prétraitement terminé\n\n");
    
    return preprocessed;
}

// ========== PIPELINE CONTEXT ==========

struct PipelineContext {
    ExtractionResult* extraction;
    SDL_Surface* preprocessed;
    char csv_path[256];
    char grid_path[256];
};

void free_pipeline_context(PipelineContext* ctx) {
    if (ctx) {
        if (ctx->extraction) free_extraction_result(ctx->extraction);
        if (ctx->preprocessed) SDL_FreeSurface(ctx->preprocessed);
        free(ctx);
    }
}

// ========== PHASE 1 WITH PREPROCESSING ==========

ExtractionResult* pipeline_phase1_with_preprocessing(
    SDL_Surface* original_surface,
    const char* csv_path,
    SDL_Surface** preprocessed_surface_out
) {
    if (!original_surface || !csv_path) return NULL;
    
    printf("\n╔════════════════════════════════════════╗\n");
    printf("║  PHASE 1 : Prétraitement et Extraction║\n");
    printf("╚════════════════════════════════════════╝\n");
    
    // Preprocessing
    SDL_Surface* preprocessed = preprocess_surface(original_surface);
    if (!preprocessed) return NULL;
    
    // Extraction (uses implementation from pipeline_implementation.c)
    ExtractionResult* result = surface_to_letter_grid(preprocessed);
    
    // Save CSV if needed (optional - deprecated)
    if (result && csv_path) {
        FILE* csv = fopen(csv_path, "w");
        if (csv) {
            fprintf(csv, "id");
            for (int i = 0; i < 784; i++) fprintf(csv, ",p%d", i);
            fprintf(csv, ",label\n");
            
            for (int i = 0; i < result->count; i++) {
                fprintf(csv, "%d", i);
                for (int p = 0; p < 784; p++) {
                    fprintf(csv, ",%d", result->letters[i].pixels[p]);
                }
                fprintf(csv, ",0\n");
            }
            fclose(csv);
            printf("✓ CSV écrit: %s\n", csv_path);
        }
    }
    
    // Return preprocessed surface if requested
    if (preprocessed_surface_out) {
        *preprocessed_surface_out = preprocessed;
    } else {
        SDL_FreeSurface(preprocessed);
    }
    
    return result;
}

// ========== IBRAHIM PHASE 1 ==========

PipelineContext* ibrahim_pipeline_phase1(
    SDL_Surface* input_surface,
    const char* output_dir
) {
    if (!input_surface || !output_dir) return NULL;
    
    PipelineContext* ctx = (PipelineContext*)malloc(sizeof(PipelineContext));
    if (!ctx) return NULL;
    
    // Generate paths
    snprintf(ctx->csv_path, sizeof(ctx->csv_path), "%s/grid_for_ia.csv", output_dir);
    snprintf(ctx->grid_path, sizeof(ctx->grid_path), "%s/grid_for_solver.txt", output_dir);
    
    // Launch phase 1 with preprocessing
    ctx->extraction = pipeline_phase1_with_preprocessing(
        input_surface,
        ctx->csv_path,
        &ctx->preprocessed
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

// ========== IBRAHIM PHASE 2 ==========

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
    
    // Verify letter count
    if ((int)strlen(recognized_letters) != ctx->extraction->count) {
        fprintf(stderr, "Erreur: longueur lettres incorrecte (%zu != %d)\n",
                strlen(recognized_letters), ctx->extraction->count);
        return NULL;
    }
    
    // Use implementation from pipeline_implementation.c
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

// ========== COMPLETE PIPELINE WITH NN (wrapper) ==========

SDL_Surface* pipeline_complete(
    SDL_Surface* input_surface,
    const char* model_path,
    const char** words_to_find,
    int word_count,
    const char* output_dir
) {
    if (!input_surface || !model_path) return NULL;
    
    printf("\n╔════════════════════════════════════════╗\n");
    printf("║  PIPELINE COMPLET AVEC RDN             ║\n");
    printf("╚════════════════════════════════════════╝\n");
    
    // Use implementation from pipeline_implementation.c
    return pipeline_complete_with_nn(
        input_surface,
        model_path,
        words_to_find,
        word_count,
        output_dir ? output_dir : "."
    );
}



int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <image.png> <model.bin> [word1 word2 ...]\n", argv[0]);
        fprintf(stderr, "Example: %s grid.png model.bin HELLO WORLD\n", argv[0]);
        return 1;
    }
    
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    
    int flags = IMG_INIT_PNG | IMG_INIT_JPG;
    if (!(IMG_Init(flags) & flags)) {
        fprintf(stderr, "IMG_Init: %s\n", IMG_GetError());
        SDL_Quit();
        return 1;
    }
    
    // Load image
    const char* image_path = argv[1];
    const char* model_path = argv[2];
    
    SDL_Surface* surface = IMG_Load(image_path);
    if (!surface) {
        fprintf(stderr, "IMG_Load: %s\n", IMG_GetError());
        IMG_Quit();
        SDL_Quit();
        return 1;
    }
    
    printf("✓ Image chargée: %s (%dx%d)\n", image_path, surface->w, surface->h);
    
    // Prepare word list
    int word_count = (argc > 3) ? (argc - 3) : 1;
    const char** words = (const char**)malloc(word_count * sizeof(char*));
    
    if (argc > 3) {
        for (int i = 0; i < word_count; i++) {
            words[i] = argv[3 + i];
        }
    } else {
        words[0] = "TEST";
    }
    
    printf("Mots à chercher: ");
    for (int i = 0; i < word_count; i++) {
        printf("%s ", words[i]);
    }
    printf("\n");
    
    // Run complete pipeline
    SDL_Surface* result = pipeline_complete(
        surface,
        model_path,
        words,
        word_count,
        "."
    );
    
    if (result) {
        const char* output_path = "pipeline_result.png";
        if (IMG_SavePNG(result, output_path) == 0) {
            printf("\n✓ Résultat sauvegardé: %s\n", output_path);
        } else {
            fprintf(stderr, "Erreur sauvegarde: %s\n", IMG_GetError());
        }
        SDL_FreeSurface(result);
    } else {
        fprintf(stderr, "Erreur: pipeline échoué\n");
    }
    
    // Cleanup
    free(words);
    SDL_FreeSurface(surface);
    IMG_Quit();
    SDL_Quit();
    
    return result ? 0 : 1;
}

