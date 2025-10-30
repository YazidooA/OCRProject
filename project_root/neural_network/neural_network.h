#ifndef NEURAL_NETWORK_H
#define NEURAL_NETWORK_H

#include "../common.h"

#define MAX_LAYERS 10
#define INPUT_SIZE 784  // 28x28 pixels
#define OUTPUT_SIZE 26  // 26 lettres A-Z

typedef struct {
    double* weights;
    double* biases;
    double* activations;
    double* deltas;
    int input_size;
    int output_size;
} Layer;

typedef struct {
    Layer* layers;
    int layer_count;
    double learning_rate;
    double momentum;
    int epochs_trained;
} NeuralNetwork;

// Création et destruction
NeuralNetwork* create_network(int input_size, int output_size, int hidden_size);
NeuralNetwork* create_network_advanced(int* layer_sizes, int layer_count);
void free_network(NeuralNetwork* nn);

// Propagation
double* forward_pass(NeuralNetwork* nn, const double* input);
void backward_pass(NeuralNetwork* nn, const double* input, const double* expected);

// Sauvegarde et chargement
ErrorCode save_network(const NeuralNetwork* nn, const char* filename);
NeuralNetwork* load_network(const char* filename);

// Utilitaires
void randomize_weights(NeuralNetwork* nn);
void print_network_info(const NeuralNetwork* nn);

#endif // NEURAL_NETWORK_H