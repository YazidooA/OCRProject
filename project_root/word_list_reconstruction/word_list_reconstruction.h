#ifndef WORD_LIST_RECONSTRUCTION_H
#define WORD_LIST_RECONSTRUCTION_H

#include "../common.h"
#include "solver.h"
#include "../neural_network/neural_network.h"
#include "../image_processing/character_segmentation.h"

// Reconstruction de liste de mots
WordList* reconstruct_word_list_from_characters(Character* chars, int char_count);
WordList* reconstruct_word_list_from_image(const Image* img, NeuralNetwork* nn);

// Segmentation de mots
char** segment_characters_into_words(Character* chars, int char_count, int* word_count);
void detect_word_boundaries(Character* chars, int count, int** boundaries, int* boundary_count);

// Validation de mots
bool is_valid_word(const char* word);
void correct_word_errors(char* word);
void apply_dictionary_correction(WordList* word_list, const char** dictionary, int dict_size);

// Post-traitement
void clean_word_list(WordList* word_list);
void remove_duplicate_words(WordList* word_list);
void sort_words_alphabetically(WordList* word_list);

#endif // WORD_LIST_RECONSTRUCTION_H
