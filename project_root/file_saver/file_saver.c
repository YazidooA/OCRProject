#include "file_saver.h"

// Simple text file writer
void write_text_file(const char* filename, char** lines, int line_count) {
    FILE* fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "Erreur: impossible d'ouvrir %s\n", filename);
        return;
    }
    
    for (int i = 0; i < line_count; i++) {
        if (lines[i] != NULL) {
            fprintf(fp, "%s\n", lines[i]);
        }
    }
    
    fclose(fp);
    printf("✓ Fichier sauvegardé: %s\n", filename);
}

// Convert SearchResult array to text lines
char** results_to_text_lines(SearchResult* results, int result_count, int* line_count) {
    char** lines = (char**)malloc(result_count * sizeof(char*));
    *line_count = 0;
    
    for (int i = 0; i < result_count; i++) {
        lines[i] = (char*)malloc(256 * sizeof(char));
        
        if (results[i].found) {
            snprintf(lines[i], 256, "%s: (%d,%d) -> (%d,%d)",
                    results[i].word,
                    results[i].start.x, results[i].start.y,
                    results[i].end.x, results[i].end.y);
        } else {
            snprintf(lines[i], 256, "%s: Not found", results[i].word);
        }
        (*line_count)++;
    }
    
    return lines;
}

// Free text lines array
void free_text_lines(char** lines, int line_count) {
    if (lines) {
        for (int i = 0; i < line_count; i++) {
            free(lines[i]);
        }
        free(lines);
    }
}

// Prepare letter image for neural network (normalize to [0-1] or binary)
static void prepare_letter_for_nn(Letter* letter, float* input_buffer) {
    // Le RDN attend des pixels en float [0-1]
    for (int i = 0; i < INPUT_SIZE; i++) {
        // Normaliser de [0-255] vers [0-1]
        input_buffer[i] = letter->data[i] / 255.0f;
    }
}

// Convert LetterGrid to matrix using NEURAL NETWORK recognition
char** letter_grid_to_matrix_with_nn(LetterGrid* grid, Network* net) {
    if (!grid || !net) return NULL;
    
    printf("\n=== Reconnaissance des lettres avec RDN ===\n");
    
    char** matrix = (char**)malloc(grid->rows * sizeof(char*));
    for (int i = 0; i < grid->rows; i++) {
        matrix[i] = (char*)malloc((grid->cols + 1) * sizeof(char));
        matrix[i][grid->cols] = '\0';
    }
    
    float input_buffer[INPUT_SIZE];  // Buffer pour normaliser l'entrée
    
    // Reconnaître chaque lettre avec le RDN
    for (int i = 0; i < grid->count; i++) {
        int row = grid->letters[i].grid_row;
        int col = grid->letters[i].grid_col;
        
        if (row >= 0 && row < grid->rows && col >= 0 && col < grid->cols) {
            // Préparer l'image pour le réseau
            prepare_letter_for_nn(&grid->letters[i], input_buffer);
            
            // Prédiction: retourne 0-25 pour A-Z
            int prediction = predict(net, input_buffer);
            
            // Convertir en caractère
            char recognized_letter = 'A' + prediction;
            matrix[row][col] = recognized_letter;
            
            // Afficher progression tous les 50 caractères
            if ((i + 1) % 50 == 0 || i == grid->count - 1) {
                printf("  Progression: %d/%d lettres reconnues\r", 
                       i + 1, grid->count);
                fflush(stdout);
            }
        }
    }
    
    printf("\n✓ Reconnaissance terminée: %d lettres\n", grid->count);
    
    return matrix;
}

// Save grid matrix to text file (format for solver.c)
void save_grid_matrix(const char* filename, char** matrix, int rows, int cols) {
    FILE* fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "Erreur: impossible d'ouvrir %s\n", filename);
        return;
    }
    
    // Write dimensions first (format expected by solver.c)
    fprintf(fp, "%d %d\n", rows, cols);
    
    // Write grid matrix
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            fprintf(fp, "%c", matrix[r][c]);
            if (c < cols - 1) fprintf(fp, " ");
        }
        fprintf(fp, "\n");
    }
    
    fclose(fp);
    printf("✓ Grille RDN sauvegardée: %s\n", filename);
}

// Free matrix memory
void free_matrix(char** matrix, int rows) {
    if (matrix) {
        for (int i = 0; i < rows; i++) {
            free(matrix[i]);
        }
        free(matrix);
    }
}

