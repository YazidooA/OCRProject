#include "pipeline_interface.h"
#include "structure_detection.h"
#include "letter_extractor.h"
#include "solver.h"
#include "draw_outline.h"
#include "file_saver.h"
#include "../neural_network/neural_network.h"  // *** AJOUT DU RDN ***
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Helper: free image
static void free_image_local(Image* img) {
    if (img) {
        if (img->data) free(img->data);
        free(img);
    }
}

// Convert SDL_Surface to Image structure
static Image* surface_to_image(SDL_Surface* surface) {
    if (!surface) return NULL;
    
    SDL_Surface* gray = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGB888, 0);
    if (!gray) return NULL;
    
    Image* img = (Image*)malloc(sizeof(Image));
    img->width = gray->w;
    img->height = gray->h;
    img->channels = 1;
    img->data = (unsigned char*)malloc(img->width * img->height);
    
    SDL_LockSurface(gray);
    for (int y = 0; y < img->height; y++) {
        for (int x = 0; x < img->width; x++) {
            Uint32 pixel = ((Uint32*)gray->pixels)[y * (gray->pitch/4) + x];
            Uint8 r, g, b;
            SDL_GetRGB(pixel, gray->format, &r, &g, &b);
            img->data[y * img->width + x] = (unsigned char)(0.299*r + 0.587*g + 0.114*b);
        }
    }
    SDL_UnlockSurface(gray);
    SDL_FreeSurface(gray);
    
    return img;
}

// *** NOUVELLE FONCTION: Extraction avec structure complète ***
ExtractionResult* surface_to_letter_grid(SDL_Surface* surface) {
    if (!surface) return NULL;
    
    printf("\n=== Phase 1: Extraction complète ===\n");
    
    // Convert surface to Image
    Image* img = surface_to_image(surface);
    if (!img) {
        fprintf(stderr, "Failed to convert surface\n");
        return NULL;
    }
    
    // Detect grid
    Rectangle grid = detect_grid_area(img);
    printf("Grid: x=%d, y=%d, w=%d, h=%d\n", grid.x, grid.y, grid.width, grid.height);
    
    // Detect cells
    int rows, cols, cell_count;
    Rectangle* cells = detect_grid_cells(img, grid, &cell_count, &rows, &cols);
    if (!cells) {
        free_image_local(img);
        return NULL;
    }
    
    printf("Detected: %d cells (%dx%d)\n", cell_count, rows, cols);
    
    // Extract letters with proper normalization (28x28)
    LetterGrid* letter_grid = extract_letters_from_grid(img, grid, rows, cols);
    if (!letter_grid) {
        free(cells);
        free_image_local(img);
        return NULL;
    }
    
    // Create result structure compatible with existing interface
    ExtractionResult* result = (ExtractionResult*)malloc(sizeof(ExtractionResult));
    result->count = letter_grid->count;
    result->rows = letter_grid->rows;
    result->cols = letter_grid->cols;
    result->grid_area = grid;
    result->letters = (ExtractedLetter*)malloc(cell_count * sizeof(ExtractedLetter));
    
    // Copy letter data into result structure
    for (int i = 0; i < cell_count; i++) {
        result->letters[i].id = i;
        result->letters[i].grid_row = letter_grid->letters[i].grid_row;
        result->letters[i].grid_col = letter_grid->letters[i].grid_col;
        result->letters[i].pixel_x = letter_grid->letters[i].original_x;
        result->letters[i].pixel_y = letter_grid->letters[i].original_y;
        
        // Copy 28x28 normalized pixels
        memcpy(result->letters[i].pixels, letter_grid->letters[i].data, 784);
    }
    
    // Store the letter_grid pointer for later use
    result->letter_grid_internal = letter_grid;
    
    free(cells);
    free_image_local(img);
    
    printf("✓ Extraction terminée: %d lettres (28x28 normalisées)\n", cell_count);
    
    return result;
}

