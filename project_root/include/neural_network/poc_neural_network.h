#ifndef POC_NEURAL_NETWORK_H
#define POC_NEURAL_NETWORK_H

#include "../common.h"
#include "neural_network.h"

// Preuve de concept pour la fonction XOR (A.B + A̅.B̅)
void test_xor_function(void);
void test_and_function(void);
void test_or_function(void);

// Fonctions de test
NeuralNetwork* create_xor_network(void);
void train_xor_network(NeuralNetwork* nn);
void test_xor_network(NeuralNetwork* nn);

// Utilitaires de démonstration
void print_truth_table(NeuralNetwork* nn, const char* function_name);
void demonstrate_learning_progress(NeuralNetwork* nn, int epochs);

#endif // POC_NEURAL_NETWORK_H