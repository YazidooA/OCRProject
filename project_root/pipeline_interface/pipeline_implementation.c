#include "pipeline_interface.h"
#include "structure_detection.h"
#include "letter_extractor.h"
#include "solver.h"
#include "draw_outline.h"
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

// Implementation of surface_to_csv_for_ia
ExtractionResult* surface_to_csv_for_ia(SDL_Surface* surface, const char* csv_output_path) {
    if (!surface || !csv_output_path) return NULL;
    
    printf("\n=== Phase 1: Extraction ===\n");
    
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
    
    // Create result structure
    ExtractionResult* result = (ExtractionResult*)malloc(sizeof(ExtractionResult));
    result->count = cell_count;
    result->rows = rows;
    result->cols = cols;
    result->grid_area = grid;
    result->letters = (ExtractedLetter*)malloc(cell_count * sizeof(ExtractedLetter));
    
    // Open CSV file
    FILE* csv = fopen(csv_output_path, "w");
    if (!csv) {
        free(cells);
        free_image_local(img);
        free(result->letters);
        free(result);
        return NULL;
    }
    
    // Write CSV header
    fprintf(csv, "id");
    for (int i = 0; i < 784; i++) fprintf(csv, ",p%d", i);
    fprintf(csv, ",label\n");
    
    // Extract each letter
    for (int i = 0; i < cell_count; i++) {
        result->letters[i].id = i;
        result->letters[i].grid_row = i / cols;
        result->letters[i].grid_col = i % cols;
        result->letters[i].pixel_x = cells[i].x + cells[i].width / 2;
        result->letters[i].pixel_y = cells[i].y + cells[i].height / 2;
        
        // Extract 28x28 pixels (simplified - just take center region)
        for (int p = 0; p < 784; p++) {
            int py = p / 28;
            int px = p % 28;
            int src_x = cells[i].x + (px * cells[i].width) / 28;
            int src_y = cells[i].y + (py * cells[i].height) / 28;
            
            if (src_x < img->width && src_y < img->height) {
                result->letters[i].pixels[p] = img->data[src_y * img->width + src_x];
            } else {
                result->letters[i].pixels[p] = 255;
            }
        }
        
        // Write to CSV
        fprintf(csv, "%d", i);
        for (int p = 0; p < 784; p++) {
            fprintf(csv, ",%d", result->letters[i].pixels[p]);
        }
        fprintf(csv, ",0\n"); // dummy label
    }
    
    fclose(csv);
    free(cells);
    free_image_local(img);
    
    printf("✓ CSV written: %s (%d letters)\n", csv_output_path, cell_count);
    
    return result;
}

// Free extraction result
void free_extraction_result(ExtractionResult* result) {
    if (result) {
        if (result->letters) free(result->letters);
        free(result);
    }
}

// Implementation of pipeline_phase2_solve_and_draw
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
            
            printf("✓ Found '%s' at (%d,%d)->(%d,%d)\n", upper_word, out[0], out[1], out[2], out[3]);
            
            // Draw outline
            draw_outline(renderer, px1, py1, px2, py2, cell_h/2, 3);
        } else {
            printf("✗ Not found: '%s'\n", upper_word);
        }
    }
    
    SDL_DestroyRenderer(renderer);
    freeMatrix(matrix, rows);
    
    return result;
}
