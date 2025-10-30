#ifndef GRID_RECONSTRUCTION_H
#define GRID_RECONSTRUCTION_H

#include "../common.h"
#include "solver.h"
#include "../neural_network/neural_network.h"
#include "../image_processing/character_segmentation.h"

// Reconstruction de grille depuis OCR
WordGrid* reconstruct_grid_from_characters(Character* chars, int char_count, int expected_rows, int expected_cols);
WordGrid* reconstruct_grid_from_image(const Image* img, NeuralNetwork* nn);

// Validation et correction
bool validate_reconstructed_grid(const WordGrid* grid);
void correct_grid_errors(WordGrid* grid);
void apply_character_corrections(WordGrid* grid, double confidence_threshold);

// Organisation spatiale
void sort_characters_by_position(Character* chars, int count);
int detect_grid_dimensions(Character* chars, int count, int* rows, int* cols);
void organize_characters_into_grid(Character* chars, int count, WordGrid* grid);

// Post-traitement
void clean_grid_characters(WordGrid* grid);
void fill_missing_characters(WordGrid* grid);

#endif // GRID_RECONSTRUCTION_H