// *** ANCIENNE FONCTION (deprecated) - gardée pour compatibilité ***
ExtractionResult* surface_to_csv_for_ia(SDL_Surface* surface, const char* csv_output_path) {
    if (!surface || !csv_output_path) return NULL;
    
    // Utiliser la nouvelle fonction
    ExtractionResult* result = surface_to_letter_grid(surface);
    if (!result) return NULL;
    
    // Write CSV if requested
    FILE* csv = fopen(csv_output_path, "w");
    if (csv) {
        fprintf(csv, "id");
        for (int i = 0; i < 784; i++) fprintf(csv, ",p%d", i);
        fprintf(csv, ",label\n");
        
        for (int i = 0; i < result->count; i++) {
            fprintf(csv, "%d", i);
            for (int p = 0; p < 784; p++) {
                fprintf(csv, ",%d", result->letters[i].pixels[p]);
            }
            fprintf(csv, ",0\n"); // dummy label
        }
        fclose(csv);
        printf("✓ CSV written: %s\n", csv_output_path);
    }
    
    return result;
}

// Free extraction result
void free_extraction_result(ExtractionResult* result) {
    if (result) {
        if (result->letter_grid_internal) {
            free_letter_grid((LetterGrid*)result->letter_grid_internal);
        }
        if (result->letters) free(result->letters);
        free(result);
    }
}

// *** NOUVELLE FONCTION: Reconnaissance avec RDN ***
char* recognize_letters_with_nn(ExtractionResult* extraction, const char* model_path) {
    if (!extraction || !model_path) return NULL;
    
    printf("\n=== Reconnaissance avec réseau de neurones ===\n");
    printf("Modèle: %s\n", model_path);
    
    // Load neural network model
    Network net;
    init_network(&net);
    
    if (load_model(model_path, &net) != 0) {
        fprintf(stderr, "Erreur: impossible de charger %s\n", model_path);
        fprintf(stderr, "Assurez-vous d'avoir entraîné le modèle avec:\n");
        fprintf(stderr, "  ./nn train training_data.csv model.bin\n");
        free_network(&net);
        return NULL;
    }
    
    printf("✓ Modèle chargé: %d → %d (ReLU) → %d\n", 
           INPUT_SIZE, HIDDEN_SIZE, OUTPUT_SIZE);
    
    // Allocate result string
    char* recognized = (char*)malloc((extraction->count + 1) * sizeof(char));
    if (!recognized) {
        free_network(&net);
        return NULL;
    }
    
    // Buffer for normalized input
    float input_buffer[INPUT_SIZE];
    
    // Recognize each letter
    for (int i = 0; i < extraction->count; i++) {
        // Normalize pixels to [0-1] for neural network
        for (int p = 0; p < INPUT_SIZE; p++) {
            input_buffer[p] = extraction->letters[i].pixels[p] / 255.0f;
        }
        
        // Predict with neural network
        int prediction = predict(&net, input_buffer);
        recognized[i] = 'A' + prediction;
        
        // Progress indicator
        if ((i + 1) % 50 == 0 || i == extraction->count - 1) {
            printf("  Progression: %d/%d lettres\r", i + 1, extraction->count);
            fflush(stdout);
        }
    }
    recognized[extraction->count] = '\0';
    
    printf("\n✓ Reconnaissance terminée: %d lettres\n", extraction->count);
    
    free_network(&net);
    return recognized;
}