// Save solved grid with highlighted words
void save_solved_grid(Image* img, SearchResult* results, int result_count, 
                     const char* output_path) {
    if (!img || !img->data || !results || !output_path) {
        fprintf(stderr, "Erreur: paramètres invalides\n");
        return;
    }

    // Create output image copy
    Image* output = (Image*)malloc(sizeof(Image));
    if (!output) {
        fprintf(stderr, "Erreur: allocation mémoire\n");
        return;
    }
    
    output->width = img->width;
    output->height = img->height;
    output->channels = img->channels;
    output->data = (unsigned char*)malloc(img->width * img->height * img->channels);
    
    if (!output->data) {
        fprintf(stderr, "Erreur: allocation données image\n");
        free(output);
        return;
    }
    
    memcpy(output->data, img->data, img->width * img->height * img->channels);
    
    // Highlight parameters
    unsigned char highlight_color = 200;
    int line_thickness = 3;
    
    // Convert results to text format
    int line_count;
    char** text_lines = results_to_text_lines(results, result_count, &line_count);
    
    // Draw each found word
    for (int i = 0; i < result_count; i++) {
        if (results[i].found) {
            // Validate coordinates
            if (results[i].start.x < 0 || results[i].start.x >= output->width ||
                results[i].start.y < 0 || results[i].start.y >= output->height ||
                results[i].end.x < 0 || results[i].end.x >= output->width ||
                results[i].end.y < 0 || results[i].end.y >= output->height) {
                fprintf(stderr, "Warning: coordonnées invalides pour '%s'\n", 
                       results[i].word);
                continue;
            }
            
            printf("Dessin: '%s' de (%d,%d) à (%d,%d)\n",
                   results[i].word,
                   results[i].start.x, results[i].start.y,
                   results[i].end.x, results[i].end.y);
            
            draw_line(output, results[i].start, results[i].end, 
                     highlight_color, line_thickness);
        }
    }

    // Save image (PGM format)
    FILE* fp = fopen(output_path, "wb");
    if (fp) {
        fprintf(fp, "P5\n%d %d\n255\n", output->width, output->height);
        fwrite(output->data, 1, output->width * output->height, fp);
        fclose(fp);
        printf("✓ Image résolue sauvegardée: %s\n", output_path);
    } else {
        fprintf(stderr, "Erreur: impossible de sauvegarder l'image\n");
    }
    
    // Save text results
    char text_path[512];
    strncpy(text_path, output_path, 500);
    char* ext = strrchr(text_path, '.');
    if (ext) strcpy(ext, ".txt");
    else strcat(text_path, ".txt");
    
    write_text_file(text_path, text_lines, line_count);
    
    // Cleanup
    free_text_lines(text_lines, line_count);
    free(output->data);
    free(output);
}

// NOUVELLE FONCTION PRINCIPALE: Save recognized grid using NEURAL NETWORK
void save_recognized_grid_with_nn(LetterGrid* grid, Network* net, 
                                  const char* output_path) {
    if (!grid || !net || !output_path) {
        fprintf(stderr, "Erreur: paramètres invalides\n");
        return;
    }
    
    printf("\n╔════════════════════════════════════════╗\n");
    printf("║  RECONNAISSANCE PAR RÉSEAU DE NEURONES ║\n");
    printf("╚════════════════════════════════════════╝\n");
    
    // Utiliser le RDN pour reconnaître les lettres
    char** matrix = letter_grid_to_matrix_with_nn(grid, net);
    if (!matrix) {
        fprintf(stderr, "Erreur: conversion grille impossible\n");
        return;
    }
    
    // Sauvegarder au format solver.c
    save_grid_matrix(output_path, matrix, grid->rows, grid->cols);
    
    // Afficher un aperçu de la grille
    printf("\n=== Grille reconnue (aperçu) ===\n");
    int preview_rows = (grid->rows > 10) ? 10 : grid->rows;
    int preview_cols = (grid->cols > 20) ? 20 : grid->cols;
    
    for (int r = 0; r < preview_rows; r++) {
        for (int c = 0; c < preview_cols; c++) {
            printf("%c ", matrix[r][c]);
        }
        if (grid->cols > preview_cols) printf("...");
        printf("\n");
    }
    if (grid->rows > preview_rows) {
        printf("...\n");
    }
    printf("================================\n\n");
    
    free_matrix(matrix, grid->rows);
}