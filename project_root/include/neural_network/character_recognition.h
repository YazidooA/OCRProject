// ===========================================
#ifndef CHARACTER_RECOGNITION_H
#define CHARACTER_RECOGNITION_H

#include "../common.h"
#include "neural_network.h"
#include "../image_processing/character_segmentation.h"

// Reconnaissance de caractères
char recognize_character(NeuralNetwork* nn, const Image* char_img);
double* get_character_probabilities(NeuralNetwork* nn, const Image* char_img);

// Préparation des données
double* image_to_vector(const Image* img);
double* normalize_input_vector(double* vector, int size);

// Utilitaires OCR
char get_most_likely_character(const double* probabilities);
double get_recognition_confidence(const double* probabilities);
bool is_confident_recognition(const double* probabilities, double threshold);

// Correction d'erreurs
char* correct_word_spelling(const char* recognized_word, const char** dictionary, int dict_size);
void apply_context_correction(char* recognized_text, int length);

#endif // CHARACTER_RECOGNITION_H