// *** NOUVELLE VERSION: Phase 2 avec lettres reconnues par RDN ***
SDL_Surface* pipeline_phase2_solve_and_draw(
    SDL_Surface* surface,
    ExtractionResult* extraction,
    const char* recognized_letters,
    const char** words_to_find,
    int word_count,
    const char* grid_path
) {
    if (!surface || !extraction || !recognized_letters) return NULL;
    
    printf("\n=== Phase 2: Solving ===\n");
    
    int len = strlen(recognized_letters);
    if (len != extraction->count) {
        fprintf(stderr, "Error: letter count mismatch (%d != %d)\n", len, extraction->count);
        return NULL;
    }
    
    // Write grid file for solver
    FILE* grid_file = fopen(grid_path, "w");
    if (!grid_file) {
        fprintf(stderr, "Failed to create grid file\n");
        return NULL;
    }
    
    fprintf(grid_file, "%d %d\n", extraction->rows, extraction->cols);
    for (int i = 0; i < len; i++) {
        fprintf(grid_file, "%c", recognized_letters[i]);
        if ((i + 1) % extraction->cols == 0) fprintf(grid_file, "\n");
        else fprintf(grid_file, " ");
    }
    fclose(grid_file);
    
    printf("✓ Grid file: %s (%dx%d)\n", grid_path, extraction->rows, extraction->cols);
    
    // Display grid preview
    printf("\n=== Grille reconnue (aperçu) ===\n");
    int preview_rows = (extraction->rows > 10) ? 10 : extraction->rows;
    int preview_cols = (extraction->cols > 20) ? 20 : extraction->cols;
    
    for (int r = 0; r < preview_rows; r++) {
        for (int c = 0; c < preview_cols; c++) {
            int idx = r * extraction->cols + c;
            if (idx < len) printf("%c ", recognized_letters[idx]);
        }
        if (extraction->cols > preview_cols) printf("...");
        printf("\n");
    }
    if (extraction->rows > preview_rows) printf("...\n");
    printf("================================\n\n");
    
    // Read grid back for solver
    int rows, cols;
    char** matrix = read_grid_from_file(grid_path, &rows, &cols);
    if (!matrix) {
        fprintf(stderr, "Failed to read grid\n");
        return NULL;
    }
    
    // Create result surface
    SDL_Surface* result = SDL_ConvertSurface(surface, surface->format, 0);
    SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(result);
    if (!renderer) {
        fprintf(stderr, "Failed to create renderer\n");
        freeMatrix(matrix, rows);
        return result;
    }
    
    // Search for each word
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // Red
    
    int found_count = 0;
    for (int w = 0; w < word_count; w++) {
        char* word = (char*)words_to_find[w];
        int out[4];
        
        // Convert word to uppercase
        char upper_word[100];
        for (int i = 0; word[i] && i < 99; i++) {
            upper_word[i] = toupper((unsigned char)word[i]);
            upper_word[i+1] = '\0';
        }
        
        resolution(matrix, rows, cols, upper_word, out);
        
        if (out[0] != -1) {
            // Convert grid coordinates to pixel coordinates
            int cell_w = extraction->grid_area.width / cols;
            int cell_h = extraction->grid_area.height / rows;
            
            int px1 = extraction->grid_area.x + out[0] * cell_w + cell_w/2;
            int py1 = extraction->grid_area.y + out[1] * cell_h + cell_h/2;
            int px2 = extraction->grid_area.x + out[2] * cell_w + cell_w/2;
            int py2 = extraction->grid_area.y + out[3] * cell_h + cell_h/2;
            
            printf("✓ Found '%s' at grid(%d,%d)->(%d,%d) pixel(%d,%d)->(%d,%d)\n", 
                   upper_word, out[0], out[1], out[2], out[3],
                   px1, py1, px2, py2);
            
            // Draw outline
            draw_outline(renderer, px1, py1, px2, py2, cell_h/2, 3);
            found_count++;
        } else {
            printf("✗ Not found: '%s'\n", upper_word);
        }
    }
    
    printf("\n=== Résumé: %d/%d mots trouvés ===\n", found_count, word_count);
    
    SDL_DestroyRenderer(renderer);
    freeMatrix(matrix, rows);
    
    return result;
}

// *** FONCTION COMPLÈTE AVEC RDN ***
SDL_Surface* pipeline_complete_with_nn(
    SDL_Surface* surface,
    const char* model_path,
    const char** words_to_find,
    int word_count,
    const char* output_dir
) {
    if (!surface || !model_path) return NULL;
    
    printf("\n╔════════════════════════════════════════╗\n");
    printf("║  PIPELINE COMPLET AVEC RDN             ║\n");
    printf("╚════════════════════════════════════════╝\n");
    
    // Phase 1: Extract letters
    ExtractionResult* extraction = surface_to_letter_grid(surface);
    if (!extraction) {
        fprintf(stderr, "Erreur phase 1: extraction\n");
        return NULL;
    }
    
    // Phase 1.5: Recognize with neural network
    char* recognized_letters = recognize_letters_with_nn(extraction, model_path);
    if (!recognized_letters) {
        fprintf(stderr, "Erreur: reconnaissance RDN\n");
        free_extraction_result(extraction);
        return NULL;
    }
    
    // Phase 2: Solve and draw
    char grid_path[512];
    snprintf(grid_path, sizeof(grid_path), "%s/grid_for_solver.txt", 
             output_dir ? output_dir : ".");
    
    SDL_Surface* result = pipeline_phase2_solve_and_draw(
        surface,
        extraction,
        recognized_letters,
        words_to_find,
        word_count,
        grid_path
    );
    
    // Cleanup
    free(recognized_letters);
    free_extraction_result(extraction);
    
    printf("\n╔════════════════════════════════════════╗\n");
    printf("║  TRAITEMENT TERMINÉ ✓                  ║\n");
    printf("╚════════════════════════════════════════╝\n");
    
    return result;
}