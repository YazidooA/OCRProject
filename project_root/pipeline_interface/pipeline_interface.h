#ifndef PIPELINE_INTERFACE_H
#define PIPELINE_INTERFACE_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "structure_detection.h"

// ========================================
//  STRUCTURES
// ========================================

/**
 * Extracted letter data (28x28 normalized)
 */
typedef struct {
    int id;                      // Letter index in grid
    int grid_row;                // Row position in grid
    int grid_col;                // Column position in grid
    int pixel_x;                 // Original pixel X coordinate
    int pixel_y;                 // Original pixel Y coordinate
    unsigned char pixels[784];   // 28x28 grayscale pixels
} ExtractedLetter;

/**
 * Result of extraction phase
 */
typedef struct {
    int count;                   // Total number of letters
    int rows;                    // Grid rows
    int cols;                    // Grid columns
    Rectangle grid_area;         // Grid position in image
    ExtractedLetter* letters;    // Array of extracted letters
    void* letter_grid_internal;  // Internal pointer (opaque)
} ExtractionResult;

/**
 * Pipeline context (opaque - for two-phase pipeline)
 */
typedef struct PipelineContext PipelineContext;

// ========================================
//  PHASE 1: EXTRACTION
// ========================================

/**
 * Extract letter grid from surface
 * Returns normalized 28x28 letters ready for neural network
 * 
 * @param surface Input image
 * @return Extraction result (must be freed with free_extraction_result)
 */
ExtractionResult* surface_to_letter_grid(SDL_Surface* surface);

/**
 * Free extraction result
 */
void free_extraction_result(ExtractionResult* result);

// ========================================
//  PHASE 1.5: NEURAL NETWORK RECOGNITION
// ========================================

/**
 * Recognize letters using trained neural network
 * 
 * @param extraction Result from surface_to_letter_grid
 * @param model_path Path to model.bin file
 * @return String of recognized letters (A-Z), must be freed with free()
 */
char* recognize_letters_with_nn(ExtractionResult* extraction, const char* model_path);

// ========================================
//  PHASE 2: SOLVING AND DRAWING
// ========================================

/**
 * Solve word search and draw outlines on image
 * 
 * @param surface Original surface to annotate
 * @param extraction Extraction result from phase 1
 * @param recognized_letters String of letters (A-Z) from neural network
 * @param words_to_find Array of words to search
 * @param word_count Number of words
 * @param grid_path Output path for grid file (for solver.c)
 * @return New surface with red outlines on found words
 */
SDL_Surface* pipeline_phase2_solve_and_draw(
    SDL_Surface* surface,
    ExtractionResult* extraction,
    const char* recognized_letters,
    const char** words_to_find,
    int word_count,
    const char* grid_path
);

// ========================================
//  COMPLETE PIPELINE (ONE CALL)
// ========================================

/**
 * Complete pipeline: preprocess + extract + recognize + solve + draw
 * 
 * @param surface Input image
 * @param model_path Path to trained neural network (model.bin)
 * @param words_to_find Array of words to search for
 * @param word_count Number of words in array
 * @param output_dir Directory for output files (grid_for_solver.txt)
 * @return Annotated surface with found words highlighted in red
 * 
 * Example:
 *   const char* words[] = {"HELLO", "WORLD"};
 *   SDL_Surface* result = pipeline_complete(surface, "model.bin", words, 2, ".");
 */
SDL_Surface* pipeline_complete(
    SDL_Surface* surface,
    const char* model_path,
    const char** words_to_find,
    int word_count,
    const char* output_dir
);

// ========================================
//  TWO-PHASE PIPELINE (FOR EXTERNAL IA)
// ========================================

/**
 * Phase 1: Preprocessing + Extraction + CSV generation
 * Use this if you want to call external IA between phases
 * 
 * @param input_surface Input image
 * @param output_dir Directory for CSV file
 * @return Pipeline context (keep for phase 2)
 * 
 * Generated files:
 *   - output_dir/grid_for_ia.csv     (for IA input)
 */
PipelineContext* ibrahim_pipeline_phase1(
    SDL_Surface* input_surface,
    const char* output_dir
);

/**
 * Phase 2: Solving + Drawing with recognized letters
 * Call this after your IA returns the recognized letters
 * 
 * @param ctx Context from phase 1
 * @param recognized_letters String of A-Z letters from your IA
 * @param words_to_find Array of words to search
 * @param word_count Number of words
 * @return Annotated surface with found words
 * 
 * Generated files:
 *   - output_dir/grid_for_solver.txt (grid in text format)
 */
SDL_Surface* ibrahim_pipeline_phase2(
    PipelineContext* ctx,
    const char* recognized_letters,
    const char** words_to_find,
    int word_count
);

/**
 * Free pipeline context
 */
void free_pipeline_context(PipelineContext* ctx);

// ========================================
//  UTILITY FUNCTIONS
// ========================================

/**
 * Preprocess surface: grayscale + otsu + noise removal
 * 
 * @param original Input surface
 * @return Preprocessed surface (must be freed)
 */
SDL_Surface* preprocess_surface(SDL_Surface* original);

#endif /* PIPELINE_INTERFACE_H */