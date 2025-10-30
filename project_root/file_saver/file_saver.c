#include "file_saver.h"
#include "letter_extractor.h"
#include "draw_outline.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

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

// Convert LetterGrid recognized letters to matrix string format
char** letter_grid_to_matrix(LetterGrid* grid, char* recognized_letters) {
    if (!grid || !recognized_letters) return NULL;
    
    char** matrix = (char**)malloc(grid->rows * sizeof(char*));
    for (int i = 0; i < grid->rows; i++) {
        matrix[i] = (char*)malloc((grid->cols + 1) * sizeof(char));
        matrix[i][grid->cols] = '\0';
    }
    
    for (int i = 0; i < grid->count; i++) {
        int row = grid->letters[i].grid_row;
        int col = grid->letters[i].grid_col;
        
        if (row >= 0 && row < grid->rows && col >= 0 && col < grid->cols) {
            matrix[row][col] = recognized_letters[i];
        }
    }
    
    return matrix;
}

// Save grid matrix to text file (format for solver.c)
void save_grid_matrix(const char* filename, char** matrix, int rows, int cols) {
    FILE* fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "Erreur: impossible d'ouvrir %s\n", filename);
        return;
    }
    
    fprintf(fp, "%d %d\n", rows, cols);
    
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

// Draw line using Bresenham (kept for compatibility, but could use draw_outline)
static void draw_line_bresenham(Image* img, Position start, Position end, 
                                unsigned char color, int thickness) {
    int dx = abs(end.x - start.x);
    int dy = abs(end.y - start.y);
    int sx = start.x < end.x ? 1 : -1;
    int sy = start.y < end.y ? 1 : -1;
    int err = dx - dy;
    
    int x = start.x;
    int y = start.y;
    
    while (1) {
        for (int ty = -thickness/2; ty <= thickness/2; ty++) {
            for (int tx = -thickness/2; tx <= thickness/2; tx++) {
                int px = x + tx;
                int py = y + ty;
                if (px >= 0 && px < img->width && py >= 0 && py < img->height) {
                    img->data[py * img->width + px] = color;
                }
            }
        }
        
        if (x == end.x && y == end.y) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
}

// Save solved grid with highlighted words (old version - kept for compatibility)
void save_solved_grid(Image* img, SearchResult* results, int result_count, 
                     const char* output_path) {
    if (!img || !img->data || !results || !output_path) {
        fprintf(stderr, "Erreur: paramètres invalides\n");
        return;
    }

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
    
    unsigned char highlight_color = 200;
    int line_thickness = 3;
    
    int line_count;
    char** text_lines = results_to_text_lines(results, result_count, &line_count);
    
    for (int i = 0; i < result_count; i++) {
        if (results[i].found) {
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
            
            draw_line_bresenham(output, results[i].start, results[i].end, 
                               highlight_color, line_thickness);
        }
    }

    FILE* fp = fopen(output_path, "wb");
    if (fp) {
        fprintf(fp, "P5\n%d %d\n255\n", output->width, output->height);
        fwrite(output->data, 1, output->width * output->height, fp);
        fclose(fp);
        printf("✓ Image résolue sauvegardée: %s\n", output_path);
    } else {
        fprintf(stderr, "Erreur: impossible de sauvegarder l'image\n");
    }
    
    char text_path[512];
    strncpy(text_path, output_path, 500);
    char* ext = strrchr(text_path, '.');
    if (ext) strcpy(ext, ".txt");
    else strcat(text_path, ".txt");
    
    write_text_file(text_path, text_lines, line_count);
    
    free_text_lines(text_lines, line_count);
    free(output->data);
    free(output);
}

// Save recognized grid for solver.c input
void save_recognized_grid_for_solver(LetterGrid* grid, char* recognized_letters, 
                                     const char* output_path) {
    if (!grid || !recognized_letters || !output_path) {
        fprintf(stderr, "Erreur: paramètres invalides pour save_recognized_grid\n");
        return;
    }
    
    char** matrix = letter_grid_to_matrix(grid, recognized_letters);
    if (!matrix) {
        fprintf(stderr, "Erreur: conversion grille impossible\n");
        return;
    }
    
    save_grid_matrix(output_path, matrix, grid->rows, grid->cols);
    
    printf("\n=== Grille reconnue (RDN) ===\n");
    for (int r = 0; r < grid->rows; r++) {
        for (int c = 0; c < grid->cols; c++) {
            printf("%c ", matrix[r][c]);
        }
        printf("\n");
    }
    printf("=============================\n\n");
    
    free_matrix(matrix, grid->rows);
}

// NEW: Complete RDN output with SDL-based drawing (uses draw_outline.c)
RDNOutput save_complete_rdn_output(Image* img, SearchResult* results, 
                                    int result_count, const char* base_output_path) {
    RDNOutput output = {0};
    
    if (!img || !img->data || !results || !base_output_path) {
        fprintf(stderr, "Erreur: paramètres invalides pour RDN output\n");
        return output;
    }
    
    // Generate output paths
    snprintf(output.image_path, sizeof(output.image_path), "%s_solved.png", base_output_path);
    snprintf(output.text_path, sizeof(output.text_path), "%s_results.txt", base_output_path);
    
    // Count found words
    output.words_total = result_count;
    output.words_found = 0;
    for (int i = 0; i < result_count; i++) {
        if (results[i].found) output.words_found++;
    }
    output.success_rate = result_count > 0 ? 
        (100.0f * output.words_found / output.words_total) : 0.0f;
    
    // === Save detailed text file ===
    FILE* txt = fopen(output.text_path, "w");
    if (txt) {
        fprintf(txt, "=== RDN - Résultats de la recherche ===\n");
        fprintf(txt, "Mots trouvés: %d/%d (%.1f%%)\n\n", 
                output.words_found, output.words_total, output.success_rate);
        
        fprintf(txt, "=== Mots trouvés ===\n");
        for (int i = 0; i < result_count; i++) {
            if (results[i].found) {
                fprintf(txt, "✓ %s: (%d,%d) -> (%d,%d)\n",
                       results[i].word,
                       results[i].start.x, results[i].start.y,
                       results[i].end.x, results[i].end.y);
            }
        }
        
        fprintf(txt, "\n=== Mots non trouvés ===\n");
        int not_found = 0;
        for (int i = 0; i < result_count; i++) {
            if (!results[i].found) {
                fprintf(txt, "✗ %s\n", results[i].word);
                not_found++;
            }
        }
        if (not_found == 0) {
            fprintf(txt, "(aucun)\n");
        }
        
        fclose(txt);
        printf("✓ Résultats texte sauvegardés: %s\n", output.text_path);
    }
    
    // === Save image with SDL (using draw_outline for better quality) ===
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "Erreur SDL_Init: %s\n", SDL_GetError());
        return output;
    }
    
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        fprintf(stderr, "Erreur IMG_Init: %s\n", IMG_GetError());
        SDL_Quit();
        return output;
    }
    
    // Convert grayscale Image to SDL_Surface
    SDL_Surface* surface = SDL_CreateRGBSurface(0, img->width, img->height, 32,
                                                0xFF000000, 0x00FF0000, 
                                                0x0000FF00, 0x000000FF);
    if (!surface) {
        fprintf(stderr, "Erreur création surface: %s\n", SDL_GetError());
        IMG_Quit();
        SDL_Quit();
        return output;
    }
    
    // Copy grayscale data to RGB surface
    uint32_t* pixels = (uint32_t*)surface->pixels;
    for (int y = 0; y < img->height; y++) {
        for (int x = 0; x < img->width; x++) {
            unsigned char gray = img->data[y * img->width + x];
            pixels[y * img->width + x] = (gray << 16) | (gray << 8) | gray | 0xFF000000;
        }
    }
    
    // Create renderer
    SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(surface);
    if (!renderer) {
        fprintf(stderr, "Erreur création renderer: %s\n", SDL_GetError());
        SDL_FreeSurface(surface);
        IMG_Quit();
        SDL_Quit();
        return output;
    }
    
    // Draw outlines for found words (using draw_outline.c)
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // Red
    for (int i = 0; i < result_count; i++) {
        if (results[i].found) {
            printf("Dessin outline: '%s' de (%d,%d) à (%d,%d)\n",
                   results[i].word,
                   results[i].start.x, results[i].start.y,
                   results[i].end.x, results[i].end.y);
            
            draw_outline(renderer,
                        results[i].start.x, results[i].start.y,
                        results[i].end.x, results[i].end.y,
                        15,  // width
                        3);  // stroke
        }
    }
    
    // Save PNG
    if (IMG_SavePNG(surface, output.image_path) != 0) {
        fprintf(stderr, "Erreur sauvegarde PNG: %s\n", IMG_GetError());
    } else {
        printf("✓ Image RDN sauvegardée: %s\n", output.image_path);
    }
    
    // Cleanup
    SDL_DestroyRenderer(renderer);
    SDL_FreeSurface(surface);
    IMG_Quit();
    SDL_Quit();
    
    // Summary
    printf("\n=== RDN OUTPUT SUMMARY ===\n");
    printf("Image: %s\n", output.image_path);
    printf("Texte: %s\n", output.text_path);
    printf("Taux de réussite: %.1f%% (%d/%d mots)\n", 
           output.success_rate, output.words_found, output.words_total);
    printf("==========================\n\n");
    
    return output;